# Resource: AGENTS.md

**What it documents**: The single canonical reference for architecture and design decisions — module dependency graph, `WifiConnectionManager` design pattern, dwell/auto connect strategies with timing data, API table, `HomeData` struct (with C++ field names vs JSON field names distinction), `InverterController` public interface, configuration constants, build/upload commands, common task recipes, gotchas, and performance characteristics.

**Source of truth**: The full codebase — primarily `wifi_bridge.h/cpp`, `inverter_controller.h/cpp`, `api.cpp`, `settings.cpp`, `logger.h`.

**Single-source-of-truth rules**:
- WiFi dwell/auto strategy details (parameters, timing, log format) live HERE only. Other docs link here.
- Module dependency graph lives HERE only.
- Performance numbers (connect times, heap, timeouts) live HERE only.
- C++ struct field names vs JSON field name mapping lives HERE only.

**Update when**:
- Module structure changes (new file, renamed file, changed responsibilities).
- `WifiConnectionManager` interface changes.
- Connect strategies (dwell/auto) change behaviour or parameters.
- API endpoints change.
- `HomeData` struct fields change.
- Configuration constants added or removed from `settings.cpp`.
- A new gotcha or performance characteristic is identified.

**Do NOT update for**: Documentation-only changes, wiring changes, or test procedure changes.
