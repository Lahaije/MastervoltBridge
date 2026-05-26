# Changelog

## [Unreleased] — Home Assistant MQTT integration

### Added

- **MQTT bridge module** (`mqtt_bridge.{h,cpp}`) — PubSubClient over UIPEthernet, persisted broker IP + HA-enable flag in NVS namespace `mqtt`.
- **HA MQTT Discovery** — bridge announces itself as a single HA device "Mastervolt SOLADIN 1500" with retained discovery payloads under `homeassistant/<component>/mastervolt_soladin_1500/...`.
- **Cache + dual-topic pattern** for write-only inverter settings:
  - `InverterMonitor` now caches `power_limit` and `shadow` in RAM + NVS (namespace `invmon`) on every successful `POST /api/power` / `POST /api/shadow`.
  - `MqttBridge` publishes the cache to retained `state/power_limit` and `state/shadow` topics so HA tiles repopulate after every restart.
  - HA Discovery declares both `cmd_t` (writes) and `stat_t` (mirror) for the Power Limit number and Shadow switch entities.
- **`POST /api/shadow`** and **`GET /api/shadow`** — toggle / read the inverter's shadow function.
- **`GET /api/mqtt` / `POST /api/mqtt`** — runtime config of MQTT broker IP and HA-enable flag.
- **`/24` broker auto-discovery** — on each Ethernet DHCP lease, the bridge probes its local subnet one host per main-loop iteration for TCP/1883 and persists the first hit.
- **`/api/info` extensions** — exposes `power_limit_watts` + `power_limit_known` + `shadow` + `shadow_known` from the bridge cache.
- **`/api/health` extensions** — adds `ha_mqtt_enabled`, `mqtt_broker`, `mqtt_connected`, `mqtt_scanning`.

### Changed

- API surface grew from 9 to **13 endpoints**.
- `docs/HOME_ASSISTANT.md` rewritten: MQTT Discovery is now the recommended path; REST polling kept as fallback.
- `docs/API_REFERENCE.md`, `docs/TEST_README.md`, `AGENTS.md`, and `skills/api-validation/validate_api.py` updated for the new endpoints and behaviour.

### Fixed

- `DHCP_HOSTNAME` defined in `settings.cpp` — UIPEthernet 2.0.12 declares it `extern` but provides no definition, which broke the link step.

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

#### API Endpoints (9)

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
