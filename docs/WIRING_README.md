# Wiring README

Current firmware defaults are defined in firmware/esp32_inverter_bridge/settings.cpp.

## ENC28J60 SPI Mapping (Current Defaults)

| ENC28J60 | ESP32 GPIO |
|---|---:|
| VCC | 3V3 |
| GND | GND |
| SO (MISO) | 10 |
| SI (MOSI) | 11 |
| SCK | 9 |
| CS | 8 |

## Inverter WiFi Control Pin

| Function | ESP32 GPIO |
|---|---:|
| Inverter WiFi wake/control output | 36 |

Signal behavior:

- Active pulse pattern
- PULSE_HIGH_MS = 50
- PULSE_GAP_MS = 50

## Practical Notes

- Keep wiring short and stable; pulse integrity matters for deterministic inverter button behavior.
- Ensure common ground between ESP32 and inverter control interface.
- If using a transistor/opto interface to emulate button press, verify logic polarity and pulse timing at the inverter side.

## Power and Electrical Notes

- Use 3.3V logic levels only.
- Use a stable USB supply/cable.
- Add local decoupling near ENC28J60.

## If You Change Pins

Update these constants in settings.cpp and rewire:

- PIN_ETH_SCK
- PIN_ETH_MISO
- PIN_ETH_MOSI
- PIN_ETH_CS
- PIN_INVERTER_WIFI_WAKE
