# Test README

This document describes validation of the current ESP32 bridge firmware, including WiFi recovery timing measurements.

## Preconditions

- Firmware flashed successfully.
- Ethernet link active.
- Bridge reachable on port 8080.
- Inverter WiFi availability is known (daytime vs nighttime).

## Quick API Sanity

- GET / should return endpoint discovery.
- GET /api/health should always respond when bridge is alive.
- GET /api/logs should return log entries.

## Inverter-Dependent Endpoints

These require inverter WiFi to be available:

- GET /api/info
- POST /api/power
- POST /api/inverter/fetch

If inverter is off/unavailable, 502 responses are expected.

## Recovery Measurement Workflow (Critical)

Use this exact cycle:

1. GET /pulse
2. POST /wifi/off
3. wait 2-3 seconds
4. repeat

Reason:

- OFF -> double press -> ON
- ON -> single press -> OFF
- ON -> double press leads to undefined inverter state and invalid measurements

## Python Scripts

### Clean run

python run_clean_tests.py 10

### Analyze logs

python analyze_logs.py

## Baseline Reference (before latest optimization)

- Success rate: 100% (with correct cycle)
- Time range: ~3752-4752 ms
- Average: ~4252 ms
- Channels observed: 1, 6, 11

## What Changed In Firmware

- Recovery timeout now 8000 ms
- Scan dwell 500 ms, settle 100 ms
- WiFi radio reset before each measurement
- Scan-before-connect uses discovered channel + BSSID
- Recovery scan logs reduced to inverter-focused entries
- /wifi/off endpoint restored

## Tomorrow Continuation Checklist

1. Wait until inverter WiFi is available.
2. Run 10 clean measurements.
3. Analyze logs and compare against ~4252 ms baseline.
4. Verify logs show channel/BSSID-assisted connection path.
5. Tune retry policy and scan timings if needed.

## Troubleshooting

| Symptom | Likely cause | Action |
|---|---|---|
| /wifi/off returns 404 | Old firmware image running | Re-upload latest firmware |
| /api/logs JSON decode errors | oversized/truncated response from excessive logs | verify reduced recovery logging is active |
| /pulse timeouts at night | inverter WiFi unavailable | rerun daytime |
| /api/info returns 502 after boot | first poll not done yet | wait 20-30s |
