# Resource: docs/ESP32_UPLOAD_README.md

**What it documents**: How to compile and upload firmware to the ESP32, including the current FQBN, port expectation, and post-upload verification link.

**Source of truth**: `skills/firmware-upload/upload_firmware.py` and the Arduino CLI toolchain.

**Update when**:
- The FQBN changes.
- The default port expectation changes.
- The upload script detection or CLI flow changes.
- The post-upload verification steps change.

**Do NOT update for**: Firmware logic changes, API endpoint behavior changes, or WiFi/Ethernet configuration changes.
