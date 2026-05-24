---
name: strategy-comparison
description: Compare the dwell vs auto WiFi connect strategies using passively accumulated bridge logs. Produces a side-by-side statistical table and a verdict with recommendation.
---

<objective>
The bridge alternates between two WiFi connect strategies on every reconnect attempt:

- **dwell** — 200 ms scan dwell, uses configured AP hint as fallback.
- **auto** — 500 ms scan dwell, pure auto-discovery (no hint fallback).

Performance data accumulates passively during normal bridge operation. This skill fetches that data and produces a focused side-by-side comparison: sample counts, success rates, connect-time distribution (min/avg/median/P95/max/stdev), channel distribution, and a plain-English verdict.

This skill is **read-only** — it does not trigger `/pulse`, `/wifi/off`, or any state-changing endpoint.
</objective>

<quick_start>
```powershell
# Activate venv (required once per terminal session)
& d:\git\MastervoltBridge\.venv\Scripts\Activate.ps1

# Run comparison
python skills/strategy-comparison/compare_strategies.py
```
</quick_start>

<commands>
Run from the repository root. **The venv MUST be activated first in every terminal session.**

**Standard comparison:**
```powershell
python skills/strategy-comparison/compare_strategies.py
```

**Save result as JSON (for trend tracking):**
```powershell
python skills/strategy-comparison/compare_strategies.py --save-json results/comparison.json
```

**Custom bridge URL or minimum sample threshold:**
```powershell
python skills/strategy-comparison/compare_strategies.py --base-url http://192.168.1.48:8080 --min-samples 20
```
</commands>

<example_output>
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
</example_output>

<interpreting_results>
| Field | Meaning |
|---|---|
| Attempts | Total `[WIFI-CONNECT]` log entries for that path |
| Success rate | % of attempts that resulted in connected WiFi |
| Avg connect time | Mean time from pulse to WiFi up (successful attempts only) |
| Median connect | Middle value — less affected by outliers than avg |
| P95 connect time | 95th percentile — worst-case outside the top 5% |
| Std deviation | Spread of connect times — lower = more consistent |
| Channel hits | How often each WiFi channel was used on success |

**Verdict reliability**: flagged as unreliable if either path has fewer than `--min-samples` (default 10) attempts. The bridge alternates paths so counts should stay roughly equal over time.
</interpreting_results>

<success_criteria>
Strategy comparison is complete when:
- [ ] Logs fetched from bridge API
- [ ] Both paths have sufficient samples (≥ min-samples threshold)
- [ ] Side-by-side table printed with timing and reliability stats
- [ ] Plain-English verdict generated
</success_criteria>
