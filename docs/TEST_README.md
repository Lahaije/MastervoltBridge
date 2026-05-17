# Test README

This guide validates hardware, networking, API proxy behavior, and inverter WiFi wake pulse behavior.

## Prerequisites

- Firmware uploaded successfully.
- ENC28J60 connected to router.
- ESP8266 connected to inverter WiFi credentials configured in sketch.
- Serial Monitor open at 115200 baud.

## A) Basic boot checks

1. Reset ESP8266.
2. Confirm serial output shows:
   - Ethernet startup.
   - DHCP-assigned Ethernet IP (or fallback static IP).
   - WiFi connection attempts/status.

If DHCP fails repeatedly:

- Check Ethernet cable and router port.
- Check ENC28J60 power stability.

## B) API health check

From a device on your home LAN:

1. Open:
   - `http://<esp-ethernet-ip>:8080/api/health`
2. Confirm JSON fields:
   - `wifi_connected`
   - `ethernet_ip`
   - `inverter_host`
   - `outage_active`
   - `recovery_pulse_sent_for_current_outage`

## C) Inverter info proxy check

1. Call:
   - `GET http://<esp-ethernet-ip>:8080/api/info`
2. Expect plain-text inverter status content from `10.0.0.1/home`.

If you receive 502:

- ESP is likely not connected to inverter WiFi, or inverter is unreachable.

## D) Power limit proxy check

Use one of the following:

Send power command via POST body (plain integer):

```bash
curl -X POST http://<esp-ethernet-ip>:8080/api/power -H "Content-Type: text/plain" --data "1200"
```

Expected result:

- JSON response with `requested_watts` and inverter status.

## E) Outage recovery pulse test

Goal: verify one pulse train per outage cycle.

Pulse definition:

- HIGH for 100 ms
- LOW for 200 ms
- HIGH for 100 ms

### Test method with LED (quick)

1. Connect LED + resistor from recovery pin (GPIO5/D1) to GND.
2. Force outage by turning inverter WiFi off, or changing credentials temporarily.
3. Wait at least outage detection window (~5 s).
4. Observe double blink corresponding to two pulses.
5. Keep outage active and verify no repeated pulse trains occur.
6. Restore inverter WiFi so ESP reconnects.
7. Force outage again and verify one new pulse train occurs.

### Test method with oscilloscope (precise)

Measure GPIO5 waveform during outage trigger and verify durations are approximately 100 ms, 200 ms, 100 ms.

## F) Home Assistant integration sanity check

Use ESP Ethernet API as source in HA sensors/automation:

- Read data from `/api/info`.
- Write power limit using `/api/power`.

## Troubleshooting quick list

- No Ethernet IP: check SPI wiring and power.
- WiFi never connects: verify inverter SSID/password and AP availability.
- 502 responses: inverter endpoint unreachable from ESP.
- Wrong POST behavior: inverter may expect a different payload format; if needed, adapt payload in firmware function `inverterSetPower`.
