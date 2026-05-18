#!/usr/bin/env python3
"""
Run N clean WiFi measurements.

State machine:
  WiFi OFF → pulse (double press, measure connection) → WiFi ON
  WiFi ON  → /wifi/off (single press) → WiFi OFF
  → repeat

Start condition: WiFi must be OFF. If ON, the script turns it off first.

Usage: python run_clean_tests.py [count]
"""

import requests
import time
import sys

IP = "192.168.1.48"
PORT = 8080
BASE_URL = f"http://{IP}:{PORT}"
COUNT = int(sys.argv[1]) if len(sys.argv) > 1 else 10

PULSE_TIMEOUT   = 12   # seconds — /pulse blocks until measurement done
WIFI_OFF_SETTLE = 3    # seconds after /wifi/off before next pulse

def pulse():
    """Trigger double-press measurement. WiFi goes OFF→ON. Returns True on success."""
    try:
        r = requests.get(f"{BASE_URL}/pulse", timeout=PULSE_TIMEOUT)
        return r.status_code == 200
    except Exception as e:
        print(f"✗ pulse error: {e}", end="")
        return False

def wifi_off():
    """Send single press to turn WiFi OFF. Returns True if button was pressed."""
    try:
        r = requests.post(f"{BASE_URL}/wifi/off", timeout=5)
        return r.json().get("pressed", False)
    except Exception as e:
        print(f"✗ wifi/off error: {e}", end="")
        return False

print(f"Starting {COUNT} clean measurement tests")
print(f"Flow: pulse (measure) → wifi/off → wait {WIFI_OFF_SETTLE}s → repeat\n")

# --- Ensure WiFi starts OFF ---
print("Setup: ensuring WiFi is OFF before first test...", end=" ", flush=True)
try:
    r = requests.get(f"{BASE_URL}/api/health", timeout=5)
    wifi_on = r.json().get("wifi_connected", False)
except Exception as e:
    print(f"ERROR reaching bridge: {e}")
    sys.exit(1)

if wifi_on:
    pressed = wifi_off()
    if pressed:
        print(f"✓ turned OFF. Waiting {WIFI_OFF_SETTLE}s...")
        time.sleep(WIFI_OFF_SETTLE)
    else:
        print("already OFF.")
else:
    print("already OFF.")

print()

for i in range(1, COUNT + 1):
    print(f"[{i:2d}/{COUNT}] Measuring...", end=" ", flush=True)

    ok = pulse()
    if ok:
        print("✓  ", end="", flush=True)
    else:
        print("✗  ", end="", flush=True)

    # Turn WiFi OFF to prepare for next iteration
    print("WiFi OFF...", end=" ", flush=True)
    pressed = wifi_off()
    if pressed:
        print(f"✓  Waiting {WIFI_OFF_SETTLE}s")
    else:
        print("(not pressed — WiFi may not have come up)")

    if i < COUNT:
        time.sleep(WIFI_OFF_SETTLE)

print("\nAll tests complete.")


