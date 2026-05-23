# Locking Model

This document explains every lock used by the firmware and the ordering rules that prevent deadlocks and state races.

## Overview

The firmware uses three synchronization domains:

1. WiFi operation serialization in `wifi_bridge.cpp`
2. Cached telemetry protection in `inverter_monitor.cpp`
3. Power state-machine protection in `inverter_monitor.cpp`

The design intentionally keeps lock hold times short and never holds a lock while waiting on network I/O unless that lock is specifically intended to serialize WiFi operations.

## Lock Inventory

### 1) `wifiOperationMutex` (global in `wifi_bridge.cpp`)

Purpose:
- Serialize all WiFi operations that must not overlap:
  - inverter HTTP calls (`fetchInverterData`)
  - connection worker pulse/connect sequence
  - `/pulse` forced reconnect
  - `/wifi/off` single-press operation

Guard helper:
- `ScopedWifiOperationLock`

Timeouts:
- `WIFI_LOCK_TIMEOUT_SHORT_MS` (50 ms): short, non-blocking probes
- `WIFI_LOCK_TIMEOUT_CONNECTED_MS` (4500 ms): queue behind in-flight inverter HTTP (HTTP timeout is 3500 ms)

Key rule:
- Only one WiFi-affecting operation is active at a time.

### 2) `dataMutex` (member of `InverterMonitor`)

Purpose:
- Protect cached telemetry and counters:
  - `cachedData`
  - `lastUpdateMs`
  - `successfulPolls`
  - `failedPolls`

Where used:
- Polling task writes cache after parsing `/home`
- `getLatestHomeData()` and `getLastUpdateMs()` read snapshots

Timeout:
- 5000 ms

Key rule:
- Treat telemetry cache as a separate domain from power control state.

### 3) `powerStateMutex` (member of `InverterMonitor`)

Purpose:
- Protect power-limit state machine fields:
  - `desiredPowerLimit`
  - `confirmedPowerLimit`
  - `desiredPowerSetAtMs`
  - `powerLimitResetAtMs`
  - `powerCommandQueued`
  - `timerTriggeredReset`

Where used:
- `setPower()`
- `applyPendingPowerCommand()`
- `checkPowerLimitResetTimer()`
- getters (`isPowerCommandQueued`, `getDesiredPowerLimit`, `getConfirmedPowerLimit`, `getPowerLimitResetAtMs`)

Timeout:
- `POWER_STATE_LOCK_TIMEOUT_MS` (1000 ms)

Key rule:
- Never hold `powerStateMutex` across HTTP calls. Snapshot state, release lock, do HTTP, re-lock and commit if state still matches.

## Lock Ordering Rules

To avoid deadlock and hidden contention, follow these rules:

1. Do not hold `powerStateMutex` while calling `fetchInverterData()`.
2. Do not nest `powerStateMutex` and `dataMutex`.
3. WiFi operations are serialized only by `wifiOperationMutex`; business-logic state locks must not be used as transport locks.
4. Keep all lock scopes minimal (field read/write only).

Current code follows these rules by design.

## Why This Split Exists

- `wifiOperationMutex` is transport-level protection.
- `dataMutex` is telemetry-cache protection.
- `powerStateMutex` is command-state protection.

Splitting these concerns avoids one coarse lock that would increase latency and increase deadlock risk.

## Common Race That Was Fixed

Before `powerStateMutex`, concurrent updates from `setPower()` and timer reset logic could interleave and clear/overwrite `powerLimitResetAtMs` incorrectly.

Current approach:
- All power-state fields are lock-protected.
- Timer and queued command logic use snapshot/commit with revalidation.
- Commit steps check whether `desiredPowerLimit` changed while HTTP was in flight.

## Safe Extension Checklist

When adding new code:

1. If touching WiFi radio or inverter HTTP path, use `ScopedWifiOperationLock`.
2. If touching telemetry cache fields, use `dataMutex`.
3. If touching power-limit fields, use `powerStateMutex`.
4. If you need both transport and power state, update power state first (short lock), release, then perform transport call.
5. Never add long `delay()` sections inside lock scopes.
