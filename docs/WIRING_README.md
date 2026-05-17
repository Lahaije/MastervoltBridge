# Wiring README

This guide wires an ESP32 dev board to an ENC28J60 SPI Ethernet module, with one additional output pin used to wake inverter WiFi.

## Pin mapping

The firmware uses the ESP32 VSPI bus with explicit pin assignment. The default pins in `settings.cpp` are:

| ENC28J60 | ESP32 GPIO | Notes |
|---|---:|---|
| VCC | 3V3 | 3.3V only |
| GND | GND | Common ground |
| SO (MISO) | GPIO 10 | SPI MISO |
| SI (MOSI) | GPIO 11 | SPI MOSI |
| SCK | GPIO 9 | SPI SCK |
| CS | GPIO 8 | SPI CS |
| INT (optional) | – | Not required by current firmware |

Recovery output pin:

| Function | ESP32 GPIO |
|---|---:|
| Inverter WiFi wake pulse output | GPIO 36 |

> **Board-specific note:** Confirm the physical pad labels on your specific carrier/dev board match the GPIO numbers above. If your board silkscreen uses D-pin labels, cross-reference your board's pinout diagram.

## Adjusting pin assignments

If any of the default GPIO numbers conflict with your board's reserved or occupied pins, change the corresponding `PIN_ETH_*` and `PIN_INVERTER_WIFI_WAKE` constants in `settings.cpp` and rewire accordingly.

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
