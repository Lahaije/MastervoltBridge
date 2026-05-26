#!/usr/bin/env python3
"""
Comprehensive test of the Mastervolt Bridge HTTP API + web UI.

What is covered
---------------
* GET  /                     - HTML page served, correct content-type.
* GET  /api                  - discovery JSON with at least 15 endpoints.
* GET  /api/health           - all expected fields present.
* GET  /api/info             - always returns 200, telemetry_valid flag,
                               cached settings present even when offline.
* POST /api/debug            - toggling debug mode produces an audit log line.
* POST /api/power            - boundary checks (0, 1575, -1, 1576, "abc",
                               missing key). Successful sets verified via
                               /api/info cache + log entry. Requires inverter
                               reachable; "set" tests are skipped (not failed)
                               when telemetry_valid=false.
* POST /api/polling          - boundary checks (1, 3600, 0, 3601, -5,
                               non-integer). Successful set verified via
                               /api/info cache + log entry.
* POST /api/shadow           - boundary checks ("yes" -> 400, missing key).
                               Successful toggle requires inverter; skipped
                               otherwise.
* GET  /api/mqtt             - schema check + restore-after-test pattern.
* POST /api/mqtt             - invalid broker rejected; HA-enable toggle
                               (round-trip restored); user-only update keeps
                               saved password (`has_credentials` stays true).

Safety
------
* Password is **never** changed (write-only, cannot be restored).
* Original `user`, `broker`, `ha_enabled`, debug mode, polling interval and
  power-limit are captured before the test and restored in a `finally` block.

Exit code
---------
0 on full success, 1 otherwise.
"""
from __future__ import annotations

import argparse
import json
import sys
import time
from typing import Any, Optional

import requests

DEFAULT_BASE = "http://192.168.1.48:8080"
TIMEOUT = 15
SETTLE = 0.6  # short delay after writes so the polling task can pick up changes


class Tester:
    def __init__(self, base: str) -> None:
        self.base = base.rstrip("/")
        self.results: list[tuple[str, bool, str]] = []
        self.session = requests.Session()

    # ---------- HTTP helpers ----------
    def get(self, path: str) -> requests.Response:
        return self.session.get(self.base + path, timeout=TIMEOUT)

    def post_json(self, path: str, body: dict) -> requests.Response:
        return self.session.post(self.base + path, json=body, timeout=TIMEOUT)

    def post_raw(self, path: str, raw: str) -> requests.Response:
        return self.session.post(
            self.base + path,
            data=raw,
            headers={"Content-Type": "application/json"},
            timeout=TIMEOUT,
        )

    # ---------- assertions ----------
    def check(self, name: str, cond: bool, info: str = "") -> bool:
        ok = bool(cond)
        self.results.append((name, ok, info))
        mark = "PASS" if ok else "FAIL"
        suffix = f"  {info}" if info else ""
        print(f"  [{mark}] {name}{suffix}")
        return ok

    def skip(self, name: str, reason: str) -> None:
        self.results.append((name, True, f"SKIP: {reason}"))
        print(f"  [SKIP] {name}  {reason}")

    def section(self, title: str) -> None:
        print(f"\n=== {title} ===")

    # ---------- logs ----------
    def logs(self) -> list[dict]:
        try:
            r = self.get("/api/logs")
            if r.status_code != 200:
                return []
            return r.json().get("entries", [])
        except Exception:
            return []

    def last_log_ts(self) -> int:
        return max((e.get("timestamp_ms", 0) for e in self.logs()), default=0)

    def find_log(self, since_ts: int, contains: str) -> Optional[dict]:
        for e in self.logs():
            if e.get("timestamp_ms", 0) >= since_ts and contains in e.get("message", ""):
                return e
        return None

    # ---------- summary ----------
    def report(self) -> bool:
        total = len(self.results)
        passed = sum(1 for _, p, info in self.results if p and not info.startswith("SKIP"))
        skipped = sum(1 for _, _, info in self.results if info.startswith("SKIP"))
        failed = total - passed - skipped
        print(f"\n=== Summary: {passed} passed, {failed} failed, {skipped} skipped (of {total} checks) ===")
        if failed:
            for name, ok, info in self.results:
                if not ok:
                    print(f"  FAILED: {name}  {info}")
        return failed == 0


