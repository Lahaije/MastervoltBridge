#ifndef SETTINGS_H
#define SETTINGS_H

#include <Arduino.h>
#include <UIPEthernet.h>

class Logger;

// User configuration
extern const char* INVERTER_WIFI_SSID;
extern const char* INVERTER_WIFI_PASSWORD;
extern const char* INVERTER_HOST;
extern const char* FIRMWARE_VERSION;
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

// mDNS hostname (advertised as <hostname>.local)
extern const char* MDNS_HOSTNAME;

// NetBIOS name for Windows discovery (max 15 characters)
extern const char* NBNS_NAME;

// Power management
extern const uint32_t MAIN_LOOP_SLEEP_MS;
extern const uint16_t INVERTER_MAX_POWER_WATTS;

// How long (ms) a queued power command remains active before expiring.
extern const uint32_t POWER_COMMAND_EXPIRY_MS;

// API server behavior
extern const uint16_t API_CLIENT_TIMEOUT_MS;

// Ethernet bridge behavior
extern const uint32_t ETHERNET_INIT_RETRY_MS;
extern const uint32_t ETHERNET_SERVICE_INTERVAL_MS;

// Ethernet config
extern byte ETH_MAC[6];

// Shared globals
extern EthernetServer apiServer;
extern Logger appLogger;
extern int lastInverterStatusCode;

// Debug mode: when true, HTTP 200 success responses from the inverter are
// written to the log buffer. Starts true at boot and is cleared at the end
// of setup() to suppress routine poll noise during normal operation.
extern bool debugMode;

#endif // SETTINGS_H
