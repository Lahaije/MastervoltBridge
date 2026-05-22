---
name: firmware-upload
description: Compile and upload ESP32 inverter bridge firmware. Detects whether the ESP32 is connected on the expected COM port before attempting upload and reports clearly if the device cannot be found.
---

# Firmware Upload Skill

## Purpose
Compile the inverter bridge firmware and upload it to the ESP32 over USB serial.
The script detects whether the target device is reachable before attempting upload,
and produces a clear diagnostic message when the ESP32 is not connected.

This skill is self-contained in one folder:
- `SKILL.md`: usage guide
- `upload_firmware.py`: compile + detect + upload script

## Requirements
- Arduino IDE installed at the standard path (includes `arduino-cli`).
- ESP32 board package installed (`esp32:esp32`).
- USB cable connected from PC to ESP32-S3.
- Run commands from the **repository root** (`d:\git\MastervoltBridge`).

## Primary Commands

Run from the repository root. **The venv MUST be activated first in every terminal session:**

```powershell
# Activate venv (required once per terminal session — always do this first)
& d:\git\MastervoltBridge\.venv\Scripts\Activate.ps1
```

After activation, use plain `python` for all commands below.

### Compile and upload (default)
```powershell
python skills/firmware-upload/upload_firmware.py
```

### Upload only (skip compile)
```powershell
python skills/firmware-upload/upload_firmware.py --skip-compile
```

### Override COM port
```powershell
python skills/firmware-upload/upload_firmware.py --port COM5
```

## What the Script Does
1. **Detect device**: Queries `arduino-cli board list` to check whether the expected
   port (default `COM9`) is present. If not found, exits immediately with a diagnostic
   message listing all currently visible ports (to help identify the correct port).
2. **Compile**: Runs `arduino-cli compile --fqbn esp32:esp32:esp32s3` on the sketch.
   Skipped with `--skip-compile`.
3. **Upload**: Runs `arduino-cli upload` to flash the compiled binary.

## Example Output — Device Not Connected
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

## Example Output — Success
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

## Troubleshooting
- **"NOT FOUND" / no ports visible**: Check USB cable and driver. On Windows, open
  Device Manager and look for "USB Serial Device" or "CP210x". Install driver if missing.
- **Wrong port**: The script lists all currently visible ports when detection fails.
  Re-run with `--port COMx` matching the actual port.
- **Compile failed**: Fix the firmware error shown in the compile output, then re-run.
- **Upload failed after compile OK**: Try pressing the BOOT button on the ESP32 during
  upload, or check that no other application (Serial Monitor, etc.) has the port open.

## Fixed Configuration
| Setting | Value |
|---------|-------|
| FQBN | `esp32:esp32:esp32s3:CDCOnBoot=cdc` |
| Default port | `COM9` |
| Sketch path | `firmware/esp32_inverter_bridge` |
| arduino-cli | `C:/Users/.../Arduino IDE/.../arduino-cli.exe` |

## Notes
- `CDCOnBoot=cdc` enables USB CDC serial output so `Serial.print()` is visible on COM9.
- Opening COM9 with DTR=true (default for most serial monitors) **resets the board**.
  To monitor without reset, disable DTR/RTS (e.g. `pyserial` with `dsrdtr=False, rtscts=False, dtr=False`).
- Changing FQBN flags (e.g. adding `CDCOnBoot=cdc`) triggers a **full core rebuild** (~5 min).
  Subsequent compiles with the same flags are incremental (~20s).
