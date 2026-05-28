# Agent Quick Reference - ESP32 Inverter Bridge

This document provides project context for AI agents working on the ESP32 inverter WiFi-to-Ethernet bridge.

## Skills-First Policy

**Always use skills for operational tasks.** Skills are in `skills/` and each has a `SKILL.md` with instructions.

| Task | Skill |
|------|-------|
| Compile + upload firmware | `skills/firmware-upload/` |
| Analyze logs, plot power | `skills/log-analysis/` |
| Validate API vs docs | `skills/api-validation/` |
| Optimize firmware (write → upload → validate loop) | `skills/firmware-optimization-loop/` |
| Compare WiFi strategies (dwell vs auto) | `skills/strategy-comparison/` |
| Determine which docs to update after a code change | `skills/documentation-update/` |
| Create or audit skill definitions | `skills/create-agent-skills/` |

**Python venv**: Always activate before running scripts: `& d:\git\MastervoltBridge\.venv\Scripts\Activate.ps1`  
**Package installs**: Use `uv pip install <pkg>` (not plain pip).

## Project Summary

**What**: Bridge firmware connecting WiFi-only inverters (Mastervolt SOLADIN 1500) to Home Assistant via Ethernet.

**Hardware**: ESP32-S3 + ENC28J60 Ethernet adapter
- WiFi: Station mode → inverter SSID `mastervolt-soladin-0103` / `10.0.0.1`
- Ethernet: DHCP client on home LAN → `192.168.1.48:8080`
- GPIO 36: inverter WiFi wake pulse (idle HIGH, active-LOW pulse)

## Architecture

### Module Dependency Graph

```
wifi_bridge.cpp/h (core network layer)
  ├── WifiConnectionManager singleton (ensureConnected, forceReconnect)
  ├── connectWifiDwell(), connectWifiAuto(), connectWifi(bool)
  ├── fetchInverterData(method, path, body, ..., contentType)
  └── deps: WiFi.h, HTTPClient.h, settings

inverter_data.cpp/h (data model)
  └── HomeData struct + getInverterData() factory

inverter_link_state.cpp/h (link-state FSM)
  ├── InverterLinkState enum: STARTING, ONLINE, RETRYING, BACKOFF, DORMANT
  ├── setInverterState() — sets state + dispatches hooks
  ├── Hook registry: registerStateChangeHook(), registerStateEntryHook()
  └── Thresholds defined in settings.h

inverter_controller.cpp/h (business logic)
  ├── InverterController singleton — FreeRTOS polling task (20s base interval)
  ├── calls WifiConnectionManager::ensureConnected() before each poll
  ├── linkStateFromStreak() — reconciles FSM state from failure duration
  ├── Hook callbacks: applyIntervalForState, loadSettingsOnBoot, updateAllInverterParam
  ├── SetResult enum (Applied, Deferred, Rejected) for setPower/setShadow
  ├── caches HomeData + cached shadow/power_limit (fetched from inverter on recovery)
  └── exposes getLatestHomeData(), setPower(), setShadow(), fetchPath(), getShadow(), getPowerLimit()

ethernet_bridge.cpp/h (network layer)
  └── ENC28J60 init + HTTP API server on port 8080

api.cpp / api.h (request routing)
  ├── handleApiClient() — routes all REST endpoints
  └── deps: api_helper, InverterController, inverter_data, web_ui

api_helper.cpp/h (HTTP/JSON utilities)
  └── sendHttpResponse(), sendFlashHtmlResponse(), sendLogsResponse(), buildHealthJson(), buildInfoJson(), etc.

web_ui.h (self-contained HTML dashboard)
  └── WEB_UI_HTML[] PROGMEM raw literal (~7.7 KB), WEB_UI_HTML_LEN, static_assert < 16 KB

esp32_inverter_bridge.ino (main entry)
  └── starts ethernetBridgeInit() + InverterController::initialize()

settings.cpp/h — all global constants (7 logical sections)
logger.h — 1000-entry circular log buffer with ms timestamps
```

