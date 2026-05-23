"""Test POST /api/power against the silent-drop hypothesis.

Manual probe for TECHNICAL_DEBT.md -> "POST /api/power Latency & Silent Drops".
Pushes a setpoint and polls /api/info until power_limit.confirmed matches
(success) or a timeout expires (probable silent drop).

Usage:
    python set_power.py                       # default 400 W
    python set_power.py --watts 800
    python set_power.py --watts 1575 --verify-timeout 120
"""

from __future__ import annotations

import argparse
import sys
import time
from typing import Any

import requests

from bridge_config import BRIDGE_BASE_URL

HTTP_TIMEOUT = 10.0


def get_json(url: str) -> tuple[int, Any]:
    r = requests.get(url, timeout=HTTP_TIMEOUT)
    try:
        return r.status_code, r.json()
    except ValueError:
        return r.status_code, None


def wait_for_link_online(base: str, deadline: float) -> bool:
    """Return True once /api/health reports inverter_link_state == ONLINE."""
    last_state = None
    while time.time() < deadline:
        try:
            status, body = get_json(f"{base}/api/health")
        except requests.RequestException as exc:
            print(f"  [health] request error: {exc}")
            time.sleep(2)
            continue
        if status != 200 or not isinstance(body, dict):
            print(f"  [health] HTTP {status}, body={body!r}")
            time.sleep(2)
            continue
        state = body.get("inverter_link_state")
        if state != last_state:
            print(
                f"  [health] inverter_link_state={state}"
                f" wifi_connected={body.get('wifi_connected')}"
            )
            last_state = state
        if state == "ONLINE":
            return True
        time.sleep(2)
    return False


def verify_setpoint(base: str, target: int, deadline: float) -> str:
    """Poll /api/info until power_limit.confirmed == target.

    Returns 'confirmed', 'timeout', or 'error:<msg>'.
    """
    last_confirmed = None
    last_queued = None
    while time.time() < deadline:
        try:
            status, info = get_json(f"{base}/api/info")
        except requests.RequestException as exc:
            return f"error:{exc}"
        if status != 200 or not isinstance(info, dict):
            return f"error:HTTP {status}"
        pl = info.get("power_limit") or {}
        confirmed = pl.get("confirmed")
        desired = pl.get("desired")
        queued = pl.get("queued")
        if (confirmed, queued) != (last_confirmed, last_queued):
            print(
                f"  [info] desired={desired} confirmed={confirmed} queued={queued}"
                f" ready={info.get('ready')} power={info.get('power')!r}"
            )
            last_confirmed = confirmed
            last_queued = queued
        if confirmed == target and not queued:
            return "confirmed"
        time.sleep(3)
    return "timeout"


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--base-url", default=BRIDGE_BASE_URL)
    parser.add_argument("--watts", type=int, default=400, help="Setpoint to push (0-1575)")
    parser.add_argument(
        "--link-timeout",
        type=int,
        default=120,
        help="Seconds to wait for inverter_link_state=ONLINE before giving up",
    )
    parser.add_argument(
        "--verify-timeout",
        type=int,
        default=90,
        help="Seconds to wait for power_limit.confirmed to match after POST",
    )
    args = parser.parse_args()

    base = args.base_url.rstrip("/")
    print(f"Bridge: {base}")
    print(f"Target setpoint: {args.watts} W")

    print("\n[1/3] Waiting for inverter link to come online...")
    if not wait_for_link_online(base, time.time() + args.link_timeout):
        print(f"FAIL: inverter_link_state did not reach ONLINE within {args.link_timeout}s")
        return 2

    print("\n[2/3] POST /api/power")
    t0 = time.time()
    try:
        r = requests.post(
            f"{base}/api/power",
            json={"power": args.watts},
            timeout=HTTP_TIMEOUT,
        )
    except requests.RequestException as exc:
        print(f"FAIL: POST raised: {exc}")
        return 3
    elapsed_ms = (time.time() - t0) * 1000
    print(f"  status={r.status_code} elapsed_ms={elapsed_ms:.0f} body={r.text.strip()}")

    if r.status_code == 200:
        print("  -> bridge applied the setpoint immediately")
    elif r.status_code == 202:
        print("  -> bridge queued the setpoint (inverter unreachable at POST time)")
    else:
        print(f"FAIL: unexpected status {r.status_code}")
        return 4

    print("\n[3/3] Verifying power_limit.confirmed matches target")
    result = verify_setpoint(base, args.watts, time.time() + args.verify_timeout)
    if result == "confirmed":
        print(f"PASS: bridge reports power_limit.confirmed={args.watts}")
        return 0
    if result == "timeout":
        print(
            f"FAIL: power_limit.confirmed never reached {args.watts} within"
            f" {args.verify_timeout}s -- probable silent drop"
        )
        return 5
    print(f"FAIL: verification error: {result}")
    return 6


if __name__ == "__main__":
    sys.exit(main())
