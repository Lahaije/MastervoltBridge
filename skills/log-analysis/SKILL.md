---
name: log-analysis
description: Acquire bridge logs from /api/logs and analyze WiFi connection attempt outcomes, per-path (dwell vs auto) timing, disconnection episodes, power readings, and reliability patterns. Use when checking bridge status, analyzing WiFi performance, or investigating connectivity issues.
---

<objective>
Fetch bridge logs from the ESP API and analyze WiFi connection behavior without triggering any new activity. Produces session summaries, connection analysis, power statistics, and disconnection episode breakdowns.
</objective>

<quick_start>
**For generic requests** ("show me the logs", "how is the inverter doing?", "show me the power"):
```powershell
& d:\git\MastervoltBridge\.venv\Scripts\Activate.ps1
python skills/log-analysis/analyze_and_plot.py
```
Then display `output/powerplot.png` inline and summarise findings in 4–6 bullets.

**For specific log questions** (WiFi attempts, skipped polls, reconnect time): answer with text only using `analyze_bridge_logs.py`.
</quick_start>

<agent_behaviour>
**Default workflow for generic log requests:**
1. Run `analyze_and_plot.py` → print Session Summary + Connection Analysis and save `output/powerplot.png` in one pass.
2. Display `output/powerplot.png` inline with `view_image` when relevant.
3. Summarise key findings in 4–6 bullet points (uptime, power range/trend, disconnection count, path preference, anything anomalous).

**Always persist fetched logs:** When fetching logs from the bridge, always use `--save-json output/logs_<YYYYMMDD_HHmmss>.json` to accumulate a history of snapshots. This enables offline replay and trend comparison across sessions.

**Skip the plot only if:**
- User makes a specific log request — answer with text only.
- User explicitly asks for text-only output.
- matplotlib is not installed.
</agent_behaviour>

<background>
The bridge firmware alternates between two connect paths on every connect attempt:
- **dwell**: short scan dwell (200 ms), uses configured AP hint as fallback.
- **auto**: longer scan dwell (500 ms), auto-discovery only (no hint fallback).

Both paths log structured start/complete entries tagged with path name and duration, enabling A/B comparison directly from live logs.

**Scripts in this folder:**
- `analyze_and_plot.py` — one-pass fetch + analyze + plot (fast path for generic requests)
- `analyze_bridge_logs.py` — full WiFi connection analysis, poll stats, power readings, disconnection episodes
- `plot_power.py` — power-vs-time PNG chart with disconnection bands and backoff markers
- `show_all.py` — dump all entries in chronological order
</background>

<commands>
**1. Show All Log Entries (Chronological):**
```powershell
python skills/log-analysis/show_all.py
python skills/log-analysis/show_all.py --limit 200
python skills/log-analysis/show_all.py --since-ms 300000
```

**2. Analyze + Plot (One Pass, Fast Path):**
```powershell
python skills/log-analysis/analyze_and_plot.py
python skills/log-analysis/analyze_and_plot.py --limit 400
python skills/log-analysis/analyze_and_plot.py --since-ms 300000
```

**3. Full WiFi Connection Analysis:**
```powershell
python skills/log-analysis/analyze_bridge_logs.py
python skills/log-analysis/analyze_bridge_logs.py --print-all
python skills/log-analysis/analyze_bridge_logs.py --save-json logs/latest.json
python skills/log-analysis/analyze_bridge_logs.py --since-ms 300000
```

**4. Plot Power Output:**
```powershell
python skills/log-analysis/plot_power.py
python skills/log-analysis/plot_power.py --show
python skills/log-analysis/plot_power.py --out output/today.png
```

All commands accept `--base-url http://192.168.1.48:8080` for custom bridge URL.

Output saved to `output/powerplot.png` by default (git-ignored, overwritten each run).

**Requires**: `matplotlib` — install with `uv pip install matplotlib`.
</commands>

<script_outputs>
**show_all.py** reports:
- Total entries, boot time, last entry time, total uptime
- Chronological list with formatted timestamps (Xm SSs)

**analyze_bridge_logs.py** reports:

*Session Summary (always shown):*
- Uptime span and duration
- Total log entries
- Poll counts: successful, skipped (WiFi down), total
- Power readings: min/avg/max/last + trend arrow (↑ ↓ →)
- Disconnection episode count, avg recovery time, avg retries per episode
- Backoff transition events with latest and recent switch points

*Connection Analysis:*
- Total attempts, overall success/failure rate and timing
- Per-path breakdown (dwell vs auto): success rate, timing, channel distribution
- Per-attempt detail grouped by disconnection episode

**plot_power.py** produces:
- PNG chart of power (W) vs elapsed time
- Disconnection episodes as shaded bands
- Backoff transitions as dashed vertical markers
- Post-backoff episode labels thinned for readability
</script_outputs>

