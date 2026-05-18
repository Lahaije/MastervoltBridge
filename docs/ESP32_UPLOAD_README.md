# ESP32 Upload README

Current upload workflow for firmware/esp32_inverter_bridge.

## Required Values

- CLI: C:\Users\AL33888\AppData\Local\Programs\Arduino IDE\resources\app\lib\backend\resources\arduino-cli.exe
- FQBN: esp32:esp32:esp32s3
- Port: COM9 (verify on your machine)

## Compile

arduino-cli compile --fqbn esp32:esp32:esp32s3 firmware/esp32_inverter_bridge

## Upload

arduino-cli upload --fqbn esp32:esp32:esp32s3 --port COM9 firmware/esp32_inverter_bridge

## Verify Firmware Is Current

1. GET / returns discovery including /wifi/off.
2. POST /wifi/off returns JSON with pressed true/false (not 404).
3. GET /pulse returns status pulse_complete.

## Inverter Availability Caveat

When inverter WiFi is unavailable, these endpoints should return 502:

- /api/info
- /api/power
- /api/inverter/fetch

This is expected and does not indicate upload failure.

## Next-Day Measurement Continuation

Use clean cycle for timing work:

- /pulse
- /wifi/off
- wait 2-3s
- repeat

Then run:

- python run_clean_tests.py 10
- python analyze_logs.py
