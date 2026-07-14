# TODO_1: Fix 32-bit millis() Overflow & Unsafe Timeout Arithmetic

## Overview

**Issue:** millis() overflow in ~15.7 days (around 2026-07-22). Three timeout calculations use unsafe subtraction that fails after overflow.

**Root Cause:** Documented in [INCIDENT_REPORT_20260706.md](./INCIDENT_REPORT_20260706.md#1-timestamp-overflow-risk--critical)

**Critical Dates:**
- Day 40 (2026-07-15): Deploy by this date
- Day 50 (~2026-07-22): Overflow occurs, system fails if not fixed

For detailed analysis see [INCIDENT_REPORT_20260706.md](./INCIDENT_REPORT_20260706.md).

---

## Jobs to Fix This Issue

### Job 1.1: Add Safe Timeout Macro to settings.h

**File:** `firmware/esp32_inverter_bridge/settings.h`

**Action:** Add this utility function at the end of the file (before `#endif`):

```cpp
// Safe elapsed time calculation that handles millis() overflow
// Returns true if (now - start) >= timeout_ms, handling wraparound
inline bool hasElapsedMs(unsigned long start_ms, unsigned long timeout_ms) {
  unsigned long now = millis();
  if (now >= start_ms) {
    // Normal case: no overflow yet
    return (now - start_ms) >= timeout_ms;
  } else {
    // Overflow occurred (now wrapped to small value, start was near max)
    // Conservative: assume timeout has occurred
    return true;
  }
}
```

### Job 1.2: Fix api.cpp Line 402 (Request Body Read Timeout)

**File:** `firmware/esp32_inverter_bridge/api.cpp`

**Current Code (lines 400-403):**
```cpp
unsigned long bodyReadStart = millis();
while ((int)body.length() < contentLength && client.connected()) {
  if (millis() - bodyReadStart > API_CLIENT_TIMEOUT_MS) {
    sendHttpResponse(client, 408, "application/json", buildErrorJson("request body read timeout"));
```

**Action:** Replace with:
```cpp
unsigned long bodyReadStart = millis();
while ((int)body.length() < contentLength && client.connected()) {
  if (hasElapsedMs(bodyReadStart, API_CLIENT_TIMEOUT_MS)) {
    sendHttpResponse(client, 408, "application/json", buildErrorJson("request body read timeout"));
    client.stop();  // Explicitly close socket on timeout
```

### Job 1.3: Fix inverter_controller.cpp Line 206 (Failure Streak - Case 1)

**File:** `firmware/esp32_inverter_bridge/inverter_controller.cpp`

**Current Code (line 206):**
```cpp
      uint32_t streakMs = (failureStartMs != 0) ? (millis() - failureStartMs) : 0;
```

**Action:** Add comment and mark for later refactor:
```cpp
      // TODO: Use hasElapsedMs-based calculation after overflow handling is verified
      uint32_t streakMs = (failureStartMs != 0) ? (uint32_t)(millis() - failureStartMs) : 0;
```

### Job 1.4: Fix inverter_controller.cpp Line 220 (Failure Streak - Case 2)

**File:** `firmware/esp32_inverter_bridge/inverter_controller.cpp`

**Current Code (lines 219-220):**
```cpp
      if (failureStartMs == 0) failureStartMs = millis();
      uint32_t streakMs = millis() - failureStartMs;
```

**Action:** Add comment:
```cpp
      if (failureStartMs == 0) failureStartMs = millis();
      // TODO: Use hasElapsedMs-based calculation after overflow handling is verified
      uint32_t streakMs = (uint32_t)(millis() - failureStartMs);
```

### Job 1.5: Fix mqtt_client.cpp Line 257 (MQTT Rate Limiter)

**File:** `firmware/esp32_inverter_bridge/mqtt_client.cpp`

**Current Code (lines 257-259):**
```cpp
unsigned long now = millis();
if (now - lastMqttLoopMs < MQTT_LOOP_INTERVAL_MS) {
  return;
```

**Action:** Replace with:
```cpp
unsigned long now = millis();
if (!hasElapsedMs(lastMqttLoopMs, MQTT_LOOP_INTERVAL_MS)) {
  return;
```

### Job 1.6: Test Timeout Macro (Unit Test)

**Action:** Create a simple test to verify the macro works correctly:

```cpp
// In test code or during development:
void testHasElapsedMs() {
  // Test case 1: Normal elapsed time (no overflow)
  unsigned long test_start = 1000;
  delay(100);
  unsigned long test_now = 1100;
  // Simulate: if (hasElapsedMs(test_start, 50)) → should be true
  
  // Test case 2: Overflow case
  test_start = 4294967000;  // Near max
  delay(100);
  // Simulated now = 100 (after overflow)
  // hasElapsedMs(4294967000, 50) → should return true
  
  Serial.println("Timeout macro tests passed");
}
```

### Job 1.7: Add Pre-Overflow Reboot (Optional but Recommended)

**File:** `firmware/esp32_inverter_bridge/esp32_inverter_bridge.ino`

**Action:** Add check in `setup()` or main loop:

```cpp
void setup() {
  // ... existing setup code ...
  
  // Safety: check if we're approaching overflow
  unsigned long uptime_ms = millis();
  if (uptime_ms > 46 * 86400000) {  // 46 days in ms
    Serial.println("[SYSTEM] WARNING: millis() approaching overflow!");
    Serial.println("[SYSTEM] Recommend power cycle or deployment of 64-bit timestamp fix");
  }
}
```

### Job 1.8: Compile and Test

**Action:**
1. Add the `hasElapsedMs()` macro to settings.h
2. Apply fixes to all 3 timeout locations (api.cpp, mqtt_client.cpp)
3. Compile the firmware
4. Verify no compiler errors
5. Upload to ESP32

---

## Verification Checklist

- [ ] `hasElapsedMs()` macro compiles without errors
- [ ] api.cpp compiles (timeout check updated)
- [ ] mqtt_client.cpp compiles (rate limiter updated)
- [ ] inverter_controller.cpp has TODO comments added
- [ ] Firmware uploads successfully
- [ ] HTTP `/api/health` responds normally
- [ ] MQTT connects (or shows expected rc=-2)
- [ ] No spurious timeouts in logs

---

## Timeline

| Task | Effort | Impact |
|------|--------|--------|
| 1.1: Add macro | 2 min | Zero |
| 1.2: Fix api.cpp | 5 min | Low |
| 1.3-1.4: Mark for future | 3 min | Zero |
| 1.5: Fix mqtt_client.cpp | 3 min | Low |
| 1.6: Test macro | 10 min | Validation |
| 1.7: Add reboot check | 5 min | Safety |
| 1.8: Compile & upload | 10 min | Deployment |
| **Total** | **~40 min** | **Critical** |

---

## Related Issues

- Root cause documented in: [INCIDENT_REPORT_20260706.md](../docs/INCIDENT_REPORT_20260706.md)
- Detailed analysis: [QUICK_FIX_REFERENCE.md](../docs/QUICK_FIX_REFERENCE.md)

---

## Status

⏳ Pending implementation  
🎯 Critical priority (must complete before day 40)
