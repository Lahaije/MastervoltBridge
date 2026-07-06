# Resource: docs/TEST_README.md

**What it documents**: Post-flash validation, endpoint checks, inverter-dependent failures, and troubleshooting.

**Source of truth**: `firmware/esp32_inverter_bridge/api.cpp`, `firmware/esp32_inverter_bridge/api_helper.cpp`, and `firmware/esp32_inverter_bridge/settings.cpp`.

**Single-source-of-truth rules**:
- Full API schemas live in `docs/API_REFERENCE.md`.
- Upload procedure lives in `docs/ESP32_UPLOAD_README.md`.
- WiFi strategy details live in `AGENTS.md`.

**Update when**:
- An endpoint changes expected status or schema.
- A troubleshooting case becomes relevant or obsolete.
- The bridge IP, port, or post-flash checks change.

**Do NOT update for**: WiFi strategy changes, wiring, or upload-procedure changes.
