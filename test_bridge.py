import requests
from bridge_config import BRIDGE_BASE_URL

r = requests.get(f"{BRIDGE_BASE_URL}/", timeout=15)
for endpoint in r.json()['endpoints']:
    print(f"'{endpoint['path']}' , {endpoint['method']} : {endpoint['description']}")
duration =  r.elapsed.microseconds / 1000

"""

r = requests.get(f"{BRIDGE_BASE_URL}/api/health", timeout=15)
for key, value in r.json().items():
    print(f"{key}: {value}")


r = requests.get(f"{BRIDGE_BASE_URL}/api/info", timeout=15)
print(r.text)
"""

timer = ''
try:
    r = requests.get(f"{BRIDGE_BASE_URL}/api/logs", timeout=5)
    for entry in r.json()['entries']:
        timer = int(entry['timestamp_ms'])
        minutes = timer // 60000  # 1 minute = 60,000 ms
        seconds = (timer % 60000)/1000
        print(f"{minutes}m {seconds:06.3f}: {entry['message']}")
except Exception as e:
    print(f"EXCEPTION: {e}")
    print(r.text)


print(f"{timer} Duration = {int(duration)} ms")

r = requests.get(f"http://{IP}:8080/api/info", timeout=15)
for key, value in r.json().items():
    print(f"{key}: {value}")

"""
r = requests.post(f"http://{IP}:8080/api/inverter/fetch", json={'url': '/Mastervolt.js'}, timeout=15)
print(r.text)
"""