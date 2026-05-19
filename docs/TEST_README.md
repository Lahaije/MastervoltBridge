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
| GET /api/health | JSON with keys: `wifi_connected`, `wifi_ssid`, `wifi_ip`, `ethernet_ip`, `inverter_host`, `last_inverter_status`, `debug_mode` |
| GET /api/logs | Array of log entries |
| GET /api/info | Inverter telemetry (or 502 if inverter WiFi is off) |

## Inverter-Dependent Endpoints

These require inverter WiFi to be reachable:

- GET /api/info
- POST /api/power
- POST /api/inverter/fetch

If inverter is off or unavailable (e.g. after sunset), 502 responses are expected and do not indicate a firmware problem.

## Endpoint Verification After Flash

```
curl http://192.168.1.48:8080/
curl http://192.168.1.48:8080/api/health
curl http://192.168.1.48:8080/api/logs
curl -X POST http://192.168.1.48:8080/wifi/off
curl http://192.168.1.48:8080/pulse
```

## Troubleshooting

| Symptom | Likely cause | Action |
|---|---|---|
| /wifi/off returns 404 | Old firmware image | Re-upload latest firmware |
| /api/logs JSON decode errors | Truncated response | Verify logging is not excessive |
| /pulse returns `reconnected: false` at night | Inverter WiFi unavailable | Rerun daytime |
| /api/info returns 502 after boot | First poll not yet complete | Wait 20-30 s and retry |
| Connection timeouts in logs | Inverter WiFi module asleep | Bridge will auto-pulse on next attempt |
