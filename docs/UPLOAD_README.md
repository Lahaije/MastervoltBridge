# Upload README (Arduino IDE)

This guide explains how to compile and upload the ESP32 inverter bridge firmware.

## 1) Install Arduino IDE

Install Arduino IDE 2.x from the official Arduino website.

## 2) Add ESP32 board support

1. Open Arduino IDE.
2. Go to **File > Preferences**.
3. In **Additional Boards Manager URLs**, add:
   - `https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json`
4. Go to **Tools > Board > Boards Manager**.
5. Search for **esp32** and install **esp32 by Espressif Systems**.

## 3) Install required libraries

Go to **Sketch > Include Library > Manage Libraries** and install:

- `UIPEthernet`

`WiFi` and `HTTPClient` are provided by the ESP32 board package — no separate install needed.

## 4) Open the sketch

Open:

- `firmware/esp32_inverter_bridge/esp32_inverter_bridge.ino`

## 5) Configure constants before upload

Edit `settings.cpp`:

- `INVERTER_WIFI_SSID`
- `INVERTER_WIFI_PASSWORD`
- Optional: `API_PORT`
- Optional: `ETH_MAC`
- Optional: `PIN_ETH_*` and `PIN_INVERTER_WIFI_WAKE` if your board uses different GPIO numbers

The inverter endpoint defaults are already configured:

- Host: `10.0.0.1`
- GET `/home`
- POST `/power` with text payload containing integer watts

## 6) Select board and port

1. Connect ESP32 over USB.
2. In **Tools > Board**, select:
   - **ESP32 Arduino > ESP32 Dev Module** — works for most ESP32 carrier boards.
   - If your board has a dedicated entry (e.g. LOLIN D32 Mini), prefer that.
3. In **Tools > Port**, select the correct COM port.

Recommended options:

| Setting | Value |
|---|---|
| Upload Speed | 921600 |
| CPU Frequency | 240 MHz |
| Flash Mode | DIO |
| Flash Size | 4 MB (32 Mb) |
| Partition Scheme | Default 4MB |

## 7) Put board into download mode (if needed)

Some ESP32 boards auto-reset into bootloader mode via USB. If upload fails:

1. Hold the **BOOT** button.
2. Press and release **EN/RESET**.
3. Release **BOOT**.
4. Retry upload immediately.

## 8) Verify and upload

1. Click **Verify**.
2. Click **Upload**.
3. Open **Serial Monitor** at **115200 baud**.

Expected startup messages include:

- Ethernet initialization
- DHCP address (or fallback static IP)
- WiFi connection attempts to inverter AP
- API server listening port

## 9) After upload

Use the Ethernet IP shown in Serial Monitor for Home Assistant API calls.

Example:

- `http://<esp32-ethernet-ip>:8080/api/health`
