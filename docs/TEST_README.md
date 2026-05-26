# Test README

Validation guide for the ESP32 inverter bridge firmware.

## Preconditions

- Firmware flashed successfully.
- Ethernet link active.
- Bridge reachable on port 8080.
- Inverter WiFi availability known (daytime vs nighttime).

## Quick API Sanity

| Check | Expected |
|---|---|
| GET / | Endpoint discovery JSON |
| GET /api/health | JSON with keys: `wifi_connected`, `wifi_ssid`, `wifi_ip`, `ethernet_ip`, `inverter_host`, `last_inverter_status`, `debug_mode`, `ha_mqtt_enabled`, `mqtt_broker`, `mqtt_connected`, `mqtt_scanning` |
| GET /api/logs | Array of log entries |
| GET /api/info | Inverter telemetry + cached `power_limit_*`/`shadow_*` (or 502 if inverter WiFi is off) |
| GET /api/mqtt | `{ha_enabled, broker_ip, connected, scanning}` |

## Inverter-Dependent Endpoints

These require inverter WiFi to be reachable:

- GET /api/info
- POST /api/power
- GET  /api/shadow
- POST /api/shadow
- POST /api/inverter/fetch

If inverter is off or unavailable (e.g. after sunset), 502 responses are expected and do not indicate a firmware problem.

## Endpoint Verification After Flash

```
curl http://192.168.1.48:8080/
curl http://192.168.1.48:8080/api/health
curl http://192.168.1.48:8080/api/logs
curl http://192.168.1.48:8080/api/mqtt
curl -X POST http://192.168.1.48:8080/wifi/off
curl http://192.168.1.48:8080/pulse
```

Fast script-based validation:

```
.venv\Scripts\python skills/api-validation/validate_api.py
```

## MQTT / Home Assistant Validation

After flash, with a broker reachable on the LAN:

1. `GET /api/mqtt` — confirm `connected:true` (or `scanning:true` if no broker yet stored).
2. If `scanning:false` and `connected:false`, set the broker IP explicitly:
   `curl -X POST -H "Content-Type: application/json" -d '{"ha_enabled":true,"broker_ip":"192.168.1.10"}' http://192.168.1.48:8080/api/mqtt`
3. In HA, confirm a device **"Mastervolt SOLADIN 1500"** appears with sensors (Power, Daily/Total Yield) and controls (Power Limit, Shadow Function).
4. Issue `POST /api/power` with a new value and verify HA's Power Limit slider updates within ~1 s (via the retained `state/power_limit` topic).
5. Reboot the bridge and confirm HA's Power Limit + Shadow tiles repopulate from NVS-cached values once MQTT reconnects.

## Troubleshooting

| Symptom | Likely cause | Action |
|---|---|---|
| /wifi/off returns 404 | Old firmware image | Re-upload latest firmware |
| /api/logs JSON decode errors | Truncated response | Verify logging is not excessive |
| /pulse returns `reconnected: false` at night | Inverter WiFi unavailable | Rerun daytime |
| /api/info returns 502 after boot | First poll not yet complete | Wait 20-30 s and retry |
| Connection timeouts in logs | Inverter WiFi module asleep | Bridge will auto-pulse on next attempt |
