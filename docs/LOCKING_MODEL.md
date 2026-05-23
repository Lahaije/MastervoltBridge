# Locking Model

This document explains every lock used by the firmware and the ordering rules that prevent deadlocks and state races.

## Overview

The firmware uses three synchronization domains:

1. WiFi operation serialization in `wifi_bridge.cpp`
2. Cached telemetry protection in `inverter_monitor.cpp`
3. Power state-machine protection in `inverter_monitor.cpp`
4. Polling configuration protection in `inverter_monitor.cpp`

The design intentionally keeps lock hold times short and never holds a lock while waiting on network I/O unless that lock is specifically intended to serialize WiFi operations.

## Hierarchy Enforcement (lock_guard.h)

All four mutexes are acquired through `ScopedLock<LockRank>` from
`firmware/esp32_inverter_bridge/lock_guard.h`. Each rank tags the mutex's
position in the acquisition order:

| Rank value | `LockRank` enum | Mutex |
|---|---|---|
| 0 | `POLLING_CONFIG` | `pollingConfigMutex` |
| 1 | `DATA` | `dataMutex` |
| 2 | `POWER_STATE` | `powerStateMutex` |
| 3 | `WIFI_OPERATION` | `wifiOperationMutex` |

The guard maintains a per-task bitmap of currently-held ranks and, on
every acquisition, verifies that no rank `>= R` is already held by the
calling task. Violations are logged via `appLogger` as
`[LOCK-HIERARCHY] VIOLATION: ...` (visible in `/api/logs`). The guard
still proceeds with the acquisition so the check never introduces new
failure modes, but every violation indicates a bug.

In addition, `fetchInverterData()` calls `assertNoStateLocksHeld(...)`
at entry: any business-state lock held when an HTTP call begins is
reported the same way. This catches both the most common ordering bug
("HTTP under a state lock") and the most dangerous one (held across a
3.5 s timeout).

## Lock Inventory

### 1) `wifiOperationMutex` (global in `wifi_bridge.cpp`, rank `WIFI_OPERATION`)

Purpose:
- Serialize all WiFi operations that must not overlap:
  - inverter HTTP calls (`fetchInverterData`)
  - connection worker pulse/connect sequence
  - `/pulse` forced reconnect
  - `/wifi/off` single-press operation

Behavior:
- `fetchInverterData(..., waitForConnection=true)` blocks until the connection worker has established WiFi, then waits for the operation lock.
- `fetchInverterData(..., waitForConnection=false)` triggers the connection worker in the background and returns immediately if WiFi is down.

Guard helper:
- `ScopedLock<LockRank::WIFI_OPERATION>` (aliased `ScopedWifiOperationLock`)

Timeouts:
- `WIFI_LOCK_TIMEOUT_SHORT_MS` (50 ms): short, non-blocking probes
- `WIFI_LOCK_TIMEOUT_CONNECTED_MS` (4500 ms): queue behind in-flight inverter HTTP (HTTP timeout is 3500 ms)

Key rule:
- Only one WiFi-affecting operation is active at a time.

### 2) `dataMutex` (member of `InverterMonitor`, rank `DATA`)

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

### 3) `powerStateMutex` (member of `InverterMonitor`, rank `POWER_STATE`)

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

To avoid deadlock and hidden contention, follow these rules. All are
enforced at runtime by `ScopedLock<R>` (see *Hierarchy Enforcement* above);
violations are logged as `[LOCK-HIERARCHY] VIOLATION` and visible via
`/api/logs`.

1. Acquire mutexes in ascending `LockRank` order:
   `POLLING_CONFIG` < `DATA` < `POWER_STATE` < `WIFI_OPERATION`.
2. Never hold a business-state lock (`POLLING_CONFIG`, `DATA`, or
   `POWER_STATE`) while calling `fetchInverterData()`.
3. WiFi operations are serialized only by `wifiOperationMutex`;
   business-logic state locks must not be used as transport locks.
4. Keep all lock scopes minimal (field read/write only).

The current code follows these rules by design, and the `ScopedLock`
guard plus the `assertNoStateLocksHeld()` call at the entry of
`fetchInverterData()` make every accidental future violation visible in
the logs.

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

1. If touching WiFi radio or inverter HTTP path, use
   `ScopedLock<LockRank::WIFI_OPERATION>` (alias `ScopedWifiOperationLock`).
2. If touching telemetry cache fields, use
   `ScopedLock<LockRank::DATA>` (alias `ScopedDataLock`).
3. If touching power-limit fields, use
   `ScopedLock<LockRank::POWER_STATE>` (alias `ScopedPowerStateLock`).
4. If touching polling configuration fields, use
   `ScopedLock<LockRank::POLLING_CONFIG>` (alias `ScopedPollingConfigLock`).
5. If you need both transport and power state, update power state first
   (short lock), release, then perform transport call.
6. Never add long `delay()` sections inside lock scopes.
7. Adding a new mutex? Add a new `LockRank` value at the correct position
   in `lock_guard.h` and wrap acquisitions in `ScopedLock<...>`.
