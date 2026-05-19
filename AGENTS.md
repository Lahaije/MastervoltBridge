# Agent Quick Reference - ESP32 Inverter Bridge

This document provides project context for AI agents and future developers working on the ESP32 inverter WiFi-to-Ethernet bridge.

## Project Summary

**What**: Bridge firmware connecting WiFi-only inverters (Mastervolt SOLADIN 1500) to Home Assistant via Ethernet.

**Hardware**: ESP32-S3 + ENC28J60 Ethernet adapter
- WiFi: Station mode → inverter SSID `mastervolt-soladin-0103` / `10.0.0.1`
- Ethernet: DHCP client on home LAN → `192.168.1.48:8080`
- GPIO 36: inverter WiFi wake pulse (active HIGH)

## Architecture

### Module Dependency Graph

```
wifi_bridge.cpp/h (core network layer)
  ├── WifiConnectionManager singleton (ensureConnected, forceReconnect)
  ├── connectWifiDwell(), connectWifiAuto(), connectWifi(bool)
  ├── fetchInverterData(method, path, body, ...)
  └── deps: WiFi.h, HTTPClient.h, settings

inverter_data.cpp/h (data model)
  └── HomeData struct + getInverterData() factory

inverter_monitor.cpp/h (business logic)
  ├── InverterMonitor singleton — FreeRTOS polling task (20s interval)
  ├── calls WifiConnectionManager::ensureConnected() before each poll
  └── caches HomeData; exposes getLatestHomeData(), setPower(), fetchPath()

ethernet_bridge.cpp/h (network layer)
  └── ENC28J60 init + HTTP API server on port 8080

api.cpp / api.h (request routing)
  ├── handleApiClient() — 9 REST endpoints
  └── deps: api_helper, InverterMonitor, inverter_data

api_helper.cpp/h (HTTP/JSON utilities)
  └── sendHttpResponse(), sendLogsResponse(), buildHealthJson(), buildInfoJson(), etc.

esp32_inverter_bridge.ino (main entry)
  └── starts ethernetBridgeStartTask() + InverterMonitor::initialize()

settings.cpp/h — all global constants
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
Analyze with: `.venv\Scripts\python skills/log-analysis/analyze_bridge_logs.py`

## API Endpoints

| Method | Path | Purpose |
|--------|------|---------|
| GET | `/` | API discovery |
| GET | `/api/health` | Bridge status (WiFi, Ethernet, IPs) |
| GET | `/api/logs` | Log buffer (1000 entries) |
| GET | `/api/info` | Latest cached inverter telemetry |
| POST | `/api/power` | Set inverter power (0–1575 W) |
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

> Note: JSON field names in `/api/info` responses differ — `power`, `total_yield`, `daily_yield` (see `api_helper.cpp::buildInfoJson`).

Parsed from inverter `GET /home`: 8 newline-delimited fields.

### InverterMonitor public interface (inverter_monitor.h)
```cpp
static InverterMonitor& getInstance();
void initialize();
bool getLatestHomeData(HomeData& dataOut);
unsigned long getLastUpdateMs();
bool setPower(int watts, String& responseBody, int& httpCode, String& errorMessage);
bool fetchPath(const String& path, String& responseBody, int& httpCode, String& errorMessage);
```
`dataMutex` protects all cached data; all lock attempts timeout after 5 s.

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

## Build & Upload

```powershell
# Recommended (auto COM detection):
.venv\Scripts\python skills/firmware-upload/upload_firmware.py

