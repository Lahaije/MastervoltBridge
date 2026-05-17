# Upload README (Arduino IDE)

This guide explains how to compile and upload the ESP8266 firmware.

## 1) Install Arduino IDE

Install Arduino IDE 2.x from the official Arduino website.

## 2) Add ESP8266 board support

1. Open Arduino IDE.
2. Go to **File > Preferences**.
3. In **Additional Boards Manager URLs**, add:
   - `http://arduino.esp8266.com/stable/package_esp8266com_index.json`
4. Go to **Tools > Board > Boards Manager**.
5. Search for **esp8266** and install **ESP8266 by ESP8266 Community**.

## 3) Install required libraries

Go to **Sketch > Include Library > Manage Libraries** and install:

- `UIPEthernet`

`ESP8266WiFi` and `ESP8266HTTPClient` are provided by ESP8266 board package.

## 4) Open the sketch

Open:

- `esp8266_inverter_bridge/firmware/esp8266_inverter_bridge.ino`

## 5) Configure constants before upload

Edit in sketch:

- `INVERTER_WIFI_SSID`
- `INVERTER_WIFI_PASSWORD`
- Optional: `API_PORT`
- Optional: `ETH_MAC`
- Optional: fallback static IP constants

The inverter endpoint defaults are already configured:

- Host: `10.0.0.1`
- GET `/home`
- POST `/power` with text payload containing integer watts

## 6) Select board and port

1. Connect ESP8266 over USB.
2. In **Tools > Board**, select your ESP8266 board (for example NodeMCU 1.0).
3. In **Tools > Port**, select correct COM port.

Recommended options:

- Upload Speed: 115200 (or stable value for your cable)
- CPU Frequency: 80 MHz

## 7) Verify and upload

1. Click **Verify**.
2. Click **Upload**.
3. Open **Serial Monitor** at 115200 baud.

Expected startup messages include:

- Ethernet initialization
- DHCP address (or fallback static IP)
- WiFi connection attempts to inverter AP
- API server listening port

## 8) After upload

Use the Ethernet IP shown in Serial Monitor for Home Assistant API calls.

Example:

- `http://<esp-ethernet-ip>:8080/api/health`
