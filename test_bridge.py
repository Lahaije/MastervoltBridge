import requests

IP = '192.168.1.48'
r = requests.get(f"http://{IP}:8080/", timeout=15)
for endpoint in r.json()['endpoints']:
    print(f"'{endpoint['path']}' , {endpoint['method']} : {endpoint['description']}")
duration =  r.elapsed.microseconds / 1000

"""

r = requests.get(f"http://{IP}:8080/api/health", timeout=15)
for key, value in r.json().items():
    print(f"{key}: {value}")


r = requests.get(f"http://{IP}:8080/api/info", timeout=15)
print(r.text)
"""

try:
    # r = requests.get(f"http://{IP}:8080/pulse", timeout=35)
    print(r.text)
except requests.exceptions.ReadTimeout:
    print("PULSE timeout")


r = requests.get(f"http://{IP}:8080/api/logs", timeout=5)
a = r.json()['entries']



try:
    for entry in r.json()['entries']:
        print(f"{entry['timestamp_ms']}: {entry['message']}")
except Exception as e:
    print(f"EXCEPTION: {e}")
    print(r.text)


print(f"Duration = {int(duration)} ms")

r = requests.get(f"http://{IP}:8080/api/info", timeout=15)
for key, value in r.json().items():
    print(f"{key}: {value}")


r = requests.post(f"http://{IP}:8080/api/inverter/fetch", data={'url': '/home'}, timeout=15)
print(r.text)