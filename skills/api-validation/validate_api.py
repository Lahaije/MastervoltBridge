#!/usr/bin/env python3
"""
API Validation Script — ESP32 Inverter Bridge

Calls every endpoint on the live bridge and validates real-time behavior:
  - HTTP status codes match documentation
  - Response bodies contain the expected JSON keys
  - The discovery endpoint (GET /) lists all documented endpoints
  - POST /api/power actually changes the inverter power limit (read-back verified)
  - POST /api/polling changes the poll interval (verified via /api/info)
  - POST /api/debug toggles debug mode
  - POST /api/inverter/fetch can read inverter pages
  - GET /pulse triggers reconnect

Usage (from repo root):
    .venv/Scripts/python skills/api-validation/validate_api.py
    .venv/Scripts/python skills/api-validation/validate_api.py --base-url http://<ip>:8080
    .venv/Scripts/python skills/api-validation/validate_api.py --verbose
    .venv/Scripts/python skills/api-validation/validate_api.py --skip-power  (skip destructive power test)

The bridge IP is auto-discovered via hostname/MAC. Override with --base-url if needed.

Exit code: 0 = all checks passed, 1 = one or more checks failed.
"""

import argparse
import json
import sys
import time
from pathlib import Path
from typing import Any

try:
    import requests
except ImportError:
    print("ERROR: 'requests' not installed. Run: .venv\\Scripts\\python -m pip install requests")
    sys.exit(1)

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from bridge_config import BRIDGE_BASE_URL  # noqa: E402

# ---------------------------------------------------------------------------
# Documented API surface (source of truth for this script)
# ---------------------------------------------------------------------------

# Every endpoint the documentation promises exists, with its HTTP method.
DOCUMENTED_ENDPOINTS: list[tuple[str, str]] = [
    ("GET",  "/"),
    ("GET",  "/api/version"),
    ("GET",  "/api/health"),
    ("GET",  "/api/logs"),
    ("GET",  "/api/info"),
    ("POST", "/api/polling"),
    ("POST", "/api/power"),
    ("POST", "/api/shadow"),
    ("GET",  "/api/shadow"),
    ("GET",  "/api/ha"),
    ("POST", "/api/inverter/fetch"),
    ("POST", "/wifi/off"),
    ("GET",  "/pulse"),
    ("POST", "/api/debug"),
]

# For GET endpoints: required top-level JSON keys expected in 200 responses.
# Endpoints that may return 502 (inverter offline) are marked with allow_502=True.
GET_CHECKS: list[dict[str, Any]] = [
    {
        "path": "/",
        "description": "API discovery",
        "required_keys": ["endpoints"],
        "allow_502": False,
    },
    {
        "path": "/api/version",
        "description": "Firmware version",
        "required_keys": ["firmware_version"],
        "allow_502": False,
    },
    {
        "path": "/api/health",
        "description": "Bridge health",
        "required_keys": ["wifi_connected", "ethernet_ip"],
        "allow_502": False,
    },
    {
        "path": "/api/logs",
        "description": "Log buffer",
        "required_keys": ["entries"],
        "allow_502": False,
    },
    {
        "path": "/api/info",
        "description": "Inverter telemetry (may be 502 if inverter WiFi is off)",
        "required_keys": ["firmware_version", "poll_interval_seconds", "poll_interval_ms", "power", "total_yield", "daily_yield", "power_limit"],
        "allow_502": True,
    },
    {
        "path": "/api/ha",
        "description": "Home Assistant integration (numeric values)",
        "required_keys": ["power", "total_yield", "daily_yield", "power_limit", "available"],
        "allow_502": False,
    },
    # Intentionally do not call /pulse in routine validation because it
    # actively perturbs WiFi state (forced reconnect), which can skew other
    # endpoint checks in the same run.
]

# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

PASS = "\u2713"
FAIL = "\u2717"
SKIP = "\u25cb"


def check(label: str, passed: bool, detail: str = "") -> bool:
    icon = PASS if passed else FAIL
    suffix = f"  ({detail})" if detail else ""
    print(f"  {icon} {label}{suffix}")
    return passed


def get_json(base_url: str, path: str, timeout: int = 10) -> tuple[int, Any]:
    """GET path, return (status_code, parsed_body_or_None)."""
    try:
        r = requests.get(f"{base_url}{path}", timeout=timeout)
        try:
            body = r.json()
        except Exception:
            body = r.text
        return r.status_code, body
    except requests.exceptions.ConnectionError:
        return -1, None
    except requests.exceptions.Timeout:
        return -2, None


