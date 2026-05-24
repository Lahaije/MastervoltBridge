---
name: log-analysis
description: Acquire bridge logs from /api/logs and analyze WiFi connection attempt outcomes, per-path (dwell vs auto) timing, disconnection episodes, power readings, and reliability patterns.
---

<objective>
Fetch bridge logs from the ESP API and analyze WiFi connection behavior without triggering any new activity.

The bridge firmware alternates between two connect paths on every connect attempt:
- **dwell**: short scan dwell (200 ms), uses configured AP hint as fallback.
- **auto**: longer scan dwell (500 ms), auto-discovery only (no hint fallback).

Both paths log structured start/complete entries tagged with path name and duration, enabling A/B comparison directly from live logs.

Scripts in this folder: `analyze_and_plot.py` (one-pass fast path), `analyze_bridge_logs.py` (full analysis), `analyze_logs_snapshot.py` (snapshot comparison), `plot_power.py` (power chart), `show_all.py` (chronological dump).
</objective>

<essential_principles>
**For generic requests involving logs, data, status, or power, prefer `analyze_and_plot.py` (single fetch, faster)** — e.g. "show me the logs", "analyse the logs", "how is the inverter doing?", "show me the power", "what's happening?".

Default workflow for a generic log request:
1. Run `analyze_and_plot.py` → print Session Summary + Connection Analysis and save `output/powerplot.png` in one pass.
2. Display `output/powerplot.png` inline with `view_image` when relevant.
3. Summarise the key findings in 4–6 bullet points (uptime, power range/trend, disconnection count, path preference, anything anomalous).

Only skip the plot if:
- The user makes a **specific** log request (e.g. "show me the WiFi connection attempts", "how many skipped polls?", "what is the reconnect time?") — answer with text only.
- The user explicitly asks for text-only output.
- matplotlib is not installed.
</essential_principles>

<quick_start>
```powershell
# Activate venv (required once per terminal session)
& d:\git\MastervoltBridge\.venv\Scripts\Activate.ps1

# Fast path: analyze + plot in one pass
python skills/log-analysis/analyze_and_plot.py

# Full WiFi connection analysis
python skills/log-analysis/analyze_bridge_logs.py

# Power chart only
python skills/log-analysis/plot_power.py
```
</quick_start>

<commands>
Run from repository root. **The venv MUST be activated first in every terminal session.**

**1. Show All Log Entries (Chronological)**
```powershell
python skills/log-analysis/show_all.py
python skills/log-analysis/show_all.py --base-url http://192.168.1.48:8080
python skills/log-analysis/show_all.py --limit 200
python skills/log-analysis/show_all.py --since-ms 300000
```

**2. Analyze + Plot (One Pass, Fast Path)**
```powershell
python skills/log-analysis/analyze_and_plot.py
python skills/log-analysis/analyze_and_plot.py --limit 400
python skills/log-analysis/analyze_and_plot.py --since-ms 300000
```

**3. Analyze WiFi Connection Attempts**
```powershell
python skills/log-analysis/analyze_bridge_logs.py
python skills/log-analysis/analyze_bridge_logs.py --print-all
python skills/log-analysis/analyze_bridge_logs.py --base-url http://192.168.1.48:8080 --save-json logs/latest_bridge_logs.json
python skills/log-analysis/analyze_bridge_logs.py --since-ms 300000
```

**4. Plot Power Output**
```powershell
python skills/log-analysis/plot_power.py
python skills/log-analysis/plot_power.py --show
python skills/log-analysis/plot_power.py --base-url http://192.168.1.48:8080 --out output/today.png
python skills/log-analysis/plot_power.py --since-ms 300000
python skills/log-analysis/plot_power.py --limit 200
```

Output saved to `output/powerplot.png` by default (git-ignored). **Requires**: `matplotlib` — install with `uv pip install matplotlib`.

