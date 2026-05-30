#!/usr/bin/env python3
"""
API Validation Script — ESP32 Inverter Bridge

Calls every GET endpoint on the live bridge, validates that:
  - HTTP status codes match documentation
  - Response bodies contain the expected JSON keys
    - The discovery endpoint (GET /api) lists all documented endpoints

Usage (from repo root):
    .venv/Scripts/python skills/api-validation/validate_api.py
    .venv/Scripts/python skills/api-validation/validate_api.py --base-url http://192.168.1.48:8080
    .venv/Scripts/python skills/api-validation/validate_api.py --verbose

Exit code: 0 = all checks passed, 1 = one or more checks failed.
"""

import argparse
import sys
from typing import Any

try:
    import requests
except ImportError:
    print("ERROR: 'requests' not installed. Run: .venv\\Scripts\\python -m pip install requests")
    sys.exit(1)

# ---------------------------------------------------------------------------
# Documented API surface (source of truth for this script)
# ---------------------------------------------------------------------------

# Every endpoint the documentation promises exists, with its HTTP method.
DOCUMENTED_ENDPOINTS: list[tuple[str, str]] = [
    ("GET",  "/"),
    ("GET",  "/api"),
    ("GET",  "/api/device"),
    ("GET",  "/api/health"),
    ("GET",  "/api/logs"),
    ("GET",  "/api/info"),
    ("POST", "/api/power"),
    ("POST", "/api/shadow"),
    ("POST", "/api/inverter/fetch"),
    ("POST", "/wifi/off"),
    ("GET",  "/pulse"),
    ("POST", "/api/debug"),
    ("POST", "/api/interval"),
]

# For GET endpoints: required top-level JSON keys expected in 200 responses.
# Endpoints that may return 502 (inverter offline) are marked with allow_502=True.
GET_CHECKS: list[dict[str, Any]] = [
    {
        "path": "/",
        "description": "Web UI dashboard",
        "kind": "html",
    },
    {
        "path": "/api",
        "description": "API discovery",
        "required_keys": ["endpoints"],
        "allow_502": False,
        "kind": "json",
    },
    {
        "path": "/api/device",
        "description": "Device identity",
        "required_keys": ["firmware_version", "inverter_model", "ethernet_ip", "wifi_ssid", "inverter_host"],
        "allow_502": False,
        "kind": "json",
    },
    {
        "path": "/api/health",
        "description": "Bridge health",
        "required_keys": ["wifi_connected", "inverter_link_state", "operating_status", "debug_mode"],
        "allow_502": False,
        "kind": "json",
    },
    {
        "path": "/api/logs",
        "description": "Log buffer",
        "required_keys": ["entries"],
        "allow_502": False,
        "kind": "json",
    },
    {
        "path": "/api/info",
        "description": "Inverter telemetry cache",
        "required_keys": ["power", "total_yield", "daily_yield", "failure_streak_s", "poll_interval_ms"],
        "allow_502": False,
        "kind": "json",
    },
    {
        "path": "/pulse",
        "description": "GPIO wake pulse + forced reconnect",
        "required_keys": ["reconnected"],
        "allow_502": False,
        "kind": "json",
    },
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
            body = None
        return r.status_code, body
    except requests.exceptions.ConnectionError:
        return -1, None
    except requests.exceptions.Timeout:
        return -2, None


def get_raw(base_url: str, path: str, timeout: int = 10) -> tuple[int, str, str]:
    """GET path, return (status_code, content_type, text_body)."""
    try:
        r = requests.get(f"{base_url}{path}", timeout=timeout)
        return r.status_code, r.headers.get("Content-Type", ""), r.text
    except requests.exceptions.ConnectionError:
        return -1, "", ""
    except requests.exceptions.Timeout:
        return -2, "", ""


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
    GET /api and compare the firmware-reported endpoint list against DOCUMENTED_ENDPOINTS.
    Returns (all_ok, live_endpoint_list).
    """
    print("\n[2] Discovery endpoint (GET /api)")
    status, body = get_json(base_url, "/api")
    if status != 200 or not isinstance(body, dict):
        check("GET /api returns 200 JSON", False, f"HTTP {status}")
        return False, []

    check("GET /api returns 200 JSON", True)

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
        kind = spec.get("kind", "json")

        print(f"\n  {path}  —  {desc}")

        if kind == "html":
            status, content_type, text_body = get_raw(base_url, path)

            if status == -1:
                check("reachable", False, "connection error")
                all_ok = False
                continue
            if status == -2:
                check("reachable", False, "timeout")
                all_ok = False
                continue

            ok = check(f"HTTP {status} (expected 200)", status == 200)
            all_ok = all_ok and ok
            type_ok = check("response is HTML", "text/html" in content_type.lower(),
                            f"Content-Type: {content_type}")
            all_ok = all_ok and type_ok
            body_ok = check("response body is non-empty", len(text_body.strip()) > 0)
            all_ok = all_ok and body_ok
            continue

        required_keys: list[str] = spec["required_keys"]
        allow_502: bool = spec["allow_502"]
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
    parser.add_argument("--base-url", default="http://192.168.1.48:8080",
                        help="Base URL of the bridge (default: http://192.168.1.48:8080)")
    parser.add_argument("--verbose", action="store_true",
                        help="Print response keys and firmware endpoint list")
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

    discovery_ok, _ = check_discovery(base_url, args.verbose)
    results["Discovery matches documentation"] = discovery_ok

    get_ok = check_get_endpoints(base_url, args.verbose)
    results["GET endpoint responses valid"] = get_ok

    all_passed = print_summary(results)
    return 0 if all_passed else 1


if __name__ == "__main__":
    sys.exit(main())