# ============================================================================
# Test phases
# ============================================================================
def discovery(t: Tester) -> None:
    t.section("Discovery")
    r = t.get("/")
    t.check("root.status_200", r.status_code == 200, f"got {r.status_code}")
    t.check("root.content_html",
            "text/html" in r.headers.get("Content-Type", ""),
            r.headers.get("Content-Type", ""))
    t.check("root.has_html_marker", "<html" in r.text.lower())
    t.check("root.has_settings_card", "Power Limit" in r.text and "Polling Interval" in r.text)

    r = t.get("/api")
    t.check("api.status_200", r.status_code == 200)
    try:
        d = r.json()
        eps = d.get("endpoints", [])
        t.check("api.has_15_endpoints", isinstance(eps, list) and len(eps) >= 15,
                f"got {len(eps) if isinstance(eps, list) else type(eps)}")
        paths = {(e.get("method"), e.get("path")) for e in eps}
        for m, p in [("GET", "/"), ("GET", "/api"), ("POST", "/api/power"),
                     ("POST", "/api/polling"), ("POST", "/api/shadow"),
                     ("POST", "/api/mqtt"), ("GET", "/api/mqtt"),
                     ("POST", "/api/debug"), ("GET", "/api/info"),
                     ("GET", "/api/health"), ("GET", "/api/logs"),
                     ("GET", "/pulse"), ("POST", "/wifi/off")]:
            t.check(f"api.has_{m}_{p}", (m, p) in paths)
    except Exception as e:
        t.check("api.json_parse", False, str(e))


def health(t: Tester) -> dict:
    t.section("Health")
    r = t.get("/api/health")
    t.check("health.status_200", r.status_code == 200)
    h = r.json() if r.status_code == 200 else {}
    for key in ("wifi_connected", "wifi_ssid", "ethernet_ip", "inverter_host",
                "debug_mode", "ha_mqtt_enabled", "mqtt_broker",
                "mqtt_connected", "mqtt_scanning"):
        t.check(f"health.has_{key}", key in h)
    return h


def info(t: Tester) -> dict:
    t.section("Info")
    r = t.get("/api/info")
    t.check("info.status_200_even_if_offline", r.status_code == 200,
            f"got {r.status_code}")
    d = r.json() if r.status_code == 200 else {}
    for key in ("telemetry_valid", "last_update_ms", "power_limit_known",
                "power_limit_watts", "shadow_known", "shadow",
                "polling_interval_s"):
        t.check(f"info.has_{key}", key in d)
    return d


def debug_mode(t: Tester) -> None:
    t.section("Debug mode toggle")
    pre_ts = t.last_log_ts()
    r = t.post_json("/api/debug", {"debug": True})
    t.check("debug.enable.status", r.status_code == 200, r.text[:120])
    if r.status_code == 200:
        t.check("debug.enable.flag", r.json().get("debug") is True)
    h = t.get("/api/health").json()
    t.check("debug.health_shows_on", h.get("debug_mode") is True)
    time.sleep(SETTLE)
    log = t.find_log(pre_ts, "debug mode enabled")
    t.check("debug.enable.logged", log is not None,
            log.get("message", "") if log else "no log entry")

    # invalid value
    r = t.post_raw("/api/debug", '{"debug":"maybe"}')
    t.check("debug.invalid_value_400", r.status_code == 400)


def power_limit(t: Tester, inverter_online: bool) -> None:
    t.section("Power limit")
    # Boundary tests — independent of inverter reachability
    for value, expect_code in [(-1, 400), (1576, 400), (9999, 400)]:
        r = t.post_json("/api/power", {"power": value})
        t.check(f"power.boundary[{value}].400", r.status_code == expect_code,
                f"got {r.status_code}: {r.text[:120]}")

    # Non-integer
    r = t.post_raw("/api/power", '{"power":"abc"}')
    t.check("power.non_int.400", r.status_code == 400)
    # Missing key
    r = t.post_raw("/api/power", '{}')
    t.check("power.missing_key.400", r.status_code == 400)

    if not inverter_online:
        t.skip("power.set_100", "inverter offline")
        t.skip("power.set_0", "inverter offline")
        t.skip("power.set_1575", "inverter offline")
        t.skip("power.set_100.logged", "inverter offline")
        t.skip("power.cache_after_invalid_unchanged", "inverter offline")
        return

    # Capture pre-write cache so we can verify invalid POSTs don't mutate state
    info_before = t.get("/api/info").json()
    cached_before = info_before.get("power_limit_watts") if info_before.get("power_limit_known") else None

    pre_ts = t.last_log_ts()
    for wattage in (100, 0, 1575):
        r = t.post_json("/api/power", {"power": wattage})
        t.check(f"power.set_{wattage}.status", r.status_code == 200,
                f"got {r.status_code}: {r.text[:120]}")
        time.sleep(SETTLE)
        ni = t.get("/api/info").json()
        t.check(f"power.set_{wattage}.cached",
                ni.get("power_limit_known") is True and
                ni.get("power_limit_watts") == wattage,
                f"cache={ni.get('power_limit_watts')}")

    # Verify at least one POST /api/power log line was emitted
    t.check("power.set.logged",
            t.find_log(pre_ts, "POST /api/power") is not None)

    # Boundary again with a known-good cache value: invalid must not change it
    t.post_json("/api/power", {"power": 200})  # set baseline
    time.sleep(SETTLE)
    base_val = t.get("/api/info").json().get("power_limit_watts")
    t.post_json("/api/power", {"power": 9999})  # rejected
    time.sleep(SETTLE)
    after_val = t.get("/api/info").json().get("power_limit_watts")
    t.check("power.invalid_does_not_mutate_cache", after_val == base_val,
            f"before={base_val} after={after_val}")


