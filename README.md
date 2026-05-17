# ESP32 Inverter WiFi-to-Ethernet Bridge

Professional bridge firmware for connecting WiFi-only inverters (Mastervolt SOLADIN series) to Home Assistant via Ethernet. Uses ESP32 + ENC28J60 to bridge inverter WiFi network to home LAN.

## Architecture

**Hardware**: ESP32 with ENC28J60 Ethernet adapter
- **WiFi Interface**: Station mode, connects to inverter WiFi (10.0.0.x)
- **Ethernet Interface**: DHCP client, connects to home LAN (192.168.1.x)
- **API Server**: HTTP REST endpoint on Ethernet (port 8080)

**Firmware Design**:
- **Modular architecture**: Separated concerns (WiFi connectivity, inverter telemetry, HTTP API, Ethernet)
- **FreeRTOS-based**: Polling task runs every 20s with mutex-protected cached data
- **Generic HTTP layer**: `fetchInverterData()` in wifi_bridge module handles all inverter requests with automatic WiFi management
- **Rich logging**: 1000-entry circular buffer with millisecond timestamps

## Features

- ✅ **Automatic WiFi management**: Every inverter request checks and reconnects if needed
- ✅ **Cached telemetry**: 20-second polling interval minimizes network traffic
- ✅ **Hardware limits**: Power setting validation (0-1575W max for SOLADIN 1500)
- ✅ **Comprehensive API**: 7 REST endpoints + generic fetch passthrough
- ✅ **Diagnostics**: Real-time logs, health checks, poll statistics
- ✅ **Recovery mechanism**: GPIO pulse sequence to wake inverter WiFi if needed

## API Endpoints

| Method | Path | Purpose |
|--------|------|----------|
| GET | `/` | API discovery and endpoint list |
| GET | `/api/health` | Bridge connectivity status (WiFi, Ethernet, IPs) |
| GET | `/api/logs` | Up to 1000 cached log entries (ms timestamps) |
| GET | `/api/info` | Latest inverter telemetry from `/home` endpoint |
| POST | `/api/power` | Set inverter power: JSON body `{"power":1200}` |
| POST | `/api/inverter/fetch` | Generic inverter endpoint fetch: POST body `{'url':'/path'}` |
| GET | `/pulse` | Trigger WiFi module recovery pulse sequence |

## Response Format

**`GET /api/info`** returns structured telemetry:
```json
{
  "last_update_ms": 103850,
  "operating_status": "1",
  "error_alarm_code": "0",
  "operating_mode": "1",
  "inverter_model": "H500A0103",
  "inverter_mac_address": "00:06:66:9d:e0:36",
  "power": "674.547",
  "total_yield": "08566.628",
  "daily_yield": "12.811"
}
```

## Folder Structure

```
firmware/esp32_inverter_bridge/
├── esp32_inverter_bridge.ino          # Main sketch entry point
├── wifi_bridge.{h,cpp}                # WiFi connectivity + generic HTTP fetcher
├── inverter_data.{h,cpp}              # Data model (HomeData struct + typed accessors)
├── inverter_monitor.{h,cpp}           # Inverter telemetry polling (FreeRTOS task)
├── ethernet_bridge.{h,cpp}            # ENC28J60 Ethernet stack manager
├── api.h                              # HTTP API endpoint routing
├── api_helper.{h,cpp}                 # JSON/HTTP parsing and response utilities
├── settings.{h,cpp}                   # Global configuration (pins, SSIDs, timeouts)
├── logger.h                           # Circular log buffer (1000 entries)
└── [legacy] esp8266_inverter_bridge/  # Original ESP8266 firmware (deprecated)

docs/
├── WIRING_README.md                   # Hardware pin assignments
├── UPLOAD_README.md                   # Arduino IDE setup steps
├── TEST_README.md                     # Verification procedures
└── [ESP32 variants]                   # ESP32-specific documentation

test_bridge.py                         # Python test script for API validation
Mastervolt.js / content.js / executor.js  # Inverter web UI JavaScript (for reference)
```

## Build & Deploy

1. **Hardware Setup**: Follow [ESP32_WIRING_README.md](docs/ESP32_WIRING_README.md)
2. **Configure**: Edit `settings.cpp` for inverter SSID/password
3. **Upload**: Follow [ESP32_UPLOAD_README.md](docs/ESP32_UPLOAD_README.md)
4. **Test**: Run [ESP32_TEST_README.md](docs/ESP32_TEST_README.md) or `python test_bridge.py`

## Documentation

- **[API_REFERENCE.md](docs/API_REFERENCE.md)** — Complete REST API specification with examples
- **[AGENTS.md](AGENTS.md)** — Project structure and module guide for AI agents and future developers
- **[ESP32_WIRING_README.md](docs/ESP32_WIRING_README.md)** — Hardware pin assignments and connections
- **[ESP32_UPLOAD_README.md](docs/ESP32_UPLOAD_README.md)** — Arduino IDE setup and firmware upload steps
- **[ESP32_TEST_README.md](docs/ESP32_TEST_README.md)** — API validation and troubleshooting commands

## Known Issues & Troubleshooting

- **WiFi drops after 30-50s**: Inverter WiFi module stability issue. Test by connecting a laptop directly to inverter WiFi.
- **502 Bad Gateway on API calls**: WiFi not connected. Check `/api/health` endpoint.
- **Timeouts on `/api/info`**: Inverter `/home` endpoint slow to respond. Increase `TELEMETRY_HTTP_TIMEOUT_MS` in settings.cpp.

## Module Responsibility Matrix

| Module | Role | Exports | Depends On |
|--------|------|---------|------------|
| wifi_bridge | WiFi connection + generic HTTP | `ensureWifiConnected()`, `fetchInverterData()` | WiFi.h, HTTPClient.h, settings |
| inverter_data | Data model | `HomeData` struct, `getInverterData()` | inverter_monitor |
| inverter_monitor | Telemetry polling + caching | `InverterMonitor` singleton | wifi_bridge, inverter_data, FreeRTOS |
| ethernet_bridge | ENC28J60 + API server startup | `ethernetBridgeInit()` | UIPEthernet, api.h |
| api_helper | JSON/HTTP parsing + response building | `jsonEscape()`, `sendHttpResponse()`, `parseStringToInt()`, `parseFetchUrlFromBody()`, `buildHealthJson()`, `buildLogsJson()`, `buildApiDiscoveryJson()` | Arduino, WiFi.h |
| api.h | HTTP endpoint routing + handlers | `handleApiClient()` | api_helper, inverter_data, inverter_monitor, logger |
| settings | Configuration constants | All global config + pins | (none) |
| logger | Log buffer | `Logger` singleton, `appLogger` global | (none) |

## Development Notes

- **WiFi Stability**: Every inverter request goes through `fetchInverterData()` which ensures WiFi is connected first. This provides automatic reconnection without explicit error handling at call sites.
- **Thread Safety**: Telemetry data protected by FreeRTOS mutex. All reads/writes use 5s timeout.
- **HTTP Timeout**: Currently 3.5s for inverter requests. Adjust in settings.cpp if inverter is slow.
- **Polling Interval**: 20s by default. Balance between freshness and network load.
