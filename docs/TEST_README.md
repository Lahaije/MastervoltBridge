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
| GET / | HTML dashboard page |
| GET /api | Discovery JSON with endpoint list |
| GET /api/health | JSON with keys: `wifi_connected`, `wifi_ssid`, `wifi_ip`, `ethernet_ip`, `inverter_host`, `last_inverter_status`, `debug_mode` |
| GET /api/logs | JSON object with `total_entries` and `entries` array |
| GET /api/info | Always 200; telemetry fields may be empty before first successful poll |
| POST /api/interval | Updates base poll interval with JSON body `{"interval":...}` |

## Web UI Delivery Validation

Validate the following:

- GET / returns `Content-Type: text/html`.
- GET / returns one full page payload with inline CSS + JS.
- GET /api returns discovery JSON for machine clients.
- Repeated GET / calls do not show heap-fragmentation drift.
- Response is written in bounded chunks and aborts safely if client disconnects.
- Content-Length equals compile-time blob size (`sizeof(WEB_UI_HTML) - 1`).

## Inverter-Dependent Endpoints

These require inverter WiFi to be reachable:

- POST /api/power
- POST /api/shadow
- POST /api/inverter/fetch

If inverter is off or unavailable (e.g. after sunset), 502 responses are expected and do not indicate a firmware problem.

## Endpoint Verification After Flash

```
curl http://192.168.1.48:8080/
curl http://192.168.1.48:8080/api
curl http://192.168.1.48:8080/api/health
curl http://192.168.1.48:8080/api/logs
curl -X POST http://192.168.1.48:8080/wifi/off
curl http://192.168.1.48:8080/pulse
curl -X POST -H "Content-Type: application/json" -d '{"interval":20000}' http://192.168.1.48:8080/api/interval
```

## Troubleshooting

| Symptom | Likely cause | Action |
|---|---|---|
| /wifi/off returns 404 | Old firmware image | Re-upload latest firmware |
| /api/logs JSON decode errors | Truncated response | Verify logging is not excessive |
| /pulse returns `reconnected: false` at night | Inverter WiFi unavailable | Rerun daytime |
| /api/info fields are empty after boot | First poll not yet complete | Wait 20-30 s and retry |
| Connection timeouts in logs | Inverter WiFi module asleep | Bridge will auto-pulse on next attempt |
