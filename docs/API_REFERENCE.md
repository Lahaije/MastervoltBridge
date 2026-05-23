# ESP32 Bridge API Reference

Base URL:

http://<bridge-ethernet-ip>:8080

Example:

http://192.168.1.48:8080

## Endpoint Summary

| Method | Path | Notes |
|---|---|---|
| GET | / | API discovery |
| GET | /api/version | Firmware version currently running on ESP |
| GET | /api/health | Bridge network/inverter status |
| GET | /api/logs | Log buffer (up to 1000 entries) |
| GET | /api/info | Cached inverter /home telemetry |
| POST | /api/polling | Set monitor polling interval in seconds |
| POST | /api/power | Set inverter power |
| POST | /api/inverter/fetch | Fetch inverter endpoint |
| POST | /wifi/off | Single button press if WiFi is connected |
| GET | /pulse | Recovery pulse + connection-time measurement |
| POST | /api/debug | Enable or disable verbose HTTP 200 success logging |

## GET /

Returns discovery JSON containing all current endpoints.

## GET /api/version

Returns the firmware version string currently running on the ESP.

Response example:

{"firmware_version":"fw-20260523-193045-a1b2c3d"}

## GET /api/health

Returns bridge state, including:

- wifi_connected
- wifi_ssid
- wifi_ip
- ethernet_ip
- inverter_host
- last_inverter_status
- inverter_link_state — one of `STARTING`, `ONLINE`, `RETRYING`, `BACKOFF`, `DORMANT` (see "Inverter link state machine" below)
- inverter_failure_streak_ms — milliseconds since the current failure streak began; `0` while ONLINE/STARTING
- inverter_retry_interval_ms — the interval the polling loop will wait between attempts while in the current state
- debug_mode

When inverter is unavailable, this endpoint still works.

### Inverter link state machine

The polling loop tracks how long the bridge has been unable to reach the
inverter and exposes that as a named link state. Transitions are driven by
the failure-streak duration:

| State | Entry condition | Retry interval |
|---|---|---|
| `STARTING` | Boot, no successful poll yet | base `poll_interval_seconds` |
| `ONLINE` | Last poll succeeded | base `poll_interval_seconds` |
| `RETRYING` | Streak > 0, < 5 min | base `poll_interval_seconds` |
| `BACKOFF` | Streak ≥ 5 min, < 20 min | 60 s |
| `DORMANT` | Streak ≥ 20 min | 600 s (10 min) |

Every transition fires a single internal event hook
(`onLinkStateTransition`). Bound actions today:

- `BACKOFF` or `DORMANT` → `ONLINE`: queue a MAX-power reset and attempt
  immediate delivery. The queued command is retried on every successful
  poll until accepted (see `docs/MAX_POWER_BEHAVIOR.md`).
- All other transitions: no bound action; the transition is logged when
  `debug_mode` is enabled.

When `debug_mode` is enabled, transitions are logged as:

```
[INVERTER-MONITOR] Link state: BACKOFF -> ONLINE (streak=1247s, interval=20s)
```

## GET /api/logs

Returns diagnostic logs.

Notes:

- Log buffer size is 1000 entries.
- The response is streamed entry-by-entry, so it does not include a
  `Content-Length` header. Body framing uses `Connection: close` — clients
  must read until EOF. Standard HTTP libraries (`requests`, `urllib`,
  `curl`) handle this transparently.

## GET /api/info

Returns cached parsed inverter telemetry from /home. Always returns `200 OK`.
Before the first successful poll the telemetry string fields are empty;
clients should branch on the `ready` flag.

Response fields:

| Field | Type | Description |
|---|---|---|
| `ready` | bool | `true` once at least one poll has succeeded; while `false` all telemetry strings are empty |
| `last_update_ms` | number | Bridge uptime ms of last successful poll (`0` until ready) |
| `firmware_version` | string | Firmware version currently running on this ESP |
| `poll_interval_seconds` | number | Current normal polling interval in seconds |
| `poll_interval_ms` | number | Current normal polling interval in milliseconds |
| `operating_status` | string | "1" = normal |
| `error_alarm_code` | string | "0" = no error |
| `operating_mode` | string | "1" = production |
| `inverter_model` | string | e.g. "H500A0103" |
| `inverter_mac_address` | string | Inverter MAC |
| `power` | string | Current output power in watts, e.g. "674.547" |
| `total_yield` | string | Lifetime energy in kWh, e.g. "08566.628" |
| `daily_yield` | string | Daily energy in kWh, e.g. "12.811" |
| `power_limit.desired` | number | Latest requested power limit (W) |
| `power_limit.confirmed` | number | Last power limit confirmed by inverter (W) |
| `power_limit.queued` | bool | true when a power command is waiting for delivery |
| `power_limit.reset_timer_minutes` | number | Whole minutes until auto-reset to max, derived from `POWER_LIMIT_RESET_MINUTES` (0 = inactive/elapsed) |

This endpoint no longer returns 502 on startup. A caller polling for
readiness should check `ready == true`.

## POST /api/power

Body:

{"power":1200}

Validation:

- integer only
- range 0 to 1575

Responses:

- 200 when command is delivered immediately
- 202 when command is queued because inverter WiFi is currently unavailable (the bridge keeps retrying for up to 5 minutes)
- 400 on invalid input
- 502 on immediate inverter communication failure when command could not be queued

Successful immediate response example:

{"requested_power_watts":1200,"inverter_http_status":200,"inverter_response":"ok"}

Queued response example:

{"requested_power_watts":1200,"status":"queued","message":"Inverter unreachable; command queued for retry"}

## POST /api/polling

Body:

{"seconds":3}

Validation:

- integer only
- range 1 to 3600

Response:

{"poll_interval_seconds":3,"poll_interval_ms":3000}

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
- Logging asymmetry by HTTP method (see `wifi_bridge.cpp::fetchInverterData`):
  - **POST** request successes (e.g. `/api/power` writes) are **always** logged as
    `[WIFI-BRIDGE] POST /path success (HTTP 200, Nms)` to provide an audit trail
    for every state-changing call. They are not gated by debug mode.
  - **GET** request successes (e.g. `/home` polls) are gated by debug mode and
    logged as `[WIFI-BRIDGE] GET /path success (HTTP 200)` only when debug is on.
- Debug mode additionally gates `[INVERTER-MONITOR] Link state: ...` transition
  logs and other high-volume diagnostic messages.
- When debug mode is **off** (the default after boot), GET successes and link-state
  transitions are silent to keep the log buffer focused on errors, connectivity
  events, and write-path activity.

Body:

{"debug":true}

or

{"debug":false}

Response:

{"debug":true}

Debug mode starts as `true` at boot and is automatically set to `false` at the end of `setup()`. Use this endpoint to re-enable it temporarily during live debugging.

## Known Expected Errors (Night / Inverter Off)

When inverter WiFi is not available:

- GET /api/info -> 200 with `ready=false` if no poll has ever succeeded; otherwise 200 with the last cached telemetry (`ready=true`, but `last_update_ms` will be stale)
- POST /api/power -> 202 (queued)
- POST /api/inverter/fetch -> 502

Still expected to respond:

- /
- /api/version
- /api/health
- /api/logs
- /wifi/off
- /pulse