### Key Design Pattern: WifiConnectionManager

**Single entry point for all WiFi connections. Never call `WiFi.status()` directly.**

```cpp
class WifiConnectionManager {
  static WifiConnectionManager& getInstance();
  bool ensureConnected();   // no-op if connected; else pulses GPIO + connectWifi(alternating)
  bool forceReconnect();    // always pulses + reconnects (used by /pulse endpoint)
private:
  bool nextUseDwell_ = true; // alternates dwell/auto on every attempt
};
```

**Two connect paths (A/B performance data collected passively in production):**
- `connectWifiDwell()` — 200 ms scan dwell, AP-hint fallback. ~5 s typical.
- `connectWifiAuto()` — 500 ms scan dwell, pure auto-discovery. ~6.5 s typical.

Both emit `[WIFI-CONNECT] start/complete path=... duration_ms=... result=...` log entries.

## Data Model

### HomeData (inverter_data.h)
```cpp
struct HomeData {
  String operatingStatus;     // "1" = normal
  String errorAlarmCode;      // "0" = no error
  String operatingMode;       // "1" = production
  String inverterModel;       // "H500A0103"
  String inverterMacAddress;
  String instantaneousPower;  // watts, e.g. "674.547"
  String lifetimeEnergy;      // kWh,   e.g. "08566.628"
  String dailySessionEnergy;  // kWh,   e.g. "12.811"
  bool isValid() const;
};
```

> Note: JSON field names in `/api/info` responses differ — `power`, `total_yield`, `daily_yield` (see `api_helper.cpp::buildInfoJson`).

Parsed from inverter `GET /home`: 8 newline-delimited fields.

### InverterController public interface (inverter_controller.h)
```cpp
static InverterController& getInstance();
void initialize();
void shutdown();
bool getLatestHomeData(HomeData& dataOut);
unsigned long getLastUpdateMs();

enum class SetResult { Applied, Deferred, Rejected };
SetResult setPower(int watts, String& responseBody, int& httpCode, String& errorMessage);
SetResult setShadow(bool enabled, String& responseBody, int& httpCode, String& errorMessage);
bool fetchPath(const String& path, String& responseBody, int& httpCode, String& errorMessage);

InverterLinkState getLinkState();
uint32_t getFailureStreakMs();
uint32_t getRetryIntervalMs();
uint32_t getBasePollIntervalMs();
void setBasePollIntervalMs(uint32_t ms);
bool getShadow(bool& enabledOut);       // cached shadow function state
bool getPowerLimit(uint16_t& wattsOut); // cached inverter power limit
bool hasPendingSettings();              // true if deferred values are queued
```
`dataMutex` protects all cached data; all lock attempts timeout after 10 ms (`DATA_MUTEX_TIMEOUT_MS` in `settings.cpp`).

### InverterLinkState FSM (inverter_link_state.h)

Five-state FSM driven by the polling failure streak duration:

| State | Poll Interval | Entry Condition |
|---|---|---|
| `STARTING` | 20 s (base) | Boot; no successful poll yet |
| `ONLINE` | 20 s (base) | Last poll succeeded |
| `RETRYING` | 20 s (base) | Failure streak < 5 min |
| `BACKOFF` | 1 min | Failure streak 5–20 min |
| `DORMANT` | 10 min | Failure streak >= 20 min |

**Hook architecture** — all state-dependent behavior is driven by hooks registered in `InverterController::initialize()`:
- **Entry hooks** (`applyIntervalForState`) — adjust poll interval on any state entry.
- **Transition hooks** — `STARTING→ONLINE` loads settings on first boot; `BACKOFF/DORMANT→ONLINE` refreshes cached settings after recovery.

Register hooks at compile time using macros:
```cpp
REGISTER_STATE_ENTRY_HOOK(InverterLinkState::BACKOFF, applyIntervalForState);
REGISTER_STATE_CHANGE_HOOK(InverterLinkState::STARTING, InverterLinkState::ONLINE, loadSettingsOnBoot);
```