Plot behavior notes:
- Backoff transitions drawn as dashed vertical markers (parsed from `[INVERTER-MONITOR] Link state:` logs, debug-mode only).
- In backoff mode, episode labels are simplified; post-backoff labels thinned for readability.

**5. Compare Archived Snapshot with Live Logs**
```powershell
python skills/log-analysis/analyze_logs_snapshot.py
python skills/log-analysis/analyze_logs_snapshot.py --base-url http://192.168.1.48:8080
python skills/log-analysis/analyze_logs_snapshot.py --snapshot-dir output --snapshot-glob logs_before_disconnect_*.json
```
</commands>

<script_outputs>
**show_all.py** — Total entries, boot time, last entry time, total uptime, chronological list with formatted timestamps (Xm SSs).

**analyze_bridge_logs.py** — Session Summary (uptime, poll counts, power min/avg/max/last + trend arrow, disconnection episodes, backoff transitions) + Connection Analysis (total attempts, success/failure rate, per-path dwell vs auto breakdown with timing and channel distribution, per-attempt detail grouped by disconnection episode).

**Auto-save (all scripts)** — Every call to `fetch_logs()` automatically appends fetched entries to `output/logs_accumulated.jsonl` (JSONL, one JSON object per line). This builds a historical archive across sessions for offline analysis without extra steps.
</script_outputs>

<log_patterns>
| Pattern | Source | Meaning |
|---------|--------|---------|
| `[INVERTER-MONITOR] Inverter monitor initialized` | inverter_monitor.cpp | Boot — monitor task started |
| `[WIFI-BRIDGE] Triggering inverter WiFi wake pulse sequence.` | wifi_bridge.cpp | GPIO double-press wake sent before each connect attempt |
| `[WIFI-CONNECT] start path=dwell ...` | wifi_bridge.cpp | Dwell-path connect attempt starting (200ms scan) |
| `[WIFI-CONNECT] start path=auto ...` | wifi_bridge.cpp | Auto-path connect attempt starting (500ms scan) |
| `[WIFI-CONNECT] complete ... result=success ...` | wifi_bridge.cpp | WiFi connected — note `channel=` and `duration_ms=` |
| `[WIFI-CONNECT] complete ... result=timeout ...` | wifi_bridge.cpp | Connect timed out (~8s budget); note `final_status=` |
| `[INVERTER-MONITOR] Poll #N: Status=X Power=Y.ZW` | inverter_monitor.cpp | Successful inverter poll. Status=1 is normal. |
| `[INVERTER-MONITOR] Link state: FROM -> TO (streak=Ns, interval=Ns)` | inverter_monitor.cpp | Link state changed (debug mode only). States: STARTING / ONLINE / RETRYING / BACKOFF / DORMANT. |
| `[INVERTER-MONITOR] Recovery after Ns: queuing MAX power reset (inverter state unknown)` | inverter_monitor.cpp | First successful poll after long outage; defensive MAX-power reset queued. |
| `[INVERTER-MONITOR] Recovery after Ns: user power command still queued, not overriding with MAX` | inverter_monitor.cpp | Recovery, but pending user `/api/power` command wins. |
| `[INVERTER-MONITOR] Failed to fetch /home` | inverter_monitor.cpp | WiFi up but HTTP request to inverter failed |
| `[LOCK-HIERARCHY] VIOLATION ...` | lock_guard.cpp | Lock-ordering rule violated; firmware bug. |
| `[ETH] ENC28J60 hardware initialized. Waiting for cable...` | ethernet_bridge.cpp | Boot — Ethernet chip ready |
| `[ETH] Cable detected. Attempting DHCP...` | ethernet_bridge.cpp | Physical Ethernet link came up |
| `[ETH] DHCP OK. IP=192.168.1.48` | ethernet_bridge.cpp | LAN address assigned; API reachable |
| `[API] Listening on Ethernet port 8080 ...` | api.cpp | HTTP server started |
| `[API] GET /api/logs` | api.cpp | External client fetched logs |
| `[API] POST /api/power` | api.cpp | Power setpoint request received |
| `[API] GET /pulse` | api.cpp | External caller triggered forced reconnect |
| `[API] debug mode enabled` | api.cpp | Debug logging activated via `/api/debug` |
</log_patterns>

