# Home Assistant Integration

The Mastervolt Bridge publishes inverter telemetry to Home Assistant via MQTT auto-discovery. No manual YAML configuration is needed — entities appear automatically once the bridge connects to your MQTT broker.

## Prerequisites

1. **MQTT Broker** running and accessible from the bridge's Ethernet network (e.g. Mosquitto add-on in HA).
2. **MQTT Integration** enabled in Home Assistant (`Settings → Devices & Services → MQTT`).

## Bridge Configuration

Configure the MQTT broker address via the bridge web UI at `http://192.168.1.48:8080` → **MQTT Settings** section:

| Setting | Default | Description |
|---------|---------|-------------|
| Broker IP | `192.168.1.23` | IP address of your MQTT broker |
| Broker Port | `1883` | MQTT broker port |
| Enabled | `true` | Enable/disable MQTT publishing |
| Topic Prefix | `mastervolt_bridge` | Base topic for all MQTT messages |

Settings are stored in flash (NVS) and persist across power cycles.

You can also configure via the REST API:

```bash
# Read current MQTT settings
curl http://192.168.1.48:8080/api/mqtt

# Update MQTT settings
curl -X POST http://192.168.1.48:8080/api/mqtt \
  -H "Content-Type: application/json" \
  -d '{"broker_ip":"192.168.1.23","broker_port":1883,"enabled":true,"topic_prefix":"mastervolt_bridge"}'
```

## Auto-Discovered Entities

Once connected, the bridge publishes MQTT discovery messages and the following entities appear in Home Assistant:

### Sensors

| Entity | Unit | Description |
|--------|------|-------------|
| `sensor.mastervolt_bridge_power` | W | Real-time instantaneous power output |
| `sensor.mastervolt_bridge_total_yield` | kWh | Lifetime cumulative energy production |
| `sensor.mastervolt_bridge_daily_yield` | kWh | Today's energy production (resets at midnight on inverter) |
| `sensor.mastervolt_bridge_poll_interval` | s | Current polling interval |

### Controls

| Entity | Unit | Description |
|--------|------|-------------|
| `number.mastervolt_bridge_power_limit` | W | Power limit slider (0–1575 W) |

The power limit entity is a **Number** with:
- Min: 0 W
- Max: 1575 W
- Step: 1 W
- Mode: slider

Changing the slider in HA immediately sends a command to the bridge, which forwards it to the inverter. The slider reflects the real-time value reported by the inverter.

## Setting Up a Power Plant (Energy Dashboard)

### Step 1: Add Solar Production to Energy Dashboard

1. Go to `Settings → Dashboards → Energy`
2. Under **Solar Panels**, click **Add Solar Production**
3. Select `sensor.mastervolt_bridge_total_yield` as the production sensor
4. HA uses `total_yield` (state_class: `total_increasing`) to calculate production over time

### Step 2: Create a Power Plant Card (Optional)

Add a custom card to your dashboard for real-time monitoring:

```yaml
type: vertical-stack
cards:
  - type: gauge
    entity: sensor.mastervolt_bridge_power
    name: Solar Power
    min: 0
    max: 1575
    severity:
      green: 500
      yellow: 1000
      red: 1400
  - type: entities
    entities:
      - entity: sensor.mastervolt_bridge_daily_yield
        name: Today
      - entity: sensor.mastervolt_bridge_total_yield
        name: Lifetime
      - entity: number.mastervolt_bridge_power_limit
        name: Power Limit
      - entity: sensor.mastervolt_bridge_poll_interval
        name: Poll Interval
```

### Step 3: Power Limit Slider

The `number.mastervolt_bridge_power_limit` entity works as a slider out of the box:
- Displays the **current** inverter power limit (updated every poll cycle)
- When you move the slider, HA publishes to `mastervolt_bridge/number/power_limit/set`
- The bridge receives the command, sends it to the inverter, and publishes the confirmed value back

## MQTT Topics Reference

All topics use the configured prefix (default: `mastervolt_bridge`).

### State Topics (bridge → HA)

| Topic | Payload | Update Frequency |
|-------|---------|-----------------|
| `mastervolt_bridge/sensor/power/state` | `674.5` | Every successful poll (~20s) |
| `mastervolt_bridge/sensor/total_yield/state` | `8566.628` | Every successful poll |
| `mastervolt_bridge/sensor/daily_yield/state` | `12.811` | Every successful poll |
| `mastervolt_bridge/sensor/poll_interval/state` | `20` | Every successful poll |
| `mastervolt_bridge/number/power_limit/state` | `1575` | Every successful poll |
| `mastervolt_bridge/status` | `online` / `offline` | On connect / LWT |

### Command Topics (HA → bridge)

| Topic | Payload | Action |
|-------|---------|--------|
| `mastervolt_bridge/number/power_limit/set` | `1200` | Sets inverter power limit to 1200 W |

### Discovery Topics

Published once on connect with `retain: true`:

| Topic | Purpose |
|-------|---------|
| `homeassistant/sensor/mastervolt_bridge/power/config` | Power sensor discovery |
| `homeassistant/sensor/mastervolt_bridge/total_yield/config` | Total yield discovery |
| `homeassistant/sensor/mastervolt_bridge/daily_yield/config` | Daily yield discovery |
| `homeassistant/sensor/mastervolt_bridge/poll_interval/config` | Poll interval discovery |
| `homeassistant/number/mastervolt_bridge/power_limit/config` | Power limit number discovery |

## Availability

The bridge uses MQTT Last Will and Testament (LWT):
- **Online**: publishes `online` to `mastervolt_bridge/status` on connect
- **Offline**: broker publishes `offline` to `mastervolt_bridge/status` when connection drops

All entities show as "unavailable" in HA when the bridge is offline.

## Troubleshooting

| Symptom | Cause | Fix |
|---------|-------|-----|
| Entities don't appear | MQTT not connected | Check broker IP/port in web UI; verify broker is running |
| Values are stale | Bridge is in BACKOFF/DORMANT | Check `/api/health` for link state; inverter may be off |
| Power limit slider unresponsive | Bridge offline or inverter unreachable | Command is queued (deferred) and applied on next successful connection |
| "Unavailable" in HA | Bridge Ethernet disconnected or powered off | Check physical connection |

Check bridge logs at `http://192.168.1.48:8080/api/logs` for MQTT-related messages (prefixed with `[MQTT]`).
