# Resource: AGENTS.md

**What it documents**: The canonical architecture and operational reference — module graph, `WifiConnectionManager`, dwell/auto strategy notes, `HomeData`, `InverterController`, configuration constants, gotchas, and performance data.

**Source of truth**: The codebase, especially `wifi_bridge.h/cpp`, `inverter_controller.h/cpp`, `api.cpp`, `settings.cpp`, and `logger.h`.

**Single-source-of-truth rules**:
- WiFi dwell/auto strategy details live here.
- The module dependency graph lives here.
- Performance numbers live here.
- `HomeData` field mapping lives here.

**Update when**:
- The module graph changes.
- `WifiConnectionManager` changes.
- Dwell/auto behavior changes.
- API or data-model fields change.
- Configuration constants change.
- A new gotcha or performance note is verified.

**Do NOT update for**: Wiring changes or test-procedure changes.
