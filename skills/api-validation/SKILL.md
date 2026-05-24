---
name: api-validation
description: Validate that the live ESP32 bridge API matches documentation in docs/API_REFERENCE.md. Use after uploading new firmware, updating API docs, or adding new endpoints.
---

<objective>
Validate that the live ESP32 bridge API matches the documentation in `docs/API_REFERENCE.md`. The skill includes a Python script that calls every GET endpoint, checks JSON response keys, and cross-checks the firmware's own discovery response against the documented endpoint list.
</objective>

<quick_start>
```powershell
# Activate venv (required once per terminal session)
& d:\git\MastervoltBridge\.venv\Scripts\Activate.ps1

# Run validation
python skills/api-validation/validate_api.py

# Verbose (shows response keys and full endpoint list)
python skills/api-validation/validate_api.py --verbose
```

Exit code `0` = all checks passed. Exit code `1` = one or more failures.
</quick_start>

<when_to_use>
- After uploading new firmware — confirm all endpoints still work as documented.
- When `docs/API_REFERENCE.md` is updated — confirm the live bridge agrees.
- When adding a new endpoint to `api.cpp` — confirm it appears in the discovery response and add it to the docs.
- Periodically during development to catch documentation drift.
</when_to_use>

<commands>
Run from the repository root. **The venv MUST be activated first in every terminal session.**

**Standard validation:**
```powershell
python skills/api-validation/validate_api.py
```

**Verbose output:**
```powershell
python skills/api-validation/validate_api.py --verbose
```

**Custom bridge URL:**
```powershell
python skills/api-validation/validate_api.py --base-url http://192.168.1.48:8080
```

**Stateful inverter round-trip test** (mutates power limit, use explicitly):
```powershell
python skills/api-validation/test_inverter_endpoints.py
python skills/api-validation/test_inverter_endpoints.py --base-url http://192.168.1.48:8080 --wait-seconds 3
```

This script captures original settings, applies an 80% power limit based on measured power, probes read-only endpoints while it settles, verifies delivery, then restores original settings.
</commands>

<validation_checks>
**1. Bridge reachability** — Calls `GET /api/health`. If unreachable, stops immediately.

**2. Discovery cross-check (`GET /`)** — Verifies every documented endpoint appears in firmware response and vice versa. Discrepancies indicate stale docs or firmware mismatch.

**3. GET endpoint response validation** — For each GET endpoint, checks HTTP status 200 and documented top-level JSON keys:

| Endpoint | Expected keys |
|---|---|
| `GET /` | `endpoints` |
| `GET /api/health` | `wifi_connected`, `ethernet_ip` |
| `GET /api/logs` | `entries` |
| `GET /api/info` | `ready`, `power`, `total_yield`, `daily_yield`, `power_limit` |
| `GET /pulse` | `reconnected` |
</validation_checks>

<interpreting_results>
| Result | Meaning | Action |
|---|---|---|
| `✓ All checks passed` | Live bridge matches documentation | No action needed |
| Documented endpoint missing from firmware | `api.cpp` is missing the handler | Add endpoint to `api.cpp`, re-upload |
| Firmware endpoint missing from documentation | New endpoint added without updating docs | Update `docs/API_REFERENCE.md` and `AGENTS.md` |
| Response key missing | `api_helper.cpp` response changed, or docs are wrong | Align docs with firmware or restore the field |
| `GET /api/info` returns `ready=false` | No cached telemetry yet | Expected — wait for successful poll |
| Bridge not reachable | Ethernet disconnected or wrong IP | Check hardware and bridge IP |
</interpreting_results>

<fixing_mismatches>
**Firmware is missing a documented endpoint:**
1. Add handler in `api.cpp` (`handleApiClient()` function).
2. Add entry to `API_ENDPOINTS[]` in `api.cpp`.
3. Upload: `python skills/firmware-upload/upload_firmware.py`
4. Re-run this skill to confirm.

**Documentation is missing a firmware endpoint:**
1. Update `docs/API_REFERENCE.md` — add method, path, description, request/response schema.
2. Update endpoint table in `README.md` and `AGENTS.md`.
3. Add endpoint to `DOCUMENTED_ENDPOINTS` in `validate_api.py`.
4. Add GET check entry to `GET_CHECKS` in `validate_api.py` (for GET endpoints).

**Response key mismatch:**
- Compare `api_helper.cpp` (JSON builder functions) against `docs/API_REFERENCE.md`.
- If firmware changed: update the docs.
- If docs changed incorrectly: revert the doc change.
- Update `GET_CHECKS` `required_keys` in `validate_api.py` to match actual schema.
</fixing_mismatches>

<post_endpoints>
The script does not call POST endpoints automatically (they mutate inverter state). Manual verification:

```powershell
# Set inverter power (inverter WiFi must be on)
curl -X POST http://192.168.1.48:8080/api/power -H "Content-Type: application/json" -d "{\"power\":500}"

# Fetch inverter path
curl -X POST http://192.168.1.48:8080/api/inverter/fetch -H "Content-Type: application/json" -d "{\"url\":\"/home\"}"

# Turn inverter WiFi off (single press)
curl -X POST http://192.168.1.48:8080/wifi/off
```

Note: when inverter WiFi is down, `/api/power` may return `202` (queued) instead of failing.
</post_endpoints>

<success_criteria>
API validation is complete when:
- [ ] Bridge reachable via `GET /api/health`
- [ ] All documented endpoints appear in firmware discovery response
- [ ] All firmware endpoints appear in documentation
- [ ] GET endpoints return expected JSON keys
- [ ] Exit code is `0`
</success_criteria>
