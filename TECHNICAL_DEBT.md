# Technical Debt & Future Work

## POST /api/power Remaining Work (LOW PRIORITY)

**Priority**: Low
**Scope**: `firmware/esp32_inverter_bridge/wifi_bridge.cpp`
**Status**: Mostly resolved — one optional improvement remains

### Resolved
- ✅ Phase-level debug logs (TCP connect, send, first-byte, full response) gated on `debugMode`
- ✅ Multipart/form-data encoding matching the inverter web UI's conventions
- ✅ Deterministic timeout budget (`HTTP_POST_BUDGET_MS` = 3 s, per-phase caps)
- ✅ Fallback from multipart to text/plain if first attempt fails
- ✅ `/key` endpoint confirmed unrelated to `/power` — it's for HMAC login only

### Remaining
- [ ] Optionally validate `"Update successful!"` in the response body before
  declaring success (currently accepts any 2xx). Low priority because
  `refreshPowerLimit()` reads back the actual value immediately after, so
  a silent drop would be caught as a stale cached value.

### Related
- `docs/web-cache/Mastervolt.js` — source of the multipart-form finding
- `docs/MAX_POWER_BEHAVIOR.md` — power limit cache and queued-command behavior

---

## Inverter Web UI Endpoint Exploration (TODO)

**Priority**: Medium
**Scope**: `docs/web-cache/`, follow-up firmware features
**Status**: Inventory captured from cached JS; live probing blocked on daylight

### Context
Static analysis of `docs/web-cache/Mastervolt.js` (and the near-identical
`content.js`) surfaced the full set of HTTP endpoints used by the
inverter's own web UI. None of these are exercised by the bridge today;
the bridge only talks to `/home` (telemetry) and the still-unconfirmed
`/power` (setpoint). Probing the rest could unlock cleaner alternatives
to GPIO-based WiFi-off and yield extra metadata for `/api/health`.

### Endpoint Inventory (from cached JS)

