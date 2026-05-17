# ESP32 Bridge API Reference

Complete REST API specification for the ESP32 inverter WiFi-to-Ethernet bridge.

## Base URL

```
http://192.168.1.48:8080
```

(Replace IP with actual Ethernet IP assigned to bridge via DHCP)

## Endpoints

### 1. GET /

**Purpose**: API discovery and endpoint list

**Request**:
```bash
curl http://192.168.1.48:8080/
```

**Response** (200 OK):
```json
{
  "service": "esp32-inverter-bridge",
  "endpoints": [
    {"method": "GET", "path": "/", "description": "API discovery and endpoint overview"},
    {"method": "GET", "path": "/api/health", "description": "Bridge connectivity state"},
    {"method": "GET", "path": "/api/logs", "description": "Up to 1000 cached log entries"},
    {"method": "GET", "path": "/api/info", "description": "Latest cached inverter telemetry"},
    {"method": "POST", "path": "/api/power", "description": "Set inverter power 0-1575W"},
    {"method": "POST", "path": "/api/inverter/fetch", "description": "Fetch any inverter endpoint"},
    {"method": "GET", "path": "/pulse", "description": "Trigger WiFi recovery pulse"}
  ]
}
```

---

### 2. GET /api/health

**Purpose**: Query bridge connectivity and inverter availability

**Request**:
```bash
curl http://192.168.1.48:8080/api/health
```

**Response** (200 OK):
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

**Fields**:
- `wifi_connected` (bool): WiFi currently connected to inverter
- `wifi_ssid` (string): Inverter WiFi network name
- `wifi_ip` (string): IP address assigned by inverter
- `ethernet_ip` (string): IP address assigned to bridge by home router
- `inverter_host` (string): Inverter IP address
- `last_inverter_status` (number): HTTP status code from last inverter request (0=no attempt yet, 200=success, etc.)

**Troubleshooting**:
- `wifi_connected: false` → WiFi not connecting. Check inverter WiFi network is accessible.
- `last_inverter_status: 0` → No polls attempted yet (bridge just booted). Wait 20-30s.
- `last_inverter_status: 502` → WiFi disconnected. Logs show "Connect timeout".

---

### 3. GET /api/logs

**Purpose**: Retrieve diagnostic logs for troubleshooting

**Request**:
```bash
curl http://192.168.1.48:8080/api/logs
```

**Response** (200 OK):
```json
{
  "total_entries": 47,
  "entries": [
    {"timestamp_ms": 415, "message": "[ETH] Starting ENC28J60 DHCP..."},
    {"timestamp_ms": 4959, "message": "[API] Listening on Ethernet port 8080"},
    {"timestamp_ms": 7979, "message": "[WIFI-BRIDGE] Connected. IP=10.0.0.42"},
    {"timestamp_ms": 8238, "message": "[WIFI-BRIDGE] GET /home success (HTTP 200)"},
    {"timestamp_ms": 25664, "message": "[INVERTER-MONITOR] Poll #1: Status=1 Power=674.547W"}
  ]
}
```

**Log Message Patterns**:
- `[ETH]` — Ethernet initialization
- `[API]` — HTTP server startup
- `[WIFI-BRIDGE]` — WiFi connection events
- `[RECOVERY]` — GPIO pulse sequence
- `[INVERTER-MONITOR]` — Polling results

**Common Log Entries**:
- `Connected. IP=10.0.0.42` → WiFi connection successful
- `Connect timeout after 10000ms` → WiFi unreachable (inverter WiFi module issue)
- `GET /home success (HTTP 200)` → Poll succeeded
- `Failed to fetch /home` → HTTP error from inverter
- `Poll #N: Status=1 Power=674.547W` → Telemetry parsed successfully

---

### 4. GET /api/info

**Purpose**: Get latest cached inverter telemetry from the `/home` endpoint

**Request**:
```bash
curl http://192.168.1.48:8080/api/info
```

**Response** (200 OK - data available):
```json
{
  "last_update_ms": 103850,
  "operating_status": "1",
  "error_alarm_code": "0",
  "operating_mode": "1",
  "inverter_model": "H500A0103",
  "inverter_mac_address": "00:06:66:9d:e0:36",
  "power": "674.547",
  "total_yield": "08566.628",
  "daily_yield": "12.811"
}
```

**Response** (502 Bad Gateway - no data yet):
```
WiFi not connected, or polling has not completed yet. Check /api/health.
```

**Fields**:
- `last_update_ms` (ms since boot): When this data was last fetched from inverter
- `operating_status` (string): 1=normal, 2=error, other codes for modes
- `error_alarm_code` (string): 0=no error, other codes indicate specific errors
- `operating_mode` (string): 1=production, 2=standby, etc.
- `inverter_model` (string): Hardware model identifier
- `inverter_mac_address` (string): MAC address of inverter WiFi module
- `power` (string): Current instantaneous power output in watts
- `total_yield` (string): Total cumulative energy produced since commissioning in kWh
- `daily_yield` (string): Energy produced in current day/session in kWh

**Polling Interval**: Updated every 20 seconds

**Troubleshooting**:
- Returns 502 → Either WiFi not connected or first poll hasn't completed. Wait 20-30s after bridge boot.
- Missing fields → Parsing error. Check logs for "Failed to fetch /home".
- Old `last_update_ms` → Polling stalled. Restart bridge or check inverter WiFi.

---

### 5. POST /api/power

**Purpose**: Set inverter power output (0-1575W maximum)

**Request** - JSON body:
```bash
curl -X POST -H "Content-Type: application/json" \
  -d '{"power":500}' \
  http://192.168.1.48:8080/api/power
```

