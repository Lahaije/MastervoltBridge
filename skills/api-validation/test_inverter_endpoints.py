#!/usr/bin/env python3
"""Exercise inverter-facing bridge endpoints in one stateful round-trip.

This script prints every step to the terminal and restores the original
polling interval and power limit before it exits.

It is intentionally separate from validate_api.py:
- validate_api.py checks documentation parity for GET endpoints.
- this script performs a live mutation scenario for polling and power control.
"""

from __future__ import annotations

import argparse
import sys
import time
from typing import Any, Callable, Optional

try:
    import requests
except ImportError:
    print("ERROR: 'requests' not installed. Run: uv pip install requests")
    sys.exit(1)


DEFAULT_BASE_URL = "http://192.168.1.48:8080"


def step(title: str) -> None:
    print(f"\n=== {title} ===")


def ok(label: str, detail: str = "") -> None:
    suffix = f" - {detail}" if detail else ""
    print(f"[OK] {label}{suffix}")


def fail(label: str, detail: str = "") -> None:
    suffix = f" - {detail}" if detail else ""
    print(f"[FAIL] {label}{suffix}")


def request_json(base_url: str, method: str, path: str, timeout: float = 10.0, json_body: Optional[dict[str, Any]] = None) -> tuple[int, Any, str]:
    url = f"{base_url.rstrip('/')}{path}"
    response = requests.request(method, url, json=json_body, timeout=timeout)
    try:
        body = response.json()
    except Exception:
        body = None
    return response.status_code, body, response.text


def get_required_dict(body: Any, label: str) -> dict[str, Any]:
    if not isinstance(body, dict):
        raise RuntimeError(f"{label} did not return a JSON object")
    return body


def parse_int(value: Any, label: str) -> int:
    try:
        return int(value)
    except Exception as exc:
        raise RuntimeError(f"Could not parse integer for {label}: {value!r}") from exc


def parse_float(value: Any, label: str) -> float:
    try:
        return float(value)
    except Exception as exc:
        raise RuntimeError(f"Could not parse float for {label}: {value!r}") from exc


def fetch_info(base_url: str, timeout: float) -> dict[str, Any]:
    status, body, text = request_json(base_url, "GET", "/api/info", timeout=timeout)
    if status != 200:
        raise RuntimeError(f"GET /api/info failed with HTTP {status}: {text}")
    info = get_required_dict(body, "GET /api/info")
    return info


def set_polling(base_url: str, seconds: int, timeout: float) -> dict[str, Any]:
    status, body, text = request_json(base_url, "POST", "/api/polling", timeout=timeout, json_body={"seconds": seconds})
    if status != 200:
        raise RuntimeError(f"POST /api/polling failed with HTTP {status}: {text}")
    return get_required_dict(body, "POST /api/polling")


def set_power(base_url: str, watts: int, timeout: float) -> tuple[int, dict[str, Any], str]:
    status, body, text = request_json(base_url, "POST", "/api/power", timeout=timeout, json_body={"power": watts})
    return status, get_required_dict(body, "POST /api/power") if isinstance(body, dict) else {}, text


def get_debug_mode(base_url: str, timeout: float) -> bool:
    status, body, text = request_json(base_url, "GET", "/api/health", timeout=timeout)
    if status != 200 or not isinstance(body, dict):
        raise RuntimeError(f"GET /api/health failed with HTTP {status}: {text}")
    return bool(body.get("debug_mode", False))


def set_debug_mode(base_url: str, enabled: bool, timeout: float) -> bool:
    status, body, text = request_json(base_url, "POST", "/api/debug", timeout=timeout, json_body={"debug": enabled})
    if status != 200 or not isinstance(body, dict):
        raise RuntimeError(f"POST /api/debug failed with HTTP {status}: {text}")
    return bool(body.get("debug", enabled))


def restore_with_report(label: str, action: Callable[[], None]) -> None:
    try:
        action()
    except Exception as exc:
        fail(label, str(exc))
    else:
        ok(label)


def probe_read_only_endpoints(base_url: str, timeout: float) -> None:
    step("5. Probe read-only endpoints while the power setting settles")

    probes = [
        ("GET /", "/", False),
        ("GET /api/version", "/api/version", False),
        ("GET /api/health", "/api/health", False),
        ("GET /api/logs", "/api/logs", True),
        ("GET /api/info", "/api/info", False),
    ]

    for label, path, summarize_entries in probes:
        status, body, text = request_json(base_url, "GET", path, timeout=timeout)
        print(f"{label} HTTP {status}")
        if status != 200:
            print(f"  response: {text}")
            continue

        if path == "/api/logs" and isinstance(body, dict):
            entries = body.get("entries", [])
            count = len(entries) if isinstance(entries, list) else 0
            print(f"  entries={count}")
            if count:
                first = entries[0]
                last = entries[-1]
                print(f"  first={first.get('timestamp_ms')}: {first.get('message')}")
                print(f"  last={last.get('timestamp_ms')}: {last.get('message')}")
            continue

        if isinstance(body, dict):
            keys = ", ".join(body.keys())
            print(f"  keys={keys}")
            if path == "/api/info":
                power = body.get("power")
                power_limit = body.get("power_limit")
                print(f"  power={power}")
                if isinstance(power_limit, dict):
                    print(
                        "  power_limit="
                        f"desired={power_limit.get('desired')} "
                        f"confirmed={power_limit.get('confirmed')} "
                        f"queued={power_limit.get('queued')} "
                        f"reset_timer_minutes={power_limit.get('reset_timer_minutes')}"
                    )
        elif body is not None:
            print(f"  body={body}")


