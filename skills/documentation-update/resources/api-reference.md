# Resource: docs/API_REFERENCE.md

**What it documents**: Full API surface — base URL, all endpoints with methods, paths, request bodies, and example responses.

**Source of truth**: `firmware/esp32_inverter_bridge/api.cpp` — the `handleApiClient()` function and `API_ENDPOINTS[]` table.

**Update when**:
- An endpoint is added or removed.
- A request body schema changes (new or removed fields).
- A response body schema changes.
- HTTP status codes for a given condition change.
- The base URL or port changes.

**Do NOT update for**: Internal polling logic, WiFi strategy changes, wiring, or upload procedure changes.