<log_patterns>
| Pattern | Source | Meaning |
|---------|--------|---------|
| `[INVERTER-CONTROLLER] Inverter controller initialized` | inverter_controller.cpp | Boot — monitor task started |
| `[WIFI-BRIDGE] Triggering inverter WiFi wake pulse sequence.` | wifi_bridge.cpp | GPIO double-press wake sent |
| `[WIFI-CONNECT] start path=dwell ...` | wifi_bridge.cpp | Dwell-path connect starting (200ms scan) |
| `[WIFI-CONNECT] start path=auto ...` | wifi_bridge.cpp | Auto-path connect starting (500ms scan) |
| `[WIFI-CONNECT] complete ... result=success ...` | wifi_bridge.cpp | WiFi connected — note `channel=` and `duration_ms=` |
| `[WIFI-CONNECT] complete ... result=timeout ...` | wifi_bridge.cpp | Connect timed out (~8s budget) |
| `[INVERTER-CONTROLLER] Poll #N: Status=X Power=Y.ZW` | inverter_controller.cpp | Successful inverter poll |
| `[INVERTER-CONTROLLER] No WiFi connection; skipping poll iteration` | inverter_controller.cpp | Poll skipped — WiFi down |
| `[INVERTER-CONTROLLER] Inverter recovered; resuming normal poll interval` | inverter_controller.cpp | First poll after disconnection |
| `[INVERTER-CONTROLLER] Backoff: retry interval -> 60s` | inverter_controller.cpp | Switched to 60s retry interval |
| `[INVERTER-CONTROLLER] Backoff: retry interval -> 600s` | inverter_controller.cpp | Switched to 10-min retry interval |
| `[INVERTER-CONTROLLER] Failed to fetch /home` | inverter_controller.cpp | WiFi up but HTTP failed |
| `[ETH] ENC28J60 hardware initialized.` | ethernet_bridge.cpp | Boot — Ethernet chip ready |
| `[ETH] DHCP OK. IP=192.168.1.48` | ethernet_bridge.cpp | LAN address assigned |
| `[API] GET /api/logs` | api.cpp | External client fetched logs |
| `[API] POST /api/power` | api.cpp | Power setpoint request |
| `[API] GET /pulse` | api.cpp | Forced reconnect triggered |
| `[API] debug mode enabled` | api.cpp | Debug logging activated |
</log_patterns>

<interpretation_guide>
**"Many TIMEOUT attempts at boot"** — Normal. Inverter WiFi needs a few seconds after wake pulse. Expect 3 attempts → success on boot.

**"Episode resolved on first try"** — Inverter WiFi was still broadcasting. Bridge reconnected immediately.

**"Episode with 3–4 TIMEOUTs before success"** — Inverter had turned WiFi off. Wake pulses eventually caused it to turn back on. 4 attempts = ~80–90 seconds downtime.

**"Power trend ↓ with values <15W"** — Late afternoon solar production dropping. Single-digit readings normal near sunset.

**"Power trend ↑ in first quarter"** — Morning ramp-up from sunrise.

**"`Failed to fetch /home` after WiFi connected"** — WiFi up but inverter HTTP unreachable. Check if inverter IP changed or rebooted.

**"No WiFi connection; skipping poll iteration"** — Counted as `skipped` in Session Summary. Divide high counts by ~3 to estimate lost minutes.

**"Backoff: retry interval -> 600s"** — Sustained unreachability; reconnect attempts now 10 min apart.
</interpretation_guide>

<expected_log_signals>
The scripts look for these markers from `wifi_bridge.cpp`:
```
[WIFI-CONNECT] start path=dwell scan_dwell_ms=200 hint_fallback=1
[WIFI-CONNECT] complete path=dwell duration_ms=5413 result=success channel=11 bssid=00:06:66:9D:E0:36 ip=10.0.0.42

[WIFI-CONNECT] start path=auto scan_dwell_ms=500 hint_fallback=0
[WIFI-CONNECT] complete path=auto duration_ms=8064 result=timeout final_status=DISCONNECTED
```

Poll entries parsed for power stats:
```
[INVERTER-CONTROLLER] Poll #N: Status=1 Power=674.547W
[INVERTER-CONTROLLER] No WiFi connection; skipping poll iteration
[INVERTER-CONTROLLER] Backoff: retry interval -> 60s
[INVERTER-CONTROLLER] Backoff: retry interval -> 600s
```
</expected_log_signals>

<timestamp_format>
Both scripts use canonical format: `Xm SS.sss` (e.g. `4m 05.123`) — minutes, seconds, milliseconds since boot.
</timestamp_format>

<filtering>
- Prefer `--limit` first for speed when the question is about recent behavior.
- Use `--since-ms` to analyze only entries at or after a timestamp.
- Use `--limit` to analyze only the last N entries.
</filtering>

<troubleshooting>
- **Fetch fails** — Verify bridge Ethernet IP and that `/api/logs` is reachable.
- **Zero attempts parsed** — Confirm logs contain `[WIFI-CONNECT] start path=` markers.
- **Zero polls in Session Summary** — Confirm logs contain `[INVERTER-CONTROLLER] Poll #` markers (only present when `debugMode=true` or a poll completed). Buffer holds 1000 entries; if rolled over, run again sooner.
- **Only one path appears** — Bridge alternates paths only when WiFi is down. If WiFi stayed up, only boot connect is logged.
- **JSON save fails** — Ensure parent directory exists and is writable.
</troubleshooting>

<success_criteria>
Analysis is complete when:
- [ ] Logs fetched successfully from bridge
- [ ] Session summary printed (uptime, poll counts, power stats)
- [ ] Connection analysis shows per-path breakdown
- [ ] Plot saved (for generic requests)
- [ ] Key findings summarised for user
</success_criteria>
