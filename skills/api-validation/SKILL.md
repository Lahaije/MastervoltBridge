---
name: api-validation
description: Validate that the live ESP32 bridge API matches documentation in docs/API_REFERENCE.md. Use when uploading new firmware, updating API docs, adding endpoints, or checking for documentation drift.
---

<objective>
Validate the live ESP32 bridge API against documentation. The included Python script calls every GET endpoint, checks JSON response keys, and cross-checks the firmware's discovery response against the documented endpoint list.
</objective>

<quick_start>
Run from the repository root. The explicit venv python works in any shell — no activation required:
```powershell
.venv\Scripts\python.exe skills/api-validation/validate_api.py
```

Exit code `0` = all checks passed. Exit code `1` = one or more failures.
</quick_start>

<commands>
**Verbose output** (shows response keys and full endpoint list):
```powershell
.venv\Scripts\python.exe skills/api-validation/validate_api.py --verbose
```

**Custom bridge URL:**
```powershell
.venv\Scripts\python.exe skills/api-validation/validate_api.py --base-url http://192.168.1.48:8080
```
</commands>

<checks>
**1. Bridge reachability** — Calls `GET /api/health`. If unreachable, stops immediately with a clear error message.

**2. Discovery cross-check (`GET /`)** — Verifies every documented endpoint appears in the firmware response, and every firmware endpoint appears in the documentation (catches silent additions).

**3. GET endpoint response validation** — For each GET endpoint, checks HTTP 200 (or 502 when inverter WiFi is off) and expected JSON keys:

| Endpoint | Expected keys |
|---|---|
| `GET /` | `endpoints` |
| `GET /api/health` | `wifi_connected`, `ethernet_ip` |
| `GET /api/logs` | `entries` |
| `GET /api/info` | `power`, `total_yield`, `daily_yield` (or 502 if inverter offline) |
| `GET /pulse` | `reconnected` |
</checks>

<interpreting_results>
| Result | Meaning | Action |
|---|---|---|
| `✓ All checks passed` | Live bridge matches documentation | No action needed |
| Documented endpoint missing from firmware | `api.cpp` is missing the handler | Add endpoint to `api.cpp`, re-upload with firmware-upload skill |
| Firmware endpoint missing from documentation | New endpoint added without updating docs | Update `docs/API_REFERENCE.md` and `AGENTS.md` endpoint table |
| Response key missing | `api_helper.cpp` response changed, or docs are wrong | Align docs with firmware or restore the field |
| `GET /api/info` returns 502 | Inverter WiFi is off (e.g. nighttime) | Expected — skip or rerun daytime |
| Bridge not reachable | Ethernet disconnected or wrong IP | Check hardware and bridge IP |
</interpreting_results>

<fixing_mismatches>
**Firmware is missing a documented endpoint:**
1. Add the handler in `api.cpp` (`handleApiClient()` function).
2. Add the entry to `API_ENDPOINTS[]` in `api.cpp`.
3. Upload firmware: `python skills/firmware-upload/upload_firmware.py`
4. Re-run this skill to confirm.

**Documentation is missing a firmware endpoint:**
1. Update `docs/API_REFERENCE.md` — add method, path, description, request/response schema.
2. Update the endpoint table in `README.md` and `AGENTS.md`.
3. Add the endpoint to `DOCUMENTED_ENDPOINTS` in `validate_api.py`.
4. Add a GET check entry to `GET_CHECKS` in `validate_api.py` (for GET endpoints).

**Response key mismatch:**
- Compare `api_helper.cpp` (the JSON builder functions) against `docs/API_REFERENCE.md`.
- If firmware changed: update the docs.
- If docs changed incorrectly: revert the doc change.
- Update `GET_CHECKS` `required_keys` in `validate_api.py` to match the actual schema.
</fixing_mismatches>

<post_endpoints>
The script does not call POST endpoints automatically (they mutate inverter state). To verify POST endpoints manually:
```powershell
# Set inverter power (inverter WiFi must be on)
curl -X POST http://192.168.1.48:8080/api/power -H "Content-Type: application/json" -d "{\"power\":500}"

# Fetch inverter path
curl -X POST http://192.168.1.48:8080/api/inverter/fetch -H "Content-Type: application/json" -d "{\"url\":\"/home\"}"

# Turn inverter WiFi off (single press)
curl -X POST http://192.168.1.48:8080/wifi/off
```

Expected responses are documented in `docs/API_REFERENCE.md`.
</post_endpoints>

<success_criteria>
Validation is complete when:
- [ ] Script exits with code 0
- [ ] All documented endpoints respond with expected keys
- [ ] Discovery endpoint lists match documentation exactly
- [ ] No undocumented endpoints exist in firmware
</success_criteria>