def post_json(base_url: str, path: str, payload: dict, timeout: int = 15) -> tuple[int, Any]:
    """POST JSON payload, return (status_code, parsed_body_or_None)."""
    try:
        r = requests.post(f"{base_url}{path}", json=payload, timeout=timeout)
        try:
            body = r.json()
        except Exception:
            body = r.text
        return r.status_code, body
    except requests.exceptions.ConnectionError:
        return -1, None
    except requests.exceptions.Timeout:
        return -2, None


# ---------------------------------------------------------------------------
# Checks
# ---------------------------------------------------------------------------

def check_reachability(base_url: str) -> bool:
    print("\n[1] Bridge reachability")
    status, body = get_json(base_url, "/api/health")
    if status == -1:
        check("Bridge reachable", False, f"Connection refused — is the bridge at {base_url}?")
        return False
    if status == -2:
        check("Bridge reachable", False, "Timed out")
        return False
    ok = check("Bridge reachable", status == 200, f"HTTP {status}")
    return ok


def check_discovery(base_url: str, verbose: bool) -> tuple[bool, list[tuple[str, str]]]:
    """
    GET / and compare the firmware-reported endpoint list against DOCUMENTED_ENDPOINTS.
    Returns (all_ok, live_endpoint_list).
    """
    print("\n[2] Discovery endpoint (GET /)")
    status, body = get_json(base_url, "/")
    if status != 200 or not isinstance(body, dict):
        check("GET / returns 200 JSON", False, f"HTTP {status}")
        return False, []

    check("GET / returns 200 JSON", True)

    # Extract list of {method, path} from firmware discovery response
    raw_endpoints = body.get("endpoints", [])
    live: list[tuple[str, str]] = []
    for ep in raw_endpoints:
        m = ep.get("method", "").upper()
        p = ep.get("path", "")
        if m and p:
            live.append((m, p))

    if verbose:
        print("    Firmware-reported endpoints:")
        for m, p in live:
            print(f"      {m:6s} {p}")

    all_ok = True

    # Every documented endpoint must appear in the firmware response
    for method, path in DOCUMENTED_ENDPOINTS:
        found = (method, path) in live
        ok = check(f"  Documented {method} {path} present in firmware discovery", found)
        all_ok = all_ok and ok

    # Every firmware endpoint must be in the docs (catches undocumented additions)
    for method, path in live:
        documented = (method, path) in DOCUMENTED_ENDPOINTS
        ok = check(f"  Firmware {method} {path} present in documentation", documented,
                   "" if documented else "NOT IN docs/API_REFERENCE.md — update documentation")
        all_ok = all_ok and ok

    return all_ok, live


def check_get_endpoints(base_url: str, verbose: bool) -> bool:
    print("\n[3] GET endpoint responses")
    all_ok = True
    for spec in GET_CHECKS:
        path = spec["path"]
        desc = spec["description"]
        required_keys: list[str] = spec["required_keys"]
        allow_502: bool = spec["allow_502"]

        print(f"\n  {path}  —  {desc}")
        status, body = get_json(base_url, path)

        if status == -1:
            check("reachable", False, "connection error")
            all_ok = False
            continue
        if status == -2:
            check("reachable", False, "timeout")
            all_ok = False
            continue

        if allow_502 and status == 502:
            print(f"    {SKIP} HTTP 502 — inverter WiFi is off, skipping key checks (expected)")
            continue

        ok = check(f"HTTP {status} (expected 200)", status == 200)
        all_ok = all_ok and ok

        if status == 200 and isinstance(body, dict):
            for key in required_keys:
                key_ok = check(f"response contains '{key}'", key in body,
                               f"got keys: {list(body.keys())}" if verbose else "")
                all_ok = all_ok and key_ok
        elif status == 200:
            ok = check("response is JSON object", False, f"body type: {type(body).__name__}")
            all_ok = all_ok and ok

        if verbose and isinstance(body, dict):
            print(f"    Response keys: {list(body.keys())}")

    return all_ok


# ---------------------------------------------------------------------------
# POST endpoint behavioral tests
# ---------------------------------------------------------------------------

