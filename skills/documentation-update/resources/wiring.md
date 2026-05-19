# Resource: docs/WIRING_README.md

**What it documents**: Physical pin assignments between the ESP32 and the ENC28J60 Ethernet adapter, the inverter WiFi wake/control GPIO pin, and pulse timing values.

**Source of truth**: `firmware/esp32_inverter_bridge/settings.cpp` — constants `PIN_ETH_SCK`, `PIN_ETH_MISO`, `PIN_ETH_MOSI`, `PIN_ETH_CS`, `PIN_INVERTER_WIFI_WAKE`, `PULSE_HIGH_MS`, `PULSE_GAP_MS`.

**Update when**:
- Any of the above GPIO pin constants in `settings.cpp` changes.
- Pulse timing values (`PULSE_HIGH_MS`, `PULSE_GAP_MS`) change.
- The board is switched to a different ESP32 variant with a different pin layout.

**Do NOT update for**: Firmware logic changes, API changes, WiFi strategy changes, library updates — none of these affect the wiring tables.
