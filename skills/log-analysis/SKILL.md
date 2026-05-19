---
name: log-analysis
description: Acquire bridge logs from /api/logs and analyze WiFi connection attempt outcomes, per-path (dwell vs auto) timing, disconnection episodes, power readings, and reliability patterns.
---

# Log Analysis Skill

## Agent Behaviour — When to Run This Skill

**For generic requests involving logs, data, status, or power, prefer `analyze_and_plot.py` (single fetch, faster)** — e.g. "show me the logs", "analyse the logs", "how is the inverter doing?", "show me the power", "what's happening?".

Default workflow for a generic log request:
1. Run `analyze_and_plot.py` → print Session Summary + Connection Analysis and save `output/powerplot.png` in one pass.
2. Display `output/powerplot.png` inline with `view_image` when relevant.
3. Summarise the key findings in 4–6 bullet points (uptime, power range/trend, disconnection count, path preference, anything anomalous).

Only skip the plot if:
- The user makes a **specific** log request (e.g. "show me the WiFi connection attempts", "how many skipped polls?", "what is the reconnect time?") — answer with text only.
- The user explicitly asks for text-only output.
- matplotlib is not installed.

---

## Purpose
Fetch bridge logs from the ESP API and analyze WiFi connection behavior without triggering any new activity.

The bridge firmware alternates between two connect paths on every connect attempt:
- **dwell**: short scan dwell (200 ms), uses configured AP hint as fallback.
- **auto**: longer scan dwell (500 ms), auto-discovery only (no hint fallback).

Both paths log structured start/complete entries tagged with path name and duration, enabling A/B comparison directly from live logs.

This skill is self-contained in one folder:
- SKILL.md: usage guide and expected outputs
- analyze_and_plot.py: one-pass fetch + analyze + plot (fast path for generic requests)
- analyze_bridge_logs.py: fetch + parse + summarize WiFi connection attempts, poll stats, power readings, and disconnection episodes
- plot_power.py: fetch logs and save a power-vs-time PNG chart to `output/` (git-ignored)
- show_all.py: convenience script to dump all entries in chronological order

## Primary Commands

Run from repository root:

### 1. Show All Log Entries (Chronological)
Quick dump of every log entry since boot with formatted timestamps:
```powershell
.venv\Scripts\python skills/log-analysis/show_all.py
```

With custom bridge URL:
```powershell
.venv\Scripts\python skills/log-analysis/show_all.py --base-url http://192.168.1.48:8080
```

Faster scoped views (recommended for large buffers):
```powershell
.venv\Scripts\python skills/log-analysis/show_all.py --limit 200
.venv\Scripts\python skills/log-analysis/show_all.py --since-ms 300000
```

### 2. Analyze + Plot (One Pass, Fast Path)
Run analysis and generate the power plot from one `/api/logs` fetch:
```powershell
.venv\Scripts\python skills/log-analysis/analyze_and_plot.py
```

With filters (faster on busy systems):
```powershell
.venv\Scripts\python skills/log-analysis/analyze_and_plot.py --limit 400
.venv\Scripts\python skills/log-analysis/analyze_and_plot.py --since-ms 300000
```

### 3. Analyze WiFi Connection Attempts
Full analysis including session summary, power stats, disconnection episodes, and A/B path breakdown:
```powershell
.venv\Scripts\python skills/log-analysis/analyze_bridge_logs.py
```

Show all entries plus full analysis:
```powershell
.venv\Scripts\python skills/log-analysis/analyze_bridge_logs.py --print-all
```

Custom bridge target and save raw logs to JSON:
```powershell
.venv\Scripts\python skills/log-analysis/analyze_bridge_logs.py --base-url http://192.168.1.48:8080 --save-json logs/latest_bridge_logs.json
```

Filter entries since a specific millisecond offset (e.g., events after 5 minutes uptime):
```powershell
.venv\Scripts\python skills/log-analysis/analyze_bridge_logs.py --since-ms 300000
```

### 4. Plot Power Output
Generate a PNG chart of power (W) vs elapsed time, with disconnection episodes shown as shaded bands and explicit backoff transition markers:
```powershell
.venv\Scripts\python skills/log-analysis/plot_power.py
```

