# Agent Quick Reference - ESP32 Inverter Bridge

This document provides project context for AI agents and future developers working on the ESP32 inverter WiFi-to-Ethernet bridge.

## Project Summary

What: Bridge firmware connecting WiFi-only inverters (Mastervolt SOLADIN 1500) to Home Assistant via Ethernet.

Hardware: ESP32-S3 + ENC28J60 Ethernet adapter
- WiFi: Station mode -> inverter SSID `mastervolt-soladin-0103` / `10.0.0.1`
- Ethernet: DHCP client on home LAN -> port 8080 (IP assigned by DHCP)
- GPIO 36: inverter WiFi wake pulse (active HIGH)

## Architecture

### Module Dependency Graph

```
wifi_bridge.cpp/h (core network layer)
  |- Dedicated connection worker task (FreeRTOS)
  |- requestWifiConnection(), forceWifiReconnect(), fetchInverterData(..., waitForConnection)
  |- ScopedWifiOperationLock for WiFi HTTP/connect serialization
  |- deps: WiFi.h, HTTPClient.h, settings, lock_guard

inverter_fetch.cpp/h (raw HTTP/1.0 page fetch)
  |- fetchInverterPage(path, responseBody, httpCode, errorMessage, waitForConnection)
  |- Works for all inverter endpoints (data + HTML/JS/CSS files)
  |- Uses wifiOperationMutex from wifi_bridge via lock_guard for serialization

inverter_data.cpp/h (data model)
  |- HomeData struct + getInverterData() factory

inverter_monitor.cpp/h (business logic)
  |- InverterMonitor singleton - FreeRTOS polling task (runtime-configurable; default 20s)
  |- Polls /home via fetchInverterData(..., waitForConnection=true)
  |- Power limit: cached from inverter via refreshPowerLimit(), queued command on WiFi failure
  |- Shadow function: setShadow(), fetchShadowState()
  |- Link state machine: STARTING / ONLINE / RETRYING / BACKOFF / DORMANT
  |    transitions fire onLinkStateTransition(from, to, streakMs)
  |    bound action: BACKOFF|DORMANT -> ONLINE = refreshPowerLimit + applyPendingPowerCommand
  |- caches HomeData; exposes getLatestHomeData(), setPower(), setShadow(),
  |    getCachedPowerLimit(), getLinkState(), getFailureStreakMs(), getRetryIntervalMs()

ethernet_bridge.cpp/h (network layer)
  |- ENC28J60 init + HTTP API server on port 8080

api.cpp / api.h (request routing)
  |- handleApiClient() - 14 REST endpoints
  |- /api/power returns 200 (immediate) or 202 (queued)
  |- deps: api_helper, InverterMonitor, inverter_fetch, inverter_data

api_helper.cpp/h (HTTP/JSON utilities)
  |- sendHttpResponse(), sendLogsResponse(), buildHealthJson(), buildInfoJson(), etc.
  |- /api/info includes power_limit object (watts/queued)

lock_guard.cpp/h (locking infrastructure)
  |- ScopedLock<LockRank> RAII guard with lock-ordering validation
  |- LockRank enum: WIFI_OPERATION, DATA, POWER_STATE, POLLING_CONFIG
  |- Runtime violation detection logged as [LOCK-HIERARCHY] VIOLATION

esp32_inverter_bridge.ino (main entry)
  |- calls ethernetBridgeInit() + wifiBridgeInit() + InverterMonitor::initialize()

settings.cpp/h - all global constants
logger.h - 1000-entry circular log buffer with ms timestamps
```

### Connection Architecture

Single entry point for inverter HTTP calls:

```cpp
bool fetchInverterData(const String& method,
                       const String& path,
                       const String& body,
                       String& responseBody,
                       int& httpCode,
                       String& errorMessage,
                       bool waitForConnection);
```

Behavior:
- `waitForConnection=true`: blocks until connection worker finishes retries (used by polling loop).
- `waitForConnection=false`: fail-fast if WiFi is down, but triggers background reconnect (used by API requests).

