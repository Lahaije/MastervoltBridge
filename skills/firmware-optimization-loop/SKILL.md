---
name: firmware-optimization-loop
description: Implement ESP32 firmware optimizations using a repeatable write, upload, validate loop with measurement-based decision making.
---

# Firmware Optimization Loop Skill

## Purpose
Use a disciplined loop to optimize firmware behavior while preserving reliability:
1) write a focused code change
2) compile and upload to ESP32
3) validate with log analysis
4) decide keep or revise

## Scope
Use this skill when optimizing firmware logic in:
- firmware/esp32_inverter_bridge/wifi_bridge.cpp
- firmware/esp32_inverter_bridge/inverter_monitor.cpp
- firmware/esp32_inverter_bridge/settings.cpp
- firmware/esp32_inverter_bridge/settings.h

## Required Inputs
- Clear optimization objective (example: reduce connect latency, reduce timeout rate)
- Device target details:
  - FQBN: esp32:esp32:esp32s3
  - Port: COM9
- Validation plan:
  - Number of polling cycles to observe (each cycle is 20s)
  - Success-rate target
  - Timing target (min/avg/median)

## Background: How Measurements Are Collected
Measurement is passive and automatic. Every time the bridge reconnects to the inverter:
1. `WifiConnectionManager::ensureConnected()` triggers a GPIO wake pulse.
2. It then selects the next alternating connect path (dwell or auto).
3. The result is logged as structured `[WIFI-CONNECT]` entries readable by `analyze_bridge_logs.py`.

No manual trigger is needed to collect data. Simply let the bridge run for several polling intervals.

## Loop Phases

**Important**: The venv MUST be activated first in every terminal session before running any Python command:
```powershell
& d:\git\MastervoltBridge\.venv\Scripts\Activate.ps1
```

### Phase 1: Baseline
1. Confirm device and API reachability.
2. Let the bridge run for at least 5-10 polling intervals (100-200 seconds).
3. Collect baseline metrics:
```powershell
python skills/log-analysis/analyze_bridge_logs.py
```
Save baseline: success rate, min/avg/max/median connect time per path, timeout count.

### Phase 2: Write Code
1. Change one variable at a time.
2. Keep API behavior stable unless explicitly changing it.
3. Add concise logs for new branch behavior when needed.
4. State test intent in 1-2 sentences before upload.

Good change granularity:
- one timing constant
- one retry policy
- one connect path branch

Avoid mixing multiple independent experiments in one iteration.

### Phase 3: Compile and Upload
Use the firmware-upload skill:
```powershell
python skills/firmware-upload/upload_firmware.py
```

Or skip compilation when only re-flashing:
```powershell
python skills/firmware-upload/upload_firmware.py --skip-compile
```

The script detects the ESP32 on COM9 before uploading and exits with a clear
message if the device is not connected.

Hard requirement:
- Do not treat IDE include-path diagnostics as build failure; rely on compile result.

### Phase 4: Validate
1. Wait for several polling intervals after upload (each 20s).
2. Run log analysis:
```powershell
python skills/log-analysis/analyze_bridge_logs.py
```
3. Verify behavior expectations:
   - endpoint behavior unchanged (`/api/info` returns telemetry, `/api/health` shows wifi_connected true)
   - no new recurring failure pattern in logs

### Phase 5: Decision
Keep the revision only if both conditions hold:
1. Reliability is not worse than baseline (success rate and timeout profile)
2. Performance is improved or equal for target metric

If either fails:
- revert the experiment logic
- keep instrumentation if still useful
- start a new single-variable iteration

## Console Update Template Per Iteration
At each new loop part, print:
1. Part label (example: Loop Part 4)
2. What changed
3. What will be tested (1-2 sentences)
4. Current status after validation (pass/fail vs baseline)

## Validation Checklist
- Firmware compiled successfully
- Firmware uploaded to COM9 successfully
- At least one connect attempt logged (`[WIFI-CONNECT] complete`)
- `/api/info` returns telemetry (HTTP 200)
- Log analysis run and metrics compared against baseline
- Keep/revert decision documented

## Output Report Format
Report each iteration with:
- Objective
- Code changes summary
- Upload result
- Validation metrics (per-path dwell/auto breakdown)
- Comparison vs baseline
- Decision and next step

## Related Skills
- skills/log-analysis/SKILL.md
- skills/firmware-upload/SKILL.md

