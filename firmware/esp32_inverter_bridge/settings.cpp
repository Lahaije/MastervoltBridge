#include "settings.h"
#include "logger.h"

// --- Firmware ---
const char* FIRMWARE_VERSION = "fw-20260524-124205-df324a2";

// --- Inverter WiFi connection ---
const char* INVERTER_WIFI_SSID = "mastervolt-soladin-0103";
const char* INVERTER_WIFI_PASSWORD = "";
const char* INVERTER_HOST = "10.0.0.1";
const bool INVERTER_WIFI_AP_HINT_ENABLED = true;
const uint8_t INVERTER_WIFI_AP_HINT_CHANNELS[] = {1, 6, 11};
const uint8_t INVERTER_WIFI_AP_HINT_CHANNEL_COUNT = sizeof(INVERTER_WIFI_AP_HINT_CHANNELS);
const uint8_t INVERTER_WIFI_AP_HINT_BSSID[6] = {0x00, 0x06, 0x66, 0x9D, 0xE0, 0x36};
const uint16_t WIFI_BRIDGE_HTTP_TIMEOUT_MS = 3500;

// --- Ethernet ---
const uint8_t PIN_ETH_SCK = 9;
const uint8_t PIN_ETH_MISO = 10;
const uint8_t PIN_ETH_MOSI = 11;
const uint8_t PIN_ETH_CS = 8;
byte ETH_MAC[6] = {0x02, 0xA1, 0x82, 0x32, 0x10, 0x42};
const uint32_t ETHERNET_INIT_RETRY_MS = 5000;
const uint32_t ETHERNET_SERVICE_INTERVAL_MS = 2;

// --- API server ---
const char* DHCP_HOSTNAME = "mv-bridge";
const uint16_t API_PORT = 8080;
const uint16_t API_CLIENT_TIMEOUT_MS = 250;

// --- Inverter polling ---
const uint32_t DEFAULT_POLL_INTERVAL_MS = 20000;
const uint32_t MAIN_LOOP_SLEEP_MS = 5;

// --- Power management ---
const uint16_t INVERTER_MAX_POWER_WATTS = 1575;
const uint32_t POWER_COMMAND_EXPIRY_MS = 5u * 60u * 1000u;  // 5 minutes

// --- GPIO / pulse ---
const uint8_t PIN_INVERTER_WIFI_WAKE = 36;
const uint16_t PULSE_HIGH_MS = 50;
const uint16_t PULSE_GAP_MS = 50;

// --- Shared globals ---
EthernetServer apiServer(API_PORT);
Logger appLogger;
int lastInverterStatusCode = -1;
bool debugMode = true;