**Response** (200 OK):
```json
{
  "requested_power_watts": 500,
  "inverter_http_status": 200,
  "inverter_response": "OK"
}
```

**Response** (400 Bad Request - invalid power):
```json
{
  "error": "power must satisfy 0 < power < 1575"
}
```

**Response** (502 Bad Gateway - WiFi down):
```json
{
  "error": "WiFi not connected",
  "wifi_status": false
}
```

**Constraints**:
- Minimum: 0W
- Maximum: 1575W (SOLADIN 1500 hardware limit)
- All values are validated before HTTP request

**Troubleshooting**:
- Returns 400 with "Invalid power value" → Power out of 0-1575W range
- Returns 502 → WiFi not connected. Check `/api/health` first.
- Request times out → Inverter slow to respond. Check inverter `/power` endpoint directly.

---

### 6. POST /api/inverter/fetch

**Purpose**: Generic pass-through to any inverter endpoint (for endpoints not explicitly proxied)

**Request** - JSON with URL:
```bash
curl -X POST -H "Content-Type: application/json" \
  -d '{"url": "/home"}' \
  http://192.168.1.48:8080/api/inverter/fetch
```

**Response** (200 OK - raw inverter response):
```
1
0
1
H500A0103
00:06:66:9d:e0:36
674.547
08566.628
12.811
```
```

**Response** (502 Bad Gateway):
```json
{
  "error": "WiFi not connected",
  "url": "/home"
}
```

**Parameters**:
- `url` or plain body: Inverter path (must start with `/`)
- Only supports GET requests

**Common Use Cases**:
- `/home` — Telemetry (same as `/api/info` but raw response)
- `/power` — Power setting information
- `/settings` — Inverter configuration endpoints
- `/cid`, `/invid`, `/usertype`, `/key`, `/apstatus`, `/language` — Static endpoints for web UI

**Troubleshooting**:
- Returns 502 → WiFi not connected. Check `/api/health`.
- Empty `body` → Inverter returned empty response or HTTP error.
- Returns 400 → URL invalid or doesn't start with `/`.

---

### 7. GET /pulse

**Purpose**: Manually trigger WiFi module recovery sequence

**Request**:
```bash
curl http://192.168.1.48:8080/pulse
```

**Response** (200 OK):
```json
{
  "pulse_sent": true,
  "gpio_pin": 36,
  "description": "WiFi recovery pulse triggered: HIGH 150ms, LOW 200ms, HIGH 150ms"
}
```

**GPIO Sequence**:
1. Set GPIO pin 36 HIGH for 150ms
2. Set GPIO pin 36 LOW for 200ms
3. Set GPIO pin 36 HIGH for 150ms

**Purpose**: Wakes inverter WiFi module when it's unresponsive or has soft-crashed

**When to Use**:
- Logs show "Connect timeout after 10000ms" repeatedly
- `/api/health` shows `wifi_connected: false` and inverter WiFi is actually up
- WiFi drops completely and doesn't recover automatically

**Note**: Sequence happens via FreeRTOS task; response returns immediately.

---

## HTTP Status Codes

| Code | Meaning | Typical Cause |
|------|---------|---------------|
| 200 | Success | Request completed successfully |
| 400 | Bad Request | Invalid parameters (e.g., power out of range) |
| 404 | Not Found | Path not recognized |
| 405 | Method Not Allowed | Wrong HTTP method (e.g., GET instead of POST) |
| 502 | Bad Gateway | WiFi not connected or inverter unreachable |
| 500 | Internal Server Error | Bridge firmware error (rare) |

## Common Patterns

### Health Check Loop (for monitoring)

```bash
# Every 30 seconds
while true; do
  curl -s http://192.168.1.48:8080/api/health | jq '.wifi_connected'
  sleep 30
done
```

### Get Power with Fallback

```bash
# Try /api/info first, fall back to /api/inverter/fetch if needed
response=$(curl -s http://192.168.1.48:8080/api/info)
power=$(echo "$response" | jq '.power')  # in watts

if [ -z "$power" ]; then
  response=$(curl -s -X POST -d '/home' http://192.168.1.48:8080/api/inverter/fetch)
  echo "Fetched via generic endpoint"
fi
```

### Monitor Logs in Real Time

```bash
# Fetch logs every 5 seconds and show new entries
prev_count=0
while true; do
  response=$(curl -s http://192.168.1.48:8080/api/logs)
  count=$(echo "$response" | jq '.total_entries')
  
  if [ "$count" -gt "$prev_count" ]; then
    echo "$response" | jq '.entries[-5:]'  # Last 5 entries
    prev_count=$count
  fi
  sleep 5
done
```

## Timeout Behavior

- **API Response Timeout**: 250ms (client timeout per HTTP request)
- **Inverter HTTP Timeout**: 3500ms (for requests to inverter)
- **WiFi Connect Timeout**: 10000ms (10 seconds to establish WiFi)

If requests time out consistently, check:
1. Inverter WiFi network accessibility
2. Inverter HTTP endpoint response time
3. Bridge WiFi signal strength
4. Bridge memory/CPU usage (via logs)

## Rate Limiting

No explicit rate limiting implemented. Recommendations:
- Home Assistant: Poll `/api/health` and `/api/info` no more than once per 10s
- Diagnostics: `/api/logs` can be polled frequently (logs refresh continuously)
- Control: `/api/power` should wait 1-2s between requests

## Authentication

**No authentication required**. API is on home LAN only (not exposed to WAN). If security needed, implement at router level.

## Content Types

All responses are `application/json` with UTF-8 encoding.

Requests can be:
- `application/json` (for JSON payloads)
- `text/plain` (for raw power values or paths)
- `application/x-www-form-urlencoded` (for query parameters)
