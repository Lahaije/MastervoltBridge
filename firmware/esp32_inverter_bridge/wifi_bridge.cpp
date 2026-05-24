#include "wifi_bridge.h"

#include <WiFi.h>
#include <HTTPClient.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/event_groups.h>

#include "settings.h"
#include "logger.h"
#include "lock_guard.h"

namespace {

constexpr uint32_t WIFI_CONNECT_POLL_MS = 250;
constexpr uint32_t CONNECT_TIMEOUT_MS = 7000;
constexpr uint32_t SCAN_SETTLE_MS = 100;

// Lock timeout when WiFi is connected: wait for current HTTP request to finish.
// Must exceed WIFI_BRIDGE_HTTP_TIMEOUT_MS to allow the active request to complete.
constexpr uint32_t WIFI_LOCK_TIMEOUT_CONNECTED_MS = 4500;

// Per-request budget for the manual POST implementation below. Inverter is
// expected to respond well under 2s; anything longer is considered a stall.
constexpr uint32_t HTTP_POST_BUDGET_MS = 3000;
constexpr uint32_t HTTP_TCP_CONNECT_TIMEOUT_MS = 1500;
// Time to first response byte for a single POST attempt. Keep short so
// /api/power stays responsive even when the inverter ignores a format.
constexpr uint32_t HTTP_FIRST_BYTE_TIMEOUT_MS = 1500;

// Retry settings for connection worker: pulse once, then try connecting up to
// MAX_CONNECT_RETRIES times (alternating dwell/auto) before signaling failure.
constexpr int MAX_CONNECT_RETRIES = 3;
constexpr uint32_t RETRY_PAUSE_MS = 500;

// Dwell path: short scan dwell, hint fallback enabled.
constexpr uint32_t DWELL_SCAN_DWELL_MS = 200;
constexpr bool DWELL_USE_HINT_FALLBACK = true;

// Auto path: longer scan dwell, no hint fallback (auto-discovery only).
constexpr uint32_t AUTO_SCAN_DWELL_MS = 500;
constexpr bool AUTO_USE_HINT_FALLBACK = false;

// Event group bits for connection worker signaling.
constexpr EventBits_t EVT_CONNECT_REQUEST = (1 << 0);
constexpr EventBits_t EVT_CONNECT_DONE    = (1 << 1);
constexpr EventBits_t EVT_CONNECT_FAILED  = (1 << 2);

// Maximum time a caller blocks waiting for the connection worker (3 attempts × 7s + pauses).
constexpr uint32_t CONNECT_WAIT_TIMEOUT_MS = (MAX_CONNECT_RETRIES * CONNECT_TIMEOUT_MS) +
                                              ((MAX_CONNECT_RETRIES - 1) * RETRY_PAUSE_MS) + 2000;

}  // namespace (reopen below after external-linkage variables)

// Externally-visible synchronization primitives (shared with inverter_fetch.cpp).
SemaphoreHandle_t wifiOperationMutex = nullptr;