def check_post_debug(base_url: str, verbose: bool) -> bool:
    """Toggle debug mode on and off, verify response."""
    print("\n[4] POST /api/debug — toggle debug mode")
    all_ok = True

    # Enable debug
    status, body = post_json(base_url, "/api/debug", {"debug": True})
    ok = check("POST debug=true returns 200", status == 200, f"HTTP {status}")
    all_ok = all_ok and ok
    if status == 200 and isinstance(body, dict):
        ok = check("response contains debug=true", body.get("debug") is True)
        all_ok = all_ok and ok

    # Disable debug
    status, body = post_json(base_url, "/api/debug", {"debug": False})
    ok = check("POST debug=false returns 200", status == 200, f"HTTP {status}")
    all_ok = all_ok and ok
    if status == 200 and isinstance(body, dict):
        ok = check("response contains debug=false", body.get("debug") is False)
        all_ok = all_ok and ok

    return all_ok


def check_post_polling(base_url: str, verbose: bool) -> bool:
    """Change polling interval and verify via /api/info."""
    print("\n[5] POST /api/polling — change poll interval")
    all_ok = True

    # Read current interval from /api/info
    status, info = get_json(base_url, "/api/info")
    if status != 200 or not isinstance(info, dict):
        check("Read current poll interval from /api/info", False, f"HTTP {status}")
        return False
    original_interval = info.get("poll_interval_seconds", 20)
    if verbose:
        print(f"    Current poll interval: {original_interval}s")

    # Set to a test value (different from current)
    test_interval = 5 if original_interval != 5 else 10
    status, body = post_json(base_url, "/api/polling", {"seconds": test_interval})
    ok = check(f"POST polling={test_interval}s returns 200", status == 200, f"HTTP {status}")
    all_ok = all_ok and ok

    # Verify via /api/info
    status, info = get_json(base_url, "/api/info")
    if status == 200 and isinstance(info, dict):
        actual = info.get("poll_interval_seconds")
        ok = check(f"poll_interval_seconds == {test_interval}", actual == test_interval,
                   f"got {actual}")
        all_ok = all_ok and ok
    else:
        check("Read back poll interval", False, f"HTTP {status}")
        all_ok = False

    # Restore original
    status, _ = post_json(base_url, "/api/polling", {"seconds": original_interval})
    ok = check(f"Restore polling to {original_interval}s", status == 200, f"HTTP {status}")
    all_ok = all_ok and ok

    return all_ok


def check_post_inverter_fetch(base_url: str, verbose: bool) -> bool:
    """Fetch an inverter page via the bridge proxy."""
    print("\n[6] POST /api/inverter/fetch — proxy inverter page")
    all_ok = True

    # Fetch /power (simple numeric endpoint)
    status, body = post_json(base_url, "/api/inverter/fetch", {"url": "/power"})
    if status == 502:
        print(f"    {SKIP} HTTP 502 — inverter WiFi is off, skipping (expected)")
        return True

    ok = check("POST inverter/fetch /power returns 200", status == 200, f"HTTP {status}")
    all_ok = all_ok and ok

    if status == 200:
        try:
            watts = int(str(body).strip())
            ok = check(f"Response is numeric power value", 0 <= watts <= 1575,
                       f"got {watts}W")
            all_ok = all_ok and ok
        except (ValueError, TypeError):
            check("Response is numeric power value", False, f"got '{body}'")
            all_ok = False

    # Fetch /home (inverter telemetry page — comma-separated values)
    status, body = post_json(base_url, "/api/inverter/fetch", {"url": "/home"})
    ok = check("POST inverter/fetch /home returns 200", status == 200, f"HTTP {status}")
    all_ok = all_ok and ok
    if status == 200:
        body_str = str(body)
        ok = check("/home contains data", len(body_str) > 5,
                   f"body length={len(body_str)}")
        all_ok = all_ok and ok

    return all_ok


def get_current_power_watts(base_url: str) -> float | None:
    """Read instantaneous power from /api/info."""
    status, body = get_json(base_url, "/api/info")
    if status != 200 or not isinstance(body, dict):
        return None
    try:
        return float(body["power"])
    except (KeyError, ValueError, TypeError):
        return None


def get_power_limit(base_url: str) -> int | None:
    """Read power limit register from inverter via proxy."""
    status, body = post_json(base_url, "/api/inverter/fetch", {"url": "/power"})
    if status != 200:
        return None
    try:
        return int(str(body).strip())
    except (ValueError, TypeError):
        return None


