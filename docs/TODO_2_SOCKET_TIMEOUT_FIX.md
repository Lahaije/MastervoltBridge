# TODO_2: Increase Socket Timeout (250ms → 5000ms)

## Overview

**Issue:** API socket timeout is 250ms (10-20x too aggressive for UIPEthernet). This timeout caused a **cascade of socket pool exhaustion** over 34 days.

**Why It Failed Now (Not Day 1):**
The 250ms timeout didn't "suddenly" break — it accumulated damage over time:
1. Legitimate network delays (inverter slowness, LAN congestion) occasionally exceed 250ms
2. When timeout fires, socket cleanup may leave socket in half-closed state
3. Half-closed sockets aren't reused; pool has only ~5-10 total sockets
4. After hundreds of false timeouts over 3+ days, pool fills completely
5. Result: Any new request can't acquire a socket → entire API hangs

**Why 5000ms Fixes This:**
- Industry standard for Ethernet over LAN is 1-5 seconds (UIPEthernet is slower than native hardware)
- 5000ms allows legitimate network delays without false timeouts
- Fewer timeout events = fewer broken sockets = pool doesn't exhaust
- Fast requests (<100ms) complete normally; no practical impact

**Impact Analysis:**
| Scenario | Effect |
|---|---|
| Normal fast request (<100ms) | ✅ No change — completes normally |
| Slow but valid request (1-4s) | ✅ **FIXED** — now succeeds instead of timing out |
| Truly hung request | ⚠️ Takes 5s to detect (was 250ms) but prevents cascade |
| Socket pool health | ✅ **Massive improvement** — fewer false timeouts = fewer exhaustion events |

**Root Cause of 2026-07-06 Incident:** Cascade of legitimate slow requests hitting the aggressive 250ms timeout, causing repeated socket cleanup failures and gradual pool exhaustion until complete stall.

**Solution:** Increase timeout from 250ms to 5000ms (industry standard for Ethernet over LAN)

