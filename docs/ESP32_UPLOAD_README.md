# ESP32 Upload README

Current upload workflow for firmware/esp32_inverter_bridge.

## Required Values

- CLI: detected automatically by `skills/firmware-upload/upload_firmware.py` (checks PATH then `%LOCALAPPDATA%\Programs\Arduino IDE\...`)
- FQBN: esp32:esp32:esp32s3:CDCOnBoot=cdc
- Port: COM9 (verify on your machine)

## Compile

arduino-cli compile --fqbn esp32:esp32:esp32s3:CDCOnBoot=cdc firmware/esp32_inverter_bridge

## Upload

arduino-cli upload --fqbn esp32:esp32:esp32s3:CDCOnBoot=cdc --port COM9 firmware/esp32_inverter_bridge

Post-flash verification and inverter-dependent behavior checks live in [`docs/TEST_README.md`](TEST_README.md).
