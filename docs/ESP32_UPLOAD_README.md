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

Use the post-flash validation checklist in
[`docs/TEST_README.md`](TEST_README.md). It is the single source of truth
for which endpoints to hit and what each should return.

## Inverter Availability Caveat

When inverter WiFi is unavailable, these endpoints should return 502:

- /api/info
- /api/power
- /api/inverter/fetch

This is expected and does not indicate upload failure.

## Log Analysis

To analyze real-world connect performance accumulated during normal operation:

```powershell
.venv\Scripts\python skills/log-analysis/analyze_bridge_logs.py
```

This reports per-path (dwell/auto) success rate, min/avg/max connect time, and timeout count.