namespace {

EventGroupHandle_t connectionEvents = nullptr;
TaskHandle_t connectionWorkerHandle = nullptr;

// Alternating connect path state (owned exclusively by connection worker).
bool nextUseDwell = true;

// Discovered AP BSSID+channel from scan during the current connect attempt.
uint8_t discoveredBssid[6] = {0};
uint8_t discoveredChannel = 0;
bool hasDiscoveredAp = false;

// Optional configured AP hint from settings (expected inverter channels+BSSID).
uint8_t configuredHintBssid[6] = {0};
uint8_t hintChannelIndex = 0;
bool hasConfiguredHint = false;

// ---------------------------------------------------------------------------
// ScopedWifiOperationLock: alias for the hierarchy-aware ScopedLock at the
// highest rank. Enforces that no business-state lock is held while we are
// touching the WiFi radio or running an HTTP request.
// ---------------------------------------------------------------------------
using ScopedWifiOperationLock = ScopedLock<LockRank::WIFI_OPERATION>;

// ---------------------------------------------------------------------------
// Logging helpers.
// ---------------------------------------------------------------------------
void logWifiBridge(const String& message) {
  appLogger.log(String("[WIFI-BRIDGE] ") + message);
}

void logConnect(const String& message) {
  appLogger.log(String("[WIFI-CONNECT] ") + message);
}

// Forward declaration (definition below).
const char* wifiStatusToString(wl_status_t status);

// ---------------------------------------------------------------------------
// Low-level WiFi operations.
// ---------------------------------------------------------------------------
void powerDownWifiRadio() {
  if (debugMode) {
    appLogger.log(String("[WIFI-DEBUG] powerDownWifiRadio() called; WiFi.status()=") +
                  wifiStatusToString(WiFi.status()));
  }
  WiFi.mode(WIFI_OFF);
  hasDiscoveredAp = false;
}

void powerUpWifiRadioForConnect() {
  if (debugMode) {
    appLogger.log(String("[WIFI-DEBUG] powerUpWifiRadioForConnect(); previous status=") +
                  wifiStatusToString(WiFi.status()));
  }
  WiFi.mode(WIFI_STA);
  hasDiscoveredAp = false;
}

void pressInverterWifiButtonOnce() {
  digitalWrite(PIN_INVERTER_WIFI_WAKE, LOW);
  delay(PULSE_HIGH_MS);
  digitalWrite(PIN_INVERTER_WIFI_WAKE, HIGH);
}

bool isWifiConnected() {
  return WiFi.status() == WL_CONNECTED;
}

void initializeConfiguredHint() {
  hasConfiguredHint = INVERTER_WIFI_AP_HINT_ENABLED &&
                      INVERTER_WIFI_AP_HINT_CHANNEL_COUNT > 0;
  if (!hasConfiguredHint) {
    memset(configuredHintBssid, 0, sizeof(configuredHintBssid));
    return;
  }
  hintChannelIndex = 0;
  memcpy(configuredHintBssid, INVERTER_WIFI_AP_HINT_BSSID, sizeof(configuredHintBssid));
}

const char* wifiStatusToString(wl_status_t status) {
  switch (status) {
    case WL_IDLE_STATUS: return "IDLE";
    case WL_NO_SSID_AVAIL: return "NO_SSID";
    case WL_SCAN_COMPLETED: return "SCAN_COMPLETED";
    case WL_CONNECTED: return "CONNECTED";
    case WL_CONNECT_FAILED: return "CONNECT_FAILED";
    case WL_CONNECTION_LOST: return "CONNECTION_LOST";
    case WL_DISCONNECTED: return "DISCONNECTED";
    default: return "UNKNOWN";
  }
}

void runScanPass(uint32_t dwellMs) {
  vTaskDelay(pdMS_TO_TICKS(SCAN_SETTLE_MS));
  unsigned long scanStartMs = millis();
  int networkCount = WiFi.scanNetworks(false, true, false, dwellMs);
  unsigned long scanMs = millis() - scanStartMs;
  hasDiscoveredAp = false;
  int matchCount = 0;
  int8_t bestRssi = -127;
  int bestIdx = -1;
  if (networkCount > 0) {
    for (int i = 0; i < networkCount; i++) {
      if (WiFi.SSID(i) == INVERTER_WIFI_SSID) {
        matchCount++;
        int8_t rssi = (int8_t)WiFi.RSSI(i);
        if (rssi > bestRssi) {
          bestRssi = rssi;
          bestIdx = i;
        }
      }
    }
    if (bestIdx >= 0) {
      memcpy(discoveredBssid, WiFi.BSSID(bestIdx), 6);
      discoveredChannel = (uint8_t)WiFi.channel(bestIdx);
      hasDiscoveredAp = true;
    }
  }
  if (debugMode) {
    appLogger.log(String("[WIFI-DEBUG] scan dwell_ms=") + dwellMs +
                  " duration_ms=" + scanMs +
                  " networks=" + networkCount +
                  " matches=" + matchCount +
                  " best_rssi=" + (bestIdx >= 0 ? String(bestRssi) : String("n/a")) +
                  " best_channel=" + (bestIdx >= 0 ? String(discoveredChannel) : String("n/a")));
  }
  WiFi.scanDelete();
}

void startWifiBegin(bool useHintFallback) {
  const char* pass = strlen(INVERTER_WIFI_PASSWORD) ? INVERTER_WIFI_PASSWORD : nullptr;
  if (hasDiscoveredAp) {
    WiFi.begin(INVERTER_WIFI_SSID, pass, discoveredChannel, discoveredBssid);
  } else if (useHintFallback && hasConfiguredHint) {
    uint8_t ch = INVERTER_WIFI_AP_HINT_CHANNELS[hintChannelIndex];
    hintChannelIndex = (hintChannelIndex + 1) % INVERTER_WIFI_AP_HINT_CHANNEL_COUNT;
    WiFi.begin(INVERTER_WIFI_SSID, pass, ch, configuredHintBssid);
  } else if (pass == nullptr) {
    WiFi.begin(INVERTER_WIFI_SSID);
  } else {
    WiFi.begin(INVERTER_WIFI_SSID, pass);
  }
}

// Core connect routine. Logs path name and total duration for later analysis.
bool runConnectPath(const char* pathName, uint32_t scanDwellMs, bool useHintFallback) {
  unsigned long startMs = millis();
  logConnect(String("start path=") + pathName +
             " scan_dwell_ms=" + scanDwellMs +
             " hint_fallback=" + (useHintFallback ? "1" : "0"));

  powerUpWifiRadioForConnect();
  runScanPass(scanDwellMs);
  startWifiBegin(useHintFallback);

  wl_status_t lastStatus = WiFi.status();
  if (debugMode) {
    appLogger.log(String("[WIFI-DEBUG] connect loop start status=") +
                  wifiStatusToString(lastStatus));
  }
  while (millis() - startMs < CONNECT_TIMEOUT_MS) {
    wl_status_t status = WiFi.status();
    if (debugMode && status != lastStatus) {
      appLogger.log(String("[WIFI-DEBUG] status change path=") + pathName +
                    " " + wifiStatusToString(lastStatus) +
                    " -> " + wifiStatusToString(status) +
                    " at_ms=" + (millis() - startMs));
      lastStatus = status;
    }
    if (status == WL_CONNECTED) {
      unsigned long elapsedMs = millis() - startMs;
      uint8_t ch = WiFi.channel();
      uint8_t* bssid = WiFi.BSSID();
      char bssidStr[18] = {0};
      if (bssid != nullptr) {
        snprintf(bssidStr, sizeof(bssidStr), "%02X:%02X:%02X:%02X:%02X:%02X",
                 bssid[0], bssid[1], bssid[2], bssid[3], bssid[4], bssid[5]);
      }
      logConnect(String("complete path=") + pathName +
                 " duration_ms=" + elapsedMs +
                 " result=success channel=" + ch +
                 " bssid=" + bssidStr +
                 " ip=" + WiFi.localIP().toString());
      return true;
    }
    if (status == WL_CONNECT_FAILED || status == WL_NO_SSID_AVAIL || status == WL_CONNECTION_LOST) {
      hasDiscoveredAp = false;
      startWifiBegin(useHintFallback);
    }
    vTaskDelay(pdMS_TO_TICKS(WIFI_CONNECT_POLL_MS));
  }

  unsigned long elapsedMs = millis() - startMs;
  wl_status_t finalStatus = WiFi.status();
  logConnect(String("complete path=") + pathName +
             " duration_ms=" + elapsedMs +
             " result=timeout final_status=" + wifiStatusToString(finalStatus));
  powerDownWifiRadio();
  return false;
}

void triggerPulseSequence() {
  logWifiBridge("Triggering inverter WiFi wake pulse sequence.");
  pressInverterWifiButtonOnce();
  delay(PULSE_GAP_MS);
  pressInverterWifiButtonOnce();
}

bool connectWifiDwell() {
  return runConnectPath("dwell", DWELL_SCAN_DWELL_MS, DWELL_USE_HINT_FALLBACK);
}

bool connectWifiAuto() {
  return runConnectPath("auto", AUTO_SCAN_DWELL_MS, AUTO_USE_HINT_FALLBACK);
}

bool connectUsingNextPath() {
  bool useDwell = nextUseDwell;
  nextUseDwell = !nextUseDwell;
  return useDwell ? connectWifiDwell() : connectWifiAuto();
}

// ---------------------------------------------------------------------------
// Connection Worker Task: dedicated FreeRTOS task that owns all WiFi
// connect/pulse logic. Triggered via event group; signals result back.
// Exactly one instance — serialized by design.
// ---------------------------------------------------------------------------
void connectionWorkerTask(void* param) {
  (void)param;

  while (true) {
    // Wait for a connection request.
    xEventGroupWaitBits(connectionEvents, EVT_CONNECT_REQUEST,
                        pdTRUE,   // clear on exit
                        pdFALSE,  // wait for any bit
                        portMAX_DELAY);

    // Clear any previous result bits.
    xEventGroupClearBits(connectionEvents, EVT_CONNECT_DONE | EVT_CONNECT_FAILED);

    // If already connected, signal done immediately.
    if (isWifiConnected()) {
      xEventGroupSetBits(connectionEvents, EVT_CONNECT_DONE);
      continue;
    }

    // Acquire the WiFi operation mutex for the entire connection sequence.
    // This prevents HTTP requests from interfering during connect.
    ScopedWifiOperationLock lock(wifiOperationMutex, WIFI_LOCK_TIMEOUT_CONNECTED_MS, "wifiOperationMutex/connect");
    if (!lock.acquired()) {
      // Someone else has the lock (e.g. an HTTP request in flight).
      // Wait for them to finish and check again.
      vTaskDelay(pdMS_TO_TICKS(WIFI_LOCK_TIMEOUT_CONNECTED_MS));
      if (isWifiConnected()) {
        xEventGroupSetBits(connectionEvents, EVT_CONNECT_DONE);
      } else {
        xEventGroupSetBits(connectionEvents, EVT_CONNECT_FAILED);
      }
      continue;
    }

    // Pulse and attempt up to MAX_CONNECT_RETRIES paths.
    triggerPulseSequence();

    bool connected = false;
    for (int attempt = 0; attempt < MAX_CONNECT_RETRIES; attempt++) {
      if (connectUsingNextPath()) {
        connected = true;
        break;
      }
      vTaskDelay(pdMS_TO_TICKS(RETRY_PAUSE_MS));
    }

    if (connected) {
      xEventGroupSetBits(connectionEvents, EVT_CONNECT_DONE);
    } else {
      xEventGroupSetBits(connectionEvents, EVT_CONNECT_FAILED);
    }
  }
}

// Request the worker to connect and optionally wait for the result.
bool requestConnectionAndWait(bool wait) {
  // Signal the worker.
  xEventGroupSetBits(connectionEvents, EVT_CONNECT_REQUEST);

  if (!wait) {
    return false;
  }

  // Block until the worker signals done or failed.
  EventBits_t bits = xEventGroupWaitBits(
    connectionEvents,
    EVT_CONNECT_DONE | EVT_CONNECT_FAILED,
    pdTRUE,   // clear on exit
    pdFALSE,  // wait for any bit
    pdMS_TO_TICKS(CONNECT_WAIT_TIMEOUT_MS)
  );

  return (bits & EVT_CONNECT_DONE) != 0;
}

}  // namespace

