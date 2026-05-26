#include "settings.h"
#include "logger.h"

// TODO: Support dynamic IP discovery via mDNS hostname instead of hardcoded static IPs.
// See TECHNICAL_DEBT.md for details. Scripts should discover bridge at mastervolt-bridge.local
// or scan network for mDNS service instead of using hardcoded 192.168.1.48 / 10.0.0.1.

const char* INVERTER_WIFI_SSID = "mastervolt-soladin-0103";
const char* INVERTER_WIFI_PASSWORD = "";
const bool INVERTER_WIFI_AP_HINT_ENABLED = true;
const uint8_t INVERTER_WIFI_AP_HINT_CHANNELS[] = {1, 6, 11};
const uint8_t INVERTER_WIFI_AP_HINT_CHANNEL_COUNT = sizeof(INVERTER_WIFI_AP_HINT_CHANNELS);
const uint8_t INVERTER_WIFI_AP_HINT_BSSID[6] = {0x00, 0x06, 0x66, 0x9D, 0xE0, 0x36};

const char* INVERTER_HOST = "10.0.0.1";

const uint16_t API_PORT = 8080;

const uint8_t PIN_ETH_SCK = 9;
const uint8_t PIN_ETH_MISO = 10;
const uint8_t PIN_ETH_MOSI = 11;
const uint8_t PIN_ETH_CS = 8;

const uint8_t PIN_INVERTER_WIFI_WAKE = 36;

const uint16_t PULSE_HIGH_MS = 50;
const uint16_t PULSE_GAP_MS = 50;

const uint32_t WIFI_BRIDGE_POLL_INTERVAL_MS = 20000;
const uint16_t WIFI_BRIDGE_HTTP_TIMEOUT_MS = 3500;
const uint32_t MAIN_LOOP_SLEEP_MS = 5;
const uint16_t INVERTER_MAX_POWER_WATTS = 1575;
const uint16_t API_CLIENT_TIMEOUT_MS = 250;
const uint32_t ETHERNET_INIT_RETRY_MS = 5000;
const uint32_t ETHERNET_SERVICE_INTERVAL_MS = 2;

byte ETH_MAC[6] = {0x02, 0xA1, 0x82, 0x32, 0x10, 0x42};

// MQTT / Home Assistant
const bool HA_MQTT_ENABLED_DEFAULT = false;
const uint16_t MQTT_PORT = 1883;
const char* MQTT_CLIENT_ID = "mv-bridge";
const char* MQTT_DISCOVERY_PREFIX = "homeassistant";
const char* MQTT_DEVICE_ID = "mastervolt_soladin_1500";
const uint32_t MQTT_RECONNECT_INTERVAL_MS = 10000;
const uint32_t MQTT_PUBLISH_INTERVAL_MS = 20000;
const uint16_t MQTT_SCAN_TIMEOUT_MS = 400;
const char* FIRMWARE_VERSION = "1.0.0";

EthernetServer apiServer(API_PORT);

// UIPEthernet 2.0.12 declares `extern const char* DHCP_HOSTNAME` in Dhcp.h
// but provides no definition — the library expects the user sketch to
// supply one. Define it here so DHCP DISCOVER/REQUEST messages get a
// proper host name and the link step resolves.
const char* DHCP_HOSTNAME = "mv-bridge";
Logger appLogger;
int lastInverterStatusCode = -1;
bool debugMode = true;
