# Agent Quick Reference - ESP32 Inverter Bridge

This document provides project context for AI agents and future developers working on the ESP32 inverter WiFi-to-Ethernet bridge.

## Project Summary

**What**: Professional bridge firmware connecting WiFi-only inverters (Mastervolt SOLADIN 1500) to Home Assistant via Ethernet.

**Hardware**: ESP32 MCU + ENC28J60 Ethernet adapter
- WiFi: Station mode connecting to inverter WiFi (10.0.0.x/mastervolt-soladin-0103)
- Ethernet: DHCP client on home LAN (192.168.1.48:8080)
- GPIO: Pin 36 for WiFi recovery pulse (inverter wake signal)

**Purpose**: Proxy inverter telemetry and control to Home Assistant via REST API over Ethernet, since inverter only offers WiFi access.

## Critical Architecture

### Module Dependency Graph

```
wifi_bridge.cpp/h (core network layer)
  ├── exports: fetchInverterData(method, path, body, ...)
  ├── exports: ensureWifiConnected()
  └── WiFi.h, HTTPClient.h, settings

inverter_data.cpp/h (data model)
  ├── defines: HomeData struct with raw string fields
  ├── defines: getInverterData() factory function
  └── Units: Power in watts (W), Yields in kilowatt-hours (kWh)

inverter_monitor.cpp/h (business logic)
  ├── uses: fetchInverterData() for all HTTP requests
  ├── uses: HomeData struct from inverter_data.h
  ├── defines: InverterMonitor singleton with polling task
  └── FreeRTOS task runs every 20s, polls /home, caches result

ethernet_bridge.cpp/h (network layer 2)
  ├── manages: ENC28J60 Ethernet initialization
  ├── starts: API HTTP server on port 8080
  └── UIPEthernet, FreeRTOS

api_helper.cpp/h (HTTP/JSON utilities)
  ├── exports: jsonEscape(), sendHttpResponse()
  ├── exports: parseStringToInt(), parseFetchUrlFromBody()
  ├── exports: buildHealthJson(), buildLogsJson(), buildApiDiscoveryJson()
  └── Arduino, WiFi.h, settings

api.h (request routing)
  ├── handles: 7 REST endpoints
  ├── uses: api_helper for HTTP/JSON utilities
  ├── uses: InverterMonitor::getInstance()
  └── uses: getInverterData() for telemetry access

esp32_inverter_bridge.ino (main entry)
  ├── calls: ethernetBridgeStartTask()
  ├── calls: InverterMonitor::getInstance().initialize()
  └── loop: MAIN_LOOP_SLEEP_MS (5ms) + delay

settings.cpp/h (configuration)
  └── all global constants (SSID, pins, timeouts, IPs)

logger.h (diagnostics)
  └── 1000-entry circular buffer with ms timestamps
```

### Key Design Pattern: fetchInverterData()

**Single point of entry for all inverter communication**

```cpp
bool fetchInverterData(const String& method, const String& path, const String& body,
                       String& responseBody, int& httpCode, String& errorMessage)
```

**Responsibilities**:
1. Ensure WiFi is connected (reconnects if down)
2. Build HTTP request (GET or POST)
3. Execute HTTP call
4. Handle errors
5. Log result
6. Return via output parameters

**Used by**:
- `inverter_monitor.cpp`: For polling `/home` every 20s
- `inverter_monitor.cpp`: For setting power via `/power`
- `inverter_monitor.cpp`: For `/api/inverter/fetch` endpoint (via `fetchPath()`)

**Key benefit**: Automatic WiFi management means every operation recovers from WiFi drops transparently.

## API Endpoints (7 total)

