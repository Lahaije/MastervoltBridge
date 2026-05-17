# Test README

This guide validates hardware, networking, API behavior, and inverter WiFi wake pulse behavior for the ESP32 bridge.

## Prerequisites

- Firmware uploaded successfully.
- ENC28J60 connected to router.
- ESP32 connected to inverter WiFi (credentials configured in `settings.cpp`).
- Serial Monitor open at 115200 baud.

## A) Basic boot checks

1. Reset ESP32.
2. Confirm serial output shows:
   - Ethernet startup.
   - DHCP-assigned Ethernet IP (or fallback static IP).
   - WiFi connection attempts/status.
   - API server listening on port 8080.

If DHCP fails repeatedly:

- Check Ethernet cable and router port.
- Check ENC28J60 SPI wiring and power stability.

## B) Health check

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

## C) Inverter telemetry check

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

If you receive 502: polling hasn't completed yet (wait 20-30s) or WiFi is not connected.

## D) Power limit check

Send power command via JSON POST body:

```bash
curl -X POST http://192.168.1.48:8080/api/power \
  -H "Content-Type: application/json" \
  -d '{"power":1200}'
```

Expected response:
```json
{
  "requested_power_watts": 1200,
  "inverter_http_status": 200,
  "inverter_response": "OK"
}
```

## E) Generic inverter endpoint fetch

```bash
curl -X POST -H "Content-Type: application/json" \
  -d '{"url":"/home"}' \
  http://192.168.1.48:8080/api/inverter/fetch
```

## F) View logs

```bash
curl -s http://192.168.1.48:8080/api/logs | jq .
```

Displays up to 1000 log entries with millisecond timestamps. Look for:
- `[WIFI-BRIDGE]` — WiFi connection events
- `[INVERTER-MONITOR]` — Polling results
- `[ETH]` — Ethernet initialization
- `[RECOVERY]` — GPIO pulse sequences

## G) Recovery pulse test

Trigger WiFi recovery pulse manually:

```bash
curl http://192.168.1.48:8080/pulse
```

### Test method with LED (quick)

1. Connect LED + resistor from recovery pin (GPIO 36) to GND.
2. Force outage by turning inverter WiFi off, or changing credentials temporarily.
3. Trigger pulse via `/pulse` endpoint.
4. Observe double blink corresponding to pulse sequence (HIGH 150ms, LOW 200ms, HIGH 150ms).

### Test method with oscilloscope (precise)

Measure GPIO 36 waveform after `/pulse` trigger and verify durations are approximately 150ms HIGH, 200ms LOW, 150ms HIGH.

## H) Home Assistant integration sanity check

Use ESP32 Ethernet API as source in HA sensors/automation:

- Read data from `/api/info`.
- Write power limit using `/api/power` with JSON body.

## I) Python test script

Run comprehensive validation:

```bash
python test_bridge.py
```

## Troubleshooting

| Symptom | Check | Fix |
|---------|-------|-----|
| `Connection refused` | Is bridge powered and booted? | Check serial output and `/api/logs` |
| `wifi_connected: false` | WiFi SSID/password correct? | Edit `settings.cpp`, reupload |
| `last_inverter_status: 0` | Bridge WiFi connected but inverter unreachable | Check inverter WiFi network |
| `502 Bad Gateway` on `/api/info` | Polling hasn't completed yet | Wait 20-30s for first poll |
| No Ethernet IP | SPI wiring issue | Check CS, SCK, MISO, MOSI pins |
| Ethernet IP is 0.0.0.0 | ENC28J60 not responding | Check CS and SCK pins |
| Logs show "read Timeout" | HTTP timeout too short | Increase `WIFI_BRIDGE_HTTP_TIMEOUT_MS` in settings |
| Logs show "Connect timeout" | WiFi signal weak | Move ESP32 closer to inverter |
| Upload fails | Board not in download mode | Hold BOOT, press EN, release BOOT |
