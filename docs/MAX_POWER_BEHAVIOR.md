# Power Limit Behavior

This document describes the implemented behavior for power-limit commands.

## Definitions

- Max power: `INVERTER_MAX_POWER_WATTS` (currently 1575)
- Cached power limit: last value read from the inverter (`cachedPowerLimit`, -1 until first read)
- Queued command: pending power command waiting for WiFi availability (`queuedPowerWatts`)
- Command expiry: `POWER_COMMAND_EXPIRY_MS` (5 minutes)

## Design Principles

1. The ESP is the **sole writer** of the inverter's power limit setting.
2. The cached value is always **read back from the inverter** — never assumed from what was written.
3. A queued command bridges the gap between user intent and actual delivery when WiFi is unavailable.

## User Command Flow (`POST /api/power`)

Input validation:
- Must be integer
- Must satisfy `0 <= power <= INVERTER_MAX_POWER_WATTS`

If valid, `setPower()` attempts an immediate POST to the inverter:

**WiFi available (immediate delivery):**
1. POST sent to inverter `/power` endpoint.
2. On success: `refreshPowerLimit()` reads back the actual value from `/power` GET.
3. API returns `200`.

**WiFi unavailable (queued):**
1. Command stored in `queuedPowerWatts` with timestamp.
2. Background reconnect triggered.
3. API returns `202 Accepted`.

## Queued Command Delivery (`applyPendingPowerCommand`)

Called after successful polls and on link-state recovery. Behavior:

1. Check expiry: if queued command is older than `POWER_COMMAND_EXPIRY_MS`, discard it and log.
2. POST the queued watts to the inverter.
3. On success: clear the queue (only if no newer command replaced it), call `refreshPowerLimit()`.
4. On failure: command stays queued for next attempt.

## Power Limit Cache (`refreshPowerLimit`)

Reads the current power limit from `GET /power` and stores it in `cachedPowerLimit`.

Called:
- After startup (first successful poll: `STARTING → ONLINE`)
- After recovery from long outage (`BACKOFF/DORMANT → ONLINE`)
- After every successful `setPower()` delivery
- After every successful queued command delivery

The cached value is served by `GET /api/info` without a network call.

## Link-State Recovery

The polling loop classifies the link into `STARTING`, `ONLINE`, `RETRYING`, `BACKOFF`, `DORMANT`.

Bound action on **`BACKOFF` or `DORMANT` → `ONLINE`**:
1. `refreshPowerLimit()` — re-read in case inverter rebooted to a default.
2. `applyPendingPowerCommand()` — deliver any queued user command.

Bound action on **`STARTING` → `ONLINE`**:
1. `refreshPowerLimit()` — initial cache population.

## Expiry Rules

Queued commands expire after `POWER_COMMAND_EXPIRY_MS` (5 minutes):
- Discarded with a log message.
- No fallback action — the user must re-submit if needed.

## API Status Mapping

`POST /api/power`:
- `200`: delivered immediately, cache updated
- `202`: queued for retry when WiFi becomes available
- `400`: invalid payload or out of range

`GET /api/info` returns:
- `power_limit.watts`: cached power limit from inverter (-1 if never read)
- `power_limit.queued`: true if a command is awaiting delivery

## Important Edge Cases

1. On startup before the first successful poll, `cachedPowerLimit` is -1.
2. If the inverter reboots during a long outage, it may reset to its own default — the recovery transition re-reads the actual value.
3. A newer `setPower()` call while a command is already queued replaces the queued value (latest user intent wins).
4. The read-back after write happens immediately — the inverter commits synchronously on POST success.

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
