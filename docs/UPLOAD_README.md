# Upload README

This guide covers compile/upload for the current ESP32-S3 bridge firmware.

## Tooling

Arduino CLI executable:

C:\Users\AL33888\AppData\Local\Programs\Arduino IDE\resources\app\lib\backend\resources\arduino-cli.exe

Board FQBN:

esp32:esp32:esp32s3

Typical upload port:

COM9

## Compile

arduino-cli compile --fqbn esp32:esp32:esp32s3 firmware/esp32_inverter_bridge

## Upload

arduino-cli upload --fqbn esp32:esp32:esp32s3 --port COM9 firmware/esp32_inverter_bridge

## Post-Upload Endpoint Verification

Run these checks:

- GET /
- GET /api/health
- GET /api/logs
- POST /wifi/off
- GET /pulse

Nighttime note:

When inverter WiFi is unavailable, inverter-dependent endpoints are expected to fail with 502:

- /api/info
- /api/power
- /api/inverter/fetch

## Measurement Workflow Reminder

For clean connection timing tests, use:

- /pulse
- /wifi/off
- wait 2-3s
- repeat

Do not double-press when WiFi is already ON.
