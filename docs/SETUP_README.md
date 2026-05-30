# Project Setup

This guide covers everything needed to go from parts to a running bridge: hardware assembly, wiring, software prerequisites, and firmware upload.

---

## 1. Hardware Required

| Component | Notes |
|---|---|
| ESP32-S3 development board | Any variant with accessible SPI GPIO pins |
| ENC28J60 Ethernet module | SPI interface, 3.3V logic |
| Ethernet cable | For connecting bridge to home LAN |
| USB cable | For flashing and power during development |
| Jumper wires | For SPI + control connections |

---

## 2. Wiring

Connect the ENC28J60 to the ESP32 and wire the inverter WiFi control pin (GPIO 36). GPIO 36 idles HIGH and drives an active-LOW button pulse on the inverter's WiFi module (50 ms pulse, 50 ms gap in double-pulse sequence). Wire it through a transistor or opto-isolator if needed to match the inverter's logic level.

Full pin table, electrical notes, and how to change pins: [`docs/WIRING_README.md`](WIRING_README.md).

---

## 3. Software Prerequisites

### Arduino IDE (GUI upload)

1. Download and install [Arduino IDE 2.x](https://www.arduino.cc/en/software).
2. Open **Preferences → Additional boards manager URLs** and add the ESP32 board package URL:
   ```
   https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
   ```
3. Open **Tools → Board → Boards Manager**, search for `esp32`, and install the **esp32 by Espressif Systems** package (2.6.0 or newer).
4. Install the **UIPEthernet** library via **Tools → Manage Libraries → search UIPEthernet**.

### Visual Studio Code (extension-based upload)

1. Install the [Arduino extension for VS Code](https://marketplace.visualstudio.com/items?itemName=vsciot-vscode.vscode-arduino).
2. Follow the extension setup to point it at your Arduino IDE installation.
3. Open the sketch folder `firmware/esp32_inverter_bridge/` in VS Code.
4. Select board **esp32:esp32:esp32s3** and the correct COM port in the bottom status bar.
5. Use **Arduino: Upload** (or the upload button) to compile and flash.

### Python environment (script-based upload)

Used by the automated upload skill. Requires [Python 3.11+](https://www.python.org/) and [UV](https://github.com/astral-sh/uv).

From the repository root:

```powershell
uv sync
```

This creates `.venv/` with all required dependencies (`requests`, etc.).

---


## Important Architecture Note: Thread-Safety and Network I/O

All network I/O (Ethernet, MQTT) is performed exclusively in the `ethernet_bridge` FreeRTOS task. No other task may call UIPEthernet or PubSubClient methods directly. This is required for thread safety due to UIPEthernet's single-threaded design. Telemetry is queued for MQTT delivery and only the latest value is retained if the network is unavailable. The ethernet task stack size is set to 8192 bytes to accommodate MQTT and Ethernet operations.

---

## 4. Configuration

Before flashing, review `firmware/esp32_inverter_bridge/settings.cpp` and confirm:

| Setting | Default | Description |
|---|---|---|
| `INVERTER_WIFI_SSID` | `"mastervolt-soladin-0103"` | WiFi network broadcast by the inverter |
| `INVERTER_HOST` | `"10.0.0.1"` | Inverter IP on its own WiFi network |
| `PIN_INVERTER_WIFI_WAKE` | `36` | GPIO connected to inverter button |
| `API_PORT` | `8080` | Ethernet API port |

If your inverter has a different SSID or IP, change these values before compiling.

---

## 5. Flashing the Firmware

### Option A — Arduino IDE (GUI)

1. Open `firmware/esp32_inverter_bridge/esp32_inverter_bridge.ino` in Arduino IDE.
2. Select **Tools → Board → esp32 → ESP32S3 Dev Module**.
3. Select the correct COM port under **Tools → Port**.
4. Click **Upload** (→).

### Option B — Visual Studio Code

1. Open the repository folder in VS Code.
2. Open `firmware/esp32_inverter_bridge/esp32_inverter_bridge.ino`.
3. Confirm board and port in the status bar.
4. Run **Arduino: Upload** from the command palette or status bar button.

### Option C — Automated script (recommended for repeated uploads)

```powershell
.venv\Scripts\python skills/firmware-upload/upload_firmware.py
```

The script detects the ESP32 automatically on COM9 (override with `--port COMx`), compiles, and uploads. If the device is not found it prints a list of currently visible ports.

For full options see [`skills/firmware-upload/SKILL.md`](../skills/firmware-upload/SKILL.md).

---

## 6. Verify the Bridge Is Running

After flashing, with the Ethernet cable plugged in:

```powershell
curl http://192.168.1.48:8080/api/health
```

Expected response:

```json
{"wifi_connected": false, "ethernet_ip": "192.168.1.48", ...}
```

`wifi_connected` will be `false` until the bridge polls the inverter (~20 s after boot). Wait and call `/api/info` to confirm telemetry is flowing.

Full endpoint reference: [`docs/API_REFERENCE.md`](API_REFERENCE.md).  
Post-flash validation checklist: [`docs/TEST_README.md`](TEST_README.md).