Connection worker notes:
- Triggered by event-group request bit.
- Owns pulse + connect attempts.
- Uses alternating paths (`dwell`, `auto`) across attempts.
- Logs `[WIFI-CONNECT] start/complete ...` for analysis.

Connect paths:
- `dwell`: 200 ms scan dwell, AP-hint fallback enabled.
- `auto`: 500 ms scan dwell, auto-discovery only.

## API Endpoints

| Method | Path | Purpose |
|--------|------|---------|
| GET | `/` | API discovery |
| GET | `/api/version` | Firmware version currently running on this ESP |
| GET | `/api/health` | Bridge status (WiFi, Ethernet, IPs, inverter link state) |
| GET | `/api/logs` | Log buffer (1000 entries) |
| GET | `/api/info` | Latest cached inverter telemetry + power_limit state |
| POST | `/api/polling` | Set monitor polling interval in seconds (1-3600) |
| POST | `/api/power` | Set inverter power (0-1575 W), 200 immediate or 202 queued |
| POST | `/api/shadow` | Set shadow function: `{"enabled":true}` or `{"enabled":false}` |
| GET | `/api/shadow` | Read current shadow function state from inverter |
| GET | `/api/ha` | Home Assistant integration: numeric power, yields, power limit |
| POST | `/api/inverter/fetch` | Fetch arbitrary inverter path (raw response) |
| POST | `/wifi/off` | Single press to turn inverter WiFi off if connected |
| GET | `/pulse` | GPIO pulse sequence wake + forced reconnect; returns `{"reconnected": bool}` |
| POST | `/api/debug` | Enable/disable debug mode; returns `{"debug": bool}` |

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

### InverterMonitor public interface (inverter_monitor.h)
```cpp
static InverterMonitor& getInstance();
void initialize();
void shutdown();
bool getLatestHomeData(HomeData& dataOut);
unsigned long getLastUpdateMs();
bool setPollingIntervalSeconds(uint32_t seconds, uint32_t& appliedMs, String& errorMessage);
uint32_t getPollingIntervalMs();
bool setPower(int watts, String& responseBody, int& httpCode, String& errorMessage);
bool isPowerCommandQueued();
int getCachedPowerLimit();
int fetchPowerLimit(String& errorMessage);
bool setShadow(bool enabled, String& responseBody, int& httpCode, String& errorMessage);
int fetchShadowState(String& errorMessage);
InverterLinkState getLinkState();
uint32_t getFailureStreakMs();
uint32_t getRetryIntervalMs();
```

Synchronization:
- `dataMutex` protects cached telemetry and poll counters.
- `powerStateMutex` protects power state (cachedPowerLimit, queuedPowerWatts).
- `pollingConfigMutex` protects polling interval and link-state snapshot.

## Configuration (settings.cpp)

| Constant | Default | Purpose |
|---|---|---|
| `INVERTER_WIFI_SSID` | `"mastervolt-soladin-0103"` | Inverter WiFi SSID |
| `INVERTER_WIFI_PASSWORD` | `""` | Empty = open network |
| `INVERTER_WIFI_AP_HINT_ENABLED` | `true` | AP hint optimization for faster WiFi association |
| `INVERTER_HOST` | `"10.0.0.1"` | Inverter IP |
| `API_PORT` | `8080` | Ethernet API port |
| `PIN_ETH_SCK/MISO/MOSI/CS` | `9/10/11/8` | SPI pins for ENC28J60 |
| `PIN_INVERTER_WIFI_WAKE` | `36` | GPIO for wake pulse |
| `PULSE_HIGH_MS` | `50` | Pulse HIGH duration |
| `PULSE_GAP_MS` | `50` | Gap between pulses |
| `WIFI_BRIDGE_POLL_INTERVAL_MS` | `20000` | Poll interval |
| `WIFI_BRIDGE_HTTP_TIMEOUT_MS` | `3500` | HTTP request timeout |
| `MAIN_LOOP_SLEEP_MS` | `5` | Main loop delay |
| `INVERTER_MAX_POWER_WATTS` | `1575` | Power set limit |
| `POWER_COMMAND_EXPIRY_MS` | `300000` | Expiry for queued power commands (5 min) |
| `API_CLIENT_TIMEOUT_MS` | `250` | API client read timeout |
| `ETHERNET_INIT_RETRY_MS` | `5000` | Ethernet init retry interval |
| `ETHERNET_SERVICE_INTERVAL_MS` | `2` | Ethernet service loop interval |

