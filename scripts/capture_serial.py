"""Capture ESP32 serial output without resetting the device.

Usage: python scripts/capture_serial.py [--port COM9] [--seconds 15] [--out PATH]
"""
import argparse
import sys
import time
from pathlib import Path

import serial


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--port", default="COM9")
    ap.add_argument("--seconds", type=float, default=15.0)
    ap.add_argument("--out", default="output/serial_capture.log")
    args = ap.parse_args()

    Path(args.out).parent.mkdir(parents=True, exist_ok=True)

    # dsrdtr/dtr/rts False so opening the port doesn't reset the ESP32-S3 CDC.
    s = serial.Serial(args.port, 115200, timeout=0.2, dsrdtr=False)
    s.dtr = False
    s.rts = False

    deadline = time.time() + args.seconds
    total = 0
    with open(args.out, "wb") as out:
        while time.time() < deadline:
            data = s.read(4096)
            if data:
                out.write(data)
                out.flush()
                sys.stdout.buffer.write(data)
                sys.stdout.buffer.flush()
                total += len(data)
    s.close()
    print(f"\n[capture] wrote {total} bytes to {args.out}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
