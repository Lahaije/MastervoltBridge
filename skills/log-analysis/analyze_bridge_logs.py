#!/usr/bin/env python3
"""
Fetch and analyze ESP bridge logs from /api/logs.

This script is read-only: it does not trigger /pulse or /wifi/off.
It extracts [WIFI-CONNECT] start/complete pairs and prints per-path
(dwell vs auto) reliability and timing summaries.

Log format expected (produced by wifi_bridge.cpp):
  [WIFI-CONNECT] start path=<dwell|auto> scan_dwell_ms=<N> hint_fallback=<0|1>
  [WIFI-CONNECT] complete path=<dwell|auto> duration_ms=<N> result=<success|timeout> ...

Timestamp format used throughout: Xm SS.sss  (e.g. 4m 05.123)
This matches the format produced by format_ms() and is the preferred way
to refer to log entries when communicating about timestamps.
"""

from __future__ import annotations

import argparse
import json
import re
import statistics
import sys
import urllib.request
from dataclasses import dataclass, field
from pathlib import Path
from typing import Dict, List, Optional


def format_ms(ms: int) -> str:
    """Convert a millisecond uptime value to a human-readable 'Xm SS.sss' string.

    Examples:
        format_ms(4105234)  ->  '68m 25.234'
        format_ms(65000)    ->   '1m 05.000'
    """
    minutes = ms // 60_000
    seconds = (ms % 60_000) / 1000
    return f"{minutes}m {seconds:06.3f}"


@dataclass
class Attempt:
    index: int
    timestamp_ms: int
    path: Optional[str]          # "dwell" | "auto"
    result: str                  # SUCCESS | TIMEOUT | UNKNOWN
    result_time_ms: Optional[int]
    channel: Optional[int]
    bssid: Optional[str]


def fetch_logs(base_url: str, timeout: float) -> Dict:
    url = f"{base_url.rstrip('/')}/api/logs"
    req = urllib.request.Request(url=url, method="GET")
    req.add_header("Accept", "application/json")
    with urllib.request.urlopen(req, timeout=timeout) as resp:
        raw = resp.read().decode("utf-8", errors="replace")
    return json.loads(raw)


def parse_attempts(entries: List[Dict]) -> List[Attempt]:
    """Parse [WIFI-CONNECT] start/complete pairs from log entries."""
    attempts: List[Attempt] = []

    current_start_ts: Optional[int] = None
    current_path: Optional[str] = None
    current_result = "UNKNOWN"
    current_time: Optional[int] = None
    current_channel: Optional[int] = None
    current_bssid: Optional[str] = None

    start_re = re.compile(r"\[WIFI-CONNECT\] start path=(\w+)")

    def flush_current() -> None:
        nonlocal current_start_ts, current_path, current_result
        nonlocal current_time, current_channel, current_bssid
        if current_start_ts is None:
            return
        attempts.append(
            Attempt(
                index=len(attempts) + 1,
                timestamp_ms=current_start_ts,
                path=current_path,
                result=current_result,
                result_time_ms=current_time,
                channel=current_channel,
                bssid=current_bssid,
            )
        )
        current_start_ts = None
        current_path = None
        current_result = "UNKNOWN"
        current_time = None
        current_channel = None
        current_bssid = None

    for e in entries:
        msg = str(e.get("message", ""))
        ts = int(e.get("timestamp_ms", 0))

        m_start = start_re.search(msg)
        if m_start:
            flush_current()
            current_start_ts = ts
            current_path = m_start.group(1)
            continue

        if current_start_ts is None:
            continue

        if "[WIFI-CONNECT] complete" in msg:
            duration_m = re.search(r"duration_ms=(\d+)", msg)
            result_m = re.search(r"result=(\w+)", msg)
            channel_m = re.search(r"channel=(\d+)", msg)
            bssid_m = re.search(r"bssid=([\w:]+)", msg)
            if duration_m:
                current_time = int(duration_m.group(1))
            if result_m:
                current_result = result_m.group(1).upper()
            if channel_m:
                current_channel = int(channel_m.group(1))
            if bssid_m:
                current_bssid = bssid_m.group(1)

    flush_current()
    return attempts


