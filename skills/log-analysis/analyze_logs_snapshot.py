#!/usr/bin/env python3
"""Compare an archived pre-disconnect log snapshot with the current live logs.

This helper is useful after unplug/reboot/disconnect investigations:
- it loads the newest archived snapshot from output/
- it fetches the current /api/logs buffer from the bridge
- it prints a short summary of snapshot vs live log activity

The script is read-only and does not trigger any new bridge behavior.
"""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path
from typing import Any, Dict, Iterable, List

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from bridge_config import BRIDGE_BASE_URL
from analyze_bridge_logs import fetch_logs as _shared_fetch_logs  # noqa: E402


DEFAULT_BASE_URL = BRIDGE_BASE_URL
DEFAULT_SNAPSHOT_GLOB = "logs_before_disconnect_*.json"


def _load_json(path: Path) -> Dict[str, Any]:
    with path.open("r", encoding="utf-8") as handle:
        payload = json.load(handle)
    return payload if isinstance(payload, dict) else {}


def _latest_snapshot(snapshot_dir: Path, snapshot_glob: str) -> Path:
    candidates = sorted(
        snapshot_dir.glob(snapshot_glob),
        key=lambda entry: entry.stat().st_mtime,
        reverse=True,
    )
    if not candidates:
        raise SystemExit(f"No archived snapshot found in {snapshot_dir} matching {snapshot_glob}")
    return candidates[0]


def _entries(payload: Dict[str, Any]) -> List[Dict[str, Any]]:
    entries = payload.get("entries", [])
    return entries if isinstance(entries, list) else []


def _message_contains(entry: Dict[str, Any], needles: Iterable[str]) -> bool:
    message = str(entry.get("message", "")).lower()
    return any(needle.lower() in message for needle in needles)


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Compare an archived bridge log snapshot with the current live logs",
    )
    parser.add_argument(
        "--base-url",
        default=DEFAULT_BASE_URL,
        help=f"Bridge base URL (default: {DEFAULT_BASE_URL})",
    )
    parser.add_argument(
        "--snapshot-dir",
        default="output",
        help="Directory containing archived snapshot JSON files (default: output)",
    )
    parser.add_argument(
        "--snapshot-glob",
        default=DEFAULT_SNAPSHOT_GLOB,
        help=f"Glob for archived snapshot files (default: {DEFAULT_SNAPSHOT_GLOB})",
    )
    parser.add_argument(
        "--timeout",
        type=float,
        default=30,
        help="HTTP timeout in seconds for /api/logs (default: 30)",
    )
    args = parser.parse_args()

    snapshot_dir = Path(args.snapshot_dir)
    snapshot_path = _latest_snapshot(snapshot_dir, args.snapshot_glob)
    snapshot = _load_json(snapshot_path)

    live = _shared_fetch_logs(args.base_url, args.timeout)

    snapshot_entries = _entries(snapshot)
    live_entries = _entries(live) if isinstance(live, dict) else []

    patterns = ("boot", "restart", "brownout", "guru", "exception", "panic", "reset reason", "abort", "stack")
    restart_like = [entry for entry in live_entries if _message_contains(entry, patterns)]
    boot_live = [entry for entry in live_entries if _message_contains(entry, ("booting",))]

    large_gaps = []
    for previous, current in zip(live_entries, live_entries[1:]):
        previous_ts = int(previous.get("timestamp_ms", 0))
        current_ts = int(current.get("timestamp_ms", 0))
        delta_ms = current_ts - previous_ts
        if delta_ms > 15_000:
            large_gaps.append({
                "delta_ms": delta_ms,
                "prev_ts": previous_ts,
                "cur_ts": current_ts,
                "prev_msg": previous.get("message", ""),
                "cur_msg": current.get("message", ""),
            })

    print(f"snapshot_file={snapshot_path.name}")
    print(
        f"snapshot_entries={len(snapshot_entries)} "
        f"snapshot_first={snapshot_entries[0]['timestamp_ms'] if snapshot_entries else 'n/a'} "
        f"snapshot_last={snapshot_entries[-1]['timestamp_ms'] if snapshot_entries else 'n/a'}"
    )
    print(
        f"live_entries={len(live_entries)} "
        f"live_first={live_entries[0]['timestamp_ms'] if live_entries else 'n/a'} "
        f"live_last={live_entries[-1]['timestamp_ms'] if live_entries else 'n/a'}"
    )
    print(f"boot_messages_in_live={len(boot_live)}")
    print(f"restart_like_messages_in_live={len(restart_like)}")
    print(f"large_gaps_in_live={len(large_gaps)}")

    if boot_live:
        print("boot_message_samples:")
        for entry in boot_live[:5]:
            print(f"  {entry['timestamp_ms']}: {entry['message']}")

    if restart_like:
        print("restart_like_samples:")
        for entry in restart_like[-10:]:
            print(f"  {entry['timestamp_ms']}: {entry['message']}")

    if large_gaps:
        print("large_gap_samples:")
        for gap in large_gaps[:10]:
            print(
                f"  gap={gap['delta_ms']}ms after {gap['prev_ts']} ({gap['prev_msg']}) "
                f"-> {gap['cur_ts']} ({gap['cur_msg']})"
            )

    return 0


if __name__ == "__main__":
    raise SystemExit(main())