#ifndef SETTINGS_H
#define SETTINGS_H

#include <Arduino.h>
#include <UIPEthernet.h>

// Compile-time kill switch for the MQTT bridge.
// Set to 1 to compile MQTT out entirely (no UIPEthernet socket usage).
// Used while bisecting the ENC28J60 wedge that left the bridge unreachable
// overnight. Once MQTT is rebuilt with a safer Ethernet pattern this can
// flip back to 0.
#ifndef MQTT_BRIDGE_DISABLED
#define MQTT_BRIDGE_DISABLED 1
#endif

class Logger;

// User configuration
extern const char* INVERTER_WIFI_SSID;
extern const char* INVERTER_WIFI_PASSWORD;
extern const char* INVERTER_HOST;
extern const uint16_t API_PORT;

// Optional AP hint to speed up WiFi association.
// When enabled, the bridge attempts directed association to known channels+BSSID
// (rotating through the list) before falling back to normal SSID scanning behavior.
extern const bool INVERTER_WIFI_AP_HINT_ENABLED;
extern const uint8_t INVERTER_WIFI_AP_HINT_CHANNELS[];
extern const uint8_t INVERTER_WIFI_AP_HINT_CHANNEL_COUNT;
extern const uint8_t INVERTER_WIFI_AP_HINT_BSSID[6];

// ENC28J60 SPI pin mapping
extern const uint8_t PIN_ETH_SCK;
extern const uint8_t PIN_ETH_MISO;
extern const uint8_t PIN_ETH_MOSI;
extern const uint8_t PIN_ETH_CS;

// Recovery pulse output pin (active HIGH)
extern const uint8_t PIN_INVERTER_WIFI_WAKE;

// Pulse pattern
extern const uint16_t PULSE_HIGH_MS;
extern const uint16_t PULSE_GAP_MS;

// WiFi bridge polling
extern const uint32_t WIFI_BRIDGE_POLL_INTERVAL_MS;

// Timeout for inverter HTTP requests made by the WiFi bridge task.
// Higher values tolerate slower inverter responses but can delay bridge cycles.
extern const uint16_t WIFI_BRIDGE_HTTP_TIMEOUT_MS;

// Power management
extern const uint32_t MAIN_LOOP_SLEEP_MS;
extern const uint16_t INVERTER_MAX_POWER_WATTS;

// API server behavior
extern const uint16_t API_CLIENT_TIMEOUT_MS;

// Ethernet bridge behavior
extern const uint32_t ETHERNET_INIT_RETRY_MS;
extern const uint32_t ETHERNET_SERVICE_INTERVAL_MS;
// If linkStatus() reports not-up for this long, re-init the ENC28J60 to
// recover from the silent SPI/PHY wedge where the chip keeps reporting
// LinkOFF forever.
extern const uint32_t ETHERNET_NO_LINK_RECOVERY_MS;
// If link is reportedly up and API server is running but no incoming HTTP
// client has been seen for this long, force a full re-init + fresh DHCP to
// recover from the silent RX wedge / stale lease cases.
extern const uint32_t ETHERNET_NO_ACTIVITY_RECOVERY_MS;

// Ethernet config
extern byte ETH_MAC[6];

// MQTT / Home Assistant configuration
// HA_MQTT_ENABLED_DEFAULT controls the factory default; the live value is
// persisted in NVS and can be toggled at runtime via POST /api/mqtt
// {"ha_enabled":true|false}.
extern const bool HA_MQTT_ENABLED_DEFAULT;
extern const uint16_t MQTT_PORT;
extern const char* MQTT_CLIENT_ID;
extern const char* MQTT_DISCOVERY_PREFIX;
extern const char* MQTT_DEVICE_ID;
extern const uint32_t MQTT_RECONNECT_INTERVAL_MS;
extern const uint32_t MQTT_PUBLISH_INTERVAL_MS;
// TCP-connect timeout (ms) used when probing the stored broker or scanning
// the local /24 subnet for an MQTT broker.
extern const uint16_t MQTT_SCAN_TIMEOUT_MS;
extern const char* FIRMWARE_VERSION;

// Shared globals
extern EthernetServer apiServer;
extern Logger appLogger;
extern int lastInverterStatusCode;

// Debug mode: when true, HTTP 200 success responses from the inverter are
// written to the log buffer. Starts true at boot and is cleared at the end
// of setup() to suppress routine poll noise during normal operation.
extern bool debugMode;

#endif // SETTINGS_H