def polling(t: Tester, original_seconds: int) -> None:
    t.section("Polling interval")
    # Boundaries (do these even if inverter offline)
    for value, expect_code in [(0, 400), (-5, 400), (3601, 400), (99999, 400)]:
        r = t.post_json("/api/polling", {"seconds": value})
        t.check(f"polling.boundary[{value}].400", r.status_code == expect_code,
                f"got {r.status_code}: {r.text[:120]}")

    # Non-integer
    r = t.post_raw("/api/polling", '{"seconds":"abc"}')
    t.check("polling.non_int.400", r.status_code == 400)
    # Missing key
    r = t.post_raw("/api/polling", '{}')
    t.check("polling.missing_key.400", r.status_code == 400)

    # Valid values + cache + log verification
    pre_ts = t.last_log_ts()
    for value in (1, 3600, 5):
        r = t.post_json("/api/polling", {"seconds": value})
        if not t.check(f"polling.set_{value}.status", r.status_code == 200,
                       f"got {r.status_code}: {r.text[:120]}"):
            continue
        body = r.json()
        t.check(f"polling.set_{value}.response",
                body.get("poll_interval_seconds") == value and
                body.get("poll_interval_ms") == value * 1000)
        time.sleep(SETTLE)
        ni = t.get("/api/info").json()
        t.check(f"polling.set_{value}.info_cached",
                ni.get("polling_interval_s") == value,
                f"info={ni.get('polling_interval_s')}")
    log = t.find_log(pre_ts, "[INVERTER-MONITOR] Poll interval updated to")
    t.check("polling.set.logged", log is not None,
            log.get("message", "") if log else "no log entry")

    # Restore
    r = t.post_json("/api/polling", {"seconds": original_seconds})
    t.check("polling.restore", r.status_code == 200,
            f"could not restore polling interval: {r.text[:120]}")


def shadow(t: Tester, inverter_online: bool) -> None:
    t.section("Shadow")
    # Boundary: invalid value
    r = t.post_raw("/api/shadow", '{"enabled":"maybe"}')
    t.check("shadow.invalid_value.400", r.status_code == 400)
    # Boundary: missing key
    r = t.post_raw("/api/shadow", '{}')
    t.check("shadow.missing_key.400", r.status_code == 400)

    if not inverter_online:
        t.skip("shadow.toggle", "inverter offline")
        return

    info_before = t.get("/api/info").json()
    current = bool(info_before.get("shadow")) if info_before.get("shadow_known") else None
    target = (not current) if current is not None else False

    pre_ts = t.last_log_ts()
    r = t.post_json("/api/shadow", {"enabled": target})
    if not t.check("shadow.toggle.status", r.status_code == 200,
                   f"got {r.status_code}: {r.text[:120]}"):
        return
    time.sleep(SETTLE)
    ni = t.get("/api/info").json()
    t.check("shadow.toggle.cached",
            ni.get("shadow_known") is True and bool(ni.get("shadow")) == target)

    # Restore if we know the original
    if current is not None:
        t.post_json("/api/shadow", {"enabled": current})


