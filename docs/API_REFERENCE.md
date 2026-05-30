# ESP32 Bridge API Reference

Base URL:

http://<bridge-ethernet-ip>:8080

Example:

http://192.168.1.48:8080

## Endpoint Summary

| Method | Path | Notes |
|---|---|---|
| GET | / | Web UI dashboard (self-contained HTML) |
| GET | /api | API discovery JSON |
| GET | /api/device | Stable identity: firmware version, model, MACs, IPs |
| GET | /api/health | Bridge diagnostics: link state, operating status, WiFi, debug mode |
| GET | /api/logs | Log buffer (up to 1000 entries) |
| GET | /api/info | Real-time telemetry: power, yields, tunables |
| POST | /api/power | Set inverter power |
| POST | /api/shadow | Enable/disable inverter shadow function |
| POST | /api/inverter/fetch | Fetch inverter endpoint |
| POST | /wifi/off | Single button press if WiFi is connected |
| GET | /pulse | Recovery pulse + connection-time measurement |
| POST | /api/debug | Enable or disable verbose HTTP 200 success logging |
| POST | /api/interval | Temporarily override current poll interval (milliseconds) |

## GET /

Returns a self-contained HTML dashboard page (`web_ui.h` PROGMEM blob).

Implementation details:

- HTML stored as a single compile-time constant (`WEB_UI_HTML[]` in PROGMEM).
- Served in 512-byte chunks via `sendFlashHtmlResponse()` with `client.connected()` checks.
- Content-Length derived from `sizeof(WEB_UI_HTML) - 1` (compile-time).
- CSS and JS inline — single request, no external assets.

UI refresh strategy:

- `/api/device` fetched once on page load (stable identity).
- `/api/health` refreshed every 60 seconds (diagnostics).
- `/api/info` refreshed every 5 seconds (real-time telemetry).

UI controls:

- Power limit: input + Apply → POST /api/power
- Shadow function: checkbox + Apply → POST /api/shadow
- Debug mode: checkbox + Apply → POST /api/debug
- Wake Pulse button → GET /pulse
- WiFi Off button → POST /wifi/off

## GET /api

Returns discovery JSON listing all available endpoints.

- Stable machine-readable endpoint for automation and tooling.
- GET / serves HTML; GET /api remains JSON.

## GET /api/device

Returns stable device identity. These values only change on reboot, reflash, or network reconfiguration.

Response fields:

| Field | Type | Description |
|---|---|---|
| `firmware_version` | string | Firmware version in `<semver>-<YYYYMMDD>-<commit>` format |
| `inverter_model` | string | e.g. "H500A0103"; empty before first successful poll |
| `inverter_mac_address` | string | Inverter WiFi MAC; empty before first successful poll |
| `wifi_ssid` | string | Target inverter WiFi SSID |
| `wifi_ip` | string | Bridge WiFi IP ("0.0.0.0" when disconnected) |
| `ethernet_ip` | string | Bridge Ethernet IP (DHCP) |
| `inverter_host` | string | Inverter HTTP host IP |

## GET /api/health

Returns bridge diagnostics and inverter operating state.

Response fields:

| Field | Type | Description |
|---|---|---|
| `operating_status` | string | "1" = normal; empty before first poll |
| `operating_mode` | string | "1" = production; empty before first poll |
| `error_alarm_code` | string | "0" = no error; empty before first poll |
| `wifi_connected` | boolean | Whether bridge WiFi is currently connected to inverter |
| `inverter_link_state` | string | FSM state: "STARTING", "ONLINE", "RETRYING", "BACKOFF", or "DORMANT" |
| `last_update_ms` | number | Bridge uptime ms of last successful poll (0 if never polled) |
| `last_inverter_status` | number | Last HTTP status from inverter (-1 if never contacted) |
| `debug_mode` | boolean | Whether verbose HTTP 200 logging is enabled |

When inverter is unavailable, this endpoint still works.

## GET /api/logs

Returns diagnostic logs.

Notes:

- Log buffer size is 1000 entries.
- The response is streamed entry-by-entry, so it does not include a
  `Content-Length` header. Body framing uses `Connection: close` — clients
  must read until EOF. Standard HTTP libraries (`requests`, `urllib`,
  `curl`) handle this transparently.

