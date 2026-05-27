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
print(f"  HTML page: {len(r.content)} bytes")
print()

# --- GET /api (discovery) ---
print("=== GET /api ===")
r = requests.get(f"{BASE}/api", timeout=15)
assert r.status_code == 200, f"Expected 200, got {r.status_code}"
for endpoint in r.json()['endpoints']:
    print(f"  {endpoint['method']:4s} {endpoint['path']} — {endpoint['description']}")
print()

# --- GET /api/health ---
print("=== GET /api/health ===")
r = requests.get(f"{BASE}/api/health", timeout=15)
assert r.status_code == 200, f"Expected 200, got {r.status_code}"
health = r.json()
for key, value in health.items():
    print(f"  {key}: {value}")
# Validate expected keys
for key in ["wifi_connected", "wifi_ssid", "wifi_ip", "ethernet_ip", "inverter_host", "last_inverter_status", "debug_mode"]:
    assert key in health, f"Missing key '{key}' in /api/health"
print()

# --- GET /api/info ---
print("=== GET /api/info ===")
r = requests.get(f"{BASE}/api/info", timeout=15)
assert r.status_code == 200, f"Expected 200, got {r.status_code}"
info = r.json()
for key, value in info.items():
    print(f"  {key}: {value}")
# Validate expected keys
for key in ["last_update_ms", "operating_status", "power", "total_yield", "daily_yield",
            "inverter_link_state", "failure_streak_s"]:
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

# --- Summary ---
if failed:
    print("RESULT: Some checks FAILED")
    sys.exit(1)
else:
    print("RESULT: All checks passed")
    sys.exit(0)