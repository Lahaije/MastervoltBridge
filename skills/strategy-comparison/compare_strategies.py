#!/usr/bin/env python3
"""
Strategy Comparison — dwell vs auto WiFi connect paths.

Fetches live logs from the bridge and produces a focused side-by-side
comparison of the two alternating connect strategies.

Usage (from repo root):
    .venv/Scripts/python skills/strategy-comparison/compare_strategies.py
    .venv/Scripts/python skills/strategy-comparison/compare_strategies.py --base-url http://192.168.1.48:8080
    .venv/Scripts/python skills/strategy-comparison/compare_strategies.py --save-json results/comparison.json
    .venv/Scripts/python skills/strategy-comparison/compare_strategies.py --min-samples 20

Exit code: 0 always (read-only; exit 1 only on fetch error).
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


# Minimum number of attempts per path before declaring a verdict reliable.
DEFAULT_MIN_SAMPLES = 10


# ---------------------------------------------------------------------------
# Data types
# ---------------------------------------------------------------------------

@dataclass
class Attempt:
    index: int
    timestamp_ms: int
    path: str                    # "dwell" | "auto" | "unknown"
    result: str                  # "SUCCESS" | "TIMEOUT" | "UNKNOWN"
    duration_ms: Optional[int]
    channel: Optional[int]
    bssid: Optional[str]


@dataclass
class PathStats:
    name: str
    attempts: List[Attempt] = field(default_factory=list)

    @property
    def count(self) -> int:
        return len(self.attempts)

    @property
    def successes(self) -> List[Attempt]:
        return [a for a in self.attempts if a.result == "SUCCESS"]

    @property
    def timeouts(self) -> List[Attempt]:
        return [a for a in self.attempts if a.result == "TIMEOUT"]

    @property
    def success_count(self) -> int:
        return len(self.successes)

    @property
    def timeout_count(self) -> int:
        return len(self.timeouts)

    @property
    def success_rate(self) -> float:
        return self.success_count / self.count * 100 if self.count else 0.0

    @property
    def times(self) -> List[int]:
        return [a.duration_ms for a in self.successes if a.duration_ms is not None]

    @property
    def mean_ms(self) -> Optional[float]:
        return statistics.mean(self.times) if self.times else None

    @property
    def median_ms(self) -> Optional[float]:
        return statistics.median(self.times) if self.times else None

    @property
    def min_ms(self) -> Optional[int]:
        return min(self.times) if self.times else None

    @property
    def max_ms(self) -> Optional[int]:
        return max(self.times) if self.times else None

    @property
    def p95_ms(self) -> Optional[float]:
        if len(self.times) < 2:
            return None
        sorted_t = sorted(self.times)
        idx = int(len(sorted_t) * 0.95)
        return sorted_t[min(idx, len(sorted_t) - 1)]

    @property
    def stdev_ms(self) -> Optional[float]:
        return statistics.stdev(self.times) if len(self.times) >= 2 else None

    @property
    def channels(self) -> Dict[int, int]:
        counts: Dict[int, int] = {}
        for a in self.successes:
            if a.channel is not None:
                counts[a.channel] = counts.get(a.channel, 0) + 1
        return counts


# ---------------------------------------------------------------------------
# Log fetch + parse
# ---------------------------------------------------------------------------

def fetch_logs(base_url: str, timeout: float) -> List[Dict]:
    url = f"{base_url.rstrip('/')}/api/logs"
    req = urllib.request.Request(url=url, method="GET")
    req.add_header("Accept", "application/json")
    with urllib.request.urlopen(req, timeout=timeout) as resp:
        raw = resp.read().decode("utf-8", errors="replace")
    payload = json.loads(raw)
    return payload.get("entries", []) if isinstance(payload, dict) else []


def parse_attempts(entries: List[Dict]) -> List[Attempt]:
    """Extract [WIFI-CONNECT] start/complete pairs from log entries."""
    attempts: List[Attempt] = []

    cur_ts: Optional[int] = None
    cur_path: Optional[str] = None
    cur_result = "UNKNOWN"
    cur_duration: Optional[int] = None
    cur_channel: Optional[int] = None
    cur_bssid: Optional[str] = None

    start_re = re.compile(r"\[WIFI-CONNECT\] start path=(\w+)")

    def flush() -> None:
        nonlocal cur_ts, cur_path, cur_result, cur_duration, cur_channel, cur_bssid
        if cur_ts is None:
            return
        attempts.append(Attempt(
            index=len(attempts) + 1,
            timestamp_ms=cur_ts,
            path=cur_path or "unknown",
            result=cur_result,
            duration_ms=cur_duration,
            channel=cur_channel,
            bssid=cur_bssid,
        ))
        cur_ts = cur_path = cur_duration = cur_channel = cur_bssid = None
        cur_result = "UNKNOWN"

    for e in entries:
        msg = str(e.get("message", ""))
        ts = int(e.get("timestamp_ms", 0))

        m = start_re.search(msg)
        if m:
            flush()
            cur_ts = ts
            cur_path = m.group(1)
            continue

        if cur_ts is None:
            continue

        if "[WIFI-CONNECT] complete" in msg:
            if m2 := re.search(r"duration_ms=(\d+)", msg):
                cur_duration = int(m2.group(1))
            if m2 := re.search(r"result=(\w+)", msg):
                cur_result = m2.group(1).upper()
            if m2 := re.search(r"channel=(\d+)", msg):
                cur_channel = int(m2.group(1))
            if m2 := re.search(r"bssid=([\w:]+)", msg):
                cur_bssid = m2.group(1)

    flush()
    return attempts


# ---------------------------------------------------------------------------
# Report
# ---------------------------------------------------------------------------

W = 14  # column width


def _fmt(val: Optional[float], unit: str = "ms", decimals: int = 0) -> str:
    if val is None:
        return "n/a".rjust(W)
    if decimals:
        return f"{val:.{decimals}f}{unit}".rjust(W)
    return f"{int(val)}{unit}".rjust(W)


def _row(label: str, a: str, b: str) -> str:
    return f"  {label:<22s}{a}{b}"


def _sep() -> str:
    return "  " + "-" * (22 + W * 2)


def verdict(dwell: PathStats, auto: PathStats, min_samples: int) -> str:
    lines = []

    low_sample = dwell.count < min_samples or auto.count < min_samples
    if low_sample:
        lines.append(
            f"  ! Sample size too small for reliable conclusions "
            f"(need {min_samples}+ per path; have dwell={dwell.count}, auto={auto.count})."
        )

    # Speed comparison
    if dwell.mean_ms is not None and auto.mean_ms is not None:
        diff = auto.mean_ms - dwell.mean_ms
        pct = abs(diff) / auto.mean_ms * 100 if auto.mean_ms else 0
        faster = "dwell" if diff > 0 else "auto"
        slower = "auto" if diff > 0 else "dwell"
        if abs(diff) < 200:
            lines.append("  Speed     : no meaningful difference (avg within 200 ms).")
        else:
            lines.append(
                f"  Speed     : {faster} is faster by {abs(diff):.0f} ms avg "
                f"({pct:.0f}% faster than {slower})."
            )

    # Reliability comparison
    rd, ra = dwell.success_rate, auto.success_rate
    if abs(rd - ra) < 3:
        lines.append(f"  Reliability: similar ({dwell.success_rate:.0f}% vs {auto.success_rate:.0f}%).")
    elif rd > ra:
        lines.append(f"  Reliability: dwell more reliable ({rd:.0f}% vs {ra:.0f}%).")
    else:
        lines.append(f"  Reliability: auto more reliable ({ra:.0f}% vs {rd:.0f}%).")

    # Recommendation
    dwell_score = 0
    if dwell.mean_ms is not None and auto.mean_ms is not None and dwell.mean_ms < auto.mean_ms - 200:
        dwell_score += 1
    if rd > ra + 2:
        dwell_score += 1
    auto_score = 0
    if auto.mean_ms is not None and dwell.mean_ms is not None and auto.mean_ms < dwell.mean_ms - 200:
        auto_score += 1
    if ra > rd + 2:
        auto_score += 1

    if dwell_score > auto_score:
        lines.append("  Verdict   : dwell appears to be the better strategy.")
    elif auto_score > dwell_score:
        lines.append("  Verdict   : auto appears to be the better strategy.")
    else:
        lines.append("  Verdict   : no clear winner yet — keep accumulating data.")

    if low_sample:
        lines.append(f"  Action    : continue running bridge to gather more samples.")

    return "\n".join(lines)


def print_report(dwell: PathStats, auto: PathStats, total_entries: int, min_samples: int) -> None:
    print()
    print("=" * (24 + W * 2))
    print("  WiFi Strategy Comparison: dwell vs auto")
    print("=" * (24 + W * 2))
    print(_row("", "dwell".rjust(W), "auto".rjust(W)))
    print(_sep())
    print(_row("Attempts",          str(dwell.count).rjust(W),   str(auto.count).rjust(W)))
    print(_row("Successes",         str(dwell.success_count).rjust(W), str(auto.success_count).rjust(W)))
    print(_row("Timeouts",          str(dwell.timeout_count).rjust(W), str(auto.timeout_count).rjust(W)))
    print(_row("Success rate",      f"{dwell.success_rate:.0f}%".rjust(W), f"{auto.success_rate:.0f}%".rjust(W)))
    print(_sep())
    print(_row("Min connect time",  _fmt(dwell.min_ms),   _fmt(auto.min_ms)))
    print(_row("Avg connect time",  _fmt(dwell.mean_ms),  _fmt(auto.mean_ms)))
    print(_row("Median connect",    _fmt(dwell.median_ms), _fmt(auto.median_ms)))
    print(_row("P95 connect time",  _fmt(dwell.p95_ms),   _fmt(auto.p95_ms)))
    print(_row("Max connect time",  _fmt(dwell.max_ms),   _fmt(auto.max_ms)))
    print(_row("Std deviation",     _fmt(dwell.stdev_ms), _fmt(auto.stdev_ms)))
    print(_sep())

    # Channel distributions
    all_channels = sorted(set(list(dwell.channels) + list(auto.channels)))
    for ch in all_channels:
        label = f"Channel {ch} hits"
        print(_row(label,
                   str(dwell.channels.get(ch, 0)).rjust(W),
                   str(auto.channels.get(ch, 0)).rjust(W)))

    print(_sep())
    print()
    print("  Verdict")
    print("  -------")
    print(verdict(dwell, auto, min_samples))
    print()
    print(f"  (Based on {total_entries} total log entries from bridge)")
    print("=" * (24 + W * 2))
    print()


def build_json_result(dwell: PathStats, auto: PathStats) -> dict:
    def stats_dict(s: PathStats) -> dict:
        return {
            "attempts": s.count,
            "success_count": s.success_count,
            "timeout_count": s.timeout_count,
            "success_rate_pct": round(s.success_rate, 1),
            "min_ms": s.min_ms,
            "avg_ms": round(s.mean_ms, 1) if s.mean_ms is not None else None,
            "median_ms": round(s.median_ms, 1) if s.median_ms is not None else None,
            "p95_ms": round(s.p95_ms, 1) if s.p95_ms is not None else None,
            "max_ms": s.max_ms,
            "stdev_ms": round(s.stdev_ms, 1) if s.stdev_ms is not None else None,
            "channels": {str(k): v for k, v in s.channels.items()},
        }
    return {"dwell": stats_dict(dwell), "auto": stats_dict(auto)}


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def main() -> int:
    parser = argparse.ArgumentParser(description="Compare dwell vs auto WiFi connect strategies")
    parser.add_argument("--base-url", default="http://192.168.1.48:8080")
    parser.add_argument("--timeout", type=float, default=10.0)
    parser.add_argument("--save-json", default=None, help="Save comparison result to JSON file")
    parser.add_argument("--min-samples", type=int, default=DEFAULT_MIN_SAMPLES,
                        help=f"Minimum attempts per path for a reliable verdict (default {DEFAULT_MIN_SAMPLES})")
    args = parser.parse_args()

    print(f"Fetching logs from {args.base_url} ...")
    try:
        entries = fetch_logs(args.base_url, args.timeout)
    except Exception as ex:
        print(f"ERROR: could not fetch logs: {ex}")
        return 1

    attempts = parse_attempts(entries)

    dwell = PathStats(name="dwell", attempts=[a for a in attempts if a.path == "dwell"])
    auto  = PathStats(name="auto",  attempts=[a for a in attempts if a.path == "auto"])

    if not dwell.count and not auto.count:
        print("No [WIFI-CONNECT] log entries found. Bridge may not have connected yet.")
        return 0

    print_report(dwell, auto, len(entries), args.min_samples)

    if args.save_json:
        out = Path(args.save_json)
        out.parent.mkdir(parents=True, exist_ok=True)
        with open(out, "w") as f:
            json.dump(build_json_result(dwell, auto), f, indent=2)
        print(f"Comparison saved to {out}")

    return 0


if __name__ == "__main__":
    sys.exit(main())
