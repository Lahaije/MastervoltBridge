# ESP32 Upload README

Current upload workflow for firmware/esp32_inverter_bridge.

## Required Values

- CLI: detected automatically by `skills/firmware-upload/upload_firmware.py` (checks PATH then `%LOCALAPPDATA%\Programs\Arduino IDE\...`)
- FQBN: esp32:esp32:esp32s3
- Port: COM9 (verify on your machine)

## Compile

arduino-cli compile --fqbn esp32:esp32:esp32s3 firmware/esp32_inverter_bridge

## Upload

arduino-cli upload --fqbn esp32:esp32:esp32s3 --port COM9 firmware/esp32_inverter_bridge

## Verify Firmware Is Current

1. GET / returns discovery including /wifi/off.
2. POST /wifi/off returns JSON with pressed true/false (not 404).
3. GET /pulse returns `{"reconnected": true/false}`.

## Inverter Availability Caveat

When inverter WiFi is unavailable:

- /api/info should return 200 with `ready=false` (no cached telemetry yet)
- /api/power should return 202 (command queued for retry)
- /api/inverter/fetch should return 502

This is expected and does not indicate upload failure.

## Log Analysis

To analyze real-world connect performance accumulated during normal operation:

```powershell
.venv\Scripts\python skills/log-analysis/analyze_bridge_logs.py
```

This reports per-path (dwell/auto) success rate, min/avg/max connect time, and timeout count.
