---
name: firmware-upload
description: "ALWAYS use upload_firmware.py for ANY firmware flash — never call arduino-cli directly. The script auto-stamps FIRMWARE_VERSION, detects the COM port, compiles, uploads, and commits. Calling arduino-cli manually skips version stamping and produces untraceable firmware."
---

<objective>
Compile the inverter bridge firmware and upload it to the ESP32 over USB serial. The script detects whether the target device is reachable before attempting upload, and produces a clear diagnostic message when the ESP32 is not connected.

This skill is self-contained: `SKILL.md` (usage guide) + `upload_firmware.py` (compile + detect + upload script).
</objective>

<critical_rule>
**Always use `upload_firmware.py` for compiling and flashing firmware.**
Do NOT call `arduino-cli compile` or `arduino-cli upload` directly.

The script handles critical steps that manual commands skip:
- Stamps `FIRMWARE_VERSION` with timestamp + git SHA (without this, the ESP reports a stale version)
- Detects device presence before upload (clear error if ESP not connected)
- Commits the version change to git (every flash is traceable)
</critical_rule>

<quick_start>
```powershell
# Activate venv (required once per terminal session)
& d:\git\MastervoltBridge\.venv\Scripts\Activate.ps1

# THE ONE COMMAND TO FLASH FIRMWARE:
python skills/firmware-upload/upload_firmware.py
```
</quick_start>

<requirements>
- Arduino IDE installed at the standard path (includes `arduino-cli`).
- ESP32 board package installed (`esp32:esp32`).
- USB cable connected from PC to ESP32-S3.
- Run commands from the **repository root** (`d:\git\MastervoltBridge`).
</requirements>

<commands>
Run from the repository root. **The venv MUST be activated first in every terminal session.**

**Compile and upload (default):**
```powershell
python skills/firmware-upload/upload_firmware.py
```

**Upload only (skip compile):**
```powershell
python skills/firmware-upload/upload_firmware.py --skip-compile
```

**Override COM port:**
```powershell
python skills/firmware-upload/upload_firmware.py --port COM5
```

**Pin an explicit firmware version string:**
```powershell
python skills/firmware-upload/upload_firmware.py --version v0.1.0-alpha2
```

**Skip git commit/push trace step:**
```powershell
python skills/firmware-upload/upload_firmware.py --skip-git-trace
```
</commands>

<process>
1. **Detect device** — Queries `arduino-cli board list` to check whether the expected port (default `COM9`) is present. If not found, exits immediately with a diagnostic message listing all currently visible ports.
2. **Stamp firmware version** — Updates `FIRMWARE_VERSION` in `firmware/esp32_inverter_bridge/settings.cpp`. Default format: `fw-YYYYMMDD-HHMMSS-<short-git-sha>`. Override with `--version`.
3. **Compile** — Runs `arduino-cli compile --fqbn esp32:esp32:esp32s3` on the sketch. Skipped with `--skip-compile`.
4. **Upload** — Runs `arduino-cli upload` to flash the compiled binary.
5. **Git trace** — Runs `git add -A`, `git commit`, and `git push` so every flashed version is traceable in remote history. Skipped with `--skip-git-trace`.
</process>

<anti_patterns>
Calling `arduino-cli compile` + `arduino-cli upload` directly **skips version stamping**. The result: `/api/version` reports an old commit hash, making it impossible to know what code is actually running on the device.
</anti_patterns>

<examples>
**Device not connected:**
```
ESP32 Firmware Upload
  FQBN   : esp32:esp32:esp32s3:CDCOnBoot=cdc
  Port   : COM9
  Sketch : firmware/esp32_inverter_bridge

Detecting ESP32 on COM9... NOT FOUND

Upload not possible: ESP32 not detected on COM9.

Check that:
  - USB cable is connected and fully seated
  - Board is powered on
  - Correct port is configured (current: COM9)
  - USB-Serial/JTAG driver is installed

No serial ports detected by arduino-cli at all.
```

**Success:**
```
ESP32 Firmware Upload
  FQBN   : esp32:esp32:esp32s3:CDCOnBoot=cdc
  Port   : COM9
  Sketch : firmware/esp32_inverter_bridge

Detecting ESP32 on COM9... OK

Compiling...
  "arduino-cli" compile --fqbn esp32:esp32:esp32s3:CDCOnBoot=cdc firmware/esp32_inverter_bridge
  Sketch uses 974709 bytes (74%) ...

Uploading to COM9...
  "arduino-cli" upload --fqbn esp32:esp32:esp32s3:CDCOnBoot=cdc --port COM9 firmware/esp32_inverter_bridge
  Hash of data verified.
  Hard resetting via RTS pin...

Firmware upload complete.
```
</examples>

<troubleshooting>
- **"NOT FOUND" / no ports visible** — Check USB cable and driver. On Windows, open Device Manager and look for "USB Serial Device" or "CP210x". Install driver if missing.
- **Wrong port** — The script lists all currently visible ports when detection fails. Re-run with `--port COMx` matching the actual port.
- **Compile failed** — Fix the firmware error shown in the compile output, then re-run.
- **Upload failed after compile OK** — Try pressing the BOOT button on the ESP32 during upload, or check that no other application (Serial Monitor, etc.) has the port open.
</troubleshooting>

<configuration>
| Setting | Value |
|---------|-------|
| FQBN | `esp32:esp32:esp32s3:CDCOnBoot=cdc` |
| Default port | `COM9` |
| Sketch path | `firmware/esp32_inverter_bridge` |
| arduino-cli | `C:/Users/.../Arduino IDE/.../arduino-cli.exe` |
</configuration>

<success_criteria>
Firmware upload is complete when:
- [ ] Device detected on expected COM port
- [ ] `FIRMWARE_VERSION` stamped with current timestamp + git SHA
- [ ] Compilation succeeded (or skipped with `--skip-compile`)
- [ ] Binary uploaded and ESP32 reset
- [ ] Git commit created tracing the flash (or skipped with `--skip-git-trace`)
</success_criteria>

## Notes
- `CDCOnBoot=cdc` enables USB CDC serial output so `Serial.print()` is visible on COM9.
- Opening COM9 with DTR=true (default for most serial monitors) **resets the board**.
  To monitor without reset, disable DTR/RTS (e.g. `pyserial` with `dsrdtr=False, rtscts=False, dtr=False`).
- Changing FQBN flags (e.g. adding `CDCOnBoot=cdc`) triggers a **full core rebuild** (~5 min).
  Subsequent compiles with the same flags are incremental (~20s).
