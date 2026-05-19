# Resource: skills/log-analysis/SKILL.md

**What it documents**: How to run the log-analysis script and interpret its output.

**Source of truth**: `skills/log-analysis/analyze_bridge_logs.py` and the `[WIFI-CONNECT]` log format emitted by `wifi_bridge.cpp`.

**Update when**:
- The log-analysis script gains new options or output sections.
- The `[WIFI-CONNECT]` log format in `wifi_bridge.cpp` changes (new fields, renamed keys).
- The connect strategy names (dwell/auto) change.

**Do NOT update for**: Firmware logic, API changes, wiring, or upload procedure changes.
