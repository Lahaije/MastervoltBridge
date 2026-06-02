# Resource: docs/SETUP_README.md

**What it documents**: Hardware requirements, software prerequisites, configuration checks, upload paths, and first-boot verification.

**Source of truth**: `firmware/esp32_inverter_bridge/settings.cpp` and `skills/firmware-upload/upload_firmware.py`.

**Single-source-of-truth rules**:
- Wiring pin tables live in `docs/WIRING_README.md`.
- Full upload CLI commands live in `docs/ESP32_UPLOAD_README.md`.

**Update when**:
- Hardware requirements change.
- Default configuration constants change.
- Upload methods change.
- Python/UV setup steps change.
- First-boot verification changes.

**Do NOT update for**: API changes, WiFi strategy changes, log format changes, or wiring pin changes.
