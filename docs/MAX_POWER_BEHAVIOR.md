# Max Power Behavior

This document describes the exact implemented behavior for power-limit commands and the automatic reset to max power.

## Definitions

- Max power: `INVERTER_MAX_POWER_WATTS` (currently 1575)
- Desired power: latest requested target (`desiredPowerLimit`)
- Confirmed power: last power value confirmed by inverter (`confirmedPowerLimit`)
- Queued command: pending command waiting for inverter reachability (`powerCommandQueued`)
- Reset timer: absolute deadline for auto-reset (`powerLimitResetAtMs`)
- Reset delay setting: `POWER_LIMIT_RESET_MINUTES` (minutes)

## User Command Flow (`POST /api/power`)

Input validation:
- Must be integer
- Must satisfy `0 <= power <= INVERTER_MAX_POWER_WATTS`

If valid, `setPower()`:
1. Updates desired state.
2. If request is sub-max (`power < max`), arms or extends timer using `POWER_LIMIT_RESET_MINUTES`.
3. If request is max (`power == max`), does not modify timer.
4. Attempts immediate inverter POST.

Outcomes:
- Immediate success -> API returns `200`, confirmed becomes desired, queued=false.
- Immediate failure due WiFi/path issue -> queued=true, API returns `202`.

## Polling Loop Behavior

After each successful `/home` poll, the monitor performs:

1. `checkPowerLimitResetTimer()`
2. `applyPendingPowerCommand()`

This ordering ensures timer-fired max reset can be sent in the same poll iteration.

These two calls are **steady-state** actions: they run on every successful
poll while the link is `ONLINE`. They are distinct from the
**transition-event** action described next.

## Long-disconnect Recovery (link-state transition event)

The polling loop classifies the link into one of `STARTING`, `ONLINE`,
`RETRYING`, `BACKOFF`, `DORMANT` (see `docs/API_REFERENCE.md`). Every
transition fires a single internal hook, `onLinkStateTransition(from, to,
streakMs)`, which is the only place per-transition actions are bound.

Bound action: **`BACKOFF` or `DORMANT` → `ONLINE`** (i.e. recovery after
an outage of ≥ 5 minutes):

1. `queueMaxPowerAfterLongDisconnect(streakMs)` queues a MAX reset, unless
   either:
   - a user-initiated command is already queued (user intent wins), or
   - desired and confirmed are both already MAX with no pending change.
2. The queued command is marked `timerTriggeredReset=true` so it is
   exempt from `POWER_COMMAND_EXPIRY_MS` and keeps retrying.
3. `applyPendingPowerCommand()` attempts immediate delivery in the same
   poll iteration.

Rationale: while the bridge was unreachable, the inverter may have
rebooted or its own power-limit reset timer may have elapsed, and there
is no read-back endpoint for the current limit. Defensively reasserting
MAX guarantees the inverter returns to full production after any
outage long enough to be operationally significant.

No other transition has a bound action today; transitions are only logged
(when `debug_mode` is enabled).

## Reset Timer Semantics

Timer is active when:
- `powerLimitResetAtMs != 0`

Timer fires when:
- `millis() >= powerLimitResetAtMs`

When fired:
1. If both desired and confirmed are already max, clear timer (`powerLimitResetAtMs = 0`).
2. Otherwise queue a reset command:
   - desired=max
   - `timerTriggeredReset=true`
   - `powerCommandQueued=true`
3. Actual POST is sent by `applyPendingPowerCommand()`.

On successful timer-triggered reset delivery:
- confirmed=max
- queued=false
- timer cleared (`powerLimitResetAtMs = 0`)
- `timerTriggeredReset=false`

On failed timer-triggered reset delivery:
- queued remains true
- timer remains active
- retried on future successful polls

## Expiry Rules (`POWER_COMMAND_EXPIRY_MS`)

Expiry applies only to user-initiated queued commands:
- `timerTriggeredReset == false`

If queued command age exceeds `POWER_COMMAND_EXPIRY_MS`:
- queued=false
- desired reverts to confirmed

Expiry does not apply to timer-triggered max resets:
- `timerTriggeredReset == true`
- these keep retrying until success

## API Status Mapping

`POST /api/power`:
- `200`: delivered now
- `202`: queued for retry
- `400`: invalid payload/range
- `502`: command failed and could not be queued (rare path)

`GET /api/info` returns power-limit state:
- `power_limit.desired`
- `power_limit.confirmed`
- `power_limit.queued`
- `power_limit.reset_timer_minutes`

## Important Edge Cases

1. Repeated sub-max requests push the timer forward (latest command wins).
2. Max request does not clear/reset timer by itself.
3. If desired changes while an HTTP command is in flight, commit logic checks current desired before applying completion updates.
4. On startup with no successful poll yet, `/api/info` returns 200 with `ready=false` and empty telemetry strings; the `power_limit` object is always populated.

## Practical Examples

### Example A: Daytime immediate set
- User posts 500
- Inverter reachable -> 200
- desired=500, confirmed=500, queued=false, timer active (2h)

### Example B: Nighttime queued set
- User posts 500
- Inverter unreachable -> 202
- desired=500, confirmed=previous value, queued=true, timer active (2h)
- Next successful poll attempts queued delivery

### Example C: Auto-reset at timer expiry
- Current desired/confirmed=500, timer expires
- Timer queues max reset
- If inverter reachable, max is delivered that poll
- State becomes desired=confirmed=max, queued=false, timer cleared

### Example D: Recovery after a long disconnect
- desired/confirmed=500, link drops, streak grows past 5 min -> state BACKOFF
- Streak passes 20 min -> state DORMANT (retries every 10 min)
- A retry finally succeeds -> transition `DORMANT -> ONLINE` fires
- `onLinkStateTransition` queues MAX (because the inverter may have
  rebooted during the outage) with `timerTriggeredReset=true`
- `applyPendingPowerCommand()` delivers MAX in the same poll iteration
- Final state: desired=confirmed=max, queued=false, timer cleared
- If the user had POSTed a new power value during the outage, that queued
  user command wins instead and is delivered unchanged.
