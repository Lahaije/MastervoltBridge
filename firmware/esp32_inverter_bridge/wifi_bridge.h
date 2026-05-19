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
// WifiConnectionManager: owns the alternating-path state and is the single
// entry point used by request handlers / polling tasks to obtain a working
// WiFi connection. If WiFi is already up, ensureConnected() returns true
// immediately; otherwise it pulses the inverter wake GPIO and invokes the
// next alternating connect path (dwell / auto).
//
//   dwell : scan-first with short dwell (200ms), uses configured AP hint as
//           fallback when scan does not locate the inverter.
//   auto  : scan-first with longer dwell (500ms), pure auto-discovery (no
//           hint fallback).
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
bool fetchInverterData(const String& method, const String& path, const String& body,
                       String& responseBody, int& httpCode, String& errorMessage);

// If WiFi is connected, send a single button press to turn inverter WiFi off.
// Returns true when a press was sent, false when no press was needed/busy.
bool triggerWifiOffIfConnected();

#endif // WIFI_BRIDGE_H
