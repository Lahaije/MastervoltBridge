# ESP32 Wiring README

ESP32 bridge wiring for the current firmware configuration.

## Default Pin Map (from settings.cpp)

| ENC28J60 | ESP32 GPIO |
|---|---:|
| VCC | 3V3 |
| GND | GND |
| SO (MISO) | 10 |
| SI (MOSI) | 11 |
| SCK | 9 |
| CS | 8 |

## Inverter WiFi Control

| Function | ESP32 GPIO |
|---|---:|
| Inverter WiFi wake/control output | 36 |

Pulse pattern defaults:

- HIGH 150 ms
- gap 200 ms
- HIGH 150 ms

## Board Notes

- Verify your exact ESP32 board pinout before wiring.
- If your board cannot use these pins, change values in settings.cpp.

## Stability Notes For Measurement Work

- Keep SPI and control lines short.
- Use stable 3.3V power.
- Confirm pulse pin logic level at the inverter interface if using a driver stage.
