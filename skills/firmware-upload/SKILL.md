---
name: firmware-upload
description: Compile and upload ESP32 inverter bridge firmware. Use when flashing new firmware to the ESP32-S3 over USB serial. Detects whether the device is connected and reports clearly if not found.
---

<objective>
Compile the inverter bridge firmware and upload it to the ESP32 over USB serial.

**Version tracking:** Each firmware flash is tracked via git commit. Before flashing:
1. Commit any code changes: `git commit -m "..."`
2. Get commit ID: `git rev-parse HEAD` (use first 7 chars)
3. Calculate firmware version: `<semver>-<YYYYMMDD>-<commit_short_hash>`
**Before flashing, version and commit the firmware:**

1. Commit your changes:
```powershell
git add -A
git commit -m "Your change description"
```

2. Get the commit ID and date:
```powershell
$commit = git rev-parse HEAD | Select-Object -First 7
$date = Get-Date -Format "yyyyMMdd"
# This gives you: 0.1.0-{date}-{commit} (e.g., 0.1.0-20260528-f09de5c)
```

3. Update `firmware/esp32_inverter_bridge/settings.cpp`:
   - Find `FIRMWARE_VERSION = "..."`
   - Replace with `"0.1.0-{date}-{commit}"`

4. Then compile and upload:
```powershell
.venv\Scripts\python.exe skills/firmware-upload/upload_firmware.py
```

**Or with existing commit (skip-upload to compile only):**
4. Update `FIRMWARE_VERSION` in `firmware/esp32_inverter_bridge/settings.cpp`
5. Compile and upload

This ensures every flashed firmware can be traced back to its exact git commit.

The script detects whether the target device is reachable before attempting upload,
and produces a clear diagnostic message when the ESP32 is not connected.
</objective>

<quick_start>
Run from the repository root. The explicit venv python works in any shell — no activation required:
```powershell
.venv\Scripts\python.exe skills/firmware-upload/upload_firmware.py            # compile + upload
.venv\Scripts\python.exe skills/firmware-upload/upload_firmware.py --skip-upload  # compile only (no device needed)
.venv\Scripts\python.exe skills/firmware-upload/upload_firmware.py --skip-compile # upload last build
```
</quick_start>
**Full workflow (recommended for releases):**
1. Make and test code changes
2. Commit to git: `git commit -m "description"`
3. Extract commit ID and date
4. Update `FIRMWARE_VERSION` in `settings.cpp` with format: `semver-YYYYMMDD-commit_short_hash`
5. Run upload script to compile and flash

**Script steps (automatic once version is set):**
1. **Detect device** — Queries `arduino-cli board list` to check whether `COM9` is present. If not found, exits with a diagnostic listing all visible ports.
2. **Compile** — Runs `arduino-cli compile --fqbn esp32:esp32:esp32s3:CDCOnBoot=cdc` on the sketch. Skipped with `--skip-compile`.
3. **Upload** — Runs `arduino-cli upload` to flash the compiled binary.

**Version format:** `<semver>-<YYYYMMDD>-<commit_short_hash>`
- Example: `0.1.0-20260528-f09de5c` (semantic version + date + 7-char commit hash)
- This ensures every flashed device can be traced back to its exact git commit
.venv\Scripts\python.exe skills/firmware-upload/upload_firmware.py
```

**Compile only (do not flash; no device required):**
```powershell
.venv\Scripts\python.exe skills/firmware-upload/upload_firmware.py --skip-upload
```

**Upload only (skip compile):**
```powershell
.venv\Scripts\python.exe skills/firmware-upload/upload_firmware.py --skip-compile
```

**Override COM port:**
```powershell
.venv\Scripts\python.exe skills/firmware-upload/upload_firmware.py --port COM5
```
</commands>

**Version tracking:**
- Every firmware release is tagged with a version string containing the git commit ID
- Format: `<semver>-<YYYYMMDD>-<commit_hash>` (7-character short hash)
- This allows exact reproduction of any flashed firmware from the git repository
- Always commit before flashing to ensure traceability

<process>
The script performs three steps:
1. **Detect device** — Queries `arduino-cli board list` to check whether `COM9` is present. If not found, exits with a diagnostic listing all visible ports.
2. **Compile** — Runs `arduino-cli compile --fqbn esp32:esp32:esp32s3:CDCOnBoot=cdc` on the sketch. Skipped with `--skip-compile`.
3. **Upload** — Runs `arduino-cli upload` to flash the compiled binary.
</process>

<requirements>
- Arduino IDE installed at the standard path (includes `arduino-cli`).
- ESP32 board package installed (`esp32:esp32`).
- USB cable connected from PC to ESP32-S3.
- Run commands from the **repository root** (`d:\git\MastervoltBridge`).
</requirements>

<configuration>
| Setting | Value |
|---------|-------|
| FQBN | `esp32:esp32:esp32s3:CDCOnBoot=cdc` |
| Default port | `COM9` |
| Sketch path | `firmware/esp32_inverter_bridge` |
| arduino-cli | `C:/Users/.../Arduino IDE/.../arduino-cli.exe` |
</configuration>

<troubleshooting>
- **"NOT FOUND" / no ports visible** — Check USB cable and driver. On Windows, look for "USB Serial Device" or "CP210x" in Device Manager.
- **Wrong port** — The script lists visible ports when detection fails. Re-run with `--port COMx`.
- **Compile failed** — Fix the firmware error shown in output, then re-run.
- **Upload failed after compile OK** — Press BOOT button on ESP32 during upload, or check no other app has the port open.
</troubleshooting>

<notes>
- `CDCOnBoot=cdc` enables USB CDC serial output so `Serial.print()` is visible on COM9.
- Opening COM9 with DTR=true (default) **resets the board**. To monitor without reset, disable DTR/RTS (e.g. pyserial with `dsrdtr=False, rtscts=False, dtr=False`).
- Changing FQBN flags triggers a **full core rebuild** (~5 min). Same flags = fast incremental (~20s).
</notes>

<success_criteria>
Upload is complete when:
- [ ] ESP32 detected on expected COM port
- [ ] Compile succeeds (no errors)
- [ ] Upload completes with "Hard resetting via RTS pin..."
- [ ] Device reboots and `/api/health` responds
</success_criteria>
