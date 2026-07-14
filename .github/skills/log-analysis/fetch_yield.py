#!/usr/bin/env python3
"""
Fetch and display daily/lifetime energy yield from the inverter bridge.

Calls /api/info to retrieve the current daily session yield and lifetime
total yield, then prints a summary.

Usage (from repository root):
    .venv\\Scripts\\python skills/log-analysis/fetch_yield.py
    .venv\\Scripts\\python skills/log-analysis/fetch_yield.py --base-url http://<bridge-ip>:8080
"""

from __future__ import annotations

import argparse
import json
import os
import sys
import urllib.request


def parse_optional_float(value):
    if value is None:
        return None
    if isinstance(value, (int, float)):
        return float(value)
    try:
        text = str(value).strip()
        if not text:
            return None
        return float(text)
    except (TypeError, ValueError):
        return None


def fetch_yield(base_url: str, timeout: float) -> int:
    url = f"{base_url.rstrip('/')}/api/info"
    req = urllib.request.Request(url=url, method="GET")
    req.add_header("Accept", "application/json")

    try:
        with urllib.request.urlopen(req, timeout=timeout) as resp:
            raw = resp.read().decode("utf-8", errors="replace")
    except Exception as ex:
        print(f"Failed to fetch /api/info: {ex}")
        return 1

    data = json.loads(raw)

    daily_raw = data.get("daily_yield", None)
    total_raw = data.get("total_yield", None)
    power_raw = data.get("power", "")

    daily = parse_optional_float(daily_raw)
    total = parse_optional_float(total_raw)
    power = parse_optional_float(power_raw)

    if daily is None:
        print("No yield data available (inverter may be offline).")
        return 1

    print("Inverter Yield")
    print("--------------")
    if daily is not None:
        print(f"Daily yield    : {daily:.3f} kWh")
    else:
        print(f"Daily yield    : {daily_raw}")
    if total is not None:
        print(f"Lifetime yield : {total:.3f} kWh")
    else:
        print(f"Lifetime yield : {total_raw}")
    if power is not None:
        print(f"Current power  : {power:.1f} W")

    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description="Fetch inverter daily/lifetime yield")
    parser.add_argument("--base-url", default=None, help="Bridge base URL (for example: http://<bridge-ip>:8080)")
    parser.add_argument("--timeout", type=float, default=10.0, help="HTTP timeout seconds")
    args = parser.parse_args()
    base_url = (args.base_url or os.environ.get("MASTERVOLT_BRIDGE_BASE_URL", "")).rstrip("/")
    if not base_url:
        print("Missing bridge URL. Provide --base-url or set MASTERVOLT_BRIDGE_BASE_URL.")
        return 2
    return fetch_yield(base_url, args.timeout)


if __name__ == "__main__":
    sys.exit(main())