def _path_stats(label: str, subset: List[Attempt]) -> None:
    """Print timing and channel stats for a named subset of attempts."""
    if not subset:
        return
    success = [a for a in subset if a.result == "SUCCESS"]
    pct = len(success) / len(subset) * 100
    print(f"\n  {label} ({len(subset)} attempts, {pct:.0f}% success)")
    times = [a.result_time_ms for a in success if a.result_time_ms is not None]
    if times:
        print(
            f"    timing: min={min(times)}ms avg={statistics.mean(times):.0f}ms "
            f"max={max(times)}ms median={statistics.median(times):.0f}ms"
        )
    channels: Dict[int, int] = {}
    for a in success:
        if a.channel is not None:
            channels[a.channel] = channels.get(a.channel, 0) + 1
    if channels:
        ch_line = " ".join(f"ch{ch}:{cnt}x" for ch, cnt in sorted(channels.items()))
        print(f"    channels: {ch_line}")


def summarize(attempts: List[Attempt]) -> None:
    if not attempts:
        print("No [WIFI-CONNECT] attempts found in selected log entries.")
        return

    success = [a for a in attempts if a.result == "SUCCESS"]
    failures = [a for a in attempts if a.result != "SUCCESS"]

    print("\nConnection Analysis")
    print("-------------------")
    print(f"Attempts : {len(attempts)}")
    print(f"Success  : {len(success)} ({len(success) / len(attempts) * 100:.0f}%)")
    print(f"Failure  : {len(failures)} ({len(failures) / len(attempts) * 100:.0f}%)")

    times = [a.result_time_ms for a in success if a.result_time_ms is not None]
    if times:
        print(
            f"Overall timing: min={min(times)}ms avg={statistics.mean(times):.0f}ms "
            f"max={max(times)}ms median={statistics.median(times):.0f}ms"
        )

    # Per-path breakdown
    paths = sorted({a.path for a in attempts if a.path is not None})
    if len(paths) > 1 or (len(paths) == 1 and paths[0] not in (None,)):
        print("\nPer-path breakdown:")
        for p in paths:
            _path_stats(p, [a for a in attempts if a.path == p])

    print("\nPer-attempt detail")
    for a in attempts:
        path_tag = f"[{a.path or '?':5s}]"
        ch = f"ch={a.channel}" if a.channel is not None else "ch=?"
        tm = f"{a.result_time_ms}ms" if a.result_time_ms is not None else "?ms"
        ts = format_ms(a.timestamp_ms)
        print(f"  #{a.index:02d} {path_tag} {ts}  {a.result:7s}  {tm:>6s}  {ch}")


def main() -> int:
    parser = argparse.ArgumentParser(description="Acquire and analyze bridge logs from /api/logs")
    parser.add_argument("--base-url", default="http://192.168.1.48:8080", help="Bridge base URL")
    parser.add_argument("--timeout", type=float, default=10.0, help="HTTP timeout seconds")
    parser.add_argument("--save-json", default=None, help="Optional path to save fetched logs JSON")
    parser.add_argument("--since-ms", type=int, default=None, help="Only analyze entries at/after timestamp_ms")
    parser.add_argument("--limit", type=int, default=None, help="Only analyze last N entries")
    parser.add_argument("--print-all", action="store_true", help="Print all log entries with formatted timestamps before the analysis")
    args = parser.parse_args()

    try:
        payload = fetch_logs(args.base_url, args.timeout)
    except Exception as ex:
        print(f"Failed to fetch logs: {ex}")
        return 1

    entries = payload.get("entries", []) if isinstance(payload, dict) else []
    total_entries = len(entries)

    if args.since_ms is not None:
        entries = [e for e in entries if int(e.get("timestamp_ms", 0)) >= args.since_ms]
    if args.limit is not None and args.limit > 0:
        entries = entries[-args.limit:]

    print(f"Fetched entries : {total_entries}")
    print(f"Entries analyzed: {len(entries)}")

    if args.print_all:
        print("\nAll log entries")
        print("---------------")
        for e in entries:
            ts = int(e.get("timestamp_ms", 0))
            msg = e.get("message", "")
            print(f"  {format_ms(ts)}  {msg}")
        print()

    if args.save_json:
        out_path = Path(args.save_json)
        out_path.parent.mkdir(parents=True, exist_ok=True)
        out_path.write_text(json.dumps(payload, indent=2), encoding="utf-8")
        print(f"Saved raw logs to: {out_path}")

    attempts = parse_attempts(entries)
    summarize(attempts)
    return 0


if __name__ == "__main__":
    sys.exit(main())
