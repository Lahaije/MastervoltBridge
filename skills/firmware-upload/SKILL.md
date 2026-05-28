---
name: firmware-upload
description: Compile and upload ESP32 inverter bridge firmware. Use when flashing new firmware to the ESP32-S3 over USB serial. Detects whether the device is connected and reports clearly if not found.
---

<objective>
Compile and upload the inverter bridge firmware to the ESP32-S3 over USB serial.

Enforce release traceability by requiring firmware version strings that include semantic version, date, and git short commit hash before flashing. This guarantees that every flashed binary can be mapped back to an exact repository state.

Use the upload helper script to detect the target device, compile, and flash while preserving the project's canonical Python invocation style.
</objective>

<quick_start>
From the repository root, run this release-safe sequence:

```powershell
git add -A
git commit -m "Describe firmware change"
$commit = (git rev-parse --short=7 HEAD).Trim()
$date = Get-Date -Format "yyyyMMdd"
# Update firmware/esp32_inverter_bridge/settings.cpp:
# const char* FIRMWARE_VERSION = "0.1.0-$date-$commit";
.venv\Scripts\python.exe skills/firmware-upload/upload_firmware.py
```

Expected version format: `<semver>-<YYYYMMDD>-<commit_short_hash>`.
</quick_start>

<process>
1. Stage and commit firmware-related changes so the release has a stable source snapshot.
2. Generate `date` and `commit` values and update `FIRMWARE_VERSION` in `firmware/esp32_inverter_bridge/settings.cpp`.
3. Run `.venv\Scripts\python.exe skills/firmware-upload/upload_firmware.py`.
4. If needed, use script variants:
   - `.venv\Scripts\python.exe skills/firmware-upload/upload_firmware.py --skip-upload`
   - `.venv\Scripts\python.exe skills/firmware-upload/upload_firmware.py --skip-compile`
   - `.venv\Scripts\python.exe skills/firmware-upload/upload_firmware.py --port COM5`
5. Confirm upload completion and runtime visibility of the flashed version.
</process>

<context>
- Arduino IDE installed at the standard path (includes `arduino-cli`).
- ESP32 board package installed (`esp32:esp32`).
- USB cable connected from PC to ESP32-S3.
- Run commands from the **repository root** (`d:\git\MastervoltBridge`).
</context>

<examples>
```powershell
# Compile and upload (default)
.venv\Scripts\python.exe skills/firmware-upload/upload_firmware.py

# Compile only (no board needed)
.venv\Scripts\python.exe skills/firmware-upload/upload_firmware.py --skip-upload

# Upload only (reuse previous build)
.venv\Scripts\python.exe skills/firmware-upload/upload_firmware.py --skip-compile
```
</examples>

<troubleshooting>
- **"NOT FOUND" / no ports visible** — Check USB cable and driver. On Windows, look for "USB Serial Device" or "CP210x" in Device Manager.
- **Wrong port** — The script lists visible ports when detection fails. Re-run with `--port COMx`.
- **Compile failed** — Fix the firmware error shown in output, then re-run.
- **Upload failed after compile OK** — Press BOOT button on ESP32 during upload, or check no other app has the port open.
</troubleshooting>

<validation>
- `settings.cpp` contains `FIRMWARE_VERSION` in `<semver>-<YYYYMMDD>-<commit_short_hash>` format.
- Upload output includes completion text and board reset.
- `/api/info` returns `firmware_version` matching `settings.cpp`.
- Web UI header shows the same flashed firmware version.
</validation>

<anti_patterns>
- Flashing without committing code first.
- Keeping `FIRMWARE_VERSION` as a static label (for example `0.1.0-alpha1`) after code changes.
- Using bare `python` instead of the canonical `.venv\Scripts\python.exe` invocation.
</anti_patterns>

<advanced_features>
- `CDCOnBoot=cdc` enables USB CDC serial output so `Serial.print()` is visible on COM9.
- Opening COM9 with DTR=true (default) **resets the board**. To monitor without reset, disable DTR/RTS (e.g. pyserial with `dsrdtr=False, rtscts=False, dtr=False`).
- Changing FQBN flags triggers a **full core rebuild** (~5 min). Same flags = fast incremental (~20s).
</advanced_features>

<success_criteria>
Upload is complete when:
- [ ] ESP32 detected on expected COM port
- [ ] Compile succeeds (no errors)
- [ ] Upload completes with "Hard resetting via RTS pin..."
- [ ] Device reboots and `/api/health` responds with `firmware_version` field
- [ ] Web UI displays firmware version in header
</success_criteria>
