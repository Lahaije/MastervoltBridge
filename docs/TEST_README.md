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
| GET /api/version | JSON with key: `firmware_version` |
| GET /api/health | JSON with keys: `wifi_connected`, `wifi_ssid`, `wifi_ip`, `ethernet_ip`, `inverter_host`, `last_inverter_status`, `debug_mode`, `inverter_link_state`, `inverter_failure_streak_ms`, `inverter_retry_interval_ms` |
| GET /api/logs | Array of log entries |
| GET /api/info | Inverter telemetry (`ready=true` once a poll has succeeded; `ready=false` with empty fields immediately after boot) |

## Inverter-Dependent Endpoints

These require inverter WiFi to be reachable:

- GET /api/info
- POST /api/power
- POST /api/inverter/fetch

If inverter is off or unavailable (e.g. after sunset), 502 responses are expected and do not indicate a firmware problem.

`POST /api/power` should return 202 (queued) when inverter WiFi is down.

## Endpoint Verification After Flash

```
curl http://192.168.1.48:8080/
curl http://192.168.1.48:8080/api/version
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
| /api/info shows `ready=false` after boot | First poll not yet complete | Wait 20-30 s and retry |
| /api/power returns 202 | Inverter WiFi unavailable, command queued | Expected; command will retry on future polls |
| Connection timeouts in logs | Inverter WiFi module asleep | Bridge will auto-pulse on next attempt |