GET / XHR (small JSON or text, polled with `Math.random()` cache-buster):
- `/cid` — customer / company id
- `/invid` — inverter id (likely `H500A0103` model string)
- `/usertype` — current user role (admin / installer / guest)
- `/key` — session / CSRF-like token (POSTs may require this)
- `/apstatus` — WiFi AP status (read-side for the UI's "WiFi On/Off")
- `/language` — current UI language code
- `/SaveCountryWiz` — triggers the country-setup wizard save (GET, oddly)
- `/reginfo` — cloud-registration info

POST / form submissions (action URLs set by JS, payload is `multipart/form-data`):
- `/login` — `submit_Loginform(document.frmLogin,'/login')`
- `\language` *(sic — backslash typo in the firmware UI)* — `submit_Lang`
- Generic `submit_form(form, action_url, reqpage, msgid)` dispatcher whose
  `reqpage` branches on: `CountryWiz`, `NormalMode`, `NwWizSave`,
  `NwWizSaveConn`, `OptionsPage`, `CountryID`, `Insualtion` (sic),
  `CountrySettings`, `loginpwd`, `MpptSet`, `IdentifierSet`, `UrlPort`,
  `lang`. The actual action URL is set inside the corresponding
  `*Main.html` page, which is NOT currently cached.

SPA HTML fragments (GET via XHR — useful as a hint to which form lives where):
- `Setup0Main.html` … `Setup7Main.html`
- `StatusMain.html`, `UpgradeMain.html`, `OptionsMain.html`,
  `MonitoringMain.html`, `CountryMain.html`, `DiagnosticsMain.html`,
  `ApTimeout.html`

Other URLs referenced:
- `/frontend/#register/` — `CommonURL` constant for cloud-registration redirect
- `/favicon.ico?d=…` — connectivity probe (`ChkOnlineURl`)

### Open Questions
- Is there an HTTP equivalent to the GPIO-based WiFi-off? The `/apstatus`
  endpoint is the matching read-side; the matching write almost certainly
  lives in one of the un-cached `*Main.html` pages.
- Are `/cid`, `/invid`, and `/usertype` stable enough to surface as fields
  on `/api/health` / `/api/info` (firmware identification at a glance)?

### Resolved Questions
- `/key` is confirmed to be part of the HMAC-SHA1 login flow only (challenge-
  response auth). It does NOT need to be fetched before `/power` POSTs.
- `/power` and `/home` are confirmed real endpoints (working in production).

### Next Steps (when inverter is back online)
1. Cache the missing HTML pages under `docs/web-cache/`:
   `OptionsMain.html`, `MonitoringMain.html`, `StatusMain.html`,
   `Setup3Main.html`, `ApTimeout.html` (priority order). Extract the
   `<form action="…">` URLs and any inline `<script>` blocks.
2. Probe each cached endpoint via `POST /api/inverter/fetch` from the
   bridge and record responses under `docs/web-cache/responses/`.
   Start with the safe read-only ones (`/cid`, `/invid`, `/usertype`,
   `/apstatus`, `/language`, `/reginfo`).
3. Decide whether to surface stable identifiers (`/invid`, `/cid`) in
   `/api/info` or `/api/health`.
4. If a write-side WiFi-off endpoint is identified, replace or augment
   the GPIO double-press in `wifi_bridge.cpp` with an HTTP call (more
   reliable, no hardware dependency).

### Blocked By
- Inverter availability (daylight-only).
- No cached copy of the inverter's `*Main.html` SPA pages.

### Related
- `docs/web-cache/Mastervolt.js`, `docs/web-cache/content.js`
- `firmware/esp32_inverter_bridge/api.cpp` — `/api/inverter/fetch`
  handler that can be used as the probing tool

---

## Cold-Boot WiFi Association Delay (~90 s) (TODO)

**Priority**: Low
**Scope**: `firmware/esp32_inverter_bridge/wifi_bridge.cpp`
**Status**: Not started

### Problem
- On cold boot, the first WiFi association to the inverter AP routinely
  takes ~90 s, even though subsequent reconnects are sub-second.
- Suspected cause: scan/auth handshake against a low-power inverter AP
  that is slow to respond on first contact.

### Possible Approaches
- Pre-pulse the inverter wake GPIO before the first connect attempt
  (currently the wake pulse only fires on the first reconnect, not on
  cold-boot connect).
- Tune the initial scan dwell on the cold-boot path independently of
  the steady-state `dwell` / `auto` strategies.

### Blocked By
- None (investigation only; needs side-by-side timing data with and
  without a pre-pulse).

---

## Dynamic IP Discovery (TODO)

**Priority**: Medium  
**Scope**: Network infrastructure, scripting, skills  
**Status**: Not started

### Problem
- All scripts, skills, and examples hardcode static IP: `192.168.1.48`
- Device DHCP assignment may change if power-cycled or router reboots
- Users must manually find bridge IP on network before running any tools
- No automated discovery mechanism

### Solution Approach
1. **Add hostname to firmware**
   - Set mDNS hostname on boot (e.g., `mastervolt-bridge.local`)
   - Allow runtime hostname configuration via API endpoint
   - Requires: ESP32 mDNS library (already available in Arduino core)

2. **Update scripts and skills to support discovery**
   - Add `--discover` flag to auto-find bridge on network (mDNS or ARP scan)
   - Add `--host` / `--hostname` parameter (fallback to `mastervolt-bridge.local`)
   - Affected files:
     - `skills/firmware-upload/upload_firmware.py` (already has `--port` override)
     - `skills/api-validation/validate_api.py`
     - `skills/log-analysis/analyze_bridge_logs.py`
     - `skills/log-analysis/show_all.py`
     - `test_bridge.py`

3. **Configuration precedence**
   - CLI argument (highest)
   - Environment variable `MASTERVOLT_BRIDGE_HOST`
   - mDNS hostname lookup (e.g., `mastervolt-bridge.local`)
   - Static IP fallback `192.168.1.48` (lowest)

4. **Documentation**
   - Update `docs/SETUP_README.md` with discovery examples
   - Add mDNS hostname to `AGENTS.md`
   - Document `MASTERVOLT_BRIDGE_HOST` env var usage

### Implementation Notes
- **mDNS Library**: `ESPmDNS.h` (part of Arduino core for ESP32)
- **Python Discovery**: Use `mdns` or `zeroconf` packages (add to `pyproject.toml`)
- **Windows Compatibility**: May need fallback to ARP scan on networks without mDNS

### Blocked By
- None (can implement independently)

### Related Issues
- Static IP hardcoding in:
  - `firmware/esp32_inverter_bridge/settings.cpp` (example uses)
  - All Python scripts (see above)
  - `README.md` examples
  - Test scripts

---

## Future Enhancement Ideas

- [ ] Web-based configuration dashboard (firmware side)
- [ ] Home Assistant auto-discovery (mDNS + SSDP)
- [ ] TLS/HTTPS for API (requires certificate management)
- [ ] Multiple inverter support (mesh/multi-unit coordination)
- [ ] OTA firmware updates (with integrity checking)
- [ ] MQTT bridge mode (publish/subscribe inverter data)