<interpretation_guide>
- **Many TIMEOUTs at boot** — Normal. Inverter WiFi needs seconds after wake pulse. Expect 3 attempts → success.
- **Episode resolved on first try** — Inverter WiFi was still broadcasting from previous session.
- **3–4 TIMEOUTs before success** — Inverter had WiFi off; wake pulses turned it back on. ~80–90s downtime.
- **Power trend ↓ with <15W** — Late afternoon/evening solar dropping. Single-digit W is normal near sunset.
- **Power trend ↑ in first quarter** — Morning ramp-up from sunrise.
- **`Failed to fetch /home` after WiFi connected** — Inverter HTTP unreachable. Check IP or reboot.
- **Link state → DORMANT (interval=600s)** — Sustained unreachability; attempts now 10 min apart.
- **BACKOFF/DORMANT → ONLINE** — Recovery from long outage. Look for `queuing MAX power reset` afterwards.
</interpretation_guide>

<expected_log_signals>
The scripts parse these markers from `wifi_bridge.cpp`:
```
[WIFI-CONNECT] start path=dwell scan_dwell_ms=200 hint_fallback=1
[WIFI-CONNECT] complete path=dwell duration_ms=5413 result=success channel=11 bssid=00:06:66:9D:E0:36 ip=10.0.0.42

[WIFI-CONNECT] start path=auto scan_dwell_ms=500 hint_fallback=0
[WIFI-CONNECT] complete path=auto duration_ms=8064 result=timeout final_status=DISCONNECTED
```

Poll entries parsed for power stats:
```
[INVERTER-MONITOR] Poll #N: Status=1 Power=674.547W
[INVERTER-MONITOR] Link state: ONLINE -> RETRYING (streak=20s, interval=20s)
[INVERTER-MONITOR] Link state: RETRYING -> BACKOFF (streak=305s, interval=60s)
[INVERTER-MONITOR] Link state: BACKOFF -> DORMANT (streak=1205s, interval=600s)
[INVERTER-MONITOR] Link state: DORMANT -> ONLINE (streak=2400s, interval=20s)
```

Wake pulse marker:
```
[WIFI-BRIDGE] Triggering inverter WiFi wake pulse sequence.
```

Timestamp format: `Xm SS.sss` (e.g. `4m 05.123`) — minutes, seconds, milliseconds since boot.
</expected_log_signals>

<filtering>
- Prefer `--limit` first for speed when the question is about recent behavior.
- Use `--since-ms` to analyze only entries at or after a timestamp.
- Use `--limit` to analyze only the last N entries.
</filtering>

<troubleshooting>
- **Fetch fails** — Verify bridge Ethernet IP and that `/api/logs` is reachable.
- **Zero attempts parsed** — Confirm logs contain `[WIFI-CONNECT] start path=` markers.
- **Zero polls shown** — Confirm logs contain `[INVERTER-MONITOR] Poll #` markers. Log buffer holds 1000 entries; if rolled over, run again sooner. Reduce filters.
- **Only one path appears** — Bridge alternates paths only when WiFi is down. If WiFi stayed up, only boot connect is logged.
- **JSON save path fails** — Ensure parent directory exists and is writable.
</troubleshooting>

<success_criteria>
Log analysis is complete when:
- [ ] Logs fetched successfully from bridge API
- [ ] Session summary printed (uptime, poll counts, power stats)
- [ ] Connection analysis shown (per-path breakdown with timing)
- [ ] Key findings summarized in 4–6 bullet points
- [ ] Power plot saved (when using analyze_and_plot.py or plot_power.py)
</success_criteria>
