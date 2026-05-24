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
    .venv/Scripts/python skills/api-validation/validate_api.py --base-url http://192.168.1.48:8080
    .venv/Scripts/python skills/api-validation/validate_api.py --verbose
    .venv/Scripts/python skills/api-validation/validate_api.py --skip-power  (skip destructive power test)

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


def check_post_power(base_url: str, verbose: bool) -> bool:
    """Set power limit, verify via inverter read-back, then restore original."""
    print("\n[7] POST /api/power — set power limit (live inverter test)")
    all_ok = True

    # Step 1: Read current power limit from inverter
    status, body = post_json(base_url, "/api/inverter/fetch", {"url": "/power"})
    if status == 502:
        print(f"    {SKIP} HTTP 502 — inverter WiFi is off, cannot test power (expected)")
        return True
    if status != 200:
        check("Read current power limit from inverter", False, f"HTTP {status}")
        return False

    try:
        original_watts = int(str(body).strip())
    except (ValueError, TypeError):
        check("Parse current power limit", False, f"got '{body}'")
        return False
    check(f"Current power limit: {original_watts}W", True)

    # Step 2: Set a different test value
    test_watts = 1000 if original_watts != 1000 else 1100
    print(f"    Setting power to {test_watts}W...")
    status, body = post_json(base_url, "/api/power", {"power": test_watts})

    if status == 202:
        # Queued — inverter WiFi dropped between read and write
        print(f"    {SKIP} HTTP 202 — command queued (WiFi dropped), cannot verify")
        return True

    ok = check(f"POST /api/power {test_watts}W returns 200", status == 200, f"HTTP {status}")
    all_ok = all_ok and ok

    if status == 200 and isinstance(body, dict):
        ok = check("Response contains 'inverter_response'",
                   "inverter_response" in body,
                   f"keys: {list(body.keys())}" if verbose else "")
        all_ok = all_ok and ok

    # Step 3: Wait briefly for inverter to process, then read back
    time.sleep(1)
    status, readback = post_json(base_url, "/api/inverter/fetch", {"url": "/power"})
    if status == 200:
        try:
            actual_watts = int(str(readback).strip())
            ok = check(f"Inverter confirms power = {test_watts}W", actual_watts == test_watts,
                       f"expected {test_watts}, got {actual_watts}")
            all_ok = all_ok and ok
        except (ValueError, TypeError):
            check("Parse read-back power", False, f"got '{readback}'")
            all_ok = False
    else:
        check("Read back power limit after set", False, f"HTTP {status}")
        all_ok = False

    # Step 4: Restore original power limit
    print(f"    Restoring power to {original_watts}W...")
    status, body = post_json(base_url, "/api/power", {"power": original_watts})
    ok = check(f"Restore power to {original_watts}W", status in (200, 202), f"HTTP {status}")
    all_ok = all_ok and ok

    if status == 200:
        time.sleep(1)
        status, readback = post_json(base_url, "/api/inverter/fetch", {"url": "/power"})
        if status == 200:
            try:
                restored = int(str(readback).strip())
                ok = check(f"Inverter confirms restored to {original_watts}W",
                           restored == original_watts,
                           f"got {restored}")
                all_ok = all_ok and ok
            except (ValueError, TypeError):
                pass

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
