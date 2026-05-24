#!/usr/bin/env python3
"""
Plot inverter power output over time from bridge logs.

Fetches log data from /api/logs, extracts Poll power readings, and saves a
PNG chart to the output/ folder at the repository root (git-ignored).

The chart shows:
  - Power (W) vs elapsed time (minutes)
  - Shaded bands for disconnection episodes (WiFi down, no data)
  - Episode labels with retry count and recovery time
  - Annotation of peak power and last reading

Usage (from repository root):
    .venv\\Scripts\\python skills/log-analysis/plot_power.py
    .venv\\Scripts\\python skills/log-analysis/plot_power.py --base-url http://<ip>:8080
    .venv\\Scripts\\python skills/log-analysis/plot_power.py --out output/my_plot.png
    .venv\\Scripts\\python skills/log-analysis/plot_power.py --show

The bridge IP is auto-discovered via hostname/MAC. Override with --base-url if needed.
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from bridge_config import BRIDGE_BASE_URL  # noqa: E402

# Reuse fetch + parse helpers from sibling script
_SKILL_DIR = Path(__file__).parent
sys.path.insert(0, str(_SKILL_DIR))
from analyze_bridge_logs import (  # noqa: E402
    fetch_logs,
    format_ms,
    group_into_episodes,
    parse_attempts,
    parse_backoff_events,
    parse_polls,
)

import matplotlib
matplotlib.use("Agg")  # headless by default; overridden by --show
import matplotlib.pyplot as plt
import matplotlib.patches as mpatches
from matplotlib.lines import Line2D


def build_plot(
    polls,
    episodes,
    out_path: Path,
    backoff_events=None,
    show: bool = False,
) -> None:
    if not polls:
        print("No poll data to plot.")
        return

    # X in minutes, Y in watts — inject 0W during backoff gaps
    times_min: list[float] = []
    powers_w: list[float] = []

    first_backoff_ts = None
    if backoff_events:
        first_backoff_ts = min(ev.timestamp_ms for ev in backoff_events)

    for i, p in enumerate(polls):
        if i > 0 and first_backoff_ts is not None:
            prev = polls[i - 1]
            gap_ms = p.timestamp_ms - prev.timestamp_ms
            # If gap > 60s and falls after first backoff event, insert 0W boundaries
            if gap_ms > 60_000 and prev.timestamp_ms >= first_backoff_ts:
                times_min.append((prev.timestamp_ms + 1000) / 60_000)
                powers_w.append(0.0)
                times_min.append((p.timestamp_ms - 1000) / 60_000)
                powers_w.append(0.0)
        times_min.append(p.timestamp_ms / 60_000)
        powers_w.append(p.power_w)

    # Keep image dimensions moderate so chat/image viewers do not clip wide renders.
    fig, ax = plt.subplots(figsize=(11, 5.5))
    fig.subplots_adjust(left=0.06, right=0.98, top=0.92, bottom=0.10)
    fig.patch.set_facecolor("#1a1a2e")
    ax.set_facecolor("#16213e")

    # --- Disconnection episode bands ---
    post_backoff_labels_seen = 0
    last_backoff_label_x = None
    for ep in episodes:
        if not ep.attempts:
            continue
        band_start = ep.attempts[0].timestamp_ms / 60_000
        last = ep.attempts[-1]
        ep_end_ts = last.timestamp_ms + (last.result_time_ms or 8000)
        band_end = (last.timestamp_ms + (last.result_time_ms or 8000)) / 60_000
        ax.axvspan(band_start, band_end, color="#ff6b6b", alpha=0.15, zorder=1)
        label_y = max(powers_w) * 0.92
        retries = ep.retries_before_success
        rec_s = ep.recovery_duration_ms
        in_backoff_mode = first_backoff_ts is not None and ep_end_ts >= (first_backoff_ts - 20_000)
        if in_backoff_mode:
            # In backoff mode, do not show retry counts and thin dense labels.
            post_backoff_labels_seen += 1
            label_x = (band_start + band_end) / 2
            too_close = last_backoff_label_x is not None and (label_x - last_backoff_label_x) < 12.0
            if post_backoff_labels_seen % 3 != 1 or too_close:
                ep_label = None
            else:
                ep_label = f"Ep{ep.number}"
                last_backoff_label_x = label_x
        else:
            ep_label = f"Ep{ep.number}\n{retries} retr."
            if rec_s is not None:
                ep_label += f"\n{rec_s/1000:.0f}s"
        if ep_label:
            ax.text(
                (band_start + band_end) / 2,
                label_y,
                ep_label,
                ha="center", va="top",
                fontsize=6.5, color="#ff9999",
                zorder=4,
            )

    # --- Power line ---
    ax.plot(times_min, powers_w, color="#4fc3f7", linewidth=1.2, zorder=3, label="Power (W)")
    ax.fill_between(times_min, powers_w, alpha=0.12, color="#4fc3f7", zorder=2)

    # --- Backoff transition markers ---
    if backoff_events:
        y_top = max(powers_w) * 0.82
        y_alt = max(powers_w) * 0.74
        for idx, ev in enumerate(backoff_events):
            x_min = ev.timestamp_ms / 60_000
            ax.axvline(x_min, color="#ffd54f", linestyle="--", linewidth=0.8, alpha=0.8, zorder=4)
            label_y = y_top if idx % 2 == 0 else y_alt
            ax.text(
                x_min,
                label_y,
                f"backoff {ev.interval_seconds}s",
                rotation=90,
                va="top",
                ha="right",
                fontsize=6,
                color="#ffe082",
                zorder=5,
            )

    # --- Peak annotation ---
    peak_w = max(powers_w)
    peak_idx = powers_w.index(peak_w)
    ax.annotate(
        f"Peak {peak_w:.0f} W",
        xy=(times_min[peak_idx], peak_w),
        xytext=(times_min[peak_idx] + 0.5, peak_w * 1.04),
        fontsize=7.5, color="#ffe082",
        arrowprops=dict(arrowstyle="->", color="#ffe082", lw=0.8),
    )

    # --- Last reading annotation ---
    ax.annotate(
        f"Last {powers_w[-1]:.1f} W",
        xy=(times_min[-1], powers_w[-1]),
        xytext=(times_min[-1] - 3, powers_w[-1] + peak_w * 0.06),
        fontsize=7.5, color="#a5d6a7",
        arrowprops=dict(arrowstyle="->", color="#a5d6a7", lw=0.8),
    )

    # --- Axes styling ---
    ax.set_xlabel("Elapsed time (minutes)", color="#cfd8dc", labelpad=6)
    ax.set_ylabel("Power (W)", color="#cfd8dc", labelpad=6)
    total_min = times_min[-1]
    uptime_str = format_ms(polls[-1].timestamp_ms)
    ax.set_title(
        f"Inverter power output  —  {len(polls)} polls over {total_min:.1f} min (uptime {uptime_str})",
        color="#eceff1", fontsize=10, pad=10,
    )
    ax.tick_params(colors="#90a4ae")
    for spine in ax.spines.values():
        spine.set_edgecolor("#37474f")
    ax.set_xlim(left=times_min[0] - 0.5)
    ax.set_ylim(bottom=0)
    ax.grid(True, color="#263238", linewidth=0.6, zorder=0)

    # --- Legend ---
    ep_patch = mpatches.Patch(color="#ff6b6b", alpha=0.4, label="Disconnection episode")
    backoff_line = Line2D([0], [0], color="#ffd54f", linestyle="--", linewidth=1.0, label="Backoff transition")
    ax.legend(
        handles=[ax.lines[0], ep_patch, backoff_line],
        facecolor="#0f3460", edgecolor="#37474f",
        labelcolor="#eceff1", fontsize=8,
        loc="upper right",
    )

    out_path.parent.mkdir(parents=True, exist_ok=True)
    fig.savefig(out_path, dpi=130)
    print(f"Saved plot → {out_path}")

    if show:
        matplotlib.use("TkAgg")  # switch to interactive backend
        plt.show()

    plt.close(fig)


def main() -> int:
    parser = argparse.ArgumentParser(description="Plot inverter power from bridge logs")
    parser.add_argument("--base-url", default=BRIDGE_BASE_URL, help="Bridge base URL")
    parser.add_argument("--timeout", type=float, default=10.0, help="HTTP timeout seconds")
    parser.add_argument(
        "--out",
        default=None,
        help="Output PNG path (default: output/powerplot.png; overwritten each run)",
    )
    parser.add_argument("--show", action="store_true", help="Open the plot in a window after saving")
    parser.add_argument("--since-ms", type=int, default=None, help="Only include entries at/after timestamp_ms")
    parser.add_argument("--limit", type=int, default=None, help="Only include last N entries")
    args = parser.parse_args()

    try:
        payload = fetch_logs(args.base_url, args.timeout)
    except Exception as ex:
        print(f"Failed to fetch logs: {ex}")
        return 1

    entries = payload.get("entries", []) if isinstance(payload, dict) else []
    if args.since_ms is not None:
        entries = [e for e in entries if int(e.get("timestamp_ms", 0)) >= args.since_ms]
    if args.limit is not None and args.limit > 0:
        entries = entries[-args.limit:]

    polls, skipped = parse_polls(entries)
    attempts = parse_attempts(entries)
    backoff_events = parse_backoff_events(entries)
    episodes = group_into_episodes(attempts)

    print(f"Fetched {len(entries)} entries — {len(polls)} polls, {skipped} skipped, {len(episodes)} episodes")

    if not polls:
        print("No Poll entries found; nothing to plot.")
        return 1

    out_path = Path(args.out) if args.out else Path("output") / "powerplot.png"

    build_plot(polls, episodes, out_path, backoff_events=backoff_events, show=args.show)
    return 0


if __name__ == "__main__":
    sys.exit(main())