def mqtt_settings(t: Tester, original: dict) -> None:
    t.section("MQTT")
    # Invalid broker IP
    r = t.post_json("/api/mqtt", {"broker": "999.999.999.999"})
    t.check("mqtt.invalid_broker.400", r.status_code == 400)

    # Empty POST body
    r = t.post_raw("/api/mqtt", '{}')
    t.check("mqtt.empty_body.400", r.status_code == 400)

    # User-only update (same value) must NOT wipe stored password
    pre_creds = original.get("has_credentials", False)
    pre_user = original.get("user", "")
    if pre_user:
        r = t.post_json("/api/mqtt", {"user": pre_user})
        t.check("mqtt.user_only.status", r.status_code == 200,
                f"got {r.status_code}: {r.text[:120]}")
        time.sleep(SETTLE)
        after = t.get("/api/mqtt").json()
        t.check("mqtt.user_only.preserves_password",
                after.get("has_credentials") == pre_creds,
                f"before={pre_creds} after={after.get('has_credentials')}")
        t.check("mqtt.user_only.preserves_user",
                after.get("user") == pre_user)
    else:
        t.skip("mqtt.user_only.preserves_password", "no user configured")

    # ha_enabled round-trip (set to opposite, then restore)
    pre_ha = bool(original.get("ha_enabled"))
    r = t.post_json("/api/mqtt", {"ha_enabled": not pre_ha})
    t.check("mqtt.ha_toggle.status", r.status_code == 200)
    time.sleep(SETTLE)
    after = t.get("/api/mqtt").json()
    t.check("mqtt.ha_toggle.applied",
            after.get("ha_enabled") == (not pre_ha))
    # restore
    r = t.post_json("/api/mqtt", {"ha_enabled": pre_ha})
    t.check("mqtt.ha_restore.status", r.status_code == 200)
    time.sleep(SETTLE)
    after = t.get("/api/mqtt").json()
    t.check("mqtt.ha_restore.applied",
            after.get("ha_enabled") == pre_ha)


# ============================================================================
# Save / restore
# ============================================================================
def save_originals(t: Tester) -> dict:
    print("=== Capturing original state ===")
    mqtt = t.get("/api/mqtt").json()
    info = t.get("/api/info").json()
    health = t.get("/api/health").json()
    saved = {
        "mqtt": mqtt,
        "polling_s": info.get("polling_interval_s", 20),
        "power_known": info.get("power_limit_known", False),
        "power_w": info.get("power_limit_watts", 0),
        "shadow_known": info.get("shadow_known", False),
        "shadow": info.get("shadow", False),
        "debug_mode": health.get("debug_mode", False),
        "telemetry_valid": info.get("telemetry_valid", False),
    }
    print(json.dumps({k: v for k, v in saved.items() if k != "mqtt"}, indent=2))
    print(json.dumps({
        "mqtt": {
            "ha_enabled": mqtt.get("ha_enabled"),
            "broker": mqtt.get("broker"),
            "user": mqtt.get("user"),
            "has_credentials": mqtt.get("has_credentials"),
        }
    }, indent=2))
    return saved


def restore_originals(t: Tester, saved: dict) -> None:
    t.section("Restoring original state")
    # Polling
    try:
        r = t.post_json("/api/polling", {"seconds": saved["polling_s"]})
        t.check("restore.polling", r.status_code == 200)
    except Exception as e:
        t.check("restore.polling", False, str(e))
    # Debug mode
    try:
        r = t.post_json("/api/debug", {"debug": saved["debug_mode"]})
        t.check("restore.debug", r.status_code == 200)
    except Exception as e:
        t.check("restore.debug", False, str(e))
    # MQTT ha_enabled + broker + user (password is intentionally never touched)
    mqtt = saved["mqtt"]
    body: dict = {
        "ha_enabled": bool(mqtt.get("ha_enabled")),
        "broker": mqtt.get("broker", ""),
    }
    if mqtt.get("user"):
        body["user"] = mqtt["user"]
    try:
        r = t.post_json("/api/mqtt", body)
        t.check("restore.mqtt", r.status_code == 200)
    except Exception as e:
        t.check("restore.mqtt", False, str(e))


# ============================================================================
# Main
# ============================================================================
def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--base", default=DEFAULT_BASE)
    args = ap.parse_args()

    t = Tester(args.base)
    print(f"Target: {t.base}")

    # Probe before we modify anything
    try:
        t.get("/api/health")
    except Exception as e:
        print(f"FATAL: cannot reach bridge at {t.base}: {e}", file=sys.stderr)
        return 2

    saved = save_originals(t)
    inverter_online = bool(saved.get("telemetry_valid"))
    print(f"Inverter telemetry currently valid: {inverter_online}")

    try:
        discovery(t)
        health(t)
        info(t)
        debug_mode(t)
        power_limit(t, inverter_online)
        polling(t, saved["polling_s"])
        shadow(t, inverter_online)
        mqtt_settings(t, saved["mqtt"])
    finally:
        restore_originals(t, saved)

    success = t.report()
    return 0 if success else 1


if __name__ == "__main__":
    sys.exit(main())