# Direct arduino-cli:
arduino-cli compile --fqbn esp32:esp32:esp32s3 firmware/esp32_inverter_bridge
arduino-cli upload  --fqbn esp32:esp32:esp32s3 --port COM9 firmware/esp32_inverter_bridge
```

**Agent policy**: Agents may compile and upload when needed (FQBN `esp32:esp32:esp32s3`, port `COM9`) unless the user says not to.  
See `docs/SETUP_README.md` for library prerequisites and IDE setup.

## Common Tasks

### Add a New API Endpoint
1. Add handler in `api.cpp` → `handleApiClient()`
2. Add entry to `API_ENDPOINTS[]` in `api.cpp`
3. Use `InverterMonitor::getInstance()` or `fetchInverterData()` for inverter communication
4. Respond via `sendHttpResponse(client, code, contentType, body)`
5. Update `docs/API_REFERENCE.md`, `README.md`, and `skills/api-validation/validate_api.py`

### Change Polling Interval
Edit `WIFI_BRIDGE_POLL_INTERVAL_MS` in `settings.cpp` (normal rate, stage 0).  
To adjust the overnight backoff schedule, edit `BACKOFF_STAGES[]` in `inverter_monitor.cpp`.

### Debug WiFi Issues
Check `GET /api/logs`. Key patterns:
- `[API] GET /pulse` → external caller triggered a reconnect
- `[API] POST /api/power` → power set request received
- `[API] debug mode enabled` → debug mode activated via `/api/debug`
- `[WIFI-BRIDGE] GET /home success (HTTP 200)` → inverter poll OK (**only logged when `debugMode=true`**)
- `[WIFI-CONNECT] complete path=dwell duration_ms=5413 result=success` → connected OK
- `[WIFI-CONNECT] complete path=auto duration_ms=8064 result=timeout` → unreachable
- `[WIFI-BRIDGE] Triggering inverter WiFi wake pulse sequence.` → pulse sent
- `[INVERTER-MONITOR] Failed to fetch /home` → HTTP failed after WiFi up

### Validate API vs Documentation
```powershell
.venv\Scripts\python skills/api-validation/validate_api.py
```

## Gotchas

1. **Never call `WiFi.status()` directly** — use `WifiConnectionManager::getInstance().ensureConnected()`. It pulses and reconnects; `WiFi.status()` does neither.
2. **Include order**: `inverter_monitor.cpp` needs `wifi_bridge.h` before `fetchInverterData()`.
3. **Mutex timeouts**: Keep lock time short. Threads waiting >5 s will fail silently.
4. **502 = WiFi not connected** (usually). Check `/api/health` first; `/api/info` also returns 502 for ~20 s after boot until first poll completes.
5. **Pulse state machine**: single press toggles WiFi. `/wifi/off` = one press (OFF). `/pulse` = double-press (OFF→ON). Calling `/pulse` while connected will disconnect then reconnect.
6. **Power limit**: `setPower()` validates 0–`INVERTER_MAX_POWER_WATTS` before the HTTP request.

## Performance

| Metric | Value |
|---|---|
| First poll after boot | ~6–8 s |
| Reconnect time (dwell path) | ~5 s typical |
| Reconnect time (auto path) | ~6.5 s typical |
| Connect budget (timeout) | 8 s |
| Polling interval | 20 s |
| HTTP request timeout | 3.5 s |
| API response (cached data) | <100 ms |
| Heap used / available | ~65 KB used / ~300 KB free |

## File Map

**Firmware** (`firmware/esp32_inverter_bridge/`):
`esp32_inverter_bridge.ino`, `wifi_bridge.{h,cpp}`, `inverter_data.{h,cpp}`, `inverter_monitor.{h,cpp}`, `ethernet_bridge.{h,cpp}`, `api.{h,cpp}`, `api_helper.{h,cpp}`, `settings.{h,cpp}`, `logger.h`

**Documentation** (`docs/`):
- `SETUP_README.md` — Hardware assembly, wiring, prerequisites, flash instructions
- `WIRING_README.md` — Pin table + electrical notes
- `ESP32_UPLOAD_README.md` — Upload procedure + post-flash verification
- `API_REFERENCE.md` — Full endpoint reference
- `TEST_README.md` — Validation checklist + troubleshooting

**Skills** (`skills/`):
`firmware-upload/`, `firmware-optimization-loop/`, `log-analysis/`, `api-validation/`, `documentation-update/`

**Root**: `README.md`, `AGENTS.md`, `test_bridge.py`, `pyproject.toml`


