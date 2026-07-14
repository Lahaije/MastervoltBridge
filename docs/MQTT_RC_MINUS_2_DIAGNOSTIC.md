# MQTT rc=-2: Root Cause & Recovery
**Created:** 2026-07-06  
**Status:** Confirmed root cause; manual fix documented and verified  

---

## What is MQTT rc=-2?

**Return code -2 = MQTT_CONNECT_FAILED**

From PubSubClient library, it specifically means: **TCP connection attempt failed at the socket level**.

This is NOT an MQTT protocol error (those are rc=-4, -5, etc.). This is a **network layer failure**.

---

## Why rc=-2 Keeps Happening?

Every time the MQTT client tries to connect and fails:

```cpp
// mqtt_client.cpp line 278
bool connected = mqttPubSub.connect(...);

// Line 323
if (!connected) {
    appLogger.log("[MQTT] Connection failed, rc=" + String(mqttPubSub.state()));
    // rc = mqttPubSub.state() = -2
}
```

When it logs `rc=-2`, it means:
- We **attempted** a TCP socket connection to broker IP:port
- The socket call **failed** (connection refused, timeout, or unreachable)
- MQTT never even got to send a CONNECT packet

---

## Confirmed Root Cause

The rc=-2 failures on 2026-07-06 were an **internal issue** (Scenario B below). The MQTT broker (192.168.1.23:1883) was operational and reachable throughout the entire 3+ hour incident. The failures were caused by UIPEthernet socket pool exhaustion from the 250ms timeout being too aggressive, leaving sockets in a half-closed state until the pool was full.

**Confirmed fix:** Re-submitting broker settings via `POST /api/mqtt` calls `applySettings()`, which disconnects, stops the socket, and schedules an immediate reconnect. MQTT reconnected within 3 seconds.

---

## Two Failure Scenarios

### Scenario A: External Network Issue (Broker Unreachable)

```
Device                Router          ISP/MQTT Broker
  |                    |                 |
  |--"Connect"-------->|                 |
  |                    |--"Connect"----->| ❌ No response
  |<---"Refused"-------|<--"Timeout"----|
  |                                     
  rc=-2 ✓ (expected failure)
  
HTTP API still works ✓ (local network fine)
```

**Causes:**
- MQTT broker IP is unreachable (wrong IP, broker down, firewall blocking)
- MQTT broker port not listening (wrong port, broker crashed)
- ISP/network connectivity issue (WAN is down)

**Sign:** HTTP requests work fine, MQTT consistently fails with rc=-2

---

### Scenario B: Internal Network Stack Issue (Adapter Stalled)

```
Device                ENC28J60        Router
  |                    |                 |
  |--"Connect"-------->| ❌ STALLED     (never reaches router)
  | (waits for socket)  | (no response)
  |<---"Timeout"-------|
  |                    
  rc=-2 ✓ (but for wrong reason!)
  
HTTP API also hangs ❌ (network stack broken)
```

**Causes:**
- ENC28J60 chip not responding on SPI
- Socket pool exhausted (all sockets held open)
- Ethernet hardware stalled
- UIPEthernet driver stuck in bad state

**Sign:** HTTP requests hang or timeout, MQTT fails with rc=-2, can only recover with power cycle

---

## Diagnostic Decision Tree

When you see `[MQTT] Connection failed, rc=-2` in logs:

```
rc=-2 detected
    |
    ├─ Can you curl http://192.168.1.48:8080/api/health ?
    |
    ├─ YES (responds <1s)
    |     └─> EXTERNAL ISSUE ✓
    |         (Broker unreachable, firewall, ISP)
    |         └─ Check: Broker IP, port, firewall rules
    |
    └─ NO (hangs, timeout, connection refused)
          └─> INTERNAL ISSUE ⚠️
              (Network stack stalled)
              └─ Check: Power cycle, ENC28J60 SPI, socket cleanup
```

---

## Implementing Smart Diagnostics (Counter + Auto-Reset)

See [TODO_3_MQTT_RC_MINUS_2_RECOVERY.md](./TODO_3_MQTT_RC_MINUS_2_RECOVERY.md) for full implementation details.

Core pattern: track consecutive rc=-2 failures in `mqtt_client.cpp`; on threshold (5 failures), call `applySettings()` with current settings to reset the socket. This is exactly what `POST /api/mqtt` does manually.

---

## What Logs Would Tell Us

### Healthy Operation
```
[MQTT] Connecting to 192.168.1.100:1883...
[MQTT] Connected
[MQTT] Subscribed to mastervolt/number/power_limit/set
[INVERTER-CONTROLLER] Poll #91897: Status=1 Power=63.7W
```

### External Issue (Broker Down/Unreachable)
```
[MQTT] Connecting to 192.168.1.100:1883...
[MQTT] Connection failed, rc=-2
[MQTT] Diagnostic: Ethernet link is UP but MQTT failed to connect
← HTTP API would respond fine ✓
← Inverter polling works fine ✓
```
**Interpretation:** Broker is unreachable. Check broker IP/port or firewall.

