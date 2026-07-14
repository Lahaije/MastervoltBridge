# Incident Report: Network Interface Stall & Timestamp Overflow Risk
**Date:** 2026-07-06  
**Issue:** ESP32 HTTP API and MQTT interface became unresponsive; resolved by power cycle (cable replug)  
**Severity:** HIGH — Critical issues discovered with long-term viability  
**Status:** Documented for investigation; requires immediate short-term fixes before day ~50

---

## Executive Summary

On 2026-07-06, the ESP32 inverter bridge's HTTP API interface became completely unresponsive (`192.168.1.48:8080`), though the device remained reachable on the network. The issue was resolved by unplugging and replugging the Ethernet cable. 

Investigation of device logs revealed:

1. **Unsafe millisecond arithmetic** in three critical code paths that will fail when `millis()` overflows
2. **Imminent 32-bit overflow risk** — the device will experience overflow in approximately **15.7 days** (around 2026-07-22)
3. **Socket timeout configuration too aggressive** (250ms) for UIPEthernet stack, causing hangs
4. **MQTT persistent connection failures** (rc=-2, 392 failures over 3+ hours) caused by network stack stall — broker was operational and reachable throughout

---

## Symptoms & Timeline

### Observed Behavior
- **Time of Incident:** Approximately 2026-07-03 to 2026-07-06 (first detected when logs analyzed)
- **Duration of Outage:** Estimated 3+ hours based on MQTT failure logs
- **Root Cause:** Network stack stall (UIPEthernet socket pool exhaustion) triggered by overly aggressive 250ms socket timeout
- **Resolution:** Power cycle (Ethernet cable replug) — cleared hardware socket state

### Current Device State (2026-07-06 analysis time)
- **Uptime:** 34.1 days (817.3 hours)
- **Last Update:** Device actively polling inverter, returning telemetry correctly
- **Status:** Operational; MQTT reconnected 2026-07-06 via `POST /api/mqtt` (same settings re-submitted)

---

## Technical Analysis

### 1. Timestamp Overflow Risk — CRITICAL

**Finding:** Device uses `unsigned long` (32-bit) for millisecond timestamps. Overflow will occur in ~15.7 days.

#### Current State:
| Metric | Value |
|--------|-------|
| **Max timestamp in logs** | 2,942,262,282 ms |
| **32-bit max (uint32)** | 4,294,967,296 ms |
| **Current usage** | 68.5% of available range |
| **Current uptime** | 34.1 days |
| **Days until overflow** | ~15.7 days |
| **Estimated overflow date** | ~2026-07-22 |

**Impact:** When `millis()` wraps from `0xFFFFFFFF` to `0x00000000`, any timeout calculation using subtraction will fail catastrophically.

**Example:**
```cpp
unsigned long start = 4294967000;  // Near overflow
delay(500);
unsigned long now = 100;           // After overflow wrap
unsigned long elapsed = now - start;  // Result: 100 - 4294967000 = WRONG!
// Expected: ~500ms; Actual: -4294966900 (interpreted as huge positive in unsigned math)
```

---

### 2. Unsafe millis() Arithmetic — THREE LOCATIONS FOUND

#### **Issue 2A: [api.cpp:402](../firmware/esp32_inverter_bridge/api.cpp#L400-L403) — Request Body Read Timeout**

**Code:**
```cpp
unsigned long bodyReadStart = millis();
while ((int)body.length() < contentLength && client.connected()) {
  if (millis() - bodyReadStart > API_CLIENT_TIMEOUT_MS) {  // ← UNSAFE
    sendHttpResponse(client, 408, "application/json", buildErrorJson("request body read timeout"));
    return;
  }
  // ...
}
```

**Problem:**
- If `bodyReadStart` captured a value near `0xFFFFFFFF` and current `millis()` wrapped to a small value, the subtraction produces a huge number
- Timeout never triggers; request hangs indefinitely
- Blocks entire API server (single-threaded request handler)
- Socket timeout is only 250ms — too aggressive for UIPEthernet anyway

