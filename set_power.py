import requests, time

from bridge_config import BRIDGE_BASE_URL

BASE = BRIDGE_BASE_URL

# Wait for natural recovery (up to 2 min)
for i in range(12):
    time.sleep(10)
    r = requests.get(f"{BASE}/api/health")
    h = r.json()
    connected = h["wifi_connected"]
    print(f"  [{(i+1)*10}s] wifi={connected}")
    if connected:
        # Try power set
        r2 = requests.post(f"{BASE}/api/power", json={"power": 400}, timeout=10)
        print(f"  Power set: {r2.status_code} {r2.text}")
        if r2.status_code == 200:
            # Wait 30s and check if power dropped
            print("  Waiting 30s to verify power change...")
            time.sleep(30)
            r3 = requests.get(f"{BASE}/api/info")
            info = r3.json()
            print(f"  Current power: {info['power']}W")
        break
else:
    print("WiFi did not recover in 120s")
