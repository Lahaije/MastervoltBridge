# ESP32 Inverter WiFi-to-Ethernet Bridge

Professional bridge firmware for connecting WiFi-only inverters (Mastervolt SOLADIN series) to Home Assistant over Ethernet.

## Current Snapshot (May 18, 2026)

- Hardware: ESP32-S3 + ENC28J60
- Ethernet API: port 8080
- Inverter SSID: mastervolt-soladin-0103
- Inverter IP: 10.0.0.1
- WiFi wake pin: GPIO 36
- Pulse timing: HIGH 150 ms, gap 200 ms, HIGH 150 ms

## Architecture

- WiFi layer: firmware/esp32_inverter_bridge/wifi_bridge.cpp
- Polling/caching: firmware/esp32_inverter_bridge/inverter_monitor.cpp
- API router: firmware/esp32_inverter_bridge/api.cpp
- API helpers/JSON: firmware/esp32_inverter_bridge/api_helper.cpp
- Config: firmware/esp32_inverter_bridge/settings.cpp

## API Endpoints (Current)

| Method | Path | Purpose |
|---|---|---|
| GET | / | API discovery |
| GET | /api/health | Bridge WiFi/Ethernet status |
| GET | /api/logs | Circular log buffer |
| GET | /api/info | Latest cached inverter telemetry |
| POST | /api/power | Set inverter power (0-1575 W) |
| POST | /api/inverter/fetch | Fetch arbitrary inverter path |
| POST | /wifi/off | Single press if bridge WiFi is connected |
| GET | /pulse | Double-press recovery + connection timing measurement |

## WiFi Measurement Findings

### Valid inverter state machine

- OFF -> double press (/pulse) -> ON
- ON -> single press (/wifi/off) -> OFF
- ON -> double press -> undefined state (avoid)

### Required clean test flow

- pulse (measure)
- wifi/off
- wait 2-3 seconds
- repeat

### Measured baseline before latest optimization

- Success rate: 100% when using the clean flow
- Connection time range: about 3752-4752 ms
- Average connection time: about 4252 ms
- Inverter channels observed: 1, 6, 11

### Firmware changes applied for speed/reliability

- Recovery timeout reduced to 8000 ms
- Scan dwell reduced to 500 ms and settle to 100 ms
- WiFi radio reset before each measurement (WIFI_OFF -> WIFI_STA)
- Scan-before-connect with discovered channel + BSSID passed into WiFi.begin
- Recovery scan logging reduced to inverter-focused lines
- /wifi/off endpoint restored

## Important Test Constraint

If inverter WiFi is unavailable (for example after sunset), endpoints that need inverter WiFi are expected to fail with 502:

- /api/info
- /api/power
- /api/inverter/fetch

The following should still work:

- /
- /api/health
- /api/logs
- /wifi/off (typically returns pressed false when WiFi already off)
- /pulse (returns pulse_complete, but connection may timeout)

## Tomorrow Handoff For Firmware Auto-Optimization

1. Start with inverter WiFi available (daylight).
2. Run clean measurements with run_clean_tests.py using pulse -> wifi/off -> wait loop.
3. Parse results with analyze_logs.py and compare against ~4252 ms baseline.
4. Confirm logs show channel/BSSID-assisted connect path.
5. If needed, iterate on:
- scan dwell/settle values
- retry strategy after WL_NO_SSID_AVAIL or WL_CONNECTION_LOST
- BSSID/channel fallback policy

## Build and Upload

Arduino CLI path used in this repo:

C:\Users\AL33888\AppData\Local\Programs\Arduino IDE\resources\app\lib\backend\resources\arduino-cli.exe

Typical commands:

- compile: arduino-cli compile --fqbn esp32:esp32:esp32s3 firmware/esp32_inverter_bridge
- upload: arduino-cli upload --fqbn esp32:esp32:esp32s3 --port COM9 firmware/esp32_inverter_bridge

## Documentation Index

- docs/API_REFERENCE.md
- docs/TEST_README.md
- docs/WIRING_README.md
- docs/UPLOAD_README.md
- docs/ESP32_WIRING_README.md
- docs/ESP32_UPLOAD_README.md
- AGENTS.md
