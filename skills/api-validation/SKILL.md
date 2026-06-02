---
name: api-validation
description: Check the live bridge API against docs/API_REFERENCE.md and docs/TEST_README.md. Use when endpoints or schemas change, or when you want to confirm the firmware still matches the docs.
---

<objective>
Compare the live bridge with the documented API. The script checks discovery, GET responses, and the current JSON keys.
</objective>

<quick_start>
Run from the repository root. The explicit venv python works in any shell — no activation required:
```powershell
.venv\Scripts\python.exe skills/api-validation/validate_api.py
```

Exit code `0` means the live API matches the docs. Exit code `1` means one or more checks failed.
</quick_start>

<examples>
**Verbose output**:
```powershell
.venv\Scripts\python.exe skills/api-validation/validate_api.py --verbose
```

**Custom bridge URL:**
```powershell
.venv\Scripts\python.exe skills/api-validation/validate_api.py --base-url http://<bridge-ip>:8080
```
</examples>

<validation>
**1. Reachability** - Calls `GET /api/health` first and stops if the bridge is unreachable.

**2. Discovery** - Verifies `GET /api` lists every documented endpoint and no undocumented endpoint.

**3. GET responses** - Checks HTTP status and response keys for the current read-only endpoints:

| Endpoint | Expected keys |
|---|---|
| `GET /` | HTML response (`Content-Type: text/html`) |
| `GET /config` | HTML response (`Content-Type: text/html`) |
| `GET /api` | `endpoints` |
| `GET /api/device` | `firmware_version`, `inverter_model`, `inverter_mac_address`, `wifi_ssid`, `wifi_ip`, `ethernet_ip`, `inverter_host` |
| `GET /api/health` | `operating_status`, `operating_mode`, `error_alarm_code`, `wifi_connected`, `inverter_link_state`, `last_update_ms`, `last_inverter_status`, `debug_mode` |
| `GET /api/logs` | `entries` |
| `GET /api/info` | `power`, `failure_streak_s`, `poll_interval_ms`, `power_limit_watts`, `shadow_enabled`, `total_yield`, `daily_yield` |
| `GET /pulse` | `reconnected` |
| `GET /api/mqtt` | `broker_ip`, `broker_port`, `enabled`, `topic_prefix`, `connected` |
</validation>

<process>
1. If firmware changed, update `firmware/esp32_inverter_bridge/api.cpp` or `api_helper.cpp`.
2. If docs changed, update `docs/API_REFERENCE.md` and `docs/TEST_README.md`.
3. If the validation script changed, keep `DOCUMENTED_ENDPOINTS` and `GET_CHECKS` aligned with the docs.
</process>

<advanced_features>
The script does not call POST endpoints automatically. Verify them manually when needed:
```powershell
# Set inverter power (inverter WiFi must be on)
curl -X POST http://<bridge-ip>:8080/api/power -H "Content-Type: application/json" -d "{\"power\":500}"

# Fetch inverter path
curl -X POST http://<bridge-ip>:8080/api/inverter/fetch -H "Content-Type: application/json" -d "{\"url\":\"/home\"}"

# Turn inverter WiFi off (single press)
curl -X POST http://<bridge-ip>:8080/wifi/off
```

Expected responses live in `docs/API_REFERENCE.md`.
</advanced_features>

<success_criteria>
Validation is complete when:
- [ ] Script exits with code 0
- [ ] All documented GET endpoints respond with expected keys
- [ ] Discovery output matches `docs/API_REFERENCE.md`
</success_criteria>
