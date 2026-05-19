# Technical Debt & Future Work

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