def set_power_limit(base_url: str, watts: int) -> tuple[bool, str]:
    """Set power limit, return (success, detail)."""
    status, body = post_json(base_url, "/api/power", {"power": watts})
    if status == 200:
        return True, "immediate"
    elif status == 202:
        return False, "queued (WiFi unavailable)"
    else:
        return False, f"HTTP {status}"


def check_post_power(base_url: str, verbose: bool) -> bool:
    """
    Real-time power limiting test:
    1. Read current power output and power limit (pre-test baseline)
    2. Set limit to 80% of current output
    3. Poll until output drops to/below the limit (or timeout)
    4. Restore original power limit
    """
    print("\n[7] POST /api/power — real-time power limiting test")
    all_ok = True

    # Step 1: Read pre-test baseline
    original_limit = get_power_limit(base_url)
    if original_limit is None:
        print(f"    {SKIP} Cannot read power limit — inverter WiFi may be off")
        return True
    check(f"Pre-test power limit: {original_limit}W", True)

    current_power = get_current_power_watts(base_url)
    if current_power is None or current_power < 10:
        print(f"    {SKIP} Inverter not producing power ({current_power}W) — cannot test limiting")
        # Still verify register write works
        return check_post_power_register_only(base_url, original_limit, verbose)

    check(f"Current power output: {current_power:.0f}W", True)

    # Step 2: Calculate 80% limit
    test_limit = int(current_power * 0.80)
    # Clamp to valid range
    test_limit = max(0, min(test_limit, 1575))
    if test_limit >= current_power:
        print(f"    {SKIP} 80% limit ({test_limit}W) not below current output — cannot observe reduction")
        return check_post_power_register_only(base_url, original_limit, verbose)

    print(f"    Setting limit to 80% of output: {test_limit}W (expecting drop from {current_power:.0f}W)")
    ok, detail = set_power_limit(base_url, test_limit)
    if not ok:
        check(f"Set power limit to {test_limit}W", False, detail)
        return False
    check(f"Set power limit to {test_limit}W", True, detail)

    # Verify register was written
    time.sleep(1)
    readback = get_power_limit(base_url)
    if readback is not None:
        ok = check(f"Register confirms {test_limit}W", readback == test_limit,
                   f"got {readback}W")
        all_ok = all_ok and ok

    # Step 3: Poll power output until it drops to the limit (max 30s)
    print(f"    Waiting for power to drop to <={test_limit}W...")
    # Set polling to 1s for faster observation
    post_json(base_url, "/api/polling", {"seconds": 1})

    limit_reached = False
    poll_start = time.time()
    max_wait = 30  # seconds
    readings: list[float] = []

    while (time.time() - poll_start) < max_wait:
        time.sleep(3)
        power_now = get_current_power_watts(base_url)
        if power_now is None:
            continue
        readings.append(power_now)
        elapsed = time.time() - poll_start
        if verbose:
            print(f"      {elapsed:.0f}s: {power_now:.1f}W")
        if power_now <= test_limit * 1.05:  # 5% tolerance
            limit_reached = True
            break

    if readings:
        final_power = readings[-1]
        ok = check(
            f"Power dropped to limit",
            limit_reached,
            f"final={final_power:.0f}W, limit={test_limit}W, "
            f"readings={[f'{r:.0f}' for r in readings]}"
        )
        all_ok = all_ok and ok
    else:
        check("Got power readings during wait", False, "no readings received")
        all_ok = False

    # Step 4: Restore original power limit
    print(f"    Restoring power limit to {original_limit}W...")
    ok, detail = set_power_limit(base_url, original_limit)
    ok_check = check(f"Restore power limit to {original_limit}W", ok, detail)
    all_ok = all_ok and ok_check

    if ok:
        time.sleep(1)
        readback = get_power_limit(base_url)
        if readback is not None:
            ok = check(f"Register confirms restored to {original_limit}W",
                       readback == original_limit, f"got {readback}W")
            all_ok = all_ok and ok

    # Restore polling to 20s
    post_json(base_url, "/api/polling", {"seconds": 20})

    return all_ok


