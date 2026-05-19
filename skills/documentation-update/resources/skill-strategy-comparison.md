# Resource: skills/strategy-comparison/SKILL.md

**What it documents**: How to run the strategy comparison script and interpret the dwell vs auto comparison table and verdict.

**Source of truth**: `skills/strategy-comparison/compare_strategies.py` and the `[WIFI-CONNECT]` log format emitted by `wifi_bridge.cpp`.

**Update when**:
- The comparison script gains new metrics or output sections.
- The `[WIFI-CONNECT]` log format changes (new fields, renamed keys).
- The connect strategy names (dwell/auto) change.
- The minimum sample threshold default changes.

**Do NOT update for**: Firmware logic changes, API changes, wiring, or upload procedure changes.