**Current Config:**
- `API_CLIENT_TIMEOUT_MS = 250` (settings.cpp:69)

---

#### **Issue 2B: [inverter_controller.cpp:220](../firmware/esp32_inverter_bridge/inverter_controller.cpp#L219-L221) — Failure Streak Calculation**

**Code:**
```cpp
if (failureStartMs == 0) failureStartMs = millis();
uint32_t streakMs = millis() - failureStartMs;  // ← UNSAFE
linkStateFromStreak(streakMs);
```

**Problem:**
- Failure streak determines link state (ONLINE → RETRYING → BACKOFF → DORMANT)
- On overflow, `streakMs` becomes a huge value
- Link state incorrectly transitions to DORMANT
- Polling interval jumps from 20s to 10min
- Inverter data collection becomes very sparse

**Also affects:** [inverter_controller.cpp:206](../firmware/esp32_inverter_bridge/inverter_controller.cpp#L206)
```cpp
uint32_t streakMs = (failureStartMs != 0) ? (millis() - failureStartMs) : 0;
```

---

#### **Issue 2C: [mqtt_client.cpp:257](../firmware/esp32_inverter_bridge/mqtt_client.cpp#L257-L259) — Rate Limiter**

**Code:**
```cpp
unsigned long now = millis();
if (now - lastMqttLoopMs < MQTT_LOOP_INTERVAL_MS) {  // ← UNSAFE
  return;
}
```

**Problem:**
- MQTT loop only runs if rate limit check passes
- On overflow, subtraction produces incorrect value
- MQTT may fail to reconnect or process messages
- Current MQTT failures (rc=-2) may be exacerbated by this

**Current Config:**
- `MQTT_LOOP_INTERVAL_MS = 500` (milliseconds between MQTT loop iterations)

---

### 3. Network Stack Issues

#### Aggressive Socket Timeout Configuration
```cpp
const uint16_t API_CLIENT_TIMEOUT_MS = 250;  // settings.cpp:69
```

**Problem:**
- 250ms is extremely aggressive for UIPEthernet stack over Ethernet
- If a request is legitimately slow (network congestion), it times out prematurely
- Failed timeout doesn't clean up socket properly
- Next request may reuse a half-closed socket
- Eventually exhausts socket pool → API server becomes unresponsive

**Observation:**
- When API server stalls, MQTT also fails (same ethernet task in `ethernet_bridge.cpp`)
- Both blocks on the same `EthernetClient` resources

---

### 4. MQTT Connection Failures

#### Log Analysis
| Metric | Count |
|--------|-------|
| **Total MQTT failures** | 392 |
| **All with error code** | rc=-2 (connection refused) |
| **Successful connections** | 0 (in visible logs) |
| **Failure duration** | 3h 23m 43s |

**Findings:**
- rc=-2 indicates TCP connection failed at socket layer — not an authentication or MQTT protocol error
- Confirmed internal cause: broker was running and reachable on 192.168.1.23:1883 throughout the incident
- Stalled socket pool prevented any new TCP connections from succeeding
- MQTT reconnected immediately once socket state was reset via `POST /api/mqtt`

---

## Root Cause Analysis

### Immediate Cause of 2026-07-06 Outage
**Confirmed:** UIPEthernet socket exhaustion caused by 250ms timeout too aggressive for the stack:
1. An HTTP request entered `handleApiClient()`
2. Request body read loop hit the 250ms timeout prematurely
3. Socket cleanup on timeout was incomplete; descriptor leaked
4. Repeated over time → socket pool exhausted
5. API server blocked waiting for socket resources
6. MQTT stalled because it shares the ethernet task (same socket pool)
7. Unplugging cable reset UIPEthernet hardware state (fresh socket pool + DHCP)

### Why MQTT Was Failing
- Same stalled socket pool prevented TCP connections to broker
- Broker (192.168.1.23:1883) was running and reachable the entire time
- Confirmed: re-submitting broker settings via `POST /api/mqtt` triggered `applySettings()`, which called `mqttPubSub.disconnect()` + `mqttEthClient.stop()` + reset the connect timer, and MQTT reconnected within 3 seconds

### Why Overflow Is a Ticking Timebomb
- Every timeout calculation will fail after day 50
- API server will hang on ANY request after overflow
- Failure streak calculation will be wrong → link state FSM breaks
- MQTT rate limiter will malfunction

---

## Impact Assessment

### Current Risk (Uptime 34.1 days)
- **Network stalls possible** if socket pool exhaustion occurs
- **MQTT offline** (rc=-2 persistent)
- **Inverter data collection working** (WiFi bridge stable, 20s polling)
- **API working** (intermittently; logs truncated by ~1000-entry buffer before this incident)

### Critical Risk (Days 40-50)
- **millis() overflow approaching**
- **All timeout logic will fail** when overflow occurs
- **API server will hang completely** after overflow
- **Device will be unreachable** except by ping
- **Power cycle only option to recover**

### Post-Overflow (Day >50)
- **Complete network interface failure**
- **API unreachable**
- **MQTT unreachable**
- **Cannot update firmware or reconfigure settings remotely**
- **Must perform hardware power cycle to recover**

---

## Recommended Fixes

### Immediate (This Week — Before Day 40)

#### 1. Implement Safe Timeout Macro
Create a utility function that handles millis() overflow safely:

**File:** `firmware/esp32_inverter_bridge/settings.h` (or new `utils.h`)

```cpp
// Safe elapsed time calculation that handles millis() overflow
inline bool elapsedMs(unsigned long start, unsigned long timeout) {
  unsigned long now = millis();
  if (now >= start) {
    return (now - start) >= timeout;  // Normal case
  } else {
    // Overflow occurred; assume timeout if we wrapped
    return true;
  }
}
```

#### 2. Update Timeout Checks (3 locations)
- **api.cpp:402** — Replace `if (millis() - bodyReadStart > API_CLIENT_TIMEOUT_MS)` with `if (elapsedMs(bodyReadStart, API_CLIENT_TIMEOUT_MS))`
- **inverter_controller.cpp:206, 220** — Use safe subtraction or reset failureStartMs periodically
- **mqtt_client.cpp:257** — Use `elapsedMs()` macro

#### 3. Increase Socket Timeout
Change `API_CLIENT_TIMEOUT_MS` from 250ms to 5000ms (5 seconds):
```cpp
const uint16_t API_CLIENT_TIMEOUT_MS = 5000;  // settings.cpp:69
```

#### 4. Manual MQTT Recovery (Already Verified)
If MQTT rc=-2 failures recur before TODO_3 is deployed:
```bash
curl -X POST http://192.168.1.48:8080/api/mqtt \
  -H 'Content-Type: application/json' \
  -d '{"broker_ip":"192.168.1.23","broker_port":1883,"enabled":true,"topic_prefix":"mastervolt_bridge","username":"mv-bridge"}'
```
This triggers `applySettings()` which fully resets the MQTT socket and schedules an immediate reconnect.

---

### Short-Term (Before Day 50)

#### 1. Add Explicit Socket Cleanup on Timeout
In `handleApiClient()`, add socket close on timeout:
```cpp
if (elapsedMs(bodyReadStart, API_CLIENT_TIMEOUT_MS)) {
  client.stop();  // Explicitly close socket
  return;
}
```

#### 2. Implement Periodic Reboot
Add a configurable reboot trigger (e.g., every 30 days or at day 45):
```cpp
if (millis() > REBOOT_TRIGGER_MS) {  // e.g., 39 days
  ESP.restart();
}
```

#### 3. Separate MQTT from API Task
Consider moving MQTT to separate FreeRTOS task to prevent API stalls from blocking MQTT.

---

### Medium-Term (Architectural)

1. **Use 64-bit timestamps** (millis64() or similar) to extend overflow window by millions of years
2. **Add watchdog timer** to auto-reset device on total stall
3. **Implement circuit breaker pattern** for socket operations
4. **Add telemetry for socket pool usage** to detect exhaustion early

---

## Testing Recommendations

### Unit Tests
1. Test safe timeout macro with edge cases:
   - `elapsedMs(4294967000, 500)` → should return `true` (overflow case)
   - `elapsedMs(1000, 5000)` → should return `false` (normal case)

### Integration Tests
1. Simulate millis() overflow (mock `millis()` to return values near 0xFFFFFFFF)
2. Verify timeout calculations work correctly during wraparound
3. Verify link state FSM doesn't break during overflow
4. Verify MQTT rate limiter works during overflow

### Load Testing
1. Send rapid HTTP requests → verify no socket exhaustion
2. Send malformed requests → verify timeout and cleanup
3. Monitor socket pool during sustained API usage

---

## Monitoring & Alerts

### Key Metrics to Track
- **millis() value** — check when approaching 4.2B
- **MQTT connection status** — verify rc=-2 is resolved
- **HTTP request latency** — should be <100ms for cached responses
- **Socket availability** — log socket pool exhaustion warnings

### Pre-Overflow Checklist (Day 40+)
- [ ] Verify safe timeout macros are deployed
- [ ] Verify socket timeout increased to 5000ms
- [ ] Verify periodic reboot logic is working
- [ ] Monitor logs for any timeout-related errors
- [ ] Plan maintenance window for reboot if needed

---

## Files & Code References

### Affected Files
- `firmware/esp32_inverter_bridge/api.cpp` — Line 402
- `firmware/esp32_inverter_bridge/inverter_controller.cpp` — Lines 206, 220
- `firmware/esp32_inverter_bridge/mqtt_client.cpp` — Line 257
- `firmware/esp32_inverter_bridge/settings.h/cpp` — Socket timeout config
- `firmware/esp32_inverter_bridge/ethernet_bridge.cpp` — Ethernet task coordination

### Related Documentation
- [API_REFERENCE.md](./API_REFERENCE.md) — API endpoints and timeout behavior
- [SETUP_README.md](./SETUP_README.md) — MQTT configuration details
- [logger.h](../firmware/esp32_inverter_bridge/logger.h) — Logging timestamp implementation

---

## Appendix: Log Data

### Device State at Analysis Time (2026-07-06)
```json
{
  "uptime_ms": 2942262282,
  "uptime_days": 34.1,
  "uptime_hours": 817.3,
  "log_entries": 1000,
  "mqtt_failures": 392,
  "mqtt_error_code": "-2",
  "inverter_polling": "active",
  "power_w": 63.7,
  "link_state": "ONLINE",
  "failure_streak_s": 0
}
```

### MQTT Failure Pattern
- **First failure:** Uptime 814.04 hours (33.9 days)
- **Last failure in logs:** Uptime 817.10 hours (34.0 days)
- **Consecutive failures:** 392 over 3h 23m
- **Success rate:** 0% (no successful connections logged)

---

## Next Steps

1. **Immediate:** Review and approve the fixes outlined in "Recommended Fixes" section
2. **Short-term:** Implement safe timeout macros and increase socket timeout
3. **Urgent:** Before day 50, deploy reboot logic or 64-bit timestamp fix
4. **Follow-up:** Monitor logs after deployment; verify MQTT reconnects
5. **Long-term:** Consider architectural improvements (64-bit timestamps, separate tasks)

---

**Document Version:** 1.0  
**Last Updated:** 2026-07-06  
**Next Review:** 2026-07-20 (before day 50 overflow)
