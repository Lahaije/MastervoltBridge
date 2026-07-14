#!/usr/bin/env python3
"""Generic parser for bridge log entries.

Parses a log entry stream into two core outputs:
1) timestamped non-poll operational events
2) timestamped power samples from poll readings
"""

from __future__ import annotations

import re
from dataclasses import dataclass, field
from typing import Dict, List


_INVERTER_PREFIX_RE = r"INVERTER-(?:CONTROLLER|MONITOR)"
_POLL_RE = re.compile(rf"\[{_INVERTER_PREFIX_RE}\] Poll #(\d+): Status=(\S+) Power=([\d.]+)W")


@dataclass
class ParsedEvent:
    timestamp_ms: int
    kind: str
    message: str
    from_state: str | None = None
    to_state: str | None = None
    streak_seconds: int | None = None
    interval_seconds: int | None = None


@dataclass
class PowerSample:
    timestamp_ms: int
    poll_number: int
    status: str
    power_w: float


@dataclass
class ParsedLogData:
    events: List[ParsedEvent] = field(default_factory=list)
    power_samples: List[PowerSample] = field(default_factory=list)
    skipped_polls: int = 0


def parse_logs(entries: List[Dict]) -> ParsedLogData:
    """Parse log entries into generic events and power samples."""
    data = ParsedLogData()

    api_power_re = re.compile(r"\[API\]\s+POST\s+/api/power")
    api_shadow_re = re.compile(r"\[API\]\s+POST\s+/api/shadow")
    api_pulse_re = re.compile(r"\[API\]\s+GET\s+/pulse")
    api_wifi_off_re = re.compile(r"\[API\].*/wifi/off")
    power_limit_read_re = re.compile(r"Power limit read:\s*(\d+)W")
    state_change_re = re.compile(
        rf"\[{_INVERTER_PREFIX_RE}\]\s+(?:Link state|State):\s+([A-Z]+)\s+->\s+([A-Z]+)"
        r"(?:\s*\(streak=(\d+)s,\s*interval=(\d+)s\)|\s+streak=(\d+)s\s+interval=(\d+)s)"
    )
    backoff_interval_re = re.compile(
        rf"\[{_INVERTER_PREFIX_RE}\] Backoff: retry interval -> (\d+)s"
    )

    for e in entries:
        msg = str(e.get("message", ""))
        ts = int(e.get("timestamp_ms", 0))

        poll_m = _POLL_RE.search(msg)
        if poll_m:
            data.power_samples.append(
                PowerSample(
                    timestamp_ms=ts,
                    poll_number=int(poll_m.group(1)),
                    status=poll_m.group(2),
                    power_w=float(poll_m.group(3)),
                )
            )
            continue

        if "No WiFi connection; skipping poll iteration" in msg:
            data.skipped_polls += 1
            data.events.append(ParsedEvent(ts, "poll_skipped", msg))
            continue

        if api_power_re.search(msg):
            data.events.append(ParsedEvent(ts, "power_update_request", msg))
            continue

        power_m = power_limit_read_re.search(msg)
        if power_m:
            watts = int(power_m.group(1))
            data.events.append(ParsedEvent(ts, "power_limit_active", f"Power limit active: {watts}W"))
            continue

        if "Queued power" in msg:
            data.events.append(ParsedEvent(ts, "power_update_queued", msg))
            continue

        if "power command delivered" in msg.lower():
            data.events.append(ParsedEvent(ts, "power_update_delivered", msg))
            continue

        if "setPower".lower() in msg.lower():
            data.events.append(ParsedEvent(ts, "power_set_result", msg))
            continue

        if api_shadow_re.search(msg):
            data.events.append(ParsedEvent(ts, "shadow_update_request", msg))
            continue

        if api_pulse_re.search(msg):
            data.events.append(ParsedEvent(ts, "pulse_request", msg))
            continue

        if api_wifi_off_re.search(msg):
            data.events.append(ParsedEvent(ts, "wifi_off_request", msg))
            continue

        if "Failed to fetch /home" in msg:
            data.events.append(ParsedEvent(ts, "inverter_fetch_failed", msg))
            continue

        if "Inverter recovered; resuming normal poll interval" in msg:
            data.events.append(ParsedEvent(ts, "inverter_recovered", msg))
            continue

        if "[WIFI-CONNECT] start" in msg:
            data.events.append(ParsedEvent(ts, "wifi_connect_start", msg))
            continue

        if "[WIFI-CONNECT] complete" in msg:
            data.events.append(ParsedEvent(ts, "wifi_connect_complete", msg))
            continue

        state_m = state_change_re.search(msg)
        if state_m:
            from_state = state_m.group(1)
            to_state = state_m.group(2)
            streak_s = int(state_m.group(3) or state_m.group(5))
            interval_s = int(state_m.group(4) or state_m.group(6))
            data.events.append(
                ParsedEvent(
                    ts,
                    "link_state_change",
                    f"State change: {from_state}->{to_state} streak={streak_s}s interval={interval_s}s",
                    from_state=from_state,
                    to_state=to_state,
                    streak_seconds=streak_s,
                    interval_seconds=interval_s,
                )
            )
            continue

        backoff_m = backoff_interval_re.search(msg)
        if backoff_m:
            interval_s = int(backoff_m.group(1))
            data.events.append(
                ParsedEvent(
                    ts,
                    "backoff_interval_change",
                    f"Backoff interval changed: {interval_s}s",
                    interval_seconds=interval_s,
                )
            )
            continue

    return data
