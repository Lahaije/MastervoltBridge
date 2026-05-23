#!/usr/bin/env python3
"""Display all log entries from the bridge since boot.

This is a convenience script that fetches and displays all entries in
chronological order with human-readable timestamps. For analysis of WiFi
connection attempts, use analyze_bridge_logs.py instead.

Usage:
    .venv\\Scripts\\python skills/log-analysis/show_all.py
    .venv\\Scripts\\python skills/log-analysis/show_all.py --base-url http://192.168.1.48:8080
"""
import argparse
import json
import sys
import urllib.request
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from bridge_config import BRIDGE_BASE_URL  # noqa: E402


def format_ts(ms):
    """Format milliseconds as Xm SSs."""
    secs = ms // 1000
    mins = secs // 60
    secs = secs % 60
    return f"{mins:3d}m {secs:02d}s"


def main():
    parser = argparse.ArgumentParser(
        description="Fetch and display all bridge log entries since boot"
    )
    parser.add_argument(
        "--base-url",
        default=BRIDGE_BASE_URL,
        help=f"Bridge base URL (default: {BRIDGE_BASE_URL})",
    )
    parser.add_argument(
        "--timeout",
        type=float,
        default=60,
        help="HTTP timeout in seconds (default: 60)",
    )
    parser.add_argument(
        "--since-ms",
        type=int,
        default=None,
        help="Only show entries at/after timestamp_ms",
    )
    parser.add_argument(
        "--limit",
        type=int,
        default=None,
        help="Only show last N entries",
    )
    args = parser.parse_args()

    try:
        url = f"{args.base_url.rstrip('/')}/api/logs"
        with urllib.request.urlopen(url, timeout=args.timeout) as r:
            data = json.loads(r.read())
    except Exception as ex:
        print(f"Failed to fetch logs: {ex}", file=sys.stderr)
        return 1

    entries = data.get("entries", [])
    if not entries:
        print("No entries in log buffer")
        return 0

    if args.since_ms is not None:
        entries = [e for e in entries if int(e.get("timestamp_ms", 0)) >= args.since_ms]
    if args.limit is not None and args.limit > 0:
        entries = entries[-args.limit:]
    if not entries:
        print("No entries matched filters")
        return 0

    print(f"Total entries: {len(entries)}\n")
    print(
        f"Boot time:    {entries[0]['timestamp_ms']} ms "
        f"({entries[0]['timestamp_ms']/1000:.1f}s)"
    )
    print(
        f"Last entry:   {entries[-1]['timestamp_ms']} ms "
        f"({entries[-1]['timestamp_ms']/1000:.1f}s)"
    )
    uptime_s = (entries[-1]["timestamp_ms"] - entries[0]["timestamp_ms"]) / 1000
    print(f"Uptime:       {uptime_s:.1f}s ({uptime_s/60:.1f}m)\n")

    print("=== All Events ===\n")
    for entry in entries:
        ts_str = format_ts(entry["timestamp_ms"])
        msg = entry["message"]
        print(f"[{ts_str}] {msg}")

    return 0


if __name__ == "__main__":
    sys.exit(main())