## GET /api/info

Returns real-time inverter telemetry and tunable parameters.

This endpoint always returns HTTP 200. If no poll has succeeded yet,
`power` will be 0, yield fields will be `null`, and tunable fields may be `null`.

Response fields:

| Field | Type | Description |
|---|---|---|
| `power` | number | Current output power in watts, e.g. 674.547 |
| `failure_streak_s` | number | Seconds since last successful poll (0 when ONLINE) |
| `poll_interval_ms` | number | Current polling interval in milliseconds |
| `power_limit_watts` | number\|null | Cached inverter power limit in watts; `null` until first successful settings read |
| `shadow_enabled` | boolean\|null | Cached inverter shadow state; `null` until first successful settings read |
| `total_yield` | number\|null | Lifetime energy in kWh, e.g. `8566.628`; `null` when unavailable |
| `daily_yield` | number\|null | Daily energy in kWh, e.g. `12.811`; `null` when unavailable |

## POST /api/power

Body:

{"power":1200}

Validation:

- integer only
- range 0 to 1575

Response (applied):

{"requested_power_watts":1200,"applied":true,"inverter_http_status":200,"inverter_response":"...","readback_power_watts":1200}

`readback_power_watts` is `null` if the POST succeeded but the subsequent cache refresh fetch failed (the polling task will retry on the next successful poll).

Response (deferred — inverter temporarily unreachable):

HTTP 202

{"deferred":true,"desired_power_watts":1200,"reason":"..."}

The deferred value is queued and applied automatically on the next successful poll.

If inverter WiFi is down, returns 502.

## POST /api/shadow

Enable or disable the inverter shadow function.

Body:

{"enabled":true}

or

{"enabled":false}

Validation:

- `enabled` field must be a boolean (true/false) or numeric (1/0)

Response (applied):

{"requested_shadow":true,"applied":true,"inverter_http_status":200,"inverter_response":"...","readback_shadow":true}

`readback_shadow` is `null` if the POST succeeded but the subsequent cache refresh fetch failed (the polling task will retry on the next successful poll).

Response (deferred — inverter temporarily unreachable):

HTTP 202

{"deferred":true,"desired_shadow":true,"reason":"..."}

The deferred value is queued and applied automatically on the next successful poll.

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

- Sends the GPIO double-press wake pulse to the inverter WiFi module, then forces a fresh WiFi reconnect using the next alternating connect strategy (dwell or auto).

Response:

{"reconnected":true}

or

{"reconnected":false}

`reconnected: false` is expected when inverter WiFi is unavailable (e.g. nighttime).

Important behavior:

- Each call advances the internal dwell/auto alternation counter.
- Avoid calling `/pulse` while bridge WiFi is already connected — it will disconnect and reconnect.

## POST /api/debug

Purpose:

- Enable or disable debug mode at runtime.
- When debug mode is **on**, `[WIFI-BRIDGE] METHOD /path success (HTTP 200)` entries are written to the log buffer on every successful inverter poll.
- When debug mode is **off** (the default after boot), HTTP 200 successes are silent to keep the log buffer focused on errors and connectivity events.

Body:

{"debug":true}

or

{"debug":false}

Response:

{"debug":true}

Debug mode starts as `true` at boot and is automatically set to `false` at the end of `setup()`. Use this endpoint to re-enable it temporarily during live debugging.

## POST /api/interval

Purpose:

- Temporarily override the current polling interval.

Body:

{"interval":20000}

Validation:

- integer only
- range 100 to 300000 (milliseconds)

Response (applied):

{"poll_interval_ms":20000}

Behavior:

- The override takes effect immediately.
- The override remains active until the next natural link-state transition.
- Entering BACKOFF or DORMANT replaces the override with that state's fixed interval.
- Entering ONLINE restores the default poll interval from settings.
- RETRYING preserves the current interval until a transition to another state occurs.

## Known Expected Errors (Night / Inverter Off)

When inverter WiFi is not available:

- POST /api/power -> 502
- POST /api/shadow -> 502
- POST /api/inverter/fetch -> 502

Still expected to respond:

- /
- /api
- /api/device
- /api/health
- /api/info
- /api/logs
- /wifi/off
- /pulse
