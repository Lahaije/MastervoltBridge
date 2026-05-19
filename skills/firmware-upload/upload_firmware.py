#!/usr/bin/env python3
"""
Compile and upload ESP32 inverter bridge firmware.

Detects whether the ESP32 is connected on the expected COM port before
attempting upload. Exits with a clear message if the device is not found.

Usage:
  python upload_firmware.py               # compile + detect + upload
  python upload_firmware.py --skip-compile
  python upload_firmware.py --port COM5
"""

from __future__ import annotations

import argparse
import json
import os
import shutil
import subprocess
import sys

# Resolve arduino-cli: prefer PATH, fall back to the standard Arduino IDE
# install location under %LOCALAPPDATA% (no username hardcoded).
def _find_arduino_cli() -> str:
    on_path = shutil.which("arduino-cli")
    if on_path:
        return on_path
    local_app_data = os.environ.get("LOCALAPPDATA", "")
    candidate = os.path.join(
        local_app_data,
        "Programs", "Arduino IDE", "resources", "app",
        "lib", "backend", "resources", "arduino-cli.exe",
    )
    return candidate

ARDUINO_CLI = _find_arduino_cli()
FQBN = "esp32:esp32:esp32s3"
DEFAULT_PORT = "COM9"
SKETCH = "firmware/esp32_inverter_bridge"


def list_board_ports(arduino_cli: str) -> list[str]:
    """Return list of port addresses reported by arduino-cli board list."""
    try:
        result = subprocess.run(
            [arduino_cli, "board", "list", "--format", "json"],
            capture_output=True,
            text=True,
            timeout=15,
        )
        if result.returncode != 0 or not result.stdout.strip():
            return []
        data = json.loads(result.stdout)
        entries = data.get("detected_ports", []) if isinstance(data, dict) else data
        return [
            entry.get("port", {}).get("address", "")
            for entry in entries
            if isinstance(entry, dict) and entry.get("port", {}).get("address")
        ]
    except Exception as ex:
        print(f"  (board list failed: {ex})")
        return []


def detect_port(arduino_cli: str, expected_port: str) -> bool:
    """Return True if the expected COM port is visible to arduino-cli."""
    ports = list_board_ports(arduino_cli)
    return any(p.upper() == expected_port.upper() for p in ports)


def run_step(label: str, cmd: list[str]) -> bool:
    print(f"\n{label}")
    print("  " + " ".join(f'"{c}"' if " " in c else c for c in cmd))
    result = subprocess.run(cmd, text=True)
    if result.returncode != 0:
        print(f"  FAILED (exit code {result.returncode})")
        return False
    return True


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Compile and upload ESP32 inverter bridge firmware"
    )
    parser.add_argument(
        "--port",
        default=DEFAULT_PORT,
        help=f"Serial port to upload to (default: {DEFAULT_PORT})",
    )
    parser.add_argument(
        "--skip-compile",
        action="store_true",
        help="Skip compilation, go straight to upload",
    )
    args = parser.parse_args()

    print("ESP32 Firmware Upload")
    print(f"  FQBN   : {FQBN}")
    print(f"  Port   : {args.port}")
    print(f"  Sketch : {SKETCH}")

    # ------------------------------------------------------------------
    # Step 1: Detect device
    # ------------------------------------------------------------------
    print(f"\nDetecting ESP32 on {args.port}...", end=" ", flush=True)
    found = detect_port(ARDUINO_CLI, args.port)

    if not found:
        print("NOT FOUND")
        print()
        print(f"Upload not possible: ESP32 not detected on {args.port}.")
        print()
        print("Check that:")
        print("  - USB cable is connected and fully seated")
        print("  - Board is powered on")
        print(f"  - Correct port is configured (current: {args.port})")
        print("  - USB-Serial/JTAG driver is installed")
        print()

        # Show what ports ARE visible, to help diagnose wrong port
        all_ports = list_board_ports(ARDUINO_CLI)
        if all_ports:
            print("Ports currently visible to arduino-cli:")
            for p in sorted(all_ports):
                print(f"  {p}")
            print(f"\nIf the ESP32 is on a different port, re-run with --port <PORT>")
        else:
            print("No serial ports detected by arduino-cli at all.")

        return 1

    print("OK")

    # ------------------------------------------------------------------
    # Step 2: Compile
    # ------------------------------------------------------------------
    if not args.skip_compile:
        ok = run_step(
            "Compiling...",
            [ARDUINO_CLI, "compile", "--fqbn", FQBN, SKETCH],
        )
        if not ok:
            return 1

    # ------------------------------------------------------------------
    # Step 3: Upload
    # ------------------------------------------------------------------
    ok = run_step(
        f"Uploading to {args.port}...",
        [ARDUINO_CLI, "upload", "--fqbn", FQBN, "--port", args.port, SKETCH],
    )
    if not ok:
        return 1

    print("\nFirmware upload complete.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