// ---------------------------------------------------------------------------
// Public API.
// ---------------------------------------------------------------------------

void wifiBridgeInit() {
  if (wifiOperationMutex == nullptr) {
    wifiOperationMutex = xSemaphoreCreateMutex();
  }
  if (connectionEvents == nullptr) {
    connectionEvents = xEventGroupCreate();
  }
  initializeConfiguredHint();

  // Boot with the radio off; the first connect attempt powers it up.
  WiFi.mode(WIFI_OFF);

  pinMode(PIN_INVERTER_WIFI_WAKE, OUTPUT);
  digitalWrite(PIN_INVERTER_WIFI_WAKE, HIGH);

  // Start the dedicated connection worker task.
  if (connectionWorkerHandle == nullptr) {
    xTaskCreatePinnedToCore(
      connectionWorkerTask,
      "wifi_connect",
      4096,
      nullptr,
      2,  // Higher priority than polling (1) so it responds quickly.
      &connectionWorkerHandle,
      0
    );
  }
}

bool isWifiConnectedStatus() {
  return isWifiConnected();
}

void requestWifiConnection() {
  xEventGroupSetBits(connectionEvents, EVT_CONNECT_REQUEST);
}

bool forceWifiReconnect() {
  // Acquire the operation lock to prevent interference.
  ScopedWifiOperationLock lock(wifiOperationMutex, WIFI_LOCK_TIMEOUT_CONNECTED_MS, "wifiOperationMutex/forceReconnect");
  if (!lock.acquired()) {
    logWifiBridge("WiFi operation busy; lock not acquired for forceReconnect.");
    return false;
  }
  triggerPulseSequence();
  return connectUsingNextPath();
}

