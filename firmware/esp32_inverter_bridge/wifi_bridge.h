#ifndef WIFI_BRIDGE_H
#define WIFI_BRIDGE_H

#include <Arduino.h>
#include <freertos/semphr.h>

/**
 * Public WiFi Bridge API - Provides WiFi connectivity and generic HTTP request utilities.
 *
 * Architecture:
 *   A dedicated connection worker task owns all WiFi connect/pulse logic.
 *   fetchInverterData() is the single entry point for all inverter HTTP requests.
 *   The waitForConnection parameter controls blocking behavior:
 *     true  = block until WiFi is established (used by polling loop)
 *     false = return error immediately if WiFi is down (used by API endpoints)
 *   When WiFi is already connected, callers wait for the operation lock with a
 *   generous timeout so concurrent requests queue rather than fail.
 */

// WiFi operation mutex - shared with inverter_fetch.cpp for page fetches.
extern SemaphoreHandle_t wifiOperationMutex;

// Initialize WiFi bridge: configure GPIO, create mutexes, start connection worker task.
void wifiBridgeInit();

// ---------------------------------------------------------------------------
// fetchInverterData: single entry point for all inverter HTTP communication.
//
//   waitForConnection=true:
//     If WiFi is down, notifies the connection worker and blocks until WiFi
//     is established (up to 3 connect attempts). Then performs the HTTP request.
//     Used by the polling loop which can afford to wait.
//
//   waitForConnection=false:
//     If WiFi is down, notifies the connection worker (so it reconnects in the
//     background) but returns immediately with an error. If WiFi is up, waits
//     for the operation lock and performs the request. Used by API endpoints
//     that need fast responses.
// ---------------------------------------------------------------------------
bool fetchInverterData(const String& method, const String& path, const String& body,
                       String& responseBody, int& httpCode, String& errorMessage,
                       bool waitForConnection);

// Request the connection worker to establish WiFi (non-blocking trigger).
// Returns immediately; the worker runs in the background.
void requestWifiConnection();

// Check if WiFi is currently connected (non-blocking status check).
bool isWifiConnectedStatus();

// Forces a fresh connect using the next alternating path, regardless of
// current WiFi status. Used by /pulse for measurement. Blocks until done.
bool forceWifiReconnect();

// If WiFi is connected, send a single button press to turn inverter WiFi off.
// Returns true when a press was sent, false when no press was needed/busy.
bool triggerWifiOffIfConnected();

#endif // WIFI_BRIDGE_H
