---
name: log-analysis
description: Acquire bridge logs from /api/logs and analyze WiFi connection attempt outcomes, per-path (dwell vs auto) timing, and reliability patterns.
---

# Log Analysis Skill

## Purpose
Fetch bridge logs from the ESP API and analyze WiFi connection behavior without triggering any new activity.

The bridge firmware alternates between two connect paths on every connect attempt:
- **dwell**: short scan dwell (200 ms), uses configured AP hint as fallback.
- **auto**: longer scan dwell (500 ms), auto-discovery only (no hint fallback).

Both paths log structured start/complete entries tagged with path name and duration, enabling A/B comparison directly from live logs.

This skill is self-contained in one folder:
- SKILL.md: usage guide and expected outputs
- analyze_bridge_logs.py: fetch + parse + summarize WiFi connection attempts
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

### 2. Analyze WiFi Connection Attempts
Detailed A/B analysis of dwell vs auto connect paths:
```powershell
.venv\Scripts\python skills/log-analysis/analyze_bridge_logs.py
```

Show all entries plus WiFi attempt analysis:
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

## What The Scripts Report

### show_all.py
- Total number of log entries
- Boot time, last entry time, and total uptime
- Chronological list of all entries with formatted timestamps (Xm SSs)

### analyze_bridge_logs.py
- Total log entries fetched and analyzed
- Number of parsed connection attempts
- Overall success/failure rate and timing (min/avg/max/median)
- Per-path breakdown (dwell vs auto): success rate, timing distribution, channel distribution
- Per-attempt detail lines with path, outcome, timing, and channel
- Optional `--print-all` output: all entries with formatted timestamps (before analysis)

## Expected Log Signals
The scripts look for these log markers produced by `wifi_bridge.cpp`:
```
[WIFI-CONNECT] start path=dwell scan_dwell_ms=200 hint_fallback=1
[WIFI-CONNECT] complete path=dwell duration_ms=5413 result=success channel=11 bssid=00:06:66:9D:E0:36 ip=10.0.0.42

[WIFI-CONNECT] start path=auto scan_dwell_ms=500 hint_fallback=0
[WIFI-CONNECT] complete path=auto duration_ms=8064 result=timeout final_status=DISCONNECTED
```

## Timestamp Format
Both scripts use a canonical timestamp format: `Xm SS.sss` (e.g. `4m 05.123`) representing minutes, seconds, and milliseconds. This format is preferred when communicating about specific log entries across the project.


A pulse is also logged when the manager wakes the inverter:
```
[WIFI-BRIDGE] Triggering inverter WiFi wake pulse sequence.
```

## Optional Filtering
- Use `--since-ms` to analyze only entries at or after a timestamp.
- Use `--limit` to analyze only the last N entries.

## Troubleshooting
- If fetch fails:
  - Verify bridge Ethernet IP and that `/api/logs` is reachable.
- If zero attempts are parsed:
  - Confirm logs contain `[WIFI-CONNECT] start path=` markers.
  - The log buffer holds 1000 entries. If it rolled over, run again sooner.
  - Reduce filters (`--since-ms`, `--limit`).
- If only one path appears:
  - The bridge alternates paths only when WiFi is down. If WiFi stayed up
    through all polling iterations, only the first boot connect is logged.
- If JSON save path fails:
  - Ensure parent directory exists and is writable.
