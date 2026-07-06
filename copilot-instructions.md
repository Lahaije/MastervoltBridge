# Copilot Instructions — ESP32 Inverter Bridge

## Project Context

This is the firmware for an **ESP32-S3 WiFi-to-Ethernet bridge** connecting WiFi-only inverters (Mastervolt SOLADIN 1500) to Home Assistant via Ethernet.

**Hardware:** ESP32-S3 + ENC28J60 Ethernet adapter
- **WiFi:** Station mode connecting to inverter SSID `mastervolt-soladin-0103`
- **Ethernet:** DHCP client on home LAN, HTTP API on port 8080
- **Wake pulse:** GPIO 36 (idle HIGH, active-LOW pulse to wake inverter WiFi)

## For Agents: Start Here

All development tasks use **skills** in `.github/skills/`:

**[→ Read Skills Overview](.github/skills/.instructions.md)**

| Task | Skill |
|---|---|
| Upload code to device | [firmware-upload](.github/skills/firmware-upload/.instructions.md) |
| Investigate logs, power output | [log-analysis](.github/skills/log-analysis/.instructions.md) |
| Test API against docs | [api-validation](.github/skills/api-validation/.instructions.md) |
| Optimize timeout/polling values | [firmware-optimization-loop](.github/skills/firmware-optimization-loop/.instructions.md) |
| Compare WiFi strategies | [strategy-comparison](.github/skills/strategy-comparison/.instructions.md) |
| Update docs after code changes | [documentation-update](.github/skills/documentation-update/.instructions.md) |

## Python Environment

Location: `.venv/`

All scripts use explicit path (no activation required):
```powershell
.venv\Scripts\python.exe .github/skills/<skill>/script.py
```

Dependencies: `requests`, `matplotlib`, `pyserial`

## Architecture Overview

**See [AGENTS.md](AGENTS.md) for:**
- Module dependency graph
- Key design patterns (WifiConnectionManager, MqttClient)
- Data model (HomeData, InverterController interface)
- Design gotchas and constraints
- Performance baseline metrics

## Firmware Version

Current: `1.0.0`

**Update location:** `firmware/esp32_inverter_bridge/settings.cpp` — edit `FIRMWARE_VERSION` string

The version is displayed in:
- `/api/device` endpoint
- Web UI device info panel

## Documentation

| File | Purpose |
|---|---|
| `README.md` | Project overview |
| `AGENTS.md` | Architecture & development guide |
| `docs/API_REFERENCE.md` | HTTP endpoint specification |
| `docs/SETUP_README.md` | Hardware and environment setup |
| `docs/WIRING_README.md` | GPIO and pin configuration |
| `docs/ESP32_UPLOAD_README.md` | Firmware upload procedure |
| `docs/TEST_README.md` | Testing and validation |
| `docs/HOME_ASSISTANT.md` | Home Assistant integration |

## Key Principles

1. **Never edit old `SKILL.md` files** — use `.instructions.md` files instead
2. **Use explicit `.venv\Scripts\python.exe` path** — works in any shell without activation
3. **Firmware settings are centralized** — all constants defined in `settings.cpp/h`
4. **Keep docs in sync** — use documentation-update skill after code changes
5. **Read AGENTS.md for architecture** — don't duplicate module knowledge in comments

## Next Steps

1. Pick a skill from the table above
2. Read its `.instructions.md` file
3. Follow the quick-start commands
4. Refer to [AGENTS.md](AGENTS.md) for module details if needed
