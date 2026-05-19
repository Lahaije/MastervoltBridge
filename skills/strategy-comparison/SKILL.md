---
name: strategy-comparison
description: Compare the dwell vs auto WiFi connect strategies using passively accumulated bridge logs. Produces a side-by-side statistical table and a verdict with recommendation.
---

# Skill: Strategy Comparison

## Purpose

The bridge alternates between two WiFi connect strategies on every reconnect attempt:

- **dwell** — 200 ms scan dwell, uses configured AP hint as fallback.
- **auto** — 500 ms scan dwell, pure auto-discovery (no hint fallback).

Performance data accumulates passively during normal bridge operation. This skill fetches that data and produces a focused side-by-side comparison: sample counts, success rates, connect-time distribution (min/avg/median/P95/max/stdev), channel distribution, and a plain-English verdict.

This skill is **read-only** — it does not trigger `/pulse`, `/wifi/off`, or any state-changing endpoint.

---

## Primary Command

```powershell
.venv\Scripts\python skills/strategy-comparison/compare_strategies.py
```

Save result as JSON (for trend tracking across sessions):

```powershell
.venv\Scripts\python skills/strategy-comparison/compare_strategies.py --save-json results/comparison.json
```

Custom bridge URL or minimum sample threshold:

```powershell
.venv\Scripts\python skills/strategy-comparison/compare_strategies.py --base-url http://192.168.1.48:8080 --min-samples 20
```

---

## Example Output

```
====================================================
  WiFi Strategy Comparison: dwell vs auto
====================================================
                                 dwell          auto
  --------------------------------------------------
  Attempts                          42            41
  Successes                         42            40
  Timeouts                           0             1
  Success rate                    100%           98%
  --------------------------------------------------
  Min connect time              4461ms        5821ms
  Avg connect time              5213ms        6498ms
  Median connect                5183ms        6421ms
  P95 connect time              5891ms        7203ms
  Max connect time              6102ms        8064ms
  Std deviation                  412ms         621ms
  --------------------------------------------------
  Channel 1 hits                    14            13
  Channel 6 hits                    15            14
  Channel 11 hits                   13            13
  --------------------------------------------------

  Verdict
  -------
  Speed     : dwell is faster by 1285 ms avg (20% faster than auto).
  Reliability: similar (100% vs 98%).
  Verdict   : dwell appears to be the better strategy.

  (Based on 1240 total log entries from bridge)
====================================================
```

---

## Interpreting Results

| Field | What it means |
|---|---|
| Attempts | Total [WIFI-CONNECT] log entries for that path |
| Success rate | % of attempts that resulted in a connected WiFi |
| Avg connect time | Mean time from pulse to WiFi up (successful attempts only) |
| Median connect | Middle value — less affected by outliers than avg |
| P95 connect time | 95th percentile — worst-case outside the top 5% |
| Std deviation | Spread of connect times — lower = more consistent |
| Channel hits | How often each WiFi channel was used on success |

**Verdict reliability**: flagged as unreliable if either path has fewer than `--min-samples` (default 10) attempts. The bridge alternates paths so counts should stay roughly equal over time.

**Sample growth**: the bridge log buffer holds 1000 entries. Once full, older entries are overwritten. Run `--save-json` periodically to preserve snapshots before the buffer rolls over.

---

## When to Run

- After the bridge has been running for a few days to accumulate enough samples.
- When considering changing the default connect strategy in `wifi_bridge.cpp`.
- When investigating intermittent WiFi issues — check if one path has more timeouts.

---

## Relationship to Log Analysis Skill

`skills/log-analysis/analyze_bridge_logs.py` is the general-purpose log reader — it shows all connection attempts in chronological order with per-attempt detail.

This skill (`compare_strategies.py`) is purpose-built for the dwell vs auto question: it only produces the comparison table and verdict, with no per-attempt dump. Use both together when needed.

---

## Source of Truth

Connect strategy parameters are defined in `firmware/esp32_inverter_bridge/wifi_bridge.cpp`.  
Full architecture description: `AGENTS.md` → "Key Design Pattern: WifiConnectionManager".
