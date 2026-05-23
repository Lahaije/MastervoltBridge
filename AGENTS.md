# Agent Quick Reference - ESP32 Inverter Bridge

This document provides project context for AI agents and future developers working on the ESP32 inverter WiFi-to-Ethernet bridge.

## Project Summary

What: Bridge firmware connecting WiFi-only inverters (Mastervolt SOLADIN 1500) to Home Assistant via Ethernet.

Hardware: ESP32-S3 + ENC28J60 Ethernet adapter
- WiFi: Station mode -> inverter SSID `mastervolt-soladin-0103` / `10.0.0.1`
- Ethernet: DHCP client on home LAN -> `192.168.1.48:8080`
- GPIO 36: inverter WiFi wake pulse (active HIGH)

## Architecture

### Module Dependency Graph

```
wifi_bridge.cpp/h (core network layer)
  |- Dedicated connection worker task (FreeRTOS)
  |- requestWifiConnection(), forceWifiReconnect(), fetchInverterData(..., waitForConnection)
  |- ScopedWifiOperationLock for WiFi HTTP/connect serialization
  |- deps: WiFi.h, HTTPClient.h, settings

inverter_data.cpp/h (data model)
  |- HomeData struct + getInverterData() factory

inverter_monitor.cpp/h (business logic)
  |- InverterMonitor singleton - FreeRTOS polling task (runtime-configurable; default 20s)
  |- Polls /home via fetchInverterData(..., waitForConnection=true)
  |- Power state machine (desired/confirmed/queued/timer)
  |- Link state machine: STARTING / ONLINE / RETRYING / BACKOFF / DORMANT
  |    transitions fire onLinkStateTransition(from, to, streakMs)
  |    bound action: BACKOFF|DORMANT -> ONLINE = queueMaxPowerAfterLongDisconnect + applyPendingPowerCommand
  |- caches HomeData; exposes getLatestHomeData(), setPower(), fetchPath(),
  |    getLinkState(), getFailureStreakMs(), getRetryIntervalMs()

ethernet_bridge.cpp/h (network layer)
  |- ENC28J60 init + HTTP API server on port 8080

api.cpp / api.h (request routing)
  |- handleApiClient() - 11 REST endpoints
  |- /api/power returns 200 (immediate) or 202 (queued)
  |- deps: api_helper, InverterMonitor, inverter_data

api_helper.cpp/h (HTTP/JSON utilities)
  |- sendHttpResponse(), sendLogsResponse(), buildHealthJson(), buildInfoJson(), etc.
  |- /api/info includes power_limit object (desired/confirmed/queued/reset_timer_minutes)

esp32_inverter_bridge.ino (main entry)
  |- starts ethernetBridgeStartTask() + wifiBridgeInit() + InverterMonitor::initialize()

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
| POST | `/api/inverter/fetch` | Fetch arbitrary inverter path (raw response) |
| POST | `/wifi/off` | Single press to turn inverter WiFi off if connected |
| GET | `/pulse` | GPIO double-press wake + forced reconnect; returns `{"reconnected": bool}` |
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
bool getLatestHomeData(HomeData& dataOut);
unsigned long getLastUpdateMs();
bool setPower(int watts, String& responseBody, int& httpCode, String& errorMessage);
bool isPowerCommandQueued();
int getDesiredPowerLimit();
int getConfirmedPowerLimit();
unsigned long getPowerLimitResetAtMs();
bool fetchPath(const String& path, String& responseBody, int& httpCode, String& errorMessage);
```

Synchronization:
- `dataMutex` protects cached telemetry and poll counters.
- `powerStateMutex` protects power state machine fields.

## Configuration (settings.cpp)

| Constant | Default | Purpose |
|---|---|---|
| `INVERTER_WIFI_SSID` | `"mastervolt-soladin-0103"` | Inverter WiFi SSID |
| `INVERTER_WIFI_PASSWORD` | `""` | Empty = open network |
| `INVERTER_HOST` | `"10.0.0.1"` | Inverter IP |
| `API_PORT` | `8080` | Ethernet API port |
| `PIN_INVERTER_WIFI_WAKE` | `36` | GPIO for wake pulse |
| `PULSE_HIGH_MS` | `50` | Pulse HIGH duration |
| `PULSE_GAP_MS` | `50` | Gap between pulses |
| `WIFI_BRIDGE_POLL_INTERVAL_MS` | `20000` | Poll interval |
| `WIFI_BRIDGE_HTTP_TIMEOUT_MS` | `3500` | HTTP request timeout |
| `INVERTER_MAX_POWER_WATTS` | `1575` | Power set limit |
| `POWER_LIMIT_RESET_MINUTES` | `120` | Auto-reset timer for sub-max limits |
| `POWER_COMMAND_EXPIRY_MS` | `300000` | Expiry for user-initiated queued commands |

## Build and Upload

```powershell
# Recommended (auto COM detection):
.venv\Scripts\python skills/firmware-upload/upload_firmware.py

# Direct arduino-cli:
arduino-cli compile --fqbn esp32:esp32:esp32s3:CDCOnBoot=cdc firmware/esp32_inverter_bridge
arduino-cli upload  --fqbn esp32:esp32:esp32s3:CDCOnBoot=cdc --port COM9 firmware/esp32_inverter_bridge
```

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
`esp32_inverter_bridge.ino`, `wifi_bridge.{h,cpp}`, `inverter_data.{h,cpp}`, `inverter_monitor.{h,cpp}`, `ethernet_bridge.{h,cpp}`, `api.{h,cpp}`, `api_helper.{h,cpp}`, `settings.{h,cpp}`, `logger.h`

Documentation (`docs/`):
- `SETUP_README.md`
- `WIRING_README.md`
- `ESP32_UPLOAD_README.md`
- `API_REFERENCE.md`
- `TEST_README.md`
- `LOCKING_MODEL.md`
- `MAX_POWER_BEHAVIOR.md`

Skills (`skills/`):
`firmware-upload/`, `firmware-optimization-loop/`, `log-analysis/`, `api-validation/`, `documentation-update/`

Root:
`README.md`, `AGENTS.md`, `test_bridge.py`, `pyproject.toml`
