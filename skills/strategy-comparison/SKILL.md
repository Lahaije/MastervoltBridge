---
name: strategy-comparison
description: Compare the dwell vs auto WiFi connect strategies using passively accumulated bridge logs. Use when evaluating which connect path performs better. Produces a side-by-side statistical table and a verdict with recommendation.
---

<objective>
Fetch passively accumulated WiFi connect performance data and produce a focused side-by-side comparison of the two strategies: sample counts, success rates, connect-time distribution (min/avg/median/P95/max/stdev), channel distribution, and a plain-English verdict.

This skill is **read-only** — it does not trigger `/pulse`, `/wifi/off`, or any state-changing endpoint.
</objective>

<quick_start>
Run from the repository root. The explicit venv python works in any shell — no activation required:
```powershell
.venv\Scripts\python.exe skills/strategy-comparison/compare_strategies.py
```
</quick_start>

<context>
The bridge alternates between two WiFi connect strategies on every reconnect attempt:
- **dwell** — 200 ms scan dwell, uses configured AP hint as fallback.
- **auto** — 500 ms scan dwell, pure auto-discovery (no hint fallback).
</context>

<examples>
**Save result as JSON** (for trend tracking across sessions):
```powershell
.venv\Scripts\python.exe skills/strategy-comparison/compare_strategies.py --save-json results/comparison.json
```

**Custom bridge URL or minimum sample threshold:**
```powershell
.venv\Scripts\python.exe skills/strategy-comparison/compare_strategies.py --base-url http://192.168.1.48:8080 --min-samples 20
```
</examples>

<validation>
| Field | What it means |
|---|---|
| Attempts | Total `[WIFI-CONNECT]` log entries for that path |
| Success rate | % of attempts that resulted in connected WiFi |
| Avg connect time | Mean time from pulse to WiFi up (successful only) |
| Median connect | Middle value — less affected by outliers than avg |
| P95 connect time | 95th percentile — worst-case outside the top 5% |
| Std deviation | Spread of connect times — lower = more consistent |
| Channel hits | How often each WiFi channel was used on success |

**Verdict reliability**: flagged as unreliable if either path has fewer than `--min-samples` (default 10) attempts. The bridge alternates paths so counts should stay roughly equal over time.
</validation>

<success_criteria>
Comparison is complete when:
- [ ] Both paths have sufficient samples (≥ min-samples)
- [ ] Statistical table printed with timing distribution
- [ ] Verdict includes speed difference (ms and %) and reliability comparison
- [ ] Recommendation is actionable (keep alternating, or prefer one path)
</success_criteria>
