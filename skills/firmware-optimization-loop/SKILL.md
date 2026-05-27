---
name: firmware-optimization-loop
description: Implement ESP32 firmware optimizations using a repeatable write, upload, validate loop with measurement-based decision making. Use when optimizing WiFi connect latency, timeout rates, or polling behavior.
---

<objective>
Use a disciplined loop to optimize firmware behavior while preserving reliability:
1) write a focused code change
2) compile and upload to ESP32
3) validate with log analysis
4) decide keep or revise
</objective>

<quick_start>
All commands below use the explicit venv python, which works in any shell — no activation required. Follow the loop phases in order:
```powershell
.venv\Scripts\python.exe skills/log-analysis/analyze_bridge_logs.py                  # baseline
.venv\Scripts\python.exe skills/firmware-upload/upload_firmware.py --skip-upload     # compile only
.venv\Scripts\python.exe skills/firmware-upload/upload_firmware.py                   # compile + flash
.venv\Scripts\python.exe skills/log-analysis/analyze_bridge_logs.py                  # validate
```
</quick_start>

<scope>
Use this skill when optimizing firmware logic in:
- `firmware/esp32_inverter_bridge/wifi_bridge.cpp`
- `firmware/esp32_inverter_bridge/inverter_controller.cpp`
- `firmware/esp32_inverter_bridge/settings.cpp`
- `firmware/esp32_inverter_bridge/settings.h`
</scope>

<required_inputs>
- Clear optimization objective (e.g. reduce connect latency, reduce timeout rate)
- Device target: FQBN `esp32:esp32:esp32s3`, Port `COM9`
- Validation plan: number of polling cycles (each 20s), success-rate target, timing target
</required_inputs>

<background>
Measurement is passive and automatic. Every reconnect:
1. `WifiConnectionManager::ensureConnected()` triggers a GPIO wake pulse.
2. It selects the next alternating connect path (dwell or auto).
3. The result is logged as structured `[WIFI-CONNECT]` entries readable by `analyze_bridge_logs.py`.

No manual trigger needed. Let the bridge run for several polling intervals.
</background>

<process>
**Phase 1: Baseline**
1. Confirm device and API reachability.
2. Let the bridge run for 5–10 polling intervals (100–200 seconds).
3. Collect baseline:
```powershell
.venv\Scripts\python.exe skills/log-analysis/analyze_bridge_logs.py
```
Save: success rate, min/avg/max/median connect time per path, timeout count.

**Phase 2: Write Code**
1. Change one variable at a time.
2. Keep API behavior stable unless explicitly changing it.
3. Add concise logs for new branch behavior when needed.
4. State test intent in 1–2 sentences before upload.

Good change granularity: one timing constant, one retry policy, one connect path branch. Avoid mixing multiple independent experiments.

**Phase 3: Compile and Upload**
```powershell
.venv\Scripts\python.exe skills/firmware-upload/upload_firmware.py
```
Do not treat IDE include-path diagnostics as build failure; rely on compile result.

**Phase 4: Validate**
1. Wait several polling intervals after upload (each 20s).
2. Run log analysis:
```powershell
.venv\Scripts\python.exe skills/log-analysis/analyze_bridge_logs.py
```
3. Verify: endpoint behavior unchanged, no new recurring failure pattern.

**Phase 5: Decision**
Keep the revision only if:
1. Reliability is not worse than baseline (success rate + timeout profile)
2. Performance is improved or equal for target metric

If either fails: revert experiment logic, keep instrumentation if useful, start new iteration.
</process>

<iteration_template>
At each loop iteration, report:
1. Part label (e.g. "Loop Part 4")
2. What changed
3. What will be tested (1–2 sentences)
4. Current status after validation (pass/fail vs baseline)
</iteration_template>

<validation_checklist>
- [ ] Firmware compiled successfully
- [ ] Firmware uploaded to COM9 successfully
- [ ] At least one connect attempt logged (`[WIFI-CONNECT] complete`)
- [ ] `/api/info` returns telemetry (HTTP 200)
- [ ] Log analysis run and metrics compared against baseline
- [ ] Keep/revert decision documented
</validation_checklist>

<output_report>
Report each iteration with:
- Objective
- Code changes summary
- Upload result
- Validation metrics (per-path dwell/auto breakdown)
- Comparison vs baseline
- Decision and next step
</output_report>

<related_skills>
- `skills/log-analysis/SKILL.md`
- `skills/firmware-upload/SKILL.md`
</related_skills>

<success_criteria>
Optimization loop is complete when:
- [ ] Baseline metrics captured
- [ ] At least one iteration completed (write → upload → validate → decide)
- [ ] Final metrics show improvement (or confirm no regression)
- [ ] Decision documented with data
</success_criteria>