Open the plot window after saving:
```powershell
.venv\Scripts\python skills/log-analysis/plot_power.py --show
```

Custom output path or bridge URL:
```powershell
.venv\Scripts\python skills/log-analysis/plot_power.py --base-url http://192.168.1.48:8080 --out output/today.png
```

Filter to last N entries or a time window:
```powershell
.venv\Scripts\python skills/log-analysis/plot_power.py --since-ms 300000
.venv\Scripts\python skills/log-analysis/plot_power.py --limit 200
```

Output is saved to `output/powerplot.png` by default and overwritten each run. The `output/` folder is git-ignored.

Plot behavior notes:
- Backoff transitions (`retry interval -> 60s` / `600s`) are drawn as dashed vertical markers.
- In backoff mode, episode labels are simplified (no retry counts) to reduce clutter.
- Post-backoff episode labels are thinned for readability in dense retry regions.

**Requires**: `matplotlib` — install once with `uv pip install matplotlib`.

## What The Scripts Report

### show_all.py
- Total number of log entries
- Boot time, last entry time, and total uptime
- Chronological list of all entries with formatted timestamps (Xm SSs)

### analyze_bridge_logs.py

**Session Summary** (always shown):
- Uptime span and duration
- Total log entries
- Poll counts: successful, skipped (WiFi down), total
- Power readings: min/avg/max/last + trend arrow (↑ ↓ →) with start-vs-end comparison
- Disconnection episode count, average recovery time, average retries per episode
- Backoff transition events (explicit `retry interval -> Ns` logs) with latest and recent switch points

**Connection Analysis**:
- Total attempts, overall success/failure rate and timing (min/avg/max/median)
- Per-path breakdown (dwell vs auto): success rate, timing, channel distribution
- Per-attempt detail grouped by **disconnection episode** — each episode shows its start timestamp and whether it was resolved

## Known Log Message Patterns

Use these as a fast-reference lookup when manually scanning `--print-all` output or raw JSON.

| Pattern | Source | Meaning |
|---------|--------|---------|
| `[INVERTER-MONITOR] Inverter monitor initialized` | inverter_monitor.cpp | Boot — monitor task started |
| `[WIFI-BRIDGE] Triggering inverter WiFi wake pulse sequence.` | wifi_bridge.cpp | GPIO double-press wake sent before each connect attempt |
| `[WIFI-CONNECT] start path=dwell ...` | wifi_bridge.cpp | Dwell-path connect attempt starting (200ms scan) |
| `[WIFI-CONNECT] start path=auto ...` | wifi_bridge.cpp | Auto-path connect attempt starting (500ms scan) |
| `[WIFI-CONNECT] complete ... result=success ...` | wifi_bridge.cpp | WiFi connected — note `channel=` and `duration_ms=` |
| `[WIFI-CONNECT] complete ... result=timeout ...` | wifi_bridge.cpp | Connect timed out (~8s budget); note `final_status=` |
| `[INVERTER-MONITOR] Poll #N: Status=X Power=Y.ZW` | inverter_monitor.cpp | Successful inverter poll. Status=1 is normal. Power in watts. |
| `[INVERTER-MONITOR] No WiFi connection; skipping poll iteration` | inverter_monitor.cpp | Poll skipped — WiFi was down; counts as a lost sample |
| `[INVERTER-MONITOR] Inverter recovered; resuming normal poll interval` | inverter_monitor.cpp | First successful poll after a disconnection episode |
| `[INVERTER-MONITOR] Backoff: retry interval -> 60s` | inverter_monitor.cpp | Retry cadence switched to 60-second interval |
| `[INVERTER-MONITOR] Backoff: retry interval -> 600s` | inverter_monitor.cpp | Retry cadence switched to 10-minute interval |
| `[INVERTER-MONITOR] Failed to fetch /home` | inverter_monitor.cpp | WiFi was up but HTTP request to inverter failed |
| `[ETH] ENC28J60 hardware initialized. Waiting for cable...` | ethernet_bridge.cpp | Boot — Ethernet chip ready |
| `[ETH] Cable detected. Attempting DHCP...` | ethernet_bridge.cpp | Physical Ethernet link came up |
| `[ETH] DHCP OK. IP=192.168.1.48` | ethernet_bridge.cpp | LAN address assigned; API is reachable |
| `[API] Listening on Ethernet port 8080 ...` | api.cpp | HTTP server started |
| `[API] GET /api/logs` | api.cpp | External client fetched logs (you, Home Assistant, etc.) |
| `[API] POST /api/power` | api.cpp | Power setpoint request received |
| `[API] GET /pulse` | api.cpp | External caller triggered forced reconnect |
| `[API] debug mode enabled` | api.cpp | Debug logging activated via `/api/debug` |