## Build and Upload

**Always use the upload script** — never call `arduino-cli` directly (it skips version stamping):

```powershell
.venv\Scripts\python skills/firmware-upload/upload_firmware.py
```

The script automatically: stamps `FIRMWARE_VERSION` with timestamp + git SHA, detects
the COM port, compiles, uploads, and commits the version change to git.

## Python Environment

All Python scripts require the project venv. Always activate before Python commands:

```powershell
& d:\git\MastervoltBridge\.venv\Scripts\Activate.ps1
```

Install missing packages with:

```powershell
uv pip install requests matplotlib pyserial
```

## Common Tasks

### Add a New API Endpoint
1. Add handler in `api.cpp` (`handleApiClient()`).
2. Add entry to `API_ENDPOINTS[]` in `api.cpp`.
3. Use `InverterMonitor::getInstance()` or `fetchInverterData()` for inverter communication.
4. Respond via `sendHttpResponse(client, code, contentType, body)`.
5. Update `docs/API_REFERENCE.md`, `README.md`, and `skills/api-validation/validate_api.py`.

### Debug WiFi / Reconnect Behavior
Run log analysis:

```powershell
.venv\Scripts\python skills/log-analysis/analyze_bridge_logs.py
.venv\Scripts\python skills/log-analysis/plot_power.py
```

Useful log patterns:
- `[API] GET /pulse`
- `[API] POST /api/power`
- `[WIFI-CONNECT] complete path=dwell duration_ms=... result=...`
- `[WIFI-CONNECT] complete path=auto duration_ms=... result=...`
- `[WIFI-BRIDGE] Triggering inverter WiFi wake pulse sequence.`
- `[INVERTER-MONITOR] Poll #N: Status=... Power=...W`
- `[INVERTER-MONITOR] Failed to fetch /home: ...`
- `[INVERTER-MONITOR] Queued power command delivered: ...W`

## Gotchas

1. Prefer `fetchInverterData(..., waitForConnection)` over direct WiFi request logic in business code.
2. Polling uses blocking connect (`waitForConnection=true`), API paths use fail-fast (`waitForConnection=false`).
3. `/api/power` can return 202 (queued) when inverter WiFi is unavailable; this is expected.
4. `/api/info` returns 200 with `ready=false` (and empty telemetry strings) until at least one successful poll has cached telemetry.
5. `/pulse` while connected intentionally drops/reconnects WiFi to force a measured connect attempt.
6. `/wifi/off` only sends a single press when bridge WiFi is currently connected.
7. ENC28J60 can crash on dead TCP write; always check `client.connected()` before streaming writes.
8. Every new terminal needs venv activation before Python scripts.

## File Map

Firmware (`firmware/esp32_inverter_bridge/`):
`esp32_inverter_bridge.ino`, `wifi_bridge.{h,cpp}`, `inverter_fetch.{h,cpp}`, `inverter_data.{h,cpp}`, `inverter_monitor.{h,cpp}`, `ethernet_bridge.{h,cpp}`, `api.{h,cpp}`, `api_helper.{h,cpp}`, `settings.{h,cpp}`, `lock_guard.{h,cpp}`, `logger.h`

Documentation (`docs/`):
- `SETUP_README.md`
- `WIRING_README.md`
- `ESP32_UPLOAD_README.md`
- `API_REFERENCE.md`
- `TEST_README.md`
- `LOCKING_MODEL.md`
- `MAX_POWER_BEHAVIOR.md`
- `HOME_ASSISTANT.md`

Skills (`skills/`):
`firmware-upload/`, `firmware-optimization-loop/`, `log-analysis/`, `api-validation/`, `documentation-update/`, `strategy-comparison/`, `create-agent-skills/`

Root:
`README.md`, `AGENTS.md`, `test_bridge.py`, `pyproject.toml`
