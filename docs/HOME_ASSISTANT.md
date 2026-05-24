# Home Assistant Integration

The ESP32 bridge exposes a dedicated endpoint optimized for Home Assistant's RESTful integration.

## Endpoint

```
GET http://192.168.1.48:8080/api/ha
```

Response:
```json
{
  "power": 674.55,
  "total_yield": 8566.628,
  "daily_yield": 12.811,
  "power_limit": 1000,
  "available": true
}
```

| Field | Type | Unit | Description |
|-------|------|------|-------------|
| `power` | float | W | Instantaneous power output |
| `total_yield` | float | kWh | Lifetime energy produced |
| `daily_yield` | float | kWh | Energy produced today (resets at midnight by inverter) |
| `power_limit` | int | W | Current power limit setting (0–1575) |
| `available` | bool | — | `true` when inverter data is fresh |

All values are `null` when unavailable (inverter offline).

## Configuration

Add to your `configuration.yaml`:

### Sensors (read)

```yaml
rest:
  - resource: http://192.168.1.48:8080/api/ha
    scan_interval: 20
    sensor:
      - name: "Solar Power"
        value_template: "{{ value_json.power }}"
        device_class: power
        state_class: measurement
        unit_of_measurement: "W"
        availability_template: "{{ value_json.available }}"
      - name: "Solar Total Yield"
        value_template: "{{ value_json.total_yield }}"
        device_class: energy
        state_class: total_increasing
        unit_of_measurement: "kWh"
        availability_template: "{{ value_json.available }}"
      - name: "Solar Daily Yield"
        value_template: "{{ value_json.daily_yield }}"
        device_class: energy
        state_class: total_increasing
        unit_of_measurement: "kWh"
        availability_template: "{{ value_json.available }}"
      - name: "Solar Power Limit"
        value_template: "{{ value_json.power_limit }}"
        device_class: power
        unit_of_measurement: "W"
```

### Power Limit Control (write)

```yaml
rest_command:
  set_solar_power_limit:
    url: http://192.168.1.48:8080/api/power
    method: POST
    content_type: application/json
    payload: '{"power": {{ power }}}'
```

#### Usage in automations

```yaml
automation:
  - alias: "Limit solar to 800W"
    action:
      - service: rest_command.set_solar_power_limit
        data:
          power: 800
```

#### Slider UI for power limit

Create an `input_number` helper and an automation to sync it:

```yaml
input_number:
  solar_power_limit:
    name: "Solar Power Limit"
    min: 0
    max: 1575
    step: 25
    unit_of_measurement: "W"
    icon: mdi:solar-power

automation:
  - alias: "Apply solar power limit from slider"
    trigger:
      - platform: state
        entity_id: input_number.solar_power_limit
    action:
      - service: rest_command.set_solar_power_limit
        data:
          power: "{{ states('input_number.solar_power_limit') | int }}"
```

## Energy Dashboard

To add the inverter to the HA Energy Dashboard:

1. Go to **Settings → Dashboards → Energy**
2. Under **Solar Panels**, click **Add solar production**
3. Select `sensor.solar_total_yield`

The `total_yield` sensor uses `state_class: total_increasing`, which HA's energy dashboard expects for tracking cumulative production.

## Notes

- The bridge polls the inverter every 20 seconds by default (configurable via `POST /api/polling`)
- Set `scan_interval` in HA to match or exceed the bridge poll interval
- When the inverter is off (night), `available` becomes `false` and sensors show as unavailable
- Power limit changes take effect within ~3 seconds on the inverter
- Valid power range: 0–1575 W (Mastervolt SOLADIN 1500)
