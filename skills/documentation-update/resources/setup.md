# Resource: docs/SETUP_README.md

**What it documents**: End-to-end project setup — hardware requirements, wiring summary (link to `docs/WIRING_README.md` for full pin table), software prerequisites (Arduino IDE, VS Code extension, Python/UV), configuration constants to review before flashing, and the three firmware upload paths (Arduino IDE GUI, VS Code, automated script). Ends with first-boot verification.

**Source of truth**: `firmware/esp32_inverter_bridge/settings.cpp` (config constants), `skills/firmware-upload/upload_firmware.py` (script path).

**Single-source-of-truth rules**:
- Wiring pin tables are NOT here — they live in `docs/WIRING_README.md` only. Section 2 links there.
- Full upload CLI commands are NOT here — they live in `docs/ESP32_UPLOAD_README.md`.

**Update when**:
- Hardware requirements change.
- Default configuration constants change.
- A new upload method is added or removed.
- Python/UV setup steps change.
- First-boot verification URL or expected response changes.

**Do NOT update for**: API changes, WiFi strategy changes, log format changes, or wiring pin changes (update WIRING_README.md instead).
