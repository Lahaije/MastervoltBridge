#!/usr/bin/env python3
"""
Fetch and analyze ESP bridge logs from /api/logs.

This script is read-only: it does not trigger /pulse or /wifi/off.
It extracts [WIFI-CONNECT] start/complete pairs and prints per-path
(dwell vs auto) reliability and timing summaries, and a session summary
including power readings and disconnection episode breakdown.

Log format expected (produced by wifi_bridge.cpp):
  [WIFI-CONNECT] start path=<dwell|auto> scan_dwell_ms=<N> hint_fallback=<0|1>
  [WIFI-CONNECT] complete path=<dwell|auto> duration_ms=<N> result=<success|timeout> ...
  [INVERTER-CONTROLLER] Poll #N: Status=X Power=Y.ZW
  [INVERTER-CONTROLLER] No WiFi connection; skipping poll iteration
  [INVERTER-CONTROLLER] Inverter recovered; resuming normal poll interval

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
from typing import Dict, List, Optional, Tuple


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


@dataclass
class PollEntry:
    timestamp_ms: int
    poll_number: int
    status: str
    power_w: float


@dataclass
class BackoffEvent:
    timestamp_ms: int
    interval_seconds: int


@dataclass
class StateChangeEvent:
    timestamp_ms: int
    from_state: str
    to_state: str
    streak_seconds: Optional[int] = None
    interval_seconds: Optional[int] = None


@dataclass
class Episode:
    """A disconnection episode: consecutive reconnect attempts with no normal poll between them."""
    number: int
    attempts: List[Attempt] = field(default_factory=list)

    @property
    def resolved(self) -> bool:
        return any(a.result == "SUCCESS" for a in self.attempts)

    @property
    def retries_before_success(self) -> int:
        """Number of failed attempts before the successful one (0 = first try succeeded)."""
        for i, a in enumerate(self.attempts):
            if a.result == "SUCCESS":
                return i
        return len(self.attempts)

    @property
    def recovery_duration_ms(self) -> Optional[int]:
        """Elapsed ms from start of first attempt to completion of the successful one."""
        success = next((a for a in self.attempts if a.result == "SUCCESS"), None)
        if success and success.result_time_ms is not None:
            return (success.timestamp_ms - self.attempts[0].timestamp_ms) + success.result_time_ms
        return None


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


# Poll/backoff logs may come from either INVERTER-CONTROLLER (older firmware)
# or INVERTER-MONITOR (current firmware).
_INVERTER_PREFIX_RE = r"INVERTER-(?:CONTROLLER|MONITOR)"
_POLL_RE = re.compile(rf"\[{_INVERTER_PREFIX_RE}\] Poll #(\d+): Status=(\S+) Power=([\d.]+)W")
_BACKOFF_RE = re.compile(rf"\[{_INVERTER_PREFIX_RE}\] Backoff: retry interval -> (\d+)s")
_LINK_STATE_RE = re.compile(
    rf"\[{_INVERTER_PREFIX_RE}\] Link state: ([A-Z]+) -> ([A-Z]+)"
    r"(?: \(streak=(\d+)s, interval=(\d+)s\))?"
)


def parse_polls(entries: List[Dict]) -> Tuple[List[PollEntry], int]:
    """Return (poll_entries, skipped_count) from log entries."""
    polls: List[PollEntry] = []
    skipped = 0
    for e in entries:
        msg = str(e.get("message", ""))
        ts = int(e.get("timestamp_ms", 0))
        m = _POLL_RE.search(msg)
        if m:
            polls.append(PollEntry(
                timestamp_ms=ts,
                poll_number=int(m.group(1)),
                status=m.group(2),
                power_w=float(m.group(3)),
            ))
        elif "No WiFi connection; skipping poll iteration" in msg:
            skipped += 1
    return polls, skipped


def parse_backoff_events(entries: List[Dict]) -> List[BackoffEvent]:
    """Parse explicit backoff interval transition events."""
    events: List[BackoffEvent] = []
    for e in entries:
        msg = str(e.get("message", ""))
        ts = int(e.get("timestamp_ms", 0))
        m = _BACKOFF_RE.search(msg)
        if m:
            events.append(BackoffEvent(timestamp_ms=ts, interval_seconds=int(m.group(1))))
    return events


def parse_state_change_events(entries: List[Dict]) -> List[StateChangeEvent]:
    """Parse inverter link state transition events (e.g. STARTING -> ONLINE)."""
    events: List[StateChangeEvent] = []
    for e in entries:
        msg = str(e.get("message", ""))
        ts = int(e.get("timestamp_ms", 0))
        m = _LINK_STATE_RE.search(msg)
        if m:
            streak = int(m.group(3)) if m.group(3) is not None else None
            interval = int(m.group(4)) if m.group(4) is not None else None
            events.append(
                StateChangeEvent(
                    timestamp_ms=ts,
                    from_state=m.group(1),
                    to_state=m.group(2),
                    streak_seconds=streak,
                    interval_seconds=interval,
                )
            )
    return events


def group_into_episodes(attempts: List[Attempt]) -> List[Episode]:
    """Group consecutive attempts into disconnection episodes.

    Two attempts belong to the same episode when they are adjacent in the
    attempt list AND the gap between end-of-first and start-of-second is
    <= 25 s (i.e., no normal 20-second poll cycle completed between them).
    """
    if not attempts:
        return []
    episodes: List[Episode] = []
    current = Episode(number=1, attempts=[attempts[0]])
    for prev, curr in zip(attempts, attempts[1:]):
        prev_end = prev.timestamp_ms + (prev.result_time_ms or 8000)
        gap_ms = curr.timestamp_ms - prev_end
        if gap_ms <= 25_000:
            current.attempts.append(curr)
        else:
            episodes.append(current)
            current = Episode(number=len(episodes) + 1, attempts=[curr])
    episodes.append(current)
    return episodes


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


def print_session_summary(
    entries: List[Dict],
    polls: List[PollEntry],
    skipped: int,
    episodes: List[Episode],
    backoff_events: Optional[List[BackoffEvent]] = None,
) -> None:
    """Print a high-level session overview before the connection analysis."""
    if not entries:
        return

    first_ts = int(entries[0].get("timestamp_ms", 0))
    last_ts = int(entries[-1].get("timestamp_ms", 0))
    uptime_ms = last_ts - first_ts

    print("\nSession Summary")
    print("---------------")
    uptime_s = uptime_ms / 1000
    print(f"Uptime         : {uptime_s/60:.1f} min ({uptime_s:.0f}s)  [{format_ms(first_ts)} → {format_ms(last_ts)}]")
    print(f"Log entries    : {len(entries)}")

    # Poll stats
    total_polls = len(polls) + skipped
    print(f"Polls          : {len(polls)} successful, {skipped} skipped (WiFi down), {total_polls} total")

    if polls:
        powers = [p.power_w for p in polls]
        print(f"Power (W)      : min={min(powers):.1f}  avg={statistics.mean(powers):.1f}  max={max(powers):.1f}  last={powers[-1]:.1f}")

        # Simple trend: compare first quarter avg vs last quarter avg
        q = max(1, len(powers) // 4)
        trend_start = statistics.mean(powers[:q])
        trend_end = statistics.mean(powers[-q:])
        delta = trend_end - trend_start
        trend_arrow = "↑" if delta > 2 else ("↓" if delta < -2 else "→")
        print(f"Power trend    : {trend_arrow}  ({trend_start:.1f}W → {trend_end:.1f}W, Δ{delta:+.1f}W)")

    # Disconnection episode summary
    if episodes:
        unresolved = [ep for ep in episodes if not ep.resolved]
        resolved = [ep for ep in episodes if ep.resolved]
        print(f"Disconnections : {len(episodes)} episodes  ({len(resolved)} resolved, {len(unresolved)} unresolved)")
        recovery_times = [ep.recovery_duration_ms for ep in resolved if ep.recovery_duration_ms is not None]
        if recovery_times:
            print(f"Recovery time  : avg={statistics.mean(recovery_times)/1000:.1f}s  max={max(recovery_times)/1000:.1f}s")
        retry_counts = [ep.retries_before_success for ep in resolved]
        if retry_counts:
            avg_retries = statistics.mean(retry_counts)
            print(f"Retries/episode: avg={avg_retries:.1f}  max={max(retry_counts)}")

    if backoff_events:
        latest = backoff_events[-1]
        transitions = " ".join(f"{ev.interval_seconds}s@{format_ms(ev.timestamp_ms)}" for ev in backoff_events[-3:])
        print(
            f"Backoff events : {len(backoff_events)} total  "
            f"(latest={latest.interval_seconds}s at {format_ms(latest.timestamp_ms)})"
        )
        print(f"Backoff recent : {transitions}")


def summarize(attempts: List[Attempt], episodes: List[Episode]) -> None:
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

    # Per-episode attempt detail
    print("\nPer-attempt detail")
    attempt_to_episode: Dict[int, int] = {}
    for ep in episodes:
        for a in ep.attempts:
            attempt_to_episode[a.index] = ep.number

    last_ep = None
    for a in attempts:
        ep_num = attempt_to_episode.get(a.index)
        if ep_num != last_ep:
            last_ep = ep_num
            ep = next((e for e in episodes if e.number == ep_num), None)
            status = "UNRESOLVED" if ep and not ep.resolved else ""
            print(f"\n  Episode {ep_num}  [{format_ms(a.timestamp_ms)}]  {status}".rstrip())
        path_tag = f"[{a.path or '?':5s}]"
        ch = f"ch={a.channel}" if a.channel is not None else "ch=?"
        tm = f"{a.result_time_ms}ms" if a.result_time_ms is not None else "?ms"
        ts = format_ms(a.timestamp_ms)
        print(f"    #{a.index:02d} {path_tag} {ts}  {a.result:7s}  {tm:>6s}  {ch}")


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
    polls, skipped = parse_polls(entries)
    backoff_events = parse_backoff_events(entries)
    episodes = group_into_episodes(attempts)

    print_session_summary(entries, polls, skipped, episodes, backoff_events)
    summarize(attempts, episodes)
    return 0


if __name__ == "__main__":
    sys.exit(main())