| Method | Path | Handler | Purpose |
|--------|------|---------|---------|
| GET | `/` | api.h:298 | API discovery (endpoint list) |
| GET | `/api/health` | api.h:303 | Bridge status (WiFi, Ethernet, IPs) |
| GET | `/api/logs` | api.h:308 | Log buffer (1000 entries) |
| GET | `/api/info` | api.h:320 | Latest inverter telemetry (from HomeData cache) |
| POST | `/api/power` | api.h:343 | Set inverter power (0-1575W max) |
| POST | `/api/inverter/fetch` | api.h:370 | Generic inverter endpoint fetch |
| GET | `/pulse` | api.h:313 | Trigger WiFi recovery pulse |

## Data Structures

### HomeData (inverter_data.h)
```cpp
struct HomeData {
  // Raw fields from /home endpoint
  String operatingStatus;        // "1" = normal
  String errorAlarmCode;         // "0" = no error
  String operatingMode;          // "1" = production
  String inverterModel;          // "H500A0103"
  String inverterMacAddress;     // "00:06:66:9d:e0:36"
  String instantaneousPower;     // "674.547" watts
  String lifetimeEnergy;         // "08566.628" kWh
  String dailySessionEnergy;     // "12.811" kWh today
  
  bool isValid() const;
  void clear();
};
```

Parsed from inverter's `GET /home` response: 8 newline-delimited fields.
**Units**: Power=watts (W), Yields=kilowatt-hours (kWh).

### InverterMonitor (inverter_monitor.h)
```cpp
class InverterMonitor {
  // Singleton access
  static InverterMonitor& getInstance();
  
  // Lifecycle
  void initialize();              // Start polling task
  void shutdown();
  
  // Public data access
  bool getLatestHomeData(HomeData& dataOut);
  unsigned long getLastUpdateMs();
  
  // Inverter control
  bool setPower(int watts, String& responseBody, int& httpCode, String& errorMessage);
  bool fetchPath(const String& path, String& responseBody, int& httpCode, String& errorMessage);
  
private:
  // FreeRTOS polling task (20s interval)
  TaskHandle_t pollingTaskHandle;
  SemaphoreHandle_t dataMutex;
  HomeData cachedData;
  unsigned long lastUpdateMs;
};
```

**Thread Safety**: `dataMutex` protects all cached data. All reads/writes timeout after 5s.

## Configuration (settings.cpp)

| Constant | Default | Purpose |
|----------|---------|---------|
| INVERTER_WIFI_SSID | "mastervolt-soladin-0103" | WiFi network name |
| INVERTER_WIFI_PASSWORD | "" | WiFi password (empty if open) |
| INVERTER_HOST | "10.0.0.1" | Inverter IP address |
| API_PORT | 8080 | Ethernet API port |
| PIN_INVERTER_WIFI_WAKE | 36 | GPIO pin for recovery pulse |
| PULSE_HIGH_MS | 150 | Recovery pulse HIGH duration |
| PULSE_GAP_MS | 200 | Gap between pulses |
| WIFI_BRIDGE_POLL_INTERVAL_MS | 20000 | Polling interval (20s) |
| WIFI_BRIDGE_HTTP_TIMEOUT_MS | 3500 | HTTP request timeout |

## Compilation & Upload

**Build**: Arduino IDE with ESP32 board package (esp-x32 2.6.0+)

**Required Libraries**:
- WiFi.h (built-in)
- HTTPClient.h (built-in)
- UIPEthernet.h (install via Library Manager)
- FreeRTOS headers (ESP32 core)

**Upload**: Board = "ESP32S3 Dev Module" (or appropriate variant)

## Common Tasks

### Add a New API Endpoint

1. Add handler in `api.h` `handleApiClient()` function
2. Register in `API_ENDPOINTS[]` struct array
3. Use `InverterMonitor::getInstance()` or `fetchInverterData()` for inverter communication
4. Send response via `sendHttpResponse(client, code, contentType, body)`

### Change Polling Interval

Edit `inverter_monitor.cpp`, line 12:
```cpp
constexpr uint32_t TELEMETRY_POLL_INTERVAL_MS = 20000;  // Change this
```

### Debug WiFi Issues

