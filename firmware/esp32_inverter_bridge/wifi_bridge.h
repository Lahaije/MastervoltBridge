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

// ---------------------------------------------------------------------------
// WifiConnectionManager: manages inverter WiFi connection state.
// ensureConnected() returns immediately if already up; otherwise pulses the
// wake GPIO and attempts connection via the next alternating path (dwell/auto).
// ---------------------------------------------------------------------------
class WifiConnectionManager {
public:
  static WifiConnectionManager& getInstance();

  // Returns true if WiFi is connected (already up, or after a successful
  // (re)connect using the next alternating path).
  bool ensureConnected();

  // Forces a fresh connect using the next alternating path, regardless of
  // current WiFi status. Used by /pulse for measurement.
  bool forceReconnect();

private:
  WifiConnectionManager() = default;
  bool connectUsingNextPath();
  bool nextUseDwell_ = true;
};

// Generic method to fetch data from inverter (GET or POST).
// Uses WifiConnectionManager to obtain a connection before issuing HTTP.
// For POST requests, contentType sets the Content-Type header; defaults to text/plain.
bool fetchInverterData(const String& method, const String& path, const String& body,
                       String& responseBody, int& httpCode, String& errorMessage,
                       const char* contentType = "text/plain");

// If WiFi is connected, send a single button press to turn inverter WiFi off.
// Returns true when a press was sent, false when no press was needed/busy.
bool triggerWifiOffIfConnected();

#endif // WIFI_BRIDGE_H