## Configuration (settings.cpp)

All tunable constants are in `settings.cpp/h`, organized in 7 logical sections:

1. **Inverter WiFi** — SSID, password, host IP, AP hint settings
2. **WiFi Connection Strategy** — timeouts, retries, dwell/auto scan parameters
3. **Polling & Link-State FSM** — base interval, HTTP timeout, streak thresholds
4. **Hardware Pins** — SPI mapping, wake pulse GPIO (canonical detail in `docs/WIRING_README.md`)
5. **Ethernet & API Server** — port, client timeout, DHCP hostname
6. **Power & System** — max power watts, main loop sleep
7. **Runtime State** — shared globals (apiServer, appLogger, debugMode)

Read `settings.h` directly for current values. Do not duplicate constant values in documentation.

## Gotchas

1. **Never call `WiFi.status()` directly** — use `WifiConnectionManager::getInstance().ensureConnected()`. It pulses and reconnects; `WiFi.status()` does neither.
2. **Include order**: `inverter_controller.cpp` needs `wifi_bridge.h` before `fetchInverterData()`.
3. **Mutex timeouts**: Keep lock time short. Threads waiting >10 ms will fail silently (`DATA_MUTEX_TIMEOUT_MS = 10` in `settings.cpp`).
4. **502 = inverter call failed** (usually WiFi not connected). Check `/api/health` first. `/api/info` now always returns HTTP 200 (with empty telemetry fields before first successful poll).
5. **Pulse state machine**: single press toggles WiFi. `/wifi/off` = one press (OFF). `/pulse` = double-press (OFF→ON). Calling `/pulse` while connected will disconnect then reconnect.
6. **Power limit**: `setPower()` validates 0–`INVERTER_MAX_POWER_WATTS` before the HTTP request.
7. **ENC28J60 crash on dead TCP write**: Writing to a disconnected `EthernetClient` via UIPEthernet crashes the device. Always check `client.connected()` before `client.write()` in streaming responses (see `sendLogsResponse()` abort guard).
8. **CDCOnBoot=cdc resets on serial open**: Opening COM9 with DTR=true resets the ESP32. Use `dsrdtr=False, dtr=False` in pyserial to monitor without reset.
9. **Changing FQBN flags triggers full rebuild**: Adding/removing flags like `CDCOnBoot=cdc` causes a ~5 min full core recompile. Same flags = fast incremental build (~20s).

## Performance

| Metric | Value |
|---|---|
| First poll after boot | ~6–8 s |
| Reconnect time (dwell path) | ~5 s typical |
| Reconnect time (auto path) | ~6.5 s typical |
| Connect budget (timeout) | 7 s |
| Polling interval | 20 s |
| HTTP request timeout | 3.5 s |
| API response (cached data) | <100 ms |
| `/api/logs` (1000 entries, 58 KB) | ~7.5 s |
| Heap used / available | ~65 KB used / ~300 KB free |

## File Map

**Firmware** (`firmware/esp32_inverter_bridge/`):
`esp32_inverter_bridge.ino`, `wifi_bridge.{h,cpp}`, `inverter_data.{h,cpp}`, `inverter_link_state.{h,cpp}`, `inverter_controller.{h,cpp}`, `ethernet_bridge.{h,cpp}`, `api.{h,cpp}`, `api_helper.{h,cpp}`, `web_ui.h`, `settings.{h,cpp}`, `logger.h`

**Documentation** (`docs/`):
`API_REFERENCE.md`, `SETUP_README.md`, `WIRING_README.md`, `ESP32_UPLOAD_README.md`, `TEST_README.md`

**Skills** (`skills/`):
`firmware-upload/`, `firmware-optimization-loop/`, `log-analysis/`, `api-validation/`, `documentation-update/`, `strategy-comparison/`, `create-agent-skills/`
