# Resource: docs/TEST_README.md

**What it documents**: Post-flash validation — preconditions, API sanity check table, which endpoints require inverter WiFi, endpoint verification curl commands, and a troubleshooting table.

**Source of truth**: `api.cpp` (endpoints), `settings.cpp` (timeouts, port).

**Single-source-of-truth rules**:
- WiFi connect strategy details (dwell/auto) are NOT here — link to `AGENTS.md`.
- Full API schemas are NOT here — link to `docs/API_REFERENCE.md`.
- Upload procedure is NOT here — link to `docs/ESP32_UPLOAD_README.md`.

**Update when**:
- Endpoints are added, removed, or change expected status codes.
- New troubleshooting cases are identified from real-world failures.
- The bridge IP or port changes.

**Do NOT update for**: WiFi strategy changes, wiring, upload procedure, or log format details.
