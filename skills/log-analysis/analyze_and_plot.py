#!/usr/bin/env python3
"""Fetch logs once, then run analysis + power plot generation.

This script speeds up generic log requests by avoiding two separate /api/logs
fetches. It reuses parsing/summary helpers from analyze_bridge_logs.py and
plot generation from plot_power.py.
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

# Local imports from this skill folder
_SKILL_DIR = Path(__file__).parent
sys.path.insert(0, str(_SKILL_DIR))

from analyze_bridge_logs import (  # noqa: E402
    fetch_logs,
    group_into_episodes,
    parse_attempts,
    parse_backoff_events,
    parse_control_events,
    parse_polls,
    parse_state_change_events,
    print_session_summary,
    summarize,
)
from plot_power import build_plot  # noqa: E402


def main() -> int:
    parser = argparse.ArgumentParser(description="Fast one-pass log analysis + power plot")
    parser.add_argument("--base-url", default="http://192.168.1.48:8080", help="Bridge base URL")
    parser.add_argument("--timeout", type=float, default=10.0, help="HTTP timeout seconds")
    parser.add_argument("--since-ms", type=int, default=None, help="Only include entries at/after timestamp_ms")
    parser.add_argument("--limit", type=int, default=None, help="Only include last N entries")
    parser.add_argument(
        "--out",
        default="output/powerplot.png",
        help="Output PNG path (default: output/powerplot.png; overwritten)",
    )
    parser.add_argument("--show", action="store_true", help="Open the plot in a window after saving")
    parser.add_argument(
        "--state-labels",
        choices=["major", "all", "none"],
        default="major",
        help="State transition label density: major (default), all, or none",
    )
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

    attempts = parse_attempts(entries)
    polls, skipped = parse_polls(entries)
    backoff_events = parse_backoff_events(entries)
    state_change_events = parse_state_change_events(entries)
    control_events = parse_control_events(entries)
    episodes = group_into_episodes(attempts)

    print_session_summary(entries, polls, skipped, episodes, backoff_events)
    summarize(attempts, episodes)

    if not polls:
        print("No Poll entries found; nothing to plot.")
        return 1

    out_path = Path(args.out)
    build_plot(
        polls,
        episodes,
        out_path,
        backoff_events=backoff_events,
        state_change_events=state_change_events,
        control_events=control_events,
        state_label_mode=args.state_labels,
        show=args.show,
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
