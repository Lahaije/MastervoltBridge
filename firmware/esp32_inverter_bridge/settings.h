#ifndef SETTINGS_H
#define SETTINGS_H

#include <Arduino.h>
#include <UIPEthernet.h>

class Logger;

// =============================================================================
// Inverter WiFi
// =============================================================================
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

// =============================================================================
// WiFi Connection Strategy
// =============================================================================
extern const uint32_t CONNECT_TIMEOUT_MS;          // Overall WiFi connect timeout
extern const uint32_t WIFI_CONNECT_POLL_MS;        // Poll interval while waiting for connect
extern const uint32_t WIFI_LOCK_TIMEOUT_MS;        // WiFi operation mutex timeout
extern const uint32_t SCAN_SETTLE_MS;              // Settle time after WiFi scan
extern const int      MAX_CONNECT_RETRIES;         // Retry count in ensureConnected()
extern const uint32_t RETRY_PAUSE_MS;              // Pause between connect retries

// Dwell path parameters (A/B strategy)
extern const uint32_t DWELL_SCAN_DWELL_MS;         // Scan dwell for dwell path
extern const bool     DWELL_USE_HINT_FALLBACK;     // Use AP hint fallback in dwell path

// Auto path parameters (A/B strategy)
extern const uint32_t AUTO_SCAN_DWELL_MS;          // Scan dwell for auto path
extern const bool     AUTO_USE_HINT_FALLBACK;      // Use AP hint fallback in auto path

// =============================================================================
// Polling & Link-State FSM
// =============================================================================
extern const uint32_t WIFI_BRIDGE_POLL_INTERVAL_MS;    // Default poll interval after boot and degraded-state recovery
extern const uint16_t WIFI_BRIDGE_HTTP_TIMEOUT_MS;     // HTTP request timeout per inverter call

// Failure-streak thresholds that trigger FSM state transitions
extern const uint32_t LINK_RETRYING_TO_BACKOFF_MS;     // Streak duration to enter BACKOFF (5 min)
extern const uint32_t LINK_BACKOFF_TO_DORMANT_MS;      // Streak duration to enter DORMANT (20 min)

// Per-state polling intervals (override base when in BACKOFF/DORMANT)
extern const uint32_t LINK_BACKOFF_INTERVAL_MS;        // Poll interval during BACKOFF (1 min)
extern const uint32_t LINK_DORMANT_INTERVAL_MS;        // Poll interval during DORMANT (10 min)

// Mutex timeout for InverterController data access
extern const uint32_t DATA_MUTEX_TIMEOUT_MS;

// =============================================================================
// Hardware Pins
// =============================================================================
// ENC28J60 SPI pin mapping
extern const uint8_t PIN_ETH_SCK;
extern const uint8_t PIN_ETH_MISO;
extern const uint8_t PIN_ETH_MOSI;
extern const uint8_t PIN_ETH_CS;

// Inverter WiFi wake pin: idles HIGH; driven LOW for each button-press pulse.
// PULSE_HIGH_MS is the duration of the active-LOW pulse (i.e. how long the
// pin is held LOW per press). PULSE_GAP_MS is the inter-press gap.
extern const uint8_t PIN_INVERTER_WIFI_WAKE;
extern const uint16_t PULSE_HIGH_MS;
extern const uint16_t PULSE_GAP_MS;

// =============================================================================
// Ethernet & API Server
// =============================================================================
extern const uint16_t API_PORT;
extern const uint16_t API_CLIENT_TIMEOUT_MS;
extern const uint32_t ETHERNET_INIT_RETRY_MS;
extern const uint32_t ETHERNET_SERVICE_INTERVAL_MS;
extern byte ETH_MAC[6];

// =============================================================================
// Power & System
// =============================================================================
extern const uint16_t INVERTER_MAX_POWER_WATTS;
extern const uint32_t MAIN_LOOP_SLEEP_MS;

// =============================================================================
// Runtime State (shared globals)
// =============================================================================
extern EthernetServer apiServer;
extern Logger appLogger;
extern int lastInverterStatusCode;

// Debug mode: when true, HTTP 200 success responses from the inverter are
// written to the log buffer. Starts true at boot and is cleared at the end
// of setup() to suppress routine poll noise during normal operation.
extern bool debugMode;

#endif // SETTINGS_H
