#!/usr/bin/env python3
"""
Analyze WiFi connection logs from the bridge.
Extract patterns, timing, success/failure rates, and channel behavior.
"""

import requests
import re
from collections import defaultdict

IP = "192.168.1.48"
PORT = 8080
BASE_URL = f"http://{IP}:{PORT}"

def get_logs():
    try:
        r = requests.get(f"{BASE_URL}/api/logs", timeout=10)
        r.raise_for_status()
        text = r.text
        # Handle truncated JSON by trimming to last complete entry
        try:
            return r.json().get('entries', [])
        except Exception:
            # Try to recover from truncated JSON
            last_brace = text.rfind('},')
            if last_brace > 0:
                text = text[:last_brace + 1] + ']}'
            return __import__('json').loads(text).get('entries', [])
    except Exception as e:
        print(f"ERROR fetching logs: {e}")
        return []

def parse_connection_attempts(entries):
    attempts = []
    current_attempt = None

    for entry in entries:
        msg = entry['message']

        if 'Measuring connection time' in msg:
            if current_attempt:
                attempts.append(current_attempt)
            current_attempt = {
                'timestamp_ms': entry['timestamp_ms'],
                'iterations': [],
                'result': None,
                'result_time': None,
                'channel': None,
                'bssid': None,
            }

        elif current_attempt and 'Iteration' in msg:
            match = re.search(r'Iteration (\d+) t=(\d+)ms status=(\d+) \((\w+)\)', msg)
            if match:
                current_attempt['iterations'].append({
                    'num': int(match.group(1)),
                    'time_ms': int(match.group(2)),
                    'status_code': int(match.group(3)),
                    'status_name': match.group(4)
                })

        elif current_attempt and 'Connected in' in msg:
            match = re.search(r'Connected in (\d+)ms.*channel=(\d+) bssid=([\w:]+)', msg)
            if match:
                current_attempt['result'] = 'SUCCESS'
                current_attempt['result_time'] = int(match.group(1))
                current_attempt['channel'] = int(match.group(2))
                current_attempt['bssid'] = match.group(3)

        elif current_attempt and 'Timeout after' in msg:
            current_attempt['result'] = 'TIMEOUT'
            current_attempt['result_time'] = 6000

    if current_attempt:
        attempts.append(current_attempt)

    return attempts

def analyze_attempts(attempts, label="ALL"):
    if not attempts:
        print("No connection attempts found in logs.")
        return

    print(f"\n{'='*90}")
    print(f"CONNECTION ATTEMPT ANALYSIS — {label}")
    print(f"{'='*90}")

    success_count = 0
    failure_count = 0
    times = []
    channels = defaultdict(int)

    for i, attempt in enumerate(attempts, 1):
        status = attempt['result']
        time_ms = attempt['result_time']
        channel = attempt['channel']

        status_str = status if status else "UNKNOWN"
        if status == 'SUCCESS':
            success_count += 1
            times.append(time_ms)
            channels[channel] += 1
            marker = "✓"
        else:
            failure_count += 1
            marker = "✗"

        final_state = attempt['iterations'][-1]['status_name'] if attempt['iterations'] else '?'

        states = []
        last_state = None
        for it in attempt['iterations']:
            if it['status_name'] != last_state:
                states.append(f"{it['status_name']}@{it['time_ms']}ms")
                last_state = it['status_name']

        ch_str = f"ch={channel}" if channel else "     "
        print(f"  {marker} Attempt {i:2d}: {status_str:9} {time_ms or 0:5d}ms  {ch_str}  [{' → '.join(states)}]")

    print(f"\n{'='*90}")
    print(f"  Total: {len(attempts)}  |  Success: {success_count} ({100*success_count/len(attempts):.0f}%)  |  Failure: {failure_count} ({100*failure_count/len(attempts):.0f}%)")
    if times:
        print(f"  Time:  min={min(times)}ms  max={max(times)}ms  avg={sum(times)/len(times):.0f}ms  median={sorted(times)[len(times)//2]}ms")
    if channels:
        ch_str = "  ".join(f"ch{ch}:{cnt}x" for ch, cnt in sorted(channels.items()))
        print(f"  Channels: {ch_str}")
    print(f"{'='*90}\n")

def main():
    print(f"\nFetching logs from {BASE_URL}...")
    entries = get_logs()
    print(f"Retrieved {len(entries)} log entries")

    attempts = parse_connection_attempts(entries)
    print(f"Parsed {len(attempts)} connection attempts")

    analyze_attempts(attempts)

if __name__ == "__main__":
    main()
