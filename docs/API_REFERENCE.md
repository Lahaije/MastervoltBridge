# ESP32 Bridge API Reference

Base URL:

http://<bridge-ethernet-ip>:8080

Example:

http://192.168.1.48:8080

## Endpoint Summary

| Method | Path | Notes |
|---|---|---|
| GET | / | API discovery |
| GET | /api/health | Bridge network/inverter/MQTT status |
| GET | /api/logs | Log buffer (up to 1000 entries) |
| GET | /api/info | Cached inverter `/home` telemetry + cached settings |
| POST | /api/power | Set inverter power limit |
| POST | /api/shadow | Enable/disable inverter shadow function |
| GET | /api/shadow | Read current shadow state from inverter |
| POST | /api/inverter/fetch | Fetch arbitrary inverter endpoint |
| POST | /wifi/off | Single button press if inverter WiFi is connected |
| GET | /pulse | Recovery pulse + forced WiFi reconnect |
| POST | /api/debug | Enable or disable verbose HTTP 200 success logging |
| GET | /api/mqtt | MQTT broker config and HA integration status |
| POST | /api/mqtt | Set MQTT broker IP and/or HA enable flag |

## GET /

Returns discovery JSON containing all current endpoints.

## GET /api/health

Returns bridge state. Fields:

| Field | Type | Description |
|---|---|---|
| `wifi_connected` | bool | Bridge → inverter WiFi link state |
| `wifi_ssid` | string | Configured inverter SSID |
| `wifi_ip` | string | Bridge IP on inverter network |
| `ethernet_ip` | string | Bridge IP on home LAN |
| `inverter_host` | string | Inverter IP target |
| `last_inverter_status` | number | Last HTTP status from inverter (-1 = none yet) |
| `debug_mode` | bool | HTTP 200 success logging on/off |
| `ha_mqtt_enabled` | bool | HA MQTT integration enabled in NVS |
| `mqtt_broker` | string | Stored broker IP (empty when unset) |
| `mqtt_connected` | bool | Currently connected to broker |
| `mqtt_scanning` | bool | `/24` broker discovery scan in progress |

When the inverter is unavailable, this endpoint still works.

## GET /api/logs

Returns diagnostic logs.

Notes:

- Log buffer size is 1000 entries.
- The response is streamed entry-by-entry, so it does not include a
  `Content-Length` header. Body framing uses `Connection: close` — clients
  must read until EOF. Standard HTTP libraries (`requests`, `urllib`,
  `curl`) handle this transparently.

## GET /api/info

Returns cached parsed inverter telemetry from `/home`, plus the bridge's
cached view of write-only inverter settings (`power_limit`, `shadow`). These
settings are not reported by `/home`; the bridge mirrors the last successful
value commanded via `POST /api/power` or `POST /api/shadow`, persists it in
NVS, and exposes it here and via MQTT (cache + dual-topic pattern).

Response fields:

| Field | Type | Description |
|---|---|---|
| `last_update_ms` | number | Bridge uptime ms of last successful poll |
| `operating_status` | string | "1" = normal |
| `error_alarm_code` | string | "0" = no error |
| `operating_mode` | string | "1" = production |
| `inverter_model` | string | e.g. "H500A0103" |
| `inverter_mac_address` | string | Inverter MAC |
| `power` | string | Current output power in watts, e.g. "674.547" |
| `total_yield` | string | Lifetime energy in kWh, e.g. "08566.628" |
| `daily_yield` | string | Daily energy in kWh, e.g. "12.811" |
| `power_limit_known` | bool | `true` once a `POST /api/power` has succeeded since the last NVS erase |
| `power_limit_watts` | number | Cached limit (W). Meaningless when `power_limit_known=false` |
| `shadow_known` | bool | `true` once a `POST /api/shadow` has succeeded since the last NVS erase |
| `shadow` | bool | Cached shadow state. Meaningless when `shadow_known=false` |

Typical failure when inverter is unavailable:

- 502 with error: No inverter telemetry data available yet

## POST /api/power

Body:

{"power":1200}

Validation:

- integer only
- range 0 to `INVERTER_MAX_POWER_WATTS` (1575)

On a successful inverter response (HTTP 200), the bridge caches the value in
NVS and publishes the new state to the MQTT `state/power_limit` topic (when
HA MQTT is enabled).

If inverter WiFi is down, returns 502.

## POST /api/shadow

Enable or disable the inverter's shadow function.

Body:

{"enabled":true}

or

{"enabled":false}

On a successful inverter response, the bridge caches the value in NVS and
publishes to the MQTT `state/shadow` topic (when HA MQTT is enabled).

If inverter WiFi is down, returns 502.

## GET /api/shadow

Returns the current shadow state read live from the inverter.

Response:

{"enabled":true}

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

## GET /api/mqtt

Returns the MQTT broker configuration and HA integration status.

Response fields:

| Field | Type | Description |
|---|---|---|
| `ha_enabled` | bool | HA MQTT integration on/off (persisted in NVS) |
| `broker_ip` | string | Configured broker IP (empty if unset) |
| `connected` | bool | Bridge currently connected to the broker |
| `scanning` | bool | `/24` broker auto-discovery scan in progress |

## POST /api/mqtt

Set the MQTT broker IP and/or the HA enable flag. Both fields are optional;
any field present is applied, any field omitted is left unchanged.

Body (any combination of):

{"ha_enabled":true,"broker_ip":"192.168.1.10"}

- Set `broker_ip` to a non-empty string to use a fixed broker (skips auto-discovery).
- Set `broker_ip` to `""` to clear the stored IP and re-trigger the next /24 scan.
- Set `ha_enabled` to `false` to stop all MQTT activity without losing the broker IP.

Both fields are persisted to NVS in the `mqtt` namespace and survive reboots.

Response: same shape as `GET /api/mqtt`.

## MQTT / Home Assistant Integration

When `ha_enabled=true` and a broker is reachable, the bridge:

1. Publishes a retained **HA MQTT Discovery** payload for each entity under
   `homeassistant/<component>/mastervolt_soladin_1500/<object>/config`.
2. Publishes live telemetry to `mastervolt/mastervolt_soladin_1500/state` on
   every successful inverter poll.
3. Subscribes to command topics (`cmd/power`, `cmd/polling`, `cmd/shadow`)
   for HA → bridge writes.
4. Mirrors the bridge's cached `power_limit` and `shadow` settings to
   `state/power_limit` and `state/shadow` (retained). This dual-topic
   pattern is required because the inverter never reports these values
   itself; the bridge is the source of truth between commands.
5. Uses LWT on `availability` so HA shows the device offline when the
   bridge reboots or loses Ethernet.

Broker auto-discovery: on each Ethernet IP acquisition, if no broker is
configured, the bridge walks the local `/24` one host per `loop()` iteration
probing TCP port 1883. The first responding host is persisted to NVS.

## Known Expected Errors (Night / Inverter Off)

When inverter WiFi is not available:

- GET /api/info -> 502
- POST /api/power -> 502
- GET  /api/shadow -> 502
- POST /api/shadow -> 502
- POST /api/inverter/fetch -> 502

Still expected to respond:

- /
- /api/health
- /api/logs
- /api/mqtt
- /wifi/off
- /pulse
- /api/debug
