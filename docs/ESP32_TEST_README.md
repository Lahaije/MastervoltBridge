# ESP32 Bridge Validation & Testing

## Health Check

Verify the bridge is running and connected:

```bash
curl -s http://192.168.1.48:8080/api/health | jq .
```

Expected response:
```json
{
  "wifi_connected": true,
  "wifi_ssid": "mastervolt-soladin-0103",
  "wifi_ip": "10.0.0.42",
  "ethernet_ip": "192.168.1.48",
  "inverter_host": "10.0.0.1",
  "last_inverter_status": 200
}
```

## Get Latest Telemetry

```bash
curl -s http://192.168.1.48:8080/api/info | jq .
```

Should return 8 fields from inverter:
- `operating_status`: 1=normal, 2=error, etc.
- `error_alarm_code`: 0=no error
- `operating_mode`: 1=production, 2=standby, etc.
- `inverter_model`: H500A0103, etc.
- `inverter_mac_address`: 00:06:66:...
- `power`: Current power output in watts (e.g., 674.547)
- `total_yield`: Total cumulative production in kWh (e.g., 8566.628)
- `daily_yield`: Daily/session production in kWh (e.g., 12.811)

## Set Inverter Power

### Via JSON POST body:
```bash
curl -X POST -H "Content-Type: application/json" \
  -d '{"power":500}' \
  http://192.168.1.48:8080/api/power
```

Response:
```json
{
  "requested_power_watts": 500,
  "inverter_http_status": 200,
  "inverter_response": "OK"
}
```

## Fetch Generic Inverter Endpoint

```bash
curl -X POST -H "Content-Type: application/json" \
  -d '{"url":"/home"}' \
  http://192.168.1.48:8080/api/inverter/fetch
```

## View Logs

```bash
curl -s http://192.168.1.48:8080/api/logs | jq .
```

Displays up to 1000 log entries with millisecond timestamps. Look for:
- `[WIFI-BRIDGE]` - WiFi connection events
- `[INVERTER-MONITOR]` - Polling results
- `[ETH]` - Ethernet initialization
- `[RECOVERY]` - GPIO pulse sequences

## Trigger WiFi Recovery

If WiFi drops, manually trigger recovery pulse:

```bash
curl http://192.168.1.48:8080/pulse
```

## Python Test Script

Run comprehensive validation:

```bash
python test_bridge.py
```

## Troubleshooting

| Symptom | Check | Fix |
|---------|-------|-----|
| `Connection refused` | Is bridge powered and booted? | Check `/api/logs` |
| `wifi_connected: false` | WiFi SSID/password correct? | Edit `settings.cpp`, reupload |
| `last_inverter_status: 0` | Bridge WiFi connected but inverter unreachable | Check inverter WiFi network |
| `502 Bad Gateway` on `/api/info` | Polling hasn't completed yet | Wait 20-30s for first poll |
| Logs show "read Timeout" | HTTP timeout too short | Increase `TELEMETRY_HTTP_TIMEOUT_MS` |
| Logs show "Connect timeout" | WiFi signal weak | Move ESP32 closer to inverter |

Use one of the following:

Send power command via JSON POST body:

```bash
curl -X POST http://<esp32-ethernet-ip>:8080/api/power \
  -H "Content-Type: application/json" \
  -d '{"power":1200}'
```

Expected result:

- JSON response with `requested_power_watts` and inverter status.

## E) Outage recovery pulse test

Goal: verify one pulse train per outage cycle.

Pulse definition:

- HIGH for 100 ms
- LOW for 200 ms
- HIGH for 100 ms

### Test method with LED (quick)

1. Connect LED + resistor from recovery pin (GPIO4) to GND.
2. Force outage by turning inverter WiFi off, or changing credentials temporarily.
3. Wait at least outage detection window (~5 s).
4. Observe double blink corresponding to two pulses.
5. Keep outage active and verify no repeated pulse trains occur.
6. Restore inverter WiFi so ESP32 reconnects.
7. Force outage again and verify one new pulse train occurs.

### Test method with oscilloscope (precise)

Measure GPIO4 waveform during outage trigger and verify durations are approximately 100 ms, 200 ms, 100 ms.

## F) Home Assistant integration sanity check

Use ESP32 Ethernet API as source in HA sensors/automation:

- Read data from `/api/info`.
- Write power limit using `/api/power`.

## Troubleshooting quick list

| Symptom | Likely cause |
|---|---|
| No Ethernet IP | Check SPI wiring, pin mapping, and power |
| Ethernet IP is 0.0.0.0 | ENC28J60 not responding — check CS and SCK pins |
| WiFi never connects | Verify inverter SSID/password and AP availability |
| 502 responses | Inverter endpoint unreachable from ESP32 |
| Wrong POST behavior | Inverter may expect a different payload format; adapt `inverterSetPower` in firmware |
| Upload fails | Enter download mode manually: hold BOOT, press EN, release BOOT |