Check `/api/logs` endpoint. Look for:
- `[WIFI-BRIDGE] Connected. IP=10.0.0.42` → WiFi OK
- `[WIFI-BRIDGE] Connect timeout after 10000ms` → WiFi unreachable
- `[INVERTER-MONITOR] Failed to fetch /home` → HTTP request failed

### Validate Telemetry Parsing

Check `/api/info` response:
- All 8 fields populated → parsing working
- Missing fields or 502 error → polling hasn't completed (wait 20-30s) or WiFi down

## Known Limitations & Quirks

1. **Inverter WiFi instability**: Inverter's WiFi module occasionally drops connections. Bridge recovers automatically, but multiple "read Timeout" logs indicate inverter-side issues.
2. **HTTP timeout**: Set to 3.5s. If inverter is slow, increase `WIFI_BRIDGE_HTTP_TIMEOUT_MS`.
3. **Polling starts after boot**: No data available for first 20s. `/api/info` returns 502 until first successful poll.
4. **Generic fetch endpoint** (`/api/inverter/fetch`): Allows fetching any inverter endpoint but doesn't parse response—returns raw body.

## Testing

**Quick validation**:
```bash
curl http://192.168.1.48:8080/api/health  # Check connectivity
curl http://192.168.1.48:8080/api/info    # Check telemetry
curl http://192.168.1.48:8080/api/logs    # Check logs
```

**Python test script**:
```bash
python test_bridge.py
```

## File Map

**Core firmware** (`firmware/esp32_inverter_bridge/`):
- `esp32_inverter_bridge.ino` — Entry point
- `wifi_bridge.{h,cpp}` — WiFi + HTTP fetcher (core layer)
- `inverter_data.{h,cpp}` — Data model (HomeData struct with raw fields)
- `inverter_monitor.{h,cpp}` — Polling + caching (business logic)
- `ethernet_bridge.{h,cpp}` — ENC28J60 + API server startup
- `api.h` — REST endpoint routing and handlers
- `api_helper.{h,cpp}` — HTTP/JSON parsing and response building utilities
- `settings.{h,cpp}` — Global configuration
- `logger.h` — Log buffer

**Documentation** (`docs/`):
- `ESP32_WIRING_README.md` — Hardware pin assignments
- `ESP32_UPLOAD_README.md` — Arduino setup
- `ESP32_TEST_README.md` — API validation commands

**Resources**:
- `README.md` — Project overview
- `test_bridge.py` — API test script
- `Mastervolt.js`, `content.js` — Web UI reference (not used by bridge)

## Gotchas for Future Work

1. **Include order matters**: `inverter_monitor.cpp` needs `wifi_bridge.h` before calling `fetchInverterData()`.
2. **Mutex timeouts**: If a thread holds the data mutex >5s, other threads will fail. Keep lock time short.
3. **WiFi status checks**: Always use `ensureWifiConnected()`, not `WiFi.status()`. The former actively reconnects.
4. **HTTP status codes**: 502 often means WiFi not connected. Check `/api/health` first.
5. **Power limit validation**: `setPower()` validates 0-1575W before HTTP request. Modify `INVERTER_MAX_POWER_WATTS` in `settings.cpp`.

## Performance Characteristics

- **Polling latency**: 20-25s from request to response in logs
- **HTTP timeout**: 3.5s per request (configurable)
- **WiFi reconnect time**: Up to 10s if WiFi down
- **API response time**: <100ms for cached data
- **Memory**: ~60-70KB heap used, 300KB available for stack/buffers

## Questions for Future Agents

- **WiFi drops frequently?** → Inverter WiFi module issue, not bridge. Test inverter WiFi directly.
- **502 errors on first call?** → Polling hasn't completed yet (20-30s). Wait and retry.
- **New inverter model?** → Edit `settings.cpp` (SSID, host), then rebuild and upload.
- **Different hardware?** → Update GPIO pins in `settings.cpp`. ENC28J60 SPI pins may differ.
