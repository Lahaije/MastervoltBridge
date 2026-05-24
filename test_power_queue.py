#!/usr/bin/env python3
"""Stress test: queued power command delivery across WiFi disruptions.

Sets 10 different power limits, sometimes triggering /pulse in between
to simulate WiFi outage. Verifies each setting is eventually applied
by polling /api/info until the power_limit reflects the requested value.

Prerequisites: bridge must be reachable and inverter online.
"""

import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from bridge_config import BRIDGE_BASE_URL

import requests

BASE = BRIDGE_BASE_URL
TIMEOUT = 15
MAX_WAIT_S = 60  # max seconds to wait for a queued command to deliver

# 10 test values spanning the range
POWER_VALUES = [1575, 800, 1200, 400, 1000, 600, 1400, 200, 900, 1575]

# Indices after which we trigger a /pulse to disrupt WiFi
PULSE_AFTER = {1, 3, 5, 7}


def api(method, path, json_body=None):
    url = f"{BASE}{path}"
    r = requests.request(method, url, json=json_body, timeout=TIMEOUT)
    return r.status_code, r.text, r


def set_polling(seconds):
    status, text, _ = api("POST", "/api/polling", {"seconds": seconds})
    assert status == 200, f"Failed to set polling: {status} {text}"
    print(f"  Polling set to {seconds}s")


def set_power(watts):
    status, text, r = api("POST", "/api/power", {"power": watts})
    assert status in (200, 202), f"Unexpected status {status}: {text}"
    body = r.json()
    # "status" field is only present when queued; immediate delivery has "inverter_http_status"
    if "status" in body:
        return body["status"]  # "queued"
    return "immediate"


def get_power_limit():
    """Return current confirmed power limit from bridge."""
    status, text, r = api("GET", "/api/info")
    if status != 200:
        return None, False
    body = r.json()
    pl = body.get("power_limit", {})
    return pl.get("watts"), not pl.get("queued", False)


def trigger_pulse():
    """Trigger /pulse to force WiFi reconnect cycle (simulates disruption)."""
    status, text, _ = api("GET", "/pulse")
    return status == 200


def wait_for_limit(expected_watts):
    """Poll until power_limit matches expected and is not queued."""
    start = time.time()
    while time.time() - start < MAX_WAIT_S:
        watts, confirmed = get_power_limit()
        if watts == expected_watts and confirmed:
            elapsed = time.time() - start
            return True, elapsed
        time.sleep(1)
    # Final check
    watts, confirmed = get_power_limit()
    return watts == expected_watts and confirmed, time.time() - start


def main():
    print(f"Target: {BASE}")
    print(f"Test values: {POWER_VALUES}")
    print(f"Pulse disruption after indices: {sorted(PULSE_AFTER)}")
    print()

    # Verify bridge is reachable
    status, _, r = api("GET", "/api/version")
    assert status == 200, "Bridge not reachable!"
    print(f"  Bridge version: {r.json()['firmware_version']}")

    # Set polling to 1s for fast testing
    set_polling(1)
    print()

    results = []
    all_passed = True

    for i, watts in enumerate(POWER_VALUES):
        print(f"[{i+1}/10] Setting power limit to {watts}W...")

        # Optionally disrupt WiFi before setting power
        if i in PULSE_AFTER:
            print(f"  >> Triggering /pulse to disrupt WiFi...")
            trigger_pulse()
            time.sleep(0.5)  # Small delay to let disruption take effect

        # Set the power
        delivery = set_power(watts)
        print(f"  POST /api/power response: {delivery}")

        # Wait for confirmation
        ok, elapsed = wait_for_limit(watts)
        status_str = "PASS" if ok else "FAIL"
        symbol = "✓" if ok else "✗"
        print(f"  {symbol} {status_str}: limit={watts}W confirmed in {elapsed:.1f}s")

        if not ok:
            all_passed = False
            actual, confirmed = get_power_limit()
            print(f"    Actual: {actual}W, confirmed={confirmed}")

        results.append((watts, delivery, ok, elapsed))
        print()

    # Restore polling to 20s
    print("Restoring polling to 20s...")
    set_polling(20)
    print()

    # Summary
    print("=" * 60)
    print("SUMMARY")
    print("=" * 60)
    print(f"{'#':<4} {'Watts':<8} {'Delivery':<12} {'Result':<6} {'Time':>6}")
    print("-" * 60)
    for i, (watts, delivery, ok, elapsed) in enumerate(results):
        symbol = "✓" if ok else "✗"
        print(f"{i+1:<4} {watts:<8} {delivery:<12} {symbol:<6} {elapsed:>5.1f}s")
    print("-" * 60)

    passed = sum(1 for _, _, ok, _ in results if ok)
    print(f"\n{passed}/10 passed")

    if all_passed:
        print("\nAll power commands delivered successfully!")
        return 0
    else:
        print("\nSome commands FAILED to deliver within timeout.")
        return 1


if __name__ == "__main__":
    sys.exit(main())
