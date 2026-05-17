# Wiring README (ESP32-Mini-1)

This guide wires an ESP32-Mini-1 dev board to an ENC28J60 SPI Ethernet module, with one additional output pin used to wake inverter WiFi.

## Pin mapping

The firmware uses the ESP32 VSPI bus with explicit pin assignment. The default pins in the sketch are:

| ENC28J60 | ESP32 GPIO | Notes |
|---|---:|---|
| VCC | 3V3 | 3.3V only |
| GND | GND | Common ground |
| SO (MISO) | GPIO19 | VSPI MISO |
| SI (MOSI) | GPIO23 | VSPI MOSI |
| SCK | GPIO18 | VSPI SCK |
| CS | GPIO5 | VSPI CS |
| INT (optional) | – | Not required by current firmware |

Recovery output pin:

| Function | ESP32 GPIO |
|---|---:|
| Inverter WiFi wake pulse output | GPIO4 |

> **Board-specific note:** The ESP32-Mini-1 module exposes GPIO numbers directly on its breakout pads. Confirm the physical pad labels on your specific carrier/dev board match the GPIO numbers above. If your board silkscreen uses D-pin labels (e.g. D5, D6), cross-reference your board's pinout diagram.

## Adjusting pin assignments

If any of the default GPIO numbers conflict with your board's reserved or occupied pins, change the corresponding `PIN_ETH_*` and `PIN_INVERTER_WIFI_WAKE` constants at the top of `esp32_inverter_bridge.ino` and rewire accordingly.

Pins to avoid on most ESP32 variants:

| GPIO | Reason to avoid |
|---|---|
| GPIO0 | Boot strapping (hold LOW = download mode) |
| GPIO2 | Boot strapping |
| GPIO12 | Boot strapping (affects flash voltage on some modules) |
| GPIO15 | Boot strapping |
| GPIO6–11 | Connected to internal flash on most ESP32 modules |

## Electrical notes

- Use **3.3V logic only** for both ESP32 and ENC28J60.
- Add a **100 nF decoupling capacitor** close to ENC28J60 VCC/GND.
- Keep SPI wires short (< 10 cm recommended).
- Use one power supply rated at least 500 mA at 3.3V equivalent board capability.

## Single-supply recommendation

Typical combined peak current is around 300 mA; budget margin is important.

- If powering via USB dev board input, use a stable USB source (5V, ≥ 1 A).
- Ensure your dev board regulator can supply ENC28J60 reliably.
- The ESP32 draws more peak current than the ESP8266; a quality cable and power supply matter more here.