bool fetchInverterData(const String& method, const String& path, const String& body,
                       String& responseBody, int& httpCode, String& errorMessage,
                       bool waitForConnection) {
  // Lock-hierarchy invariant: never hold a business-state lock while doing
  // network I/O. Logged loudly via appLogger if violated.
  assertNoStateLocksHeld("fetchInverterData");

  // --- WiFi connectivity check ---
  if (!isWifiConnected()) {
    if (waitForConnection) {
      // Block until the connection worker establishes WiFi.
      if (!requestConnectionAndWait(true)) {
        errorMessage = "WiFi connection failed after retries";
        httpCode = 0;
        return false;
      }
    } else {
      // Trigger background reconnection but don't wait.
      requestWifiConnection();
      errorMessage = "Inverter WiFi not connected";
      httpCode = 0;
      return false;
    }
  }

  // --- WiFi is connected. Acquire operation lock. ---
  // Use a generous timeout: the current lock holder is doing an HTTP request
  // (max ~3.5s). We queue behind it rather than failing.
  unsigned long lockWaitStartMs = millis();
  ScopedWifiOperationLock lock(wifiOperationMutex, WIFI_LOCK_TIMEOUT_CONNECTED_MS, "wifiOperationMutex/fetch");
  if (!lock.acquired()) {
    errorMessage = "WiFi operation busy; lock timeout";
    httpCode = 0;
    if (debugMode) {
      appLogger.log(String("[WIFI-DEBUG] op lock timeout path=") + path +
                    " waited_ms=" + (millis() - lockWaitStartMs));
    }
    return false;
  }
  if (debugMode) {
    unsigned long lockWaitMs = millis() - lockWaitStartMs;
    if (lockWaitMs > 10) {
      appLogger.log(String("[WIFI-DEBUG] op lock acquired path=") + path +
                    " waited_ms=" + lockWaitMs);
    }
  }

  // Verify WiFi is still connected after acquiring the lock.
  if (!isWifiConnected()) {
    if (waitForConnection) {
      errorMessage = "WiFi dropped while waiting for lock";
    } else {
      requestWifiConnection();
      errorMessage = "WiFi dropped before HTTP request";
    }
    httpCode = 0;
    if (debugMode) {
      appLogger.log(String("[WIFI-DEBUG] WiFi dropped before HTTP path=") + path +
                    " status=" + wifiStatusToString(WiFi.status()));
    }
    return false;
  }

  // --- Perform the HTTP request ---
  WiFiClient wifiClient;
  HTTPClient http;
  String url = String("http://") + INVERTER_HOST + path;

  // POST goes through a manual implementation to get phase-level timing logs
  // and bounded latency for /api/power.
  if (method == "POST") {
    const unsigned long postStartMs = millis();
    const uint16_t hostPort = 80;

    if (debugMode) {
      appLogger.log(String("[WIFI-DEBUG] POST begin path=") + path +
                    " host=" + INVERTER_HOST + ":" + hostPort +
                    " body_len=" + body.length());
    }

    auto attemptPost = [&](const String& contentType, const String& payload, const char* label) -> bool {
      const unsigned long nowMs = millis();
      const unsigned long elapsedMs = nowMs - postStartMs;
      if (elapsedMs >= HTTP_POST_BUDGET_MS) {
        errorMessage = String("POST ") + path + " failed: budget exceeded before attempt " + label;
        httpCode = 0;
        lastInverterStatusCode = 0;
        return false;
      }

      const unsigned long remainingBudgetMs = HTTP_POST_BUDGET_MS - elapsedMs;
      const uint32_t firstByteTimeoutMs = (remainingBudgetMs < HTTP_FIRST_BYTE_TIMEOUT_MS)
        ? (uint32_t)remainingBudgetMs
        : HTTP_FIRST_BYTE_TIMEOUT_MS;

      wifiClient.stop();
      wifiClient.setTimeout(firstByteTimeoutMs);

      unsigned long tConnect = millis();
      int connectOk = wifiClient.connect(INVERTER_HOST, hostPort, HTTP_TCP_CONNECT_TIMEOUT_MS);
      unsigned long connectMs = millis() - tConnect;
      if (!connectOk) {
        errorMessage = String("POST ") + path + " (" + label + ") failed: TCP connect (" + connectMs + "ms)";
        httpCode = 0;
        lastInverterStatusCode = 0;
        if (debugMode) {
          appLogger.log(String("[WIFI-DEBUG] POST tcp_connect_failed path=") + path +
                        " attempt=" + label +
                        " connect_ms=" + connectMs +
                        " wifi_status=" + wifiStatusToString(WiFi.status()));
        }
        return false;
      }

      String request;
      request.reserve(128 + path.length() + contentType.length() + payload.length());
      request += "POST ";
      request += path;
      request += " HTTP/1.1\r\nHost: ";
      request += INVERTER_HOST;
      request += "\r\nContent-Type: ";
      request += contentType;
      request += "\r\nContent-Length: ";
      request += String(payload.length());
      request += "\r\nConnection: close\r\n\r\n";
      request += payload;

      unsigned long tSend = millis();
      size_t sent = wifiClient.write((const uint8_t*)request.c_str(), request.length());
      wifiClient.flush();
      unsigned long sendMs = millis() - tSend;
      if (sent != request.length()) {
        errorMessage = String("POST ") + path + " (" + label + ") failed: short send (" + sent + "/" + request.length() + ")";
        httpCode = 0;
        lastInverterStatusCode = 0;
        if (debugMode) {
          appLogger.log(String("[WIFI-DEBUG] POST short_send path=") + path +
                        " attempt=" + label +
                        " sent=" + sent + " expected=" + request.length() +
                        " send_ms=" + sendMs);
        }
        return false;
      }

      unsigned long tFirstByte = millis();
      unsigned long firstByteDeadline = tFirstByte + firstByteTimeoutMs;
      while (wifiClient.connected() && wifiClient.available() == 0 && millis() < firstByteDeadline) {
        vTaskDelay(pdMS_TO_TICKS(5));
      }
      unsigned long firstByteMs = millis() - tFirstByte;
      if (wifiClient.available() == 0) {
        errorMessage = String("POST ") + path + " (" + label + ") failed: no response in " + firstByteMs + "ms";
        httpCode = 0;
        lastInverterStatusCode = 0;
        if (debugMode) {
          appLogger.log(String("[WIFI-DEBUG] POST no_response path=") + path +
                        " attempt=" + label +
                        " first_byte_ms=" + firstByteMs +
                        " connected=" + wifiClient.connected() +
                        " wifi_status=" + wifiStatusToString(WiFi.status()));
        }
        return false;
      }

      String statusLine = wifiClient.readStringUntil('\n');
      int code = 0;
      int sp1 = statusLine.indexOf(' ');
      if (sp1 > 0) {
        int sp2 = statusLine.indexOf(' ', sp1 + 1);
        if (sp2 > sp1) {
          code = statusLine.substring(sp1 + 1, sp2).toInt();
        }
      }

      while (wifiClient.connected() || wifiClient.available()) {
        String line = wifiClient.readStringUntil('\n');
        if (line.length() == 0 || line == "\r") break;
      }

      responseBody = "";
      unsigned long bodyDeadline = millis() + 500;
      while ((wifiClient.connected() || wifiClient.available()) && millis() < bodyDeadline) {
        while (wifiClient.available()) {
          responseBody += (char)wifiClient.read();
          bodyDeadline = millis() + 200;
        }
        vTaskDelay(pdMS_TO_TICKS(2));
      }
      wifiClient.stop();

      httpCode = code;
      lastInverterStatusCode = code;
      if (debugMode) {
        appLogger.log(String("[WIFI-DEBUG] POST done path=") + path +
                      " attempt=" + label +
                      " http=" + code +
                      " total_ms=" + (millis() - postStartMs) +
                      " first_byte_ms=" + firstByteMs +
                      " body_len=" + responseBody.length());
      }
      if (code <= 0) {
        errorMessage = String("POST ") + path + " (" + label + ") failed: bad status line '" + statusLine + "'";
        return false;
      }
      return true;
    };

    // For /power, first try browser-like multipart encoding.
    if (path == "/power") {
      String boundary = String("----mvb") + String(millis(), HEX);
      String multipartBody;
      multipartBody.reserve(96 + body.length());
      multipartBody += "--";
      multipartBody += boundary;
      multipartBody += "\r\nContent-Disposition: form-data; name=\"power\"\r\n\r\n";
      multipartBody += body;
      multipartBody += "\r\n--";
      multipartBody += boundary;
      multipartBody += "--\r\n";

      String multipartType = String("multipart/form-data; boundary=") + boundary;
      if (attemptPost(multipartType, multipartBody, "multipart")) {
        logWifiBridge(String("POST ") + path + " success (HTTP " + httpCode + ", multipart)");
        return true;
      }
      if (debugMode) {
        appLogger.log(String("[WIFI-DEBUG] POST fallback to text/plain path=") + path +
                      " last_error=" + errorMessage);
      }
    }

    // Fallback for /power and default path for all other POST endpoints.
    if (attemptPost("text/plain", body, "text")) {
      logWifiBridge(String("POST ") + path + " success (HTTP " + httpCode + ", text/plain)");
      return true;
    }

    wl_status_t status = WiFi.status();
    if (status != WL_CONNECTED) {
      powerDownWifiRadio();
    }
    return false;
  }

  // GET path uses HTTPClient as before.
  if (!http.begin(wifiClient, url)) {
    errorMessage = String("Failed to initialize HTTP client for ") + path;
    httpCode = 0;
    return false;
  }

  http.setTimeout(WIFI_BRIDGE_HTTP_TIMEOUT_MS);
  http.setConnectTimeout(WIFI_BRIDGE_HTTP_TIMEOUT_MS);
  int code = 0;

  if (method == "GET") {
    code = http.GET();
  } else {
    errorMessage = String("Unsupported HTTP method: ") + method;
    http.end();
    httpCode = 0;
    return false;
  }

  httpCode = code;
  lastInverterStatusCode = code;

  if (code <= 0) {
    errorMessage = method + ' ' + path + " failed: " + http.errorToString(code);
    http.end();
    // IMPORTANT: do NOT power down the radio here. HTTP-layer failures
    // (read timeout, connection refused, etc.) do not mean WiFi itself
    // is broken. If we shut the radio down on every HTTP error, the next
    // request must run a full scan+associate cycle (5-50s), which causes
    // queued retries to time out and feed back into more radio shutdowns.
    // Only powerDownWifiRadio() when WiFi itself disconnects.
    wl_status_t status = WiFi.status();
    if (status != WL_CONNECTED) {
      if (debugMode) {
        appLogger.log(String("[WIFI-DEBUG] HTTP failure with WiFi down path=") + path +
                      " status=" + wifiStatusToString(status) +
                      " err=" + errorMessage);
      }
      powerDownWifiRadio();
    } else if (debugMode) {
      appLogger.log(String("[WIFI-DEBUG] HTTP failure but WiFi up path=") + path +
                    " code=" + code +
                    " err=" + errorMessage);
    }
    return false;
  }

  responseBody = http.getString();
  http.end();

  if (debugMode) {
    logWifiBridge(method + ' ' + path + " success (HTTP " + code + ')');
  }
  return true;
}

bool triggerWifiOffIfConnected() {
  ScopedWifiOperationLock lock(wifiOperationMutex, WIFI_LOCK_TIMEOUT_CONNECTED_MS, "wifiOperationMutex/wifiOff");
  if (!lock.acquired()) {
    return false;
  }
  if (!isWifiConnected()) {
    return false;
  }

  logWifiBridge("Sending single press to turn inverter WiFi OFF.");
  pressInverterWifiButtonOnce();
  return true;
}

