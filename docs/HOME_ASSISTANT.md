# Home Assistant Integration

The bridge integrates with Home Assistant in two ways:

1. **MQTT Discovery (recommended)** — the bridge announces itself as a true
   HA *device* via the standard `homeassistant/...` retained-topic protocol.
   No YAML required on the HA side beyond the standard MQTT integration.
2. **REST polling (fallback)** — for installations without an MQTT broker.
   Uses `/api/info` and `/api/health` via HA's `rest` and `rest_command`
   integrations.

---

## Option 1 — MQTT Discovery (recommended)

### Prerequisites

- An MQTT broker reachable from the bridge's Ethernet IP (typically Mosquitto
  running on Home Assistant — install the **Mosquitto broker** add-on).
- Anonymous access on port 1883 from the bridge's IP (the ENC28J60 stack
  does not support TLS or DNS — IP + plain MQTT only).
- HA's **MQTT** integration configured against the same broker.

### Enable the integration on the bridge

Once flashed, the bridge stores the broker IP and HA-enable flag in NVS
(survives reboot). Two options:

**Auto-discovery**: on every Ethernet DHCP lease, the bridge probes its own
`/24` for any host accepting TCP on port 1883 and persists the first hit.
This works on simple flat networks but is slow (~3–6 s per dead host); the
scan runs one host per main-loop iteration so the REST API stays responsive.

**Explicit (recommended for production)**:

```bash
curl -X POST -H "Content-Type: application/json" \
     -d '{"ha_enabled":true,"broker_ip":"192.168.1.10"}' \
     http://192.168.1.48:8080/api/mqtt
```

Verify:

```bash
curl http://192.168.1.48:8080/api/mqtt
# {"ha_enabled":true,"broker_ip":"192.168.1.10","connected":true,"scanning":false}
```

Disable without losing the broker IP:

```bash
curl -X POST -H "Content-Type: application/json" \
     -d '{"ha_enabled":false}' \
     http://192.168.1.48:8080/api/mqtt
```

### What appears in Home Assistant

After the broker connects, HA auto-creates a single device
**"Mastervolt SOLADIN 1500"** with:

| Entity | Type | Source |
|---|---|---|
| Power | sensor (W) | `state` topic, every poll (~20 s) |
| Daily Yield | sensor (kWh) | `state` topic |
| Total Yield | sensor (kWh) | `state` topic |
| Operating Status | sensor | `state` topic |
| WiFi Connected | binary_sensor | `state` topic |
| Power Limit | number (0–1575 W) | `cmd/power` → bridge; mirror on `state/power_limit` |
| Shadow Function | switch | `cmd/shadow` → bridge; mirror on `state/shadow` |

All entities follow availability via the LWT topic
`mastervolt/<device>/availability` (retained `online`/`offline`).

### Topic layout

```
homeassistant/<component>/mastervolt_soladin_1500/<object>/config  (retained discovery)
mastervolt/mastervolt_soladin_1500/availability                     (LWT, retained)
mastervolt/mastervolt_soladin_1500/state                            (telemetry JSON)
mastervolt/mastervolt_soladin_1500/state/power_limit                (retained mirror)
mastervolt/mastervolt_soladin_1500/state/shadow                     (retained mirror)
mastervolt/mastervolt_soladin_1500/cmd/power                        (HA → bridge)
mastervolt/mastervolt_soladin_1500/cmd/polling                      (HA → bridge)
mastervolt/mastervolt_soladin_1500/cmd/shadow                       (HA → bridge)
```

### Cache + dual-topic pattern for write-only settings

The Soladin's `/home` endpoint never reports `power_limit` or the `shadow`
function. Without help, HA's Power Limit slider and Shadow switch would show
"unknown" after every restart. The bridge solves this by:

1. Caching the last *successfully commanded* value in RAM **and** the
   ESP32's NVS (`invmon` namespace).
2. Publishing it retained to `state/power_limit` / `state/shadow`
   immediately after a successful `POST /api/power` or `POST /api/shadow`.
3. Re-publishing the cached values on every MQTT (re)connect.

