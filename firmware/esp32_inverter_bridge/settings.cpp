#include "settings.h"
#include "logger.h"

const char* DHCP_HOSTNAME = "mv-bridge";

// =============================================================================
// Inverter WiFi
// =============================================================================
const char* INVERTER_WIFI_SSID = "mastervolt-soladin-0103";
const char* INVERTER_WIFI_PASSWORD = "";
const char* INVERTER_HOST = "10.0.0.1";

const bool INVERTER_WIFI_AP_HINT_ENABLED = true;
const uint8_t INVERTER_WIFI_AP_HINT_CHANNELS[] = {1, 6, 11};
const uint8_t INVERTER_WIFI_AP_HINT_CHANNEL_COUNT = sizeof(INVERTER_WIFI_AP_HINT_CHANNELS);
const uint8_t INVERTER_WIFI_AP_HINT_BSSID[6] = {0x00, 0x06, 0x66, 0x9D, 0xE0, 0x36};

// =============================================================================
// WiFi Connection Strategy
// =============================================================================
const uint32_t CONNECT_TIMEOUT_MS = 7000;
const uint32_t WIFI_CONNECT_POLL_MS = 250;
const uint32_t WIFI_LOCK_TIMEOUT_MS = 50;
const uint32_t SCAN_SETTLE_MS = 100;
const int      MAX_CONNECT_RETRIES = 3;
const uint32_t RETRY_PAUSE_MS = 500;

const uint32_t DWELL_SCAN_DWELL_MS = 200;
const bool     DWELL_USE_HINT_FALLBACK = true;

const uint32_t AUTO_SCAN_DWELL_MS = 500;
const bool     AUTO_USE_HINT_FALLBACK = false;

// =============================================================================
// Polling & Link-State FSM
// =============================================================================
const uint32_t WIFI_BRIDGE_POLL_INTERVAL_MS = 20000;
const uint16_t WIFI_BRIDGE_HTTP_TIMEOUT_MS = 3500;

const uint32_t LINK_RETRYING_TO_BACKOFF_MS =  5u * 60u * 1000u;   //  5 min
const uint32_t LINK_BACKOFF_TO_DORMANT_MS  = 20u * 60u * 1000u;   // 20 min
const uint32_t LINK_BACKOFF_INTERVAL_MS    =  1u * 60u * 1000u;   //  1 min
const uint32_t LINK_DORMANT_INTERVAL_MS    = 10u * 60u * 1000u;   // 10 min

const uint32_t DATA_MUTEX_TIMEOUT_MS = 10;

// =============================================================================
// Hardware Pins
// =============================================================================
const uint8_t PIN_ETH_SCK = 9;
const uint8_t PIN_ETH_MISO = 10;
const uint8_t PIN_ETH_MOSI = 11;
const uint8_t PIN_ETH_CS = 8;

const uint8_t PIN_INVERTER_WIFI_WAKE = 36;
const uint16_t PULSE_HIGH_MS = 50;
const uint16_t PULSE_GAP_MS = 50;

// =============================================================================
// Ethernet & API Server
// =============================================================================
const uint16_t API_PORT = 8080;
const uint16_t API_CLIENT_TIMEOUT_MS = 250;
const uint32_t ETHERNET_INIT_RETRY_MS = 5000;
const uint32_t ETHERNET_SERVICE_INTERVAL_MS = 2;
byte ETH_MAC[6] = {0x02, 0xA1, 0x82, 0x32, 0x10, 0x42};

// =============================================================================
// Power & System
// =============================================================================
const uint16_t INVERTER_MAX_POWER_WATTS = 1575;
const uint32_t MAIN_LOOP_SLEEP_MS = 5;

// =============================================================================
// Runtime State
// =============================================================================
EthernetServer apiServer(API_PORT);
Logger appLogger;
int lastInverterStatusCode = -1;
bool debugMode = true;

// =============================================================================
// Firmware Version
// =============================================================================
// Format: <semver>-<YYYYMMDD>-<commit_short_hash>
const char* FIRMWARE_VERSION = "0.1.0-20260528-f09de5c";