def inspect_recent_bridge_logs(base_url: str, timeout: float) -> None:
    step("9. Pull inverter logs and inspect the request trail")

    status, body, text = request_json(base_url, "GET", "/api/logs", timeout=timeout)
    print(f"GET /api/logs HTTP {status}")
    if status != 200 or not isinstance(body, dict):
        print(f"  response: {text}")
        return

    entries = body.get("entries", [])
    if not isinstance(entries, list):
        print("  entries: unavailable")
        return

    print(f"  total_entries={len(entries)}")
    recent = entries[-120:]
    interesting_markers = (
        "[API] POST /api/polling",
        "[API] POST /api/power",
        "[API] GET /api/info",
        "[API] GET /api/logs",
        "Poll interval updated to 1s",
        "Poll interval updated to 20s",
        "Power command queued",
        "Retry power command failed",
        "Failed to fetch /home",
        "WIFI-CONNECT",
    )

    print("  recent relevant entries:")
    for entry in recent:
        message = str(entry.get("message", ""))
        if any(marker in message for marker in interesting_markers):
            print(f"    {entry.get('timestamp_ms')}: {message}")

    queued = any("Power command queued" in str(entry.get("message", "")) for entry in recent)
    retry_failed = any("Retry power command failed" in str(entry.get("message", "")) for entry in recent)
    fetch_failed = any("Failed to fetch /home" in str(entry.get("message", "")) for entry in recent)
    interval_one = any("Poll interval updated to 1s" in str(entry.get("message", "")) for entry in recent)
    interval_restored = any("Poll interval updated to 20s" in str(entry.get("message", "")) for entry in recent)

    print("  analysis:")
    if queued:
        print("    - Power requests were queued because inverter WiFi was unavailable.")
    if fetch_failed:
        print("    - The polling worker hit WiFi reconnect retries and failed /home fetches while WiFi was down.")
    if interval_one and fetch_failed:
        print("    - The 1-second interval did not keep the poll loop running continuously during reconnect waits; the worker was blocked by WiFi recovery attempts.")
    if retry_failed:
        print("    - A queued power retry later timed out at the inverter HTTP layer after WiFi recovered.")
    if interval_restored:
        print("    - The polling interval was restored to 20s during cleanup.")



