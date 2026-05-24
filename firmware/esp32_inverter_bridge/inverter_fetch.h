#ifndef INVERTER_FETCH_H
#define INVERTER_FETCH_H

#include <Arduino.h>

/**
 * Raw inverter page fetch using HTTP/1.0 over TCP.
 *
 * The inverter's embedded web server serves HTML/JS/CSS files in a way that
 * is incompatible with ESP32's HTTPClient (which uses HTTP/1.1). Specifically,
 * HTTPClient returns error -7 (NO_HTTP_SERVER) for file endpoints even though
 * the inverter does respond with valid HTTP/1.0 headers.
 *
 * This module provides a minimal HTTP/1.0 client that:
 *   - Works for ALL inverter endpoints (data + files)
 *   - Tolerates non-standard responses (returns raw data if no HTTP headers)
 *   - Uses Connection: close semantics (no keep-alive complexity)
 *   - Does NOT require session priming or cookies (inverter has none)
 *
 * Use fetchInverterData() for the polling loop (HTTPClient, fast, proven).
 * Use fetchInverterPage() for the /api/inverter/fetch proxy endpoint.
 */

// Fetch any path from the inverter using raw HTTP/1.0 TCP.
// Blocks until WiFi is connected (waitForConnection=true) or fails fast (false).
// Returns true on success with responseBody containing the page content.
// httpCode is the HTTP status code (200, 404, etc.) or 0 on transport failure.
bool fetchInverterPage(const String& path, String& responseBody,
                       int& httpCode, String& errorMessage,
                       bool waitForConnection);

#endif // INVERTER_FETCH_H