### Quick Interpretation Guide

**"Why are there so many TIMEOUT attempts at boot?"**  
Normal: the inverter WiFi radio needs a few seconds after the wake pulse before it's scannable. The first 1–2 attempts always time out. Expect 3 attempts → success on boot.

**"Episode resolved on first try (single SUCCESS with no prior TIMEOUT)"**  
The inverter WiFi was still broadcasting from a previous session. The bridge reconnected immediately without needing to wake it.

**"Episode with 3–4 TIMEOUTs before success"**  
The inverter had turned its WiFi off. The wake pulses on each attempt eventually caused it to turn back on. 4 attempts = ~80–90 seconds of downtime.

**"Power trend ↓ with values <15W"**  
Late afternoon / evening solar — production is dropping. Low-light noise (single-digit W readings) is normal near sunset.

**"Power trend ↑ in first quarter"**  
Morning ramp-up from sunrise.

**"`Failed to fetch /home` after WiFi connected"**  
WiFi up but inverter HTTP unreachable. Check if the inverter IP changed or if it rebooted mid-session.

**"No WiFi connection; skipping poll iteration"**  
Counted as `skipped` in Session Summary. High skipped counts indicate prolonged disconnection; divide by ~3 to estimate lost minutes of data.

**"Backoff: retry interval -> 600s"**  
Backoff escalation is active and reconnect attempts are now 10 minutes apart. This is explicitly logged (not inferred) and usually indicates sustained unreachability.

## Expected Log Signals
The scripts look for these log markers produced by `wifi_bridge.cpp`:
```
[WIFI-CONNECT] start path=dwell scan_dwell_ms=200 hint_fallback=1
[WIFI-CONNECT] complete path=dwell duration_ms=5413 result=success channel=11 bssid=00:06:66:9D:E0:36 ip=10.0.0.42

[WIFI-CONNECT] start path=auto scan_dwell_ms=500 hint_fallback=0
[WIFI-CONNECT] complete path=auto duration_ms=8064 result=timeout final_status=DISCONNECTED
```

Poll entries parsed for power stats:
```
[INVERTER-MONITOR] Poll #N: Status=1 Power=674.547W
[INVERTER-MONITOR] No WiFi connection; skipping poll iteration
[INVERTER-MONITOR] Backoff: retry interval -> 60s
[INVERTER-MONITOR] Backoff: retry interval -> 600s
```

## Timestamp Format
Both scripts use a canonical timestamp format: `Xm SS.sss` (e.g. `4m 05.123`) representing minutes, seconds, and milliseconds since boot. This format is preferred when communicating about specific log entries across the project.

A pulse is also logged when the manager wakes the inverter:
```
[WIFI-BRIDGE] Triggering inverter WiFi wake pulse sequence.
```

## Optional Filtering
- Prefer `--limit` first for speed when the question is about recent behavior.
- Use `--since-ms` to analyze only entries at or after a timestamp.
- Use `--limit` to analyze only the last N entries.

## Troubleshooting
- If fetch fails:
  - Verify bridge Ethernet IP and that `/api/logs` is reachable.
- If zero attempts are parsed:
  - Confirm logs contain `[WIFI-CONNECT] start path=` markers.
- If zero polls are shown in Session Summary:
  - Confirm logs contain `[INVERTER-MONITOR] Poll #` markers (only present when `debugMode=true` or a poll completed).
  - The log buffer holds 1000 entries. If it rolled over, run again sooner.
  - Reduce filters (`--since-ms`, `--limit`).
- If only one path appears:
  - The bridge alternates paths only when WiFi is down. If WiFi stayed up
    through all polling iterations, only the first boot connect is logged.
- If JSON save path fails:
  - Ensure parent directory exists and is writable.