def main() -> int:
    parser = argparse.ArgumentParser(description="Exercise inverter polling and power endpoints with restore on exit")
    parser.add_argument("--base-url", default=DEFAULT_BASE_URL, help=f"Bridge base URL (default: {DEFAULT_BASE_URL})")
    parser.add_argument("--wait-seconds", type=float, default=2.0, help="Minimum seconds to wait after applying the 80%% power limit (default: 2.0)")
    parser.add_argument("--timeout", type=float, default=10.0, help="HTTP timeout in seconds (default: 10)")
    args = parser.parse_args()

    base_url = args.base_url.rstrip("/")
    restore_actions: list[tuple[str, Callable[[], None]]] = []
    target_power = None
    original_poll_seconds = None
    original_power_limit = None
    original_debug_mode: Optional[bool] = None

    return_code = 0
    try:
        step("0. Enable debug mode for the duration of the test")
        original_debug_mode = get_debug_mode(base_url, args.timeout)
        ok("Original debug_mode", str(original_debug_mode))
        restore_actions.append((
            f"Restore debug_mode to {original_debug_mode}",
            lambda value=original_debug_mode: set_debug_mode(base_url, value, args.timeout),
        ))
        new_debug = set_debug_mode(base_url, True, args.timeout)
        ok("Debug mode enabled", f"debug={new_debug}")

        step("1. Read current polling frequency and power limit")
        info = fetch_info(base_url, args.timeout)

        original_poll_seconds = parse_int(info.get("poll_interval_seconds"), "poll_interval_seconds")
        power_limit = get_required_dict(info.get("power_limit"), "power_limit")
        desired_limit = parse_int(power_limit.get("desired"), "power_limit.desired")
        confirmed_limit = parse_int(power_limit.get("confirmed"), "power_limit.confirmed")
        original_power_limit = confirmed_limit if confirmed_limit > 0 else desired_limit

        current_power = parse_float(info.get("power"), "power")
        ok("Current polling frequency", f"{original_poll_seconds} seconds")
        ok("Current power limit", f"desired={desired_limit}W confirmed={confirmed_limit}W queued={power_limit.get('queued')} reset_timer_minutes={power_limit.get('reset_timer_minutes')}")
        ok("Current inverter power", f"{current_power:.3f} W")

        restore_actions.append((
            f"Restore polling frequency to {original_poll_seconds} second(s)",
            lambda seconds=original_poll_seconds: set_polling(base_url, seconds, args.timeout),
        ))

        step("2. Set polling to 1 second")
        polling_response = set_polling(base_url, 1, args.timeout)
        ok("Polling changed", f"poll_interval_seconds={polling_response.get('poll_interval_seconds')} poll_interval_ms={polling_response.get('poll_interval_ms')}")

        step("3. Read the current power")
        info_after_poll = fetch_info(base_url, args.timeout)
        live_power = parse_float(info_after_poll.get("power"), "power")
        ok("Current inverter power", f"{live_power:.3f} W")

        target_power = max(0, int(round(live_power * 0.8)))
        if live_power > 0 and target_power == live_power:
            target_power = max(0, int(live_power) - 1)

        step("4. Limit the current power to 80%")
        print(f"Measured inverter power: {live_power:.3f}W -> target limit {target_power}W (80%)")
        power_set_started = time.monotonic()
        power_status, power_body, power_text = set_power(base_url, target_power, args.timeout)
        print(f"POST /api/power HTTP {power_status}")
        print(f"Response: {power_body if power_body else power_text}")
        if power_status not in (200, 202):
            raise RuntimeError(f"POST /api/power returned HTTP {power_status}")

        restore_actions.append((
            f"Restore power limit to {original_power_limit} W",
            lambda watts=original_power_limit: set_power(base_url, watts, args.timeout),
        ))

        probe_read_only_endpoints(base_url, args.timeout)

        elapsed = time.monotonic() - power_set_started
        remaining = args.wait_seconds - elapsed
        if remaining > 0:
            print(f"\nWaiting {remaining:.1f} more second(s) for the power setting to settle")
            time.sleep(remaining)

        step("6. Read delivered power and verify the limit")
        final_info = fetch_info(base_url, args.timeout)
        final_power = parse_float(final_info.get("power"), "power")
        final_limit = get_required_dict(final_info.get("power_limit"), "power_limit")
        final_desired = parse_int(final_limit.get("desired"), "power_limit.desired")
        final_confirmed = parse_int(final_limit.get("confirmed"), "power_limit.confirmed")
        final_queued = bool(final_limit.get("queued"))

        ok("Delivered power", f"{final_power:.3f} W")
        ok("Delivered power limit", f"desired={final_desired}W confirmed={final_confirmed}W queued={final_queued}")

        if final_confirmed == target_power:
            ok("Power limit check", f"confirmed limit matches target {target_power}W")
        else:
            fail("Power limit check", f"confirmed limit {final_confirmed}W does not match target {target_power}W")
            raise RuntimeError(f"power limit not delivered within wait window (target {target_power}W, confirmed {final_confirmed}W)")

        print(f"Actual power output is {final_power:.3f} W; limit setpoint is {final_confirmed} W")

        step("7. Reset the power to the original value")
        restore_with_report(
            f"Restore power limit to {original_power_limit} W",
            lambda watts=original_power_limit: set_power(base_url, watts, args.timeout),
        )
        restore_actions.pop()

        step("8. Reset the polling frequency to the original frequency")
        restore_with_report(
            f"Restore polling frequency to {original_poll_seconds} second(s)",
            lambda seconds=original_poll_seconds: set_polling(base_url, seconds, args.timeout),
        )
        restore_actions.pop()

        inspect_recent_bridge_logs(base_url, args.timeout)

        if original_debug_mode is not None:
            step("10. Restore debug mode to original value")
            restore_with_report(
                f"Restore debug_mode to {original_debug_mode}",
                lambda value=original_debug_mode: set_debug_mode(base_url, value, args.timeout),
            )
            # Pop the matching restore action so finally doesn't repeat it.
            for i in range(len(restore_actions) - 1, -1, -1):
                if restore_actions[i][0].startswith("Restore debug_mode"):
                    restore_actions.pop(i)
                    break

        print("\nTest completed successfully.")
    except Exception as exc:
        print(f"\nTEST FAILED: {exc}")
        return_code = 1
    finally:
        if restore_actions:
            print("\nCleanup: restoring original settings")
            while restore_actions:
                label, action = restore_actions.pop()
                restore_with_report(label, action)

        inspect_recent_bridge_logs(base_url, args.timeout)

    return return_code


if __name__ == "__main__":
    sys.exit(main())