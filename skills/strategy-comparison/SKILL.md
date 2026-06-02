---
name: strategy-comparison
description: Compare the dwell and auto WiFi connect paths using passively accumulated bridge logs. Use when checking which path performs better over time.
---

<objective>
Fetch passively accumulated WiFi connect data and produce a side-by-side comparison of the two strategies. This skill is read-only.
</objective>

<quick_start>
Run from the repository root:
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
**Save result as JSON**:
```powershell
.venv\Scripts\python.exe skills/strategy-comparison/compare_strategies.py --save-json results/comparison.json
```

**Custom bridge URL or minimum sample threshold:**
```powershell
.venv\Scripts\python.exe skills/strategy-comparison/compare_strategies.py --base-url http://<bridge-ip>:8080 --min-samples 20
```
</examples>

<validation>
| Field | Meaning |
|---|---|
| Attempts | Total `[WIFI-CONNECT]` log entries for that path |
| Success rate | Percentage of attempts that connected |
| Avg connect time | Mean successful connect time |
| Median connect | Middle successful connect time |
| P95 connect time | 95th percentile successful connect time |
| Std deviation | Spread of successful connect times |
| Channel hits | How often each WiFi channel was used on success |

**Verdict reliability**: flagged as unreliable if either path has fewer than `--min-samples` attempts (default 10).
</validation>

<success_criteria>
Comparison is complete when:
- [ ] Both paths have sufficient samples (≥ min-samples)
- [ ] Statistical table printed with timing distribution
- [ ] Verdict includes speed difference (ms and %) and reliability comparison
- [ ] Recommendation is actionable (keep alternating, or prefer one path)
</success_criteria>
