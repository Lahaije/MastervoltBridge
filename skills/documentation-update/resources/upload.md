# Resource: docs/ESP32_UPLOAD_README.md

**What it documents**: How to compile and upload firmware to the ESP32. Covers the Arduino CLI toolchain, FQBN, typical COM port, and post-upload endpoint verification.

**Source of truth**: `skills/firmware-upload/upload_firmware.py` (upload automation) and the Arduino IDE / arduino-cli toolchain.

**Update when**:
- The FQBN changes (different ESP32 board variant).
- The upload port changes permanently (different machine or board configuration).
- The arduino-cli detection logic in `upload_firmware.py` changes.
- Post-upload verification steps change (new required endpoints, removed endpoints).

**Do NOT update for**: Firmware logic changes, API endpoint behavior changes, WiFi or Ethernet configuration changes — the upload procedure is independent of what the firmware does.
