# Wiring README

This guide wires an ESP8266 dev board to an ENC28J60 SPI Ethernet module, with one additional output pin used to wake inverter WiFi.

## Pin mapping

Use the ESP8266 HSPI pins:

| ENC28J60 | ESP8266 | NodeMCU label | Notes |
|---|---:|---|---|
| VCC | 3V3 | 3V3 | 3.3V only |
| GND | GND | G | Common ground |
| SO (MISO) | GPIO12 | D6 | SPI MISO |
| SI (MOSI) | GPIO13 | D7 | SPI MOSI |
| SCK | GPIO14 | D5 | SPI clock |
| CS | GPIO15 | D8 | SPI chip select |
| INT (optional) | GPIO4 or GPIO5 | D2 or D1 | Not required by current firmware |

Recovery output pin:

| Function | ESP8266 pin | NodeMCU label |
|---|---:|---|
| Inverter WiFi wake pulse output | GPIO5 | D1 |

## Electrical notes

- Use **3.3V logic only** for ESP8266 and ENC28J60.
- Add a **100 nF decoupling capacitor** close to ENC28J60 VCC/GND.
- Keep SPI wires short.
- Use one power supply rated at least 500 mA at 3.3V equivalent board power capability.

## Single-supply recommendation

Typical combined peak current is around 250 mA, but budget margin is important.

- If powering via USB dev board input, use a stable USB source (5V, >=1A).
- Ensure your dev board regulator can supply ENC28J60 reliably.

## Boot pin caution

GPIO15 is a boot-strap pin on ESP8266. The ENC28J60 CS connection on GPIO15 is common and usually works on dev boards, but if boot issues occur:

1. Confirm proper module pull states during reset.
2. Keep CS line stable at boot.
3. If needed, adapt firmware and hardware to a different CS strategy supported by your board.
