#include "inverter_fetch.h"

#include <WiFi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "settings.h"
#include "logger.h"
#include "wifi_bridge.h"
#include "lock_guard.h"

namespace {

// TCP connect timeout for page fetches. The inverter is on a local WiFi link
// so connections should establish almost instantly.
constexpr uint32_t PAGE_TCP_CONNECT_TIMEOUT_MS = 2000;

// Maximum time to wait for the first response byte after sending the request.
constexpr uint32_t PAGE_FIRST_BYTE_TIMEOUT_MS = 3500;

// After receiving the first byte, how long to wait for additional data before
// considering the response complete. Extended while data is actively flowing.
constexpr uint32_t PAGE_READ_IDLE_TIMEOUT_MS = 500;

using ScopedWifiOperationLock = ScopedLock<LockRank::WIFI_OPERATION>;

}  // namespace

bool fetchInverterPage(const String& path, String& responseBody,
                       int& httpCode, String& errorMessage,
                       bool waitForConnection) {
  assertNoStateLocksHeld("fetchInverterPage");

  // WiFi connectivity gate.
  if (!isWifiConnectedStatus()) {
    if (waitForConnection) {
      requestWifiConnection();
      // Wait up to 25s for the connection worker to finish.
      unsigned long waitStart = millis();
      while (!isWifiConnectedStatus() && (millis() - waitStart) < 25000) {
        vTaskDelay(pdMS_TO_TICKS(250));
      }
      if (!isWifiConnectedStatus()) {
        errorMessage = "WiFi connection failed";
        httpCode = 0;
        return false;
      }
    } else {
      requestWifiConnection();
      errorMessage = "Inverter WiFi not connected";
      httpCode = 0;
      return false;
    }
  }

  // Acquire WiFi operation lock (queues behind any active HTTP request).
  ScopedWifiOperationLock lock(wifiOperationMutex, 4500, "wifiOperationMutex/pageFetch");
  if (!lock.acquired()) {
    errorMessage = "WiFi operation busy";
    httpCode = 0;
    return false;
  }

  if (!isWifiConnectedStatus()) {
    errorMessage = "WiFi dropped before page fetch";
    httpCode = 0;
    return false;
  }

  // Open TCP connection to inverter.
  WiFiClient client;
  client.setTimeout(PAGE_FIRST_BYTE_TIMEOUT_MS);

  if (!client.connect(INVERTER_HOST, 80, PAGE_TCP_CONNECT_TIMEOUT_MS)) {
    errorMessage = String("TCP connect failed for ") + path;
    httpCode = 0;
    return false;
  }

  // Build and send HTTP/1.0 GET request.
  // HTTP/1.0 is critical: the inverter's file server doesn't handle HTTP/1.1.
  String request;
  request.reserve(64 + path.length());
  request += "GET ";
  request += path;
  request += " HTTP/1.0\r\nHost: ";
  request += INVERTER_HOST;
  request += "\r\nConnection: close\r\n\r\n";

  size_t sent = client.write((const uint8_t*)request.c_str(), request.length());
  client.flush();
  if (sent != request.length()) {
    client.stop();
    errorMessage = String("Short send for ") + path;
    httpCode = 0;
    return false;
  }

  // Wait for first response byte.
  unsigned long firstByteDeadline = millis() + PAGE_FIRST_BYTE_TIMEOUT_MS;
  while (client.connected() && client.available() == 0 && millis() < firstByteDeadline) {
    vTaskDelay(pdMS_TO_TICKS(5));
  }
  if (client.available() == 0) {
    client.stop();
    errorMessage = String("No response for ") + path;
    httpCode = 0;
    return false;
  }

  // Read entire response (headers + body).
  String rawData;
  rawData.reserve(4096);
  unsigned long readDeadline = millis() + PAGE_FIRST_BYTE_TIMEOUT_MS;
  while ((client.connected() || client.available()) && millis() < readDeadline) {
    while (client.available()) {
      rawData += (char)client.read();
      readDeadline = millis() + PAGE_READ_IDLE_TIMEOUT_MS;
    }
    vTaskDelay(pdMS_TO_TICKS(2));
  }
  client.stop();

  if (rawData.length() == 0) {
    errorMessage = String("Empty response for ") + path;
    httpCode = 0;
    return false;
  }

  // Parse response: extract HTTP status and body from headers.
  if (rawData.startsWith("HTTP/")) {
    // Standard HTTP response.
    int lineEnd = rawData.indexOf('\n');
    if (lineEnd < 0) lineEnd = rawData.length();
    String statusLine = rawData.substring(0, lineEnd);

    // Parse status code from "HTTP/1.x NNN reason"
    httpCode = 0;
    int sp1 = statusLine.indexOf(' ');
    if (sp1 > 0) {
      int sp2 = statusLine.indexOf(' ', sp1 + 1);
      if (sp2 > sp1) {
        httpCode = statusLine.substring(sp1 + 1, sp2).toInt();
      }
    }

    // Find header/body boundary.
    int headerEnd = rawData.indexOf("\r\n\r\n");
    if (headerEnd >= 0) {
      responseBody = rawData.substring(headerEnd + 4);
    } else {
      headerEnd = rawData.indexOf("\n\n");
      if (headerEnd >= 0) {
        responseBody = rawData.substring(headerEnd + 2);
      } else {
        responseBody = rawData.substring(lineEnd + 1);
      }
    }
  } else {
    // Non-HTTP response: return raw content as-is.
    httpCode = 200;
    responseBody = rawData;
  }

  if (debugMode) {
    appLogger.log(String("[FETCH] ") + path +
                  " HTTP " + httpCode +
                  " body=" + responseBody.length() + "B");
  }
  return true;
}
