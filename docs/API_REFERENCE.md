# ESP32 Bridge API Reference

Base URL:

http://<bridge-ethernet-ip>:8080

Example:

http://192.168.1.48:8080

## Endpoint Summary

| Method | Path | Notes |
|---|---|---|
| GET | / | API discovery |
| GET | /api/health | Bridge network/inverter status |
| GET | /api/logs | Log buffer (up to 1000 entries) |
| GET | /api/info | Cached inverter /home telemetry |
| POST | /api/power | Set inverter power |
| POST | /api/inverter/fetch | Fetch inverter endpoint |
| POST | /wifi/off | Single button press if WiFi is connected |
| GET | /pulse | Recovery pulse + connection-time measurement |

## GET /

Returns discovery JSON containing all current endpoints.

## GET /api/health

Returns bridge state, including:

- wifi_connected
- wifi_ssid
- wifi_ip
- ethernet_ip
- inverter_host
- last_inverter_status

When inverter is unavailable, this endpoint still works.

## GET /api/logs

Returns diagnostic logs.

Notes:

- Log buffer size is 1000 entries.
- Excessive logging can make responses very large; current firmware reduces recovery scan log noise.

## GET /api/info

Returns cached parsed inverter telemetry from /home.

Typical failure when inverter is unavailable:

- 502 with error: No inverter telemetry data available yet

## POST /api/power

Body:

{"power":1200}

Validation:

- integer only
- range 0 to 1575

If inverter WiFi is down, returns 502.

## POST /api/inverter/fetch

Body:

{"url":"/home"}

Rules:

- url must start with /

If inverter WiFi is down, returns 502.

## POST /wifi/off

Purpose:

- If bridge WiFi is currently connected, sends one button press to force inverter WiFi off.

Response:

{"pressed":true}

or

{"pressed":false}

pressed false is expected when WiFi is already disconnected.

## GET /pulse

Purpose:

- Triggers the double-press recovery sequence and runs measurement logic.

Response:

{"status":"pulse_complete"}

Important behavior:

- This endpoint is used for WiFi timing experiments.
- During measurement, normal WiFi connect/fetch paths are blocked by design.

## Measurement-Specific Operational Notes

Use this safe cycle:

1. GET /pulse
2. POST /wifi/off
3. wait 2-3 seconds
4. repeat

Do not send double-press while inverter WiFi is already ON.

## Known Expected Errors (Night / Inverter Off)

When inverter WiFi is not available:

- GET /api/info -> 502
- POST /api/power -> 502
- POST /api/inverter/fetch -> 502

Still expected to respond:

- /
- /api/health
- /api/logs
- /wifi/off
- /pulse