For detailed analysis see [INCIDENT_REPORT_20260706.md](./INCIDENT_REPORT_20260706.md#3-network-stack-issues).

---

## Jobs to Fix This Issue

### Job 2.1: Update Socket Timeout in settings.cpp

**File:** `firmware/esp32_inverter_bridge/settings.cpp`

**Current Code (line 69):**
```cpp
const uint16_t API_CLIENT_TIMEOUT_MS = 250;
```

**Action:** Replace with:
```cpp
const uint16_t API_CLIENT_TIMEOUT_MS = 5000;  // 5 seconds (was 250ms, too aggressive)
```

### Job 2.2: Update settings.h Comment for Context

**File:** `firmware/esp32_inverter_bridge/settings.h`

**Current Code (around line 76):**
```cpp
extern const uint16_t API_CLIENT_TIMEOUT_MS;
```

**Action:** Add comment above it:
```cpp
// Socket timeout for API requests. Must be long enough for:
// - TCP handshake (100-200ms)
// - Full request/response cycle (variable, can be slow)
// - Network congestion (temporary queuing)
// Changed from 250ms (too aggressive) to 5000ms (safe baseline)
extern const uint16_t API_CLIENT_TIMEOUT_MS;
```

### Job 2.3: Update mqtt_client.cpp Socket Timeout

**File:** `firmware/esp32_inverter_bridge/mqtt_client.cpp`

**Current Code (line 245):**
```cpp
  mqttEthClient.setTimeout(250);
```

**Action:** Replace with:
```cpp
  mqttEthClient.setTimeout(5000);  // 5 seconds (increased from 250ms)
```

### Job 2.4: Add Diagnostic Logging for Timeout Events

**File:** `firmware/esp32_inverter_bridge/api.cpp`

**Location:** In `handleApiClient()`, when timeout is triggered (line 403):

**Current Code:**
```cpp
      if (hasElapsedMs(bodyReadStart, API_CLIENT_TIMEOUT_MS)) {
        sendHttpResponse(client, 408, "application/json", buildErrorJson("request body read timeout"));
        client.stop();
```

**Action:** Add logging:
```cpp
      if (hasElapsedMs(bodyReadStart, API_CLIENT_TIMEOUT_MS)) {
        appLogger.log("[API] Request body read timeout after " + String(API_CLIENT_TIMEOUT_MS) + "ms");
        sendHttpResponse(client, 408, "application/json", buildErrorJson("request body read timeout"));
        client.stop();
```

### Job 2.5: Document the Change in Code

**File:** `firmware/esp32_inverter_bridge/settings.cpp`

**Location:** Near line 69, add comment block above the constant:

**Action:** Add this comment:
```cpp
// ============================================================================
// API Socket Timeout (IMPORTANT)
// ============================================================================
// Increased from 250ms to 5000ms on 2026-07-06 after incident analysis.
// 
// Root cause of 2026-07-06 stall:
// - 250ms timeout was premature for UIPEthernet stack
// - Legitimate requests took >250ms (TCP handshake + processing)
// - Timeout triggered → socket didn't close properly
// - Socket pool depleted over 3+ hours → full adapter stall
// - Recovery required power cycle
//
// 5000ms is safer because:
// - Allows full TCP handshake (100-200ms)
// - Tolerates network congestion
// - Still responsive to real failures
// - Prevents socket pool exhaustion
// ============================================================================
const uint16_t API_CLIENT_TIMEOUT_MS = 5000;
```

### Job 2.6: Verify All Timeout Values Are Consistent

**File:** Review and document all timeout values in the codebase

**Action:** Search for all timeout constants and document them:

```cpp
// After changes, these should be consistent:
API_CLIENT_TIMEOUT_MS         = 5000    // API socket
WIFI_BRIDGE_HTTP_TIMEOUT_MS   = 3500    // WiFi HTTP calls to inverter (existing)
MQTT socket timeout           = 5000    // MQTT operations (changed in Job 2.3)
```

### Job 2.7: Compile and Test

**Action:**
1. Update all three locations (settings.cpp, settings.h comment, mqtt_client.cpp)
2. Add logging in api.cpp
3. Add documentation comment
4. Compile the firmware
5. Verify no errors
6. Upload to ESP32

---

## Verification Checklist

- [ ] settings.cpp has API_CLIENT_TIMEOUT_MS = 5000
- [ ] settings.h has explanatory comment
- [ ] mqtt_client.cpp socket timeout = 5000
- [ ] api.cpp has diagnostic logging for timeout events
- [ ] Code has documentation comment explaining why 5000ms
- [ ] Firmware compiles without errors
- [ ] HTTP `/api/health` responds within 5 seconds
- [ ] No premature timeout messages in logs
- [ ] No "Connection refused" errors from normal clients

---

## Expected Behavior After Fix

### Before (250ms timeout)
```
[API] Request body read timeout after 250ms  ← Premature!
[API] Request body read timeout after 250ms
[API] Request body read timeout after 250ms
[API] ❌ API Server Stalled (sockets exhausted)
```

### After (5000ms timeout)
```
[API] Request received and processed within timeout
[API] Response sent successfully
[API] Request received and processed within timeout
[API] ✅ API Server Responsive
```

---

## Timeline

| Task | Effort | Impact |
|------|--------|--------|
| 2.1: Update settings.cpp | 1 min | Critical |
| 2.2: Add settings.h comment | 2 min | Documentation |
| 2.3: Update mqtt_client.cpp | 1 min | Important |
| 2.4: Add diagnostic logging | 3 min | Observability |
| 2.5: Add code documentation | 3 min | Maintainability |
| 2.6: Verify consistency | 5 min | Validation |
| 2.7: Compile & test | 10 min | Deployment |
| **Total** | **~25 min** | **High** |

---

## Coordination with TODO_1

- **TODO_1** fixes the math (safe elapsed time calculation)
- **TODO_2** fixes the value (250ms → 5000ms)
- Both must be deployed together for full effect

---

## Related Documentation

- Root cause: [INCIDENT_REPORT_20260706.md](../docs/INCIDENT_REPORT_20260706.md) (Socket Timeout Configuration section)
- Details: [QUICK_FIX_REFERENCE.md](../docs/QUICK_FIX_REFERENCE.md) (Fix 6)

---

## Status

⏳ Pending implementation  
🎯 High priority (should deploy with TODO_1)
