#ifndef WIFI_BRIDGE_H
#define WIFI_BRIDGE_H

#include <Arduino.h>

/**
 * Public WiFi Bridge API - Provides WiFi connectivity and generic HTTP request utilities.
 */

// HTTP status code from the last inverter request (defined in settings.cpp)
extern int lastInverterStatusCode;

// Initialize WiFi bridge: configure GPIO pin for inverter wake signal
void wifiBridgeInit();

// Ensure WiFi is connected to the inverter's network
bool ensureWifiConnected();

// Ensure WiFi is connected with a caller-provided timeout in milliseconds.
// Used by the inverter monitor to cap connect attempts per polling iteration.
bool ensureWifiConnectedWithTimeout(uint32_t timeoutMs);

// Generic method to fetch data from inverter (GET or POST)
// Automatically ensures WiFi is connected before making the request
bool fetchInverterData(const String& method, const String& path, const String& body,
                       String& responseBody, int& httpCode, String& errorMessage);

// If WiFi is connected, send a single button press to turn inverter WiFi off.
// Returns true when a press was sent, false when no press was needed/busy.
bool triggerWifiOffIfConnected();

// Pulse once, then attempt WiFi connection at maximum frequency for up to 30s.
// Logs each failed attempt with visible networks. Logs elapsed time on success.
void measureConnectionTime();

#endif // WIFI_BRIDGE_H
