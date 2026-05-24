#ifndef SETTINGS_H
#define SETTINGS_H

#include <Arduino.h>
#include <UIPEthernet.h>

class Logger;

// --- Firmware ---
extern const char* FIRMWARE_VERSION;

// --- Inverter WiFi connection ---
extern const char* INVERTER_WIFI_SSID;
extern const char* INVERTER_WIFI_PASSWORD;
extern const char* INVERTER_HOST;

// Optional AP hint to speed up WiFi association.
// When enabled, the bridge attempts directed association to known channels+BSSID
// (rotating through the list) before falling back to normal SSID scanning behavior.
extern const bool INVERTER_WIFI_AP_HINT_ENABLED;
extern const uint8_t INVERTER_WIFI_AP_HINT_CHANNELS[];
extern const uint8_t INVERTER_WIFI_AP_HINT_CHANNEL_COUNT;
extern const uint8_t INVERTER_WIFI_AP_HINT_BSSID[6];

// Timeout for inverter HTTP requests made by the WiFi bridge task.
extern const uint16_t WIFI_BRIDGE_HTTP_TIMEOUT_MS;

// --- Ethernet ---
// ENC28J60 SPI pin mapping
extern const uint8_t PIN_ETH_SCK;
extern const uint8_t PIN_ETH_MISO;
extern const uint8_t PIN_ETH_MOSI;
extern const uint8_t PIN_ETH_CS;
extern byte ETH_MAC[6];

extern const uint32_t ETHERNET_INIT_RETRY_MS;
extern const uint32_t ETHERNET_SERVICE_INTERVAL_MS;

// --- API server ---
// DHCP hostname sent to router (max 14 chars due to UIPEthernet buffer limit)
extern const char* DHCP_HOSTNAME;
extern const uint16_t API_PORT;
extern const uint16_t API_CLIENT_TIMEOUT_MS;

// --- Inverter polling ---
extern const uint32_t DEFAULT_POLL_INTERVAL_MS;
extern const uint32_t MAIN_LOOP_SLEEP_MS;

// --- Power management ---
extern const uint16_t INVERTER_MAX_POWER_WATTS;
extern const uint32_t POWER_COMMAND_EXPIRY_MS;

// --- GPIO / pulse ---
extern const uint8_t PIN_INVERTER_WIFI_WAKE;
extern const uint16_t PULSE_HIGH_MS;
extern const uint16_t PULSE_GAP_MS;

// --- Shared globals ---
extern EthernetServer apiServer;
extern Logger appLogger;
extern int lastInverterStatusCode;
extern bool debugMode;

#endif // SETTINGS_H