def check_post_power_register_only(base_url: str, original_limit: int, verbose: bool) -> bool:
    """Fallback: just verify register write/read when inverter isn't producing."""
    print("    Falling back to register-only test...")
    all_ok = True

    test_watts = 1000 if original_limit != 1000 else 1100
    ok, detail = set_power_limit(base_url, test_watts)
    if not ok:
        check(f"Set power limit to {test_watts}W", False, detail)
        return False
    check(f"Set power limit to {test_watts}W", True, detail)

    time.sleep(1)
    readback = get_power_limit(base_url)
    if readback is not None:
        ok = check(f"Register confirms {test_watts}W", readback == test_watts,
                   f"got {readback}W")
        all_ok = all_ok and ok
    else:
        check("Read back power limit", False, "no response")
        all_ok = False

    # Restore
    ok, detail = set_power_limit(base_url, original_limit)
    ok_check = check(f"Restore to {original_limit}W", ok, detail)
    all_ok = all_ok and ok_check

    if ok:
        time.sleep(1)
        readback = get_power_limit(base_url)
        if readback is not None:
            ok = check(f"Register confirms restored to {original_limit}W",
                       readback == original_limit, f"got {readback}W")
            all_ok = all_ok and ok

    return all_ok


def check_pulse(base_url: str, verbose: bool) -> bool:
    """GET /pulse — triggers reconnect, verify response shape."""
    print("\n[8] GET /pulse — trigger reconnect")
    all_ok = True

    status, body = get_json(base_url, "/pulse", timeout=30)
    ok = check("GET /pulse returns 200", status == 200, f"HTTP {status}")
    all_ok = all_ok and ok

    if status == 200 and isinstance(body, dict):
        ok = check("Response contains 'reconnected'", "reconnected" in body)
        all_ok = all_ok and ok

    return all_ok


def print_summary(results: dict[str, bool]) -> bool:
    print("\n" + "=" * 60)
    print("SUMMARY")
    print("=" * 60)
    all_passed = True
    for label, passed in results.items():
        icon = PASS if passed else FAIL
        print(f"  {icon}  {label}")
        if not passed:
            all_passed = False
    print()
    if all_passed:
        print("All checks passed. Documentation matches live bridge.")
    else:
        print("One or more checks FAILED.")
        print("  - If a firmware endpoint is missing from docs: update docs/API_REFERENCE.md")
        print("  - If a documented endpoint is missing from firmware: update api.cpp and re-upload")
        print("    (use: .venv\\Scripts\\python skills/firmware-upload/upload_firmware.py)")
        print("  - If a response key is missing: check api_helper.cpp and update docs if firmware changed")
    print()
    return all_passed


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def main() -> int:
    parser = argparse.ArgumentParser(description="Validate ESP32 bridge API against documentation")
    parser.add_argument("--base-url", default=BRIDGE_BASE_URL,
                        help=f"Base URL of the bridge (default: {BRIDGE_BASE_URL})")
    parser.add_argument("--verbose", action="store_true",
                        help="Print response keys and firmware endpoint list")
    parser.add_argument("--skip-power", action="store_true",
                        help="Skip the power limit test (avoids changing inverter state)")
    args = parser.parse_args()

    base_url = args.base_url.rstrip("/")
    print(f"ESP32 Bridge API Validation")
    print(f"Target: {base_url}")
    print(f"Documented endpoints: {len(DOCUMENTED_ENDPOINTS)}")

    results: dict[str, bool] = {}

    reachable = check_reachability(base_url)
    results["Bridge reachable"] = reachable
    if not reachable:
        print("\nBridge not reachable — cannot continue.")
        print("Check Ethernet connection and verify the bridge IP/port.")
        return 1

    discovery_ok, live_endpoints = check_discovery(base_url, args.verbose)
    results["Discovery matches documentation"] = discovery_ok

    get_ok = check_get_endpoints(base_url, args.verbose)
    results["GET endpoint responses valid"] = get_ok

    # POST behavioral tests
    debug_ok = check_post_debug(base_url, args.verbose)
    results["POST /api/debug toggles correctly"] = debug_ok

    polling_ok = check_post_polling(base_url, args.verbose)
    results["POST /api/polling changes interval"] = polling_ok

    fetch_ok = check_post_inverter_fetch(base_url, args.verbose)
    results["POST /api/inverter/fetch proxies inverter"] = fetch_ok

    if not args.skip_power:
        power_ok = check_post_power(base_url, args.verbose)
        results["POST /api/power sets limit (verified)"] = power_ok
    else:
        print(f"\n[7] POST /api/power — SKIPPED (--skip-power)")

    pulse_ok = check_pulse(base_url, args.verbose)
    results["GET /pulse triggers reconnect"] = pulse_ok

    all_passed = print_summary(results)
    return 0 if all_passed else 1


if __name__ == "__main__":
    sys.exit(main())
