# Resource: docs/HOME_ASSISTANT.md

**What it documents**: Home Assistant integration — MQTT Discovery setup
(recommended) and REST polling fallback (no broker). Covers broker
configuration, the device + entities HA auto-creates, MQTT topic layout,
the dual-topic cache pattern for write-only inverter settings
(`power_limit`, `shadow`), an HA energy-dashboard recipe, and one
automation example.

**Source of truth in the codebase**:

- `firmware/esp32_inverter_bridge/mqtt_bridge.{h,cpp}` — discovery
  payload, topic constants, publish/subscribe logic, NVS-cached credentials.
- `firmware/esp32_inverter_bridge/inverter_monitor.{h,cpp}` — cached
  `power_limit` / `shadow` (RAM + NVS `invmon`) that backs the retained
  `state/power_limit` and `state/shadow` topics.
- `firmware/esp32_inverter_bridge/api.cpp` — `POST /api/mqtt` semantics
  (broker IP, HA enable flag, user/password updates that don't wipe each
  other).
- `firmware/esp32_inverter_bridge/settings.cpp` — `MQTT_PORT`,
  `MQTT_DEVICE_ID`, `MQTT_DISCOVERY_PREFIX`, default HA-enable flag.

**Update when**:

- A new HA entity is added or an existing one's `device_class`,
  `state_class`, unit, or topic changes.
- Topic layout under `homeassistant/...` or `mastervolt/<device>/...` is
  reorganized.
- The dual-topic cache pattern (RAM/NVS write-through + retained mirror)
  changes — e.g. a new write-only inverter setting is added.
- `POST /api/mqtt` accepts or stops accepting a field (`broker`,
  `ha_enabled`, `user`, `password`).
- Broker discovery semantics change (e.g. /24 scan replaced by mDNS).
- The REST-fallback section needs a sensor that doesn't map to existing
  `/api/info` fields (in which case `docs/API_REFERENCE.md` updates first).

**Do NOT update for**:

- Internal MQTT client buffer sizes, retry intervals, or scan timeouts
  (those live in `AGENTS.md` performance / settings sections).
- WiFi strategy or inverter HTTP transport details.
- Adding a new `/api/*` endpoint that isn't surfaced to HA — that is an
  `API_REFERENCE.md` change only.

**Deduplication rules**:

- The REST-fallback section MUST NOT redocument endpoint schemas. Link to
  `docs/API_REFERENCE.md` and only show the HA YAML wiring.
- Post-flash MQTT validation steps belong in `docs/TEST_README.md`. This
  doc only covers initial setup ("how do I get HA talking to the bridge"),
  not "did the flash succeed".
