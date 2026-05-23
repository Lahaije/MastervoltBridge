# Skill: API Validation

## Purpose

Validate that the live ESP32 bridge API matches the documentation in `docs/API_REFERENCE.md`.
The skill includes a Python script that calls every GET endpoint, checks JSON response keys,
and cross-checks the firmware's own discovery response against the documented endpoint list.

---

## When to Use This Skill

- After uploading new firmware — confirm all endpoints still work as documented.
- When `docs/API_REFERENCE.md` is updated — confirm the live bridge agrees.
- When adding a new endpoint to `api.cpp` — confirm it appears in the discovery response and add it to the docs.
- Periodically during development to catch documentation drift.

---

## Primary Command

Run from the repository root. **The venv MUST be activated first in every terminal session:**

```powershell
# Activate venv (required once per terminal session — always do this first)
& d:\git\MastervoltBridge\.venv\Scripts\Activate.ps1
```

After activation, use plain `python` for all commands below.

```powershell
python skills/api-validation/validate_api.py
```

With verbose output (shows response keys and full endpoint list):

```powershell
python skills/api-validation/validate_api.py --verbose
```

Custom bridge URL:

```powershell
python skills/api-validation/validate_api.py --base-url http://192.168.1.48:8080
```

Exit code `0` = all checks passed. Exit code `1` = one or more failures.

---

## What the Script Checks

### 1. Bridge reachability
Calls `GET /api/health`. If unreachable, stops immediately with a clear error message.

### 2. Discovery cross-check (`GET /`)
- Calls the firmware's own discovery endpoint.
- Verifies every **documented endpoint** appears in the firmware response.
- Verifies every **firmware endpoint** appears in the documentation (catches silent additions).
- Discrepancies indicate either stale docs or a firmware mismatch.

### 3. GET endpoint response validation
For each GET endpoint, checks:
- HTTP status code is 200 (or expected documented status for inverter-dependent endpoints).
- Response JSON contains the documented top-level keys.

| Endpoint | Expected keys |
|---|---|
| `GET /` | `endpoints` |
| `GET /api/health` | `wifi_connected`, `ethernet_ip` |
| `GET /api/logs` | `entries` |
| `GET /api/info` | `power`, `total_yield`, `daily_yield`, `power_limit` (or 502 if no cached telemetry yet) |
| `GET /pulse` | `reconnected` |

---

## Interpreting Results

| Result | Meaning | Action |
|---|---|---|
| `✓ All checks passed` | Live bridge matches documentation | No action needed |
| Documented endpoint missing from firmware | `api.cpp` is missing the handler | Add endpoint to `api.cpp`, re-upload with firmware-upload skill |
| Firmware endpoint missing from documentation | New endpoint was added without updating docs | Update `docs/API_REFERENCE.md` and `AGENTS.md` endpoint table |
| Response key missing | `api_helper.cpp` response changed, or docs are wrong | Align docs with firmware or restore the field |
| `GET /api/info` returns 502 | No cached telemetry yet (boot/inverter unavailable) | Expected — wait for successful poll or rerun daytime |
| Bridge not reachable | Ethernet disconnected or wrong IP | Check hardware and bridge IP |

---

## Fixing Mismatches

### Firmware is missing a documented endpoint
1. Add the handler in `api.cpp` (`handleApiClient()` function).
2. Add the entry to `API_ENDPOINTS[]` in `api.cpp`.
3. Upload firmware:
   ```powershell
   python skills/firmware-upload/upload_firmware.py
   ```
4. Re-run this skill to confirm.

### Documentation is missing a firmware endpoint
1. Update `docs/API_REFERENCE.md` — add method, path, description, request/response schema.
2. Update the endpoint table in `README.md` and `AGENTS.md`.
3. Add the endpoint to `DOCUMENTED_ENDPOINTS` in `validate_api.py`.
4. Add a GET check entry to `GET_CHECKS` in `validate_api.py` (for GET endpoints).

### Response key mismatch
- Compare `api_helper.cpp` (the JSON builder functions) against `docs/API_REFERENCE.md`.
- If firmware changed: update the docs.
- If docs changed incorrectly: revert the doc change.
- Update `GET_CHECKS` `required_keys` in `validate_api.py` to match the actual schema.

---

## POST Endpoints

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

Note: when inverter WiFi is down, `/api/power` may return `202` (queued) instead of failing.

---

## Keeping This Skill in Sync

The `DOCUMENTED_ENDPOINTS` and `GET_CHECKS` lists in `validate_api.py` are the source of truth for this skill.
When endpoints change, update **both** the Python script and `docs/API_REFERENCE.md` together.
See `skills/documentation-update/SKILL.md` for the full documentation update policy.
