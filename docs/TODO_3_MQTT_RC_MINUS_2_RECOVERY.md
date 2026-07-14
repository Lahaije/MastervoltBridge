# TODO_3: Implement Self-Healing Network Recovery (MQTT rc=-2 Trigger)

## Overview

**Issue:** When Ethernet adapter stalls, there's no way to detect or recover without external monitoring or power cycle.

**Solution:** Track consecutive MQTT rc=-2 failures. On 8 consecutive failures (~160 seconds of stall), automatically restart Ethernet + MQTT. Reset counter on successful connection.

**Why This Works:**
- MQTT rc=-2 indicates TCP connection failed (network layer)
- 8 consecutive failures at ~20s intervals = ~160s stall = definitely not transient
- Simple: counter-based only, no time windows or backoff logic
- Counter resets on any successful MQTT connection
- Fully autonomous, works at any polling interval

For detailed analysis see:
- [INCIDENT_REPORT_20260706.md](./INCIDENT_REPORT_20260706.md#4-mqtt-connection-failures) — MQTT failures overview
- [MQTT_RC_MINUS_2_DIAGNOSTIC.md](./MQTT_RC_MINUS_2_DIAGNOSTIC.md) — Detailed rc=-2 analysis

---

## Jobs to Fix This Issue

### Job 3.1: Add MQTT Failure Counter to mqtt_client.h

**File:** `firmware/esp32_inverter_bridge/mqtt_client.h`

**Location:** Add to private member variables (search for `private:`)

**Current Code:** (around line 70-80, inside `private:` section)
```cpp
private:
  MqttSettings settings_;
  bool initialized_ = false;
  unsigned long lastMqttLoopMs = 0;
```

**Action:** Add these members:
```cpp
private:
  MqttSettings settings_;
  bool initialized_ = false;
  unsigned long lastMqttLoopMs = 0;
  uint32_t consecutiveRcMinus2Failures = 0;   // Counter for rc=-2 failures; triggers recovery at 8
```

### Job 3.2: Define Recovery Thresholds in settings.h

**File:** `firmware/esp32_inverter_bridge/settings.h`

**Location:** Add near end of file, before `#endif`:

**Action:** Add this constant:
```cpp
// ============================================================================
// MQTT Network Recovery (Self-Healing)
// ============================================================================
// Threshold: After this many consecutive rc=-2 failures, assume adapter is stalled
// (at ~20s per attempt, 8 failures ≈ 160 seconds of persistent stall)
extern const uint32_t MQTT_RC_MINUS_2_RECOVERY_THRESHOLD;
```

### Job 3.3: Define Recovery Thresholds in settings.cpp

**File:** `firmware/esp32_inverter_bridge/settings.cpp`

**Location:** Add after other constants (around line 75):

**Action:** Add this definition:
```cpp
// ============================================================================
// MQTT Network Recovery Thresholds
// ============================================================================
const uint32_t MQTT_RC_MINUS_2_RECOVERY_THRESHOLD = 8;  // 8 consecutive rc=-2 failures triggers recovery
```

### Job 3.4: Add Reset Method to MqttClient

**File:** `firmware/esp32_inverter_bridge/mqtt_client.h`

**Location:** Add to public methods (search for `public:`)

**Current Code:** (around line 50-60)
```cpp
  bool isConnected();
  MqttSettings getSettings();
```

**Action:** Add after `getSettings()`:
```cpp
  /**
   * Force disconnect and reset MQTT client state.
   * Called during network recovery to get a clean connection attempt.
   * Will reconnect on next loop().
   */
  void reset();
```

### Job 3.5: Implement Reset Method in mqtt_client.cpp

**File:** `firmware/esp32_inverter_bridge/mqtt_client.cpp`

**Location:** Add new function after `getSettings()` (around line 430):

**Action:** Add this implementation:
```cpp
void MqttClient::reset() {
  appLogger.log("[MQTT] Resetting MQTT client for recovery");
  
  // Disconnect current session if connected
  if (mqttPubSub.connected()) {
    publishAvailability(false);
    mqttPubSub.disconnect();
  }
  
  // Stop socket (clean up resources)
  mqttEthClient.stop();
  
  // Reset connection attempt counter to force immediate retry
  lastConnectAttemptMs_ = 0;
  
  // Reset failure counter (we're starting fresh)
  consecutiveRcMinus2Failures = 0;
  
  appLogger.log("[MQTT] Reset complete; will reconnect on next loop");
}
```

### Job 3.6: Implement Failure Counter Logic in mqtt_client.cpp

**File:** `firmware/esp32_inverter_bridge/mqtt_client.cpp`

**Location:** Find `MqttClient::connect()` function (around line 278-325)

**Current Code (around line 320-323):**
```cpp
  } else {
    appLogger.log("[MQTT] Connection failed, rc=" + String(mqttPubSub.state()));
  }
}
```

**Action:** Replace with:
```cpp
  } else {
    int failureCode = mqttPubSub.state();
    appLogger.log("[MQTT] Connection failed, rc=" + String(failureCode));
    
    // Track rc=-2 failures specifically (network layer failure)
    if (failureCode == -2) {
      consecutiveRcMinus2Failures++;
      appLogger.log("[MQTT] rc=-2 failure count: " + String(consecutiveRcMinus2Failures) + 
                   "/" + String(MQTT_RC_MINUS_2_RECOVERY_THRESHOLD));
      
      // Check if we've exceeded recovery threshold
      if (consecutiveRcMinus2Failures >= MQTT_RC_MINUS_2_RECOVERY_THRESHOLD) {
        
        appLogger.log("[MQTT] ⚠️ RECOVERY TRIGGERED: " + 
                     String(consecutiveRcMinus2Failures) + 
                     " consecutive rc=-2 failures detected");
        appLogger.log("[MQTT] Network adapter appears stalled; initiating recovery sequence");
        
        // Signal that recovery is needed (set flag for ethernet task to handle)
        extern bool needsNetworkRecovery;
        needsNetworkRecovery = true;
        consecutiveRcMinus2Failures = 0;  // Reset counter after triggering recovery
      }
    }
  }
}
```

### Job 3.7: Add Recovery Flag and Handler to ethernet_bridge.cpp

**File:** `firmware/esp32_inverter_bridge/ethernet_bridge.cpp`

**Location:** Add to namespace block (near top, after `prevLinkUp`):

**Action:** Add this flag:
```cpp
namespace {
TaskHandle_t ethernetTaskHandle = nullptr;
bool apiServerStarted = false;
bool prevLinkUp = false;
bool needsNetworkRecovery = false;  // Set by MQTT when recovery needed
```

### Job 3.8: Implement Recovery Handler in ethernet_bridge.cpp

**File:** `firmware/esp32_inverter_bridge/ethernet_bridge.cpp`

**Location:** Add new function before `ethernetBridgeTask()` (around line 45):

**Action:** Add this function:
```cpp
void performNetworkRecovery() {
  appLogger.log("[ETH] ========================================");
  appLogger.log("[ETH] Starting network recovery sequence");
  appLogger.log("[ETH] ========================================");
  
  // Phase 1: Stop and reinitialize Ethernet
  appLogger.log("[ETH] Phase 1: Stopping Ethernet adapter");
  apiServerStarted = false;
  Ethernet.end();
  vTaskDelay(pdMS_TO_TICKS(500));  // Wait for cleanup
  
  appLogger.log("[ETH] Phase 2: Reinitializing Ethernet");
  if (!tryAcquireDhcp()) {
    appLogger.log("[ETH] ❌ DHCP failed during recovery; will retry");
    return;  // Will retry on next loop
  }
  
  apiServer.begin();
  String apiMsg = String("[ETH] Ethernet restarted. New IP=") + Ethernet.localIP().toString();
  appLogger.log(apiMsg);
  apiServerStarted = true;
  
  // Phase 2: Reset MQTT client
  appLogger.log("[ETH] Phase 3: Resetting MQTT client");
  MqttClient::getInstance().reset();
  
  appLogger.log("[ETH] ========================================");
  appLogger.log("[ETH] Recovery sequence complete");
  appLogger.log("[ETH] ========================================");
}
```

### Job 3.9: Integrate Recovery Check into Ethernet Loop

**File:** `firmware/esp32_inverter_bridge/ethernet_bridge.cpp`

**Location:** In `ethernetBridgeTask()`, find the main loop (search for `while (true)`)

**Current Code (around line 105-120):**
```cpp
    // Process all available API clients without delay between them.
    while (true) {
      EthernetClient client = apiServer.available();
      if (!client) break;
      handleApiClient(client);
      client.stop();
    }

    // Maintain MQTT connection and process incoming messages.
    MqttClient::getInstance().loop();

    vTaskDelay(pdMS_TO_TICKS(ETHERNET_SERVICE_INTERVAL_MS));
```

**Action:** Add recovery check before MQTT loop:
```cpp
    // Process all available API clients without delay between them.
    while (true) {
      EthernetClient client = apiServer.available();
      if (!client) break;
      handleApiClient(client);
      client.stop();
    }

    // Check if network recovery is needed (triggered by MQTT rc=-2 threshold)
    if (needsNetworkRecovery) {
      performNetworkRecovery();
      needsNetworkRecovery = false;
    }

    // Maintain MQTT connection and process incoming messages.
    MqttClient::getInstance().loop();

    vTaskDelay(pdMS_TO_TICKS(ETHERNET_SERVICE_INTERVAL_MS));
```

### Job 3.10: Add Reset to Successful Connection in mqtt_client.cpp

**File:** `firmware/esp32_inverter_bridge/mqtt_client.cpp`

**Location:** In `MqttClient::connect()`, find the success case (around line 310)

**Current Code:**
```cpp
  if (connected) {
    logMqttInfo("Connected");
    publishAvailability(true);
```

**Action:** Add counter reset:
```cpp
  if (connected) {
    logMqttInfo("Connected");
    
    // Reset failure counter on successful connection
    if (consecutiveRcMinus2Failures > 0) {
      appLogger.log("[MQTT] Connection successful! Resetting rc=-2 failure counter");
      consecutiveRcMinus2Failures = 0;

    }
    
    publishAvailability(true);
```

### Job 3.11: Add Health Status to API (Optional but Recommended)

**File:** `firmware/esp32_inverter_bridge/api_helper.cpp`

**Location:** After `buildHealthJson()` function (around line 210):

**Action:** Add new function:
```cpp
String buildRecoveryStatusJson() {
  extern uint32_t consecutiveRcMinus2Failures;
  extern const uint32_t MQTT_RC_MINUS_2_RECOVERY_THRESHOLD;
  
  bool recoveryNeeded = (consecutiveRcMinus2Failures >= MQTT_RC_MINUS_2_RECOVERY_THRESHOLD);
  
  return JsonBuilder()
    .addNumber("mqtt_rc_minus_2_count", String(consecutiveRcMinus2Failures))
    .addNumber("recovery_threshold", String(MQTT_RC_MINUS_2_RECOVERY_THRESHOLD))
    .addBool("recovery_needed", recoveryNeeded)
    .build();
}
```

### Job 3.12: Compile and Upload

**Action:**
1. Add header declarations in mqtt_client.h
2. Add recovery threshold constants to settings.h and settings.cpp
3. Implement reset() method in mqtt_client.cpp
4. Add failure counter logic to connect() function
5. Add recovery flag and handler to ethernet_bridge.cpp
6. Integrate recovery check into ethernet loop
7. Compile the firmware (verify no errors)
8. Upload to ESP32

---

## Verification Checklist

- [ ] mqtt_client.h has new counter member
- [ ] settings.h has MQTT_RC_MINUS_2_RECOVERY_THRESHOLD declared
- [ ] settings.cpp has MQTT_RC_MINUS_2_RECOVERY_THRESHOLD = 8
- [ ] mqtt_client.cpp has reset() method implemented
- [ ] mqtt_client.cpp tracks rc=-2 failures and logs count
- [ ] mqtt_client.cpp sets `needsNetworkRecovery` flag when threshold (8) exceeded
- [ ] mqtt_client.cpp resets counter to 0 on successful connection
- [ ] ethernet_bridge.cpp has `needsNetworkRecovery` flag
- [ ] ethernet_bridge.cpp has `performNetworkRecovery()` function
- [ ] ethernet_bridge.cpp checks flag and calls recovery in loop
- [ ] Firmware compiles without errors
- [ ] Upload successful

---

## Expected Log Output

### Normal Operation (No Recovery)
```
[MQTT] Connecting to 192.168.1.100:1883...
[MQTT] Connected
[MQTT] Subscribed to mastervolt/number/power_limit/set
```

### Transient Failures (Counter Increments But Doesn't Trigger)
```
[MQTT] Connection failed, rc=-2
[MQTT] rc=-2 failure count: 1/8
[MQTT] Connection failed, rc=-2
[MQTT] rc=-2 failure count: 2/8
[MQTT] Connecting to 192.168.1.100:1883...
[MQTT] Connected
[MQTT] Connection successful! Resetting rc=-2 failure counter
← Counter reset to 0, no recovery triggered
```

### Stall Detected & Recovered (8 consecutive failures)
```
[MQTT] Connection failed, rc=-2
[MQTT] rc=-2 failure count: 1/8
[MQTT] Connection failed, rc=-2
[MQTT] rc=-2 failure count: 2/8
...
[MQTT] Connection failed, rc=-2
[MQTT] rc=-2 failure count: 8/8
[MQTT] ⚠️ RECOVERY TRIGGERED: 8 consecutive rc=-2 failures detected
[MQTT] Network adapter appears stalled; initiating recovery sequence
[ETH] ========================================
[ETH] Starting network recovery sequence
[ETH] ========================================
[ETH] Phase 1: Stopping Ethernet adapter
[ETH] Phase 2: Reinitializing Ethernet
[ETH] DHCP OK. IP=192.168.1.48
[ETH] Ethernet restarted. New IP=192.168.1.48
[ETH] Phase 3: Resetting MQTT client
[MQTT] Resetting MQTT client for recovery
[MQTT] Reset complete; will reconnect on next loop
[ETH] ========================================
[ETH] Recovery sequence complete
[ETH] ========================================
[MQTT] Connecting to 192.168.1.100:1883...
[MQTT] Connected
[MQTT] Connection successful! Resetting rc=-2 failure counter
← Back to normal operation
```

---

## Timeline

| Task | Effort | Impact |
|------|--------|--------|
| 3.1: Add counter members | 2 min | Infrastructure |
| 3.2: Add settings.h constants | 3 min | Configuration |
| 3.3: Add settings.cpp constants | 2 min | Configuration |
| 3.4: Add reset() declaration | 2 min | API |
| 3.5: Implement reset() | 5 min | Core logic |
| 3.6: Add failure counter logic | 10 min | Detection |
| 3.7: Add recovery flag | 2 min | Signaling |
| 3.8: Implement recovery handler | 8 min | Recovery |
| 3.9: Integrate into loop | 3 min | Integration |
| 3.10: Reset on success | 3 min | Cleanup |
| 3.11: Add health endpoint (optional) | 5 min | Observability |
| 3.12: Compile & upload | 10 min | Deployment |
| **Total** | **~55 min** | **Critical** |

---

## Success Criteria

✅ MQTT rc=-2 detected and counted  
✅ Recovery triggered on 5+ consecutive failures  
✅ Ethernet restarted (new DHCP lease acquired)  
✅ MQTT reconnected (new socket, clean state)  
✅ HTTP API responsive after recovery  
✅ No spurious recovery triggers  
✅ Works independently (no external service)  
✅ Logs clearly show detection and recovery sequence  
✅ Telemetry survives recovery (queue protected)  

---

## Related Documentation

- Design: [SELF_HEALING_RECOVERY_PLAN.md](../docs/SELF_HEALING_RECOVERY_PLAN.md)
- MQTT analysis: [MQTT_RC_MINUS_2_DIAGNOSTIC.md](../docs/MQTT_RC_MINUS_2_DIAGNOSTIC.md)
- Root cause: [INCIDENT_REPORT_20260706.md](../docs/INCIDENT_REPORT_20260706.md)

---

## Status

⏳ Pending implementation  
🎯 High priority (autonomous self-healing)  
🔄 Can be deployed independently after TODO_1 & TODO_2