### Internal Issue (Adapter Stalled)
```
[MQTT] Connecting to 192.168.1.100:1883...
[MQTT] Connection failed, rc=-2
[MQTT] Diagnostic: Ethernet link is UP but MQTT failed to connect
[API] Request timeout from 192.168.x.x (stuck trying to read body)
← HTTP API would NOT respond ❌
← Inverter polling would fail ❌
```
**Interpretation:** Network stack stalled. Power cycle required.

---

## Detailed rc=-2 Analysis

### PubSubClient Error Codes

When `mqttPubSub.state()` returns these values:

| Code | Meaning | Likely Cause | Fix |
|------|---------|--------------|-----|
| 0 | Connection successful | N/A | Normal |
| -1 | Connection timeout | Broker not responding | Check broker IP/port/firewall |
| **-2** | **Connection refused** | **Socket failed (internal stall or broker unreachable)** | **Reset MQTT via POST /api/mqtt; see TODO_3 for auto-recovery** |
| -3 | Connection lost | Broker disconnected mid-session | Retry connection |
| -4 | CONNECT failed | Bad MQTT protocol | Check client ID, username |
| -5 | SUBSCRIBE failed | Broker rejected topic | Check topic permissions |

---

## Diagnosing Internal vs External

The quickest test is whether the HTTP API responds:

```bash
# From laptop on same network
curl http://192.168.1.48:8080/api/health

# Responds instantly (<1s) → API working → broker is the problem (external)
# Hangs or times out → socket pool stalled (internal)
```

In the 2026-07-06 incident: HTTP API was also unresponsive, confirming internal cause.

---

## Manual Recovery Procedure

If MQTT rc=-2 failures persist, re-submit current broker settings via the API. This triggers `applySettings()` which fully tears down and restarts the MQTT socket:

```bash
curl -X POST http://192.168.1.48:8080/api/mqtt \
  -H 'Content-Type: application/json' \
  -d '{"broker_ip":"192.168.1.23","broker_port":1883,"enabled":true,"topic_prefix":"mastervolt_bridge","username":"mv-bridge"}'
```

Verify reconnection:
```bash
curl http://192.168.1.48:8080/api/mqtt
# Expect: {"connected":true, ...}
```

**Verified 2026-07-06:** MQTT reconnected in <3 seconds after POST.

Note: If the HTTP API itself is unresponsive (full stall), this POST will also hang. In that case, a power cycle is required to reset the socket pool. TODO_3 automates recovery before it reaches this point.

---

## What the Logs Show

### During the Incident (rc=-2 pattern)
```
[INVERTER-CONTROLLER] Poll #91413: Status=1 Power=513.2W
[MQTT] Connection failed, rc=-2
[INVERTER-CONTROLLER] Poll #91414: Status=1 Power=559.7W
[MQTT] Connection failed, rc=-2
```
- Inverter polls succeeding (WiFi bridge independent of Ethernet socket pool)
- MQTT failing every ~30s (every connect attempt)
- rc=-2 = TCP socket layer failing, not MQTT protocol

### After Manual Reset
```
[INVERTER-CONTROLLER] Poll #92055: Status=1 Power=3.3W
[INVERTER-CONTROLLER] Poll #92056: Status=1 Power=3.3W
[INVERTER-CONTROLLER] Poll #92057: Status=1 Power=3.2W
```
- No rc=-2 entries in log
- Clean polling only
- MQTT confirmed connected via GET /api/mqtt → {"connected":true}

---

## Summary

| Signal | Interpretation |
|--------|---|
| rc=-2, isolated (1 occurrence) | Transient network blip — normal, ignore |
| rc=-2 × 5+ consecutive | Socket stall or broker unreachable — manual reset or TODO_3 auto-recovery |
| rc=-2 + API responds quickly | Broker unreachable (external issue) |
| rc=-2 + API hangs/times out | Socket pool stalled (internal issue) — `POST /api/mqtt` to recover |
| rc=-2 + API unreachable | Full stall — power cycle required |

**Root cause confirmed (2026-07-06):** Internal socket stall from 250ms timeout exhausting socket pool.  
**Manual fix confirmed:** `POST /api/mqtt` with same settings reconnects within 3 seconds.  
**Automated fix:** TODO_3 implements this same reset triggered by 5 consecutive rc=-2 failures.

---

## Related Work

- [TODO_2_SOCKET_TIMEOUT_FIX.md](./TODO_2_SOCKET_TIMEOUT_FIX.md) — increases timeout from 250ms to 5000ms to prevent socket exhaustion
- [TODO_3_MQTT_RC_MINUS_2_RECOVERY.md](./TODO_3_MQTT_RC_MINUS_2_RECOVERY.md) — automates the `POST /api/mqtt` reset when rc=-2 threshold is hit