HA Discovery declares `cmd_t` (writes) and `stat_t` (mirror) for those
entities. Result: HA's controls show the bridge's view of the inverter,
which is correct as long as commands flow through the bridge.

If you change the inverter's settings via its native web UI while the
bridge is offline, those out-of-band changes are not visible to HA until
the next successful `POST /api/power` / `POST /api/shadow` (the bridge's
cache wins).

### Energy dashboard

Settings → Dashboards → Energy → Solar Panels → Add solar production →
select `sensor.mastervolt_total_yield`.

### Automation example

```yaml
automation:
  - alias: "Limit Mastervolt to 800W when battery full"
    trigger:
      - platform: numeric_state
        entity_id: sensor.battery_level
        above: 95
    action:
      - service: number.set_value
        target:
          entity_id: number.mastervolt_power_limit
        data:
          value: 800
```

---

## Option 2 — REST fallback (no MQTT broker)

Use this if you don't have or don't want an MQTT broker. The bridge will
still serve telemetry and commands over HTTP, and HA can poll/post via its
`rest` and `rest_command` platforms.

### Endpoints used

| Endpoint | Method | Purpose |
|----------|--------|---------|
| `/api/info` | GET | Telemetry + cached power_limit/shadow |
| `/api/health` | GET | Bridge link state, MQTT status |
| `/api/power` | POST | Set power limit (0–1575 W) |
| `/api/shadow` | POST | Enable/disable shadow function |
| `/pulse` | GET | Wake inverter WiFi (GPIO double-press) |
| `/wifi/off` | POST | Turn inverter WiFi off (single press) |

### Minimal package file

`packages/mastervolt.yaml`:

```yaml
rest:
  - resource: http://192.168.1.48:8080/api/info
    scan_interval: 20
    sensor:
      - name: "Mastervolt Power"
        unique_id: mastervolt_power
        value_template: "{{ value_json.power }}"
        device_class: power
        state_class: measurement
        unit_of_measurement: "W"
      - name: "Mastervolt Total Yield"
        unique_id: mastervolt_total_yield
        value_template: "{{ value_json.total_yield }}"
        device_class: energy
        state_class: total_increasing
        unit_of_measurement: "kWh"
      - name: "Mastervolt Daily Yield"
        unique_id: mastervolt_daily_yield
        value_template: "{{ value_json.daily_yield }}"
        device_class: energy
        state_class: total_increasing
        unit_of_measurement: "kWh"
      - name: "Mastervolt Power Limit"
        unique_id: mastervolt_power_limit
        value_template: >-
          {% if value_json.power_limit_known %}{{ value_json.power_limit_watts }}{% else %}unknown{% endif %}
        unit_of_measurement: "W"

rest_command:
  mastervolt_set_power:
    url: http://192.168.1.48:8080/api/power
    method: POST
    content_type: application/json
    payload: '{"power": {{ power }}}'
  mastervolt_set_shadow:
    url: http://192.168.1.48:8080/api/shadow
    method: POST
    content_type: application/json
    payload: '{"enabled": {{ enabled }}}'
  mastervolt_pulse:
    url: http://192.168.1.48:8080/pulse
    method: GET
  mastervolt_wifi_off:
    url: http://192.168.1.48:8080/wifi/off
    method: POST
```

Add controls via `input_number` / `input_boolean` and bind them to the
`rest_command` services with automations triggered on state change.

---

## Notes & caveats

- HAOS cannot resolve DHCP hostnames (e.g. `mv-bridge.local`) — always use
  the bridge's static IP.
- The bridge polls the inverter every 20 s; MQTT publishes happen on the
  same cadence (no separate timer).
- When the inverter is off at night, telemetry stops; HA marks entities
  unavailable via the LWT, and re-publishes resume at sunrise.
- Valid power range: 0–`INVERTER_MAX_POWER_WATTS` (1575 W on the SOLADIN
  1500).
- All MQTT topic paths and the discovery prefix are configurable at compile
  time via `MQTT_DEVICE_ID` and `MQTT_DISCOVERY_PREFIX` in `settings.cpp`.
