# ESP32 Inverter WiFi-to-Ethernet Bridge

Bridge firmware connecting WiFi-only inverters (Mastervolt SOLADIN series) to Home Assistant over Ethernet. Runs on ESP32-S3 + ENC28J60, exposes a REST API on port 8080.

## Hardware

- ESP32-S3 + ENC28J60 Ethernet module
- Inverter SSID: `mastervolt-soladin-0103` / IP `10.0.0.1`
- Ethernet API: `192.168.1.48:8080`
- WiFi wake pin: GPIO 36

## API Endpoints

| Method | Path | Purpose |
|---|---|---|
| GET | / | API discovery |
| GET | /api/health | Bridge WiFi/Ethernet status |
| GET | /api/logs | Circular log buffer |
| GET | /api/info | Latest cached inverter telemetry |
| POST | /api/power | Set inverter power (0–1575 W) |
| POST | /api/inverter/fetch | Fetch arbitrary inverter path |
| POST | /wifi/off | Single press to turn inverter WiFi off |
| GET | /pulse | GPIO double-press wake + forced reconnect; returns `{"reconnected": bool}` |
| POST | /api/debug | Enable or disable verbose HTTP 200 success logging |

Full request/response schemas: [`docs/API_REFERENCE.md`](docs/API_REFERENCE.md).

## WiFi Connection Architecture

The bridge uses a `WifiConnectionManager` singleton that pulses the inverter's WiFi module before every reconnect and alternates between two named connect strategies (**dwell** ~5 s / **auto** ~6.5 s). Both strategies log structured entries for passive A/B performance analysis. See [`AGENTS.md`](AGENTS.md) for full design details.

```powershell
.venv\Scripts\python skills/log-analysis/analyze_bridge_logs.py
```

## Build and Upload

```powershell
# Recommended (auto COM-port detection):
.venv\Scripts\python skills/firmware-upload/upload_firmware.py

# Or direct arduino-cli:
arduino-cli compile --fqbn esp32:esp32:esp32s3 firmware/esp32_inverter_bridge
arduino-cli upload  --fqbn esp32:esp32:esp32s3 --port COM9 firmware/esp32_inverter_bridge
```

Full setup and IDE instructions: [`docs/SETUP_README.md`](docs/SETUP_README.md).

## Documentation Index

- [`docs/SETUP_README.md`](docs/SETUP_README.md) — Hardware assembly, wiring, prerequisites, flash instructions
- [`docs/WIRING_README.md`](docs/WIRING_README.md) — Pin table and electrical notes
- [`docs/ESP32_UPLOAD_README.md`](docs/ESP32_UPLOAD_README.md) — Upload procedure and post-flash verification
- [`docs/API_REFERENCE.md`](docs/API_REFERENCE.md) — Full endpoint reference
- [`docs/TEST_README.md`](docs/TEST_README.md) — Validation checklist and troubleshooting
- [`AGENTS.md`](AGENTS.md) — Architecture reference for agents and developers
- [`TECHNICAL_DEBT.md`](TECHNICAL_DEBT.md) — Known limitations, TODOs, and future enhancements
- [`RELEASE_INVENTORY.md`](RELEASE_INVENTORY.md) — Release file audit and packaging guide

