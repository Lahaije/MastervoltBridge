import requests
import sys

IP = '192.168.1.48'
BASE = f"http://{IP}:8080"
failed = False

# --- GET / (Web UI HTML) ---
print("=== GET / ===")
r = requests.get(f"{BASE}/", timeout=15)
assert r.status_code == 200, f"Expected 200, got {r.status_code}"
assert "text/html" in r.headers.get("Content-Type", ""), f"Expected text/html, got {r.headers.get('Content-Type')}"
assert "<!doctype html>" in r.text.lower(), "Response does not look like HTML"
assert "MQTT" in r.text, "Web UI should contain MQTT settings section"
print(f"  HTML page: {len(r.content)} bytes")
print()

# --- GET /api (discovery) ---
print("=== GET /api ===")
r = requests.get(f"{BASE}/api", timeout=15)
assert r.status_code == 200, f"Expected 200, got {r.status_code}"
endpoints = r.json()['endpoints']
for endpoint in endpoints:
    print(f"  {endpoint['method']:4s} {endpoint['path']} — {endpoint['description']}")
# Verify MQTT endpoints are listed
paths = [e['path'] for e in endpoints]
assert "/api/mqtt" in paths, "Missing /api/mqtt in endpoint discovery"
print()

# --- GET /api/health ---
print("=== GET /api/health ===")
r = requests.get(f"{BASE}/api/health", timeout=15)
assert r.status_code == 200, f"Expected 200, got {r.status_code}"
health = r.json()
for key, value in health.items():
    print(f"  {key}: {value}")
# Validate expected keys
for key in ["operating_status", "operating_mode", "error_alarm_code", "wifi_connected",
            "inverter_link_state", "last_update_ms", "last_inverter_status", "debug_mode"]:
    assert key in health, f"Missing key '{key}' in /api/health"
print()

# --- GET /api/device ---
print("=== GET /api/device ===")
r = requests.get(f"{BASE}/api/device", timeout=15)
assert r.status_code == 200, f"Expected 200, got {r.status_code}"
device = r.json()
for key, value in device.items():
    print(f"  {key}: {value}")
# Validate expected keys
for key in ["firmware_version", "inverter_model", "inverter_mac_address", "wifi_ssid",
            "wifi_ip", "ethernet_ip", "inverter_host"]:
    assert key in device, f"Missing key '{key}' in /api/device"
assert device["firmware_version"].startswith("0.1.0-"), f"Unexpected firmware version format: {device['firmware_version']}"
print()

# --- GET /api/info ---
print("=== GET /api/info ===")
r = requests.get(f"{BASE}/api/info", timeout=15)
assert r.status_code == 200, f"Expected 200, got {r.status_code}"
info = r.json()
for key, value in info.items():
    print(f"  {key}: {value}")
# Validate expected keys
for key in ["power", "failure_streak_s", "poll_interval_ms", "power_limit_watts",
            "shadow_enabled", "total_yield", "daily_yield"]:
    if key not in info:
        print(f"  FAIL: Missing key '{key}'")
        failed = True
print()

# --- GET /api/logs ---
print("=== GET /api/logs ===")
try:
    r = requests.get(f"{BASE}/api/logs", timeout=15)
    assert r.status_code == 200, f"Expected 200, got {r.status_code}"
    entries = r.json()['entries']
    print(f"  {len(entries)} log entries")
    # Show last 5 entries
    for entry in entries[-5:]:
        ts = int(entry['timestamp_ms'])
        minutes = ts // 60000
        seconds = (ts % 60000) / 1000
        print(f"  {minutes}m {seconds:06.3f}s: {entry['message']}")
except Exception as e:
    print(f"  EXCEPTION: {e}")
    failed = True
print()

# --- GET /api/mqtt ---
print("=== GET /api/mqtt ===")
r = requests.get(f"{BASE}/api/mqtt", timeout=15)
assert r.status_code == 200, f"Expected 200, got {r.status_code}"
mqtt = r.json()
for key, value in mqtt.items():
    print(f"  {key}: {value}")
# Validate expected keys
for key in ["broker_ip", "broker_port", "enabled", "topic_prefix", "connected"]:
    if key not in mqtt:
        print(f"  FAIL: Missing key '{key}' in /api/mqtt")
        failed = True
# Validate types
assert isinstance(mqtt.get("broker_port"), int), "broker_port should be int"
assert isinstance(mqtt.get("enabled"), bool), "enabled should be bool"
assert isinstance(mqtt.get("connected"), bool), "connected should be bool"
print()

# --- POST /api/mqtt (update settings) ---
print("=== POST /api/mqtt (read-modify-write) ===")
# Save current settings
original_mqtt = mqtt.copy()
# Test updating with same values (safe idempotent test)
r = requests.post(f"{BASE}/api/mqtt", json={
    "broker_ip": mqtt["broker_ip"],
    "broker_port": mqtt["broker_port"],
    "enabled": mqtt["enabled"],
    "topic_prefix": mqtt["topic_prefix"]
}, timeout=15)
assert r.status_code == 200, f"Expected 200, got {r.status_code}"
updated = r.json()
assert updated["broker_ip"] == mqtt["broker_ip"], "broker_ip mismatch after update"
assert updated["broker_port"] == mqtt["broker_port"], "broker_port mismatch after update"
assert updated["enabled"] == mqtt["enabled"], "enabled mismatch after update"
assert updated["topic_prefix"] == mqtt["topic_prefix"], "topic_prefix mismatch after update"
print(f"  Settings saved successfully (idempotent write)")
print()

# --- POST /api/mqtt (validation tests) ---
print("=== POST /api/mqtt (validation) ===")
# Invalid IP
r = requests.post(f"{BASE}/api/mqtt", json={"broker_ip": "not-an-ip"}, timeout=15)
assert r.status_code == 400, f"Expected 400 for invalid IP, got {r.status_code}"
print(f"  Invalid IP rejected: {r.json().get('error', '')}")

# Invalid port
r = requests.post(f"{BASE}/api/mqtt", json={"broker_port": 99999}, timeout=15)
assert r.status_code == 400, f"Expected 400 for invalid port, got {r.status_code}"
print(f"  Invalid port rejected: {r.json().get('error', '')}")

# Topic prefix too long
r = requests.post(f"{BASE}/api/mqtt", json={"topic_prefix": "x" * 65}, timeout=15)
assert r.status_code == 400, f"Expected 400 for long prefix, got {r.status_code}"
print(f"  Long prefix rejected: {r.json().get('error', '')}")
print()

# --- Summary ---
if failed:
    print("RESULT: Some checks FAILED")
    sys.exit(1)
else:
    print("RESULT: All checks passed")
    sys.exit(0)