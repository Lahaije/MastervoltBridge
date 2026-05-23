# Changelog

## [Unreleased]

### Firmware Behavior Updates

- Added `InverterLinkState` enum (`STARTING` / `ONLINE` / `RETRYING` / `BACKOFF` / `DORMANT`)
  driven by failure-streak duration with named thresholds (5 min, 20 min) and
  retry intervals (60 s in BACKOFF, 600 s in DORMANT).
- Added single `onLinkStateTransition(from, to, streakMs)` event hook in
  `InverterMonitor`; the polling loop is now a pure dispatcher.
- Added long-disconnect recovery action: on `BACKOFF` or `DORMANT` -> `ONLINE`,
  the monitor queues a MAX-power reset (`queueMaxPowerAfterLongDisconnect`)
  and attempts immediate delivery, so the inverter is restored to full
  production even if it rebooted or its own power-limit timer elapsed
  during the outage. User-initiated queued commands take precedence.
- Debug-mode transition logs: `[INVERTER-MONITOR] Link state: FROM -> TO (streak=..., interval=...)`.
- Added typed lock hierarchy in `lock_guard.h`. All four mutexes
  (`pollingConfigMutex`, `dataMutex`, `powerStateMutex`,
  `wifiOperationMutex`) are now acquired through `ScopedLock<LockRank>`,
  which enforces ascending-rank acquisition per task and logs any
  violation as `[LOCK-HIERARCHY] VIOLATION` (visible in `/api/logs`).
  `fetchInverterData()` additionally asserts that no business-state lock
  is held at entry.

### API Behavior Updates

- `GET /api/info` now always returns `200 OK`. A new `ready` boolean
  field is `true` once at least one poll has succeeded; while `false`,
  telemetry string fields are empty but `power_limit`, `firmware_version`,
  and `poll_interval_*` are still populated. The previous `502 "No
  inverter telemetry data available yet"` startup-race response is gone.
- `GET /api/health` now includes:
	- `inverter_link_state` (`STARTING` / `ONLINE` / `RETRYING` / `BACKOFF` / `DORMANT`)
	- `inverter_failure_streak_ms`
	- `inverter_retry_interval_ms`

### Documentation Updates

- `docs/API_REFERENCE.md`: documented the link-state machine, new
  `/api/health` fields, the always-200 behavior of `/api/info`, and the
  new `ready` field.
- `docs/MAX_POWER_BEHAVIOR.md`: documented the long-disconnect recovery
  transition event, added Example D, updated `/api/info` startup note.
- `docs/LOCKING_MODEL.md`: documented the `LockRank` hierarchy, the
  runtime enforcement performed by `ScopedLock`, and the
  `assertNoStateLocksHeld()` check at the entry of `fetchInverterData()`.
- `AGENTS.md`: updated `InverterMonitor` summary with link state machine
  and transition-event binding; corrected `/api/info` startup note.
- `docs/ESP32_UPLOAD_README.md`, `docs/TEST_README.md`,
  `skills/api-validation/SKILL.md`: updated `/api/info` expectations.

## [0.1.0-alpha2] - May 23, 2026

### Firmware Behavior Updates

- Replaced `WifiConnectionManager`-driven flow with a dedicated WiFi connection worker task.
- Added `fetchInverterData(..., waitForConnection)` behavior split:
	- blocking for polling (`waitForConnection=true`)
	- fail-fast for API handlers (`waitForConnection=false`)
- Added power-limit state machine with explicit desired/confirmed state tracking.
- Added queued power command delivery with retry-on-poll behavior.
- Added automatic max-power reset timer (`POWER_LIMIT_RESET_MINUTES`) for sub-max requests.
- Added queued command expiry window (`POWER_COMMAND_EXPIRY_MS`) for user-initiated commands.

### API Behavior Updates

- `POST /api/power` now returns:
	- `200` when delivered immediately
	- `202` when queued due to inverter WiFi unavailability
- `GET /api/info` now includes `power_limit` object:
	- `desired`
	- `confirmed`
	- `queued`
	- `reset_timer_minutes`

### Documentation Updates

- Updated `README.md`, `AGENTS.md`, and core docs to reflect current architecture.
- Added `docs/LOCKING_MODEL.md` with lock ownership and ordering rules.
- Added `docs/MAX_POWER_BEHAVIOR.md` with exact power-limit and auto-reset behavior.

## [0.1.0-alpha1] — May 19, 2026

### Initial Alpha Release

This is the first alpha release of the ESP32 WiFi-to-Ethernet bridge for Mastervolt SOLADIN series inverters.

#### Features

- **WiFi-to-Ethernet Bridge**: Connects WiFi-only inverters to Home Assistant via Ethernet
- **REST API**: 9 endpoints for inverter control, status, and diagnostics
- **WiFi State Machine**: Dual-path connection strategy (dwell/auto) with A/B performance logging
- **Stepped Backoff**: Intelligent retry strategy for overnight unavailability (20s/1m/10m intervals)
- **Streaming Logs**: Circular 1000-entry buffer with streaming JSON endpoint (no heap exhaustion)
- **Debug Mode**: Runtime-switchable verbose HTTP logging
- **GPIO Control**: Wake/pulse inverter WiFi module via button simulation on GPIO 36

#### Hardware

- **Microcontroller**: ESP32-S3 (8 MB flash, 2.5 MB PSRAM)
- **Ethernet**: ENC28J60 (SPI interface)
- **Power**: USB or Ethernet PoE (when implemented)

#### API Endpoints (9 at alpha1)

| Method | Path | Purpose |
|--------|------|---------|
| GET | `/` | API discovery |
| GET | `/api/health` | Bridge WiFi/Ethernet status |
| GET | `/api/logs` | Log buffer (1000 entries, streamed) |
| GET | `/api/info` | Latest cached inverter telemetry |
| POST | `/api/power` | Set inverter power (0–1575 W) |
| POST | `/api/inverter/fetch` | Fetch arbitrary inverter path |
| POST | `/wifi/off` | Single button press to turn inverter WiFi off |
| GET | `/pulse` | GPIO wake pulse + forced reconnect |
| POST | `/api/debug` | Enable/disable verbose HTTP 200 logging |

#### Known Limitations & Alpha Notes

1. **No Encryption**: API operates over plain HTTP on port 8080. Use on trusted networks only.
2. **Static Configuration**: Inverter SSID, IP, and GPIO pins are compile-time settings in `settings.cpp`.
3. **No OTA Updates**: Firmware updates require physical USB access.
4. **Limited Filtering**: `/api/logs` has no time-range or keyword filters.
5. **Single Inverter**: Bridge supports one inverter per instance.
6. **WiFi-Only Inverter Models**: Tested with Mastervolt SOLADIN 1500. Other models may require adaptation.

#### Documentation

- **[README.md](README.md)** — Quick start and API overview
- **[docs/SETUP_README.md](docs/SETUP_README.md)** — Hardware assembly and flashing
- **[docs/WIRING_README.md](docs/WIRING_README.md)** — Pin table and electrical details
- **[docs/API_REFERENCE.md](docs/API_REFERENCE.md)** — Complete endpoint reference
- **[docs/ESP32_UPLOAD_README.md](docs/ESP32_UPLOAD_README.md)** — Upload methods and troubleshooting
- **[docs/TEST_README.md](docs/TEST_README.md)** — Validation checklist
- **[AGENTS.md](AGENTS.md)** — Architecture and design details

#### Testing

- ✅ API endpoint discovery and request routing
- ✅ WiFi connection attempts (dwell/auto paths)
- ✅ Inverter polling and data parsing
- ✅ Power command validation and execution
- ✅ Log streaming without heap exhaustion
- ✅ GPIO wake/pulse sequence
- ✅ Debug mode toggle
- ⚠️ Long-term stability (overnight backoff) — limited field data

#### How to Get Started

1. **Assemble hardware**: ESP32-S3 + ENC28J60 + GPIO 36 to inverter button
2. **Clone repository**: `git clone https://github.com/your-repo/MastervoltBridge.git`
3. **Set up Python environment**: `uv sync`
4. **Configure inverter settings**: Edit `firmware/esp32_inverter_bridge/settings.cpp` (SSID, IP, GPIO)
5. **Flash firmware**: `.venv\Scripts\python skills/firmware-upload/upload_firmware.py`
6. **Verify**: `curl http://192.168.1.48:8080/api/health`
7. **Integrate with Home Assistant**: See `docs/SETUP_README.md` for examples

#### Feedback & Bug Reports

This is an alpha release. Known issues, suggestions, and contributions are welcome. Please report issues via GitHub Issues or pull requests.

#### License

Licensed under the GNU General Public License v3.0. See [LICENSE](LICENSE) for details.
