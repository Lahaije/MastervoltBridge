#include "wifi_bridge.h"

#include <WiFi.h>
#include <HTTPClient.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>

#include "settings.h"
#include "logger.h"

namespace {

constexpr uint32_t WIFI_CONNECT_TIMEOUT_MS = 10000;
constexpr uint32_t WIFI_CONNECT_POLL_MS = 250;
constexpr uint32_t WIFI_LOCK_TIMEOUT_MS = 50;
constexpr uint32_t RECOVERY_TIMEOUT_MS = 8000;
constexpr uint32_t RECOVERY_SCAN_DWELL_MS = 500;
constexpr uint32_t RECOVERY_SCAN_SETTLE_MS = 100;
constexpr uint32_t RECOVERY_BUSY_LOG_INTERVAL_MS = 5000;

SemaphoreHandle_t wifiOperationMutex = nullptr;
bool recoveryMeasurementInProgress = false;
unsigned long lastRecoveryBusyLogMs = 0;

// Discovered AP BSSID+channel from scan (used to speed up connect)
uint8_t discoveredBssid[6] = {0};
uint8_t discoveredChannel = 0;
bool hasDiscoveredAp = false;

class ScopedWifiOperationLock {
public:
  explicit ScopedWifiOperationLock(uint32_t timeoutMs)
      : acquired_(wifiOperationMutex != nullptr &&
                  xSemaphoreTake(wifiOperationMutex, pdMS_TO_TICKS(timeoutMs)) == pdTRUE) {}

  ~ScopedWifiOperationLock() {
    if (acquired_) {
      xSemaphoreGive(wifiOperationMutex);
    }
  }

  bool acquired() const { return acquired_; }

private:
  bool acquired_;
};

class RecoveryMeasurementGuard {
public:
  RecoveryMeasurementGuard() {
    recoveryMeasurementInProgress = true;
    lastRecoveryBusyLogMs = millis();
  }

  ~RecoveryMeasurementGuard() {
    recoveryMeasurementInProgress = false;
  }
};

void logWifiBridge(const String& message) {
  appLogger.log(String("[WIFI-BRIDGE] ") + message);
}

void logRecovery(const String& message) {
  appLogger.log(String("[RECOVERY] ") + message);
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

bool isRecoveryActiveAndLogThrottled() {
  if (!recoveryMeasurementInProgress) {
    return false;
  }

  unsigned long nowMs = millis();
  if (nowMs - lastRecoveryBusyLogMs > RECOVERY_BUSY_LOG_INTERVAL_MS) {
    logWifiBridge("Recovery measurement active; skipping regular connect attempt.");
    lastRecoveryBusyLogMs = nowMs;
  }
  return true;
}

void startConfiguredWifiConnection() {
  const char* pass = strlen(INVERTER_WIFI_PASSWORD) ? INVERTER_WIFI_PASSWORD : nullptr;
  if (hasDiscoveredAp) {
    logRecovery(String("Connecting with ch=") + discoveredChannel + String(" bssid from scan."));
    WiFi.begin(INVERTER_WIFI_SSID, pass, discoveredChannel, discoveredBssid);
  } else if (pass == nullptr) {
    WiFi.begin(INVERTER_WIFI_SSID);
  } else {
    WiFi.begin(INVERTER_WIFI_SSID, pass);
  }
}

bool waitForWifiConnection(uint32_t timeoutMs, bool abortOnRecoveryStart) {
  unsigned long startedAt = millis();
  while (millis() - startedAt < timeoutMs) {
    if (abortOnRecoveryStart && recoveryMeasurementInProgress) {
      logWifiBridge("Recovery measurement active; aborting connect attempt.");
      return false;
    }

    wl_status_t status = WiFi.status();
    if (status == WL_CONNECTED) {
      logWifiBridge(String("Connected. IP=") + WiFi.localIP().toString());
      return true;
    }

    if (status == WL_CONNECT_FAILED || status == WL_NO_SSID_AVAIL || status == WL_CONNECTION_LOST) {
      logWifiBridge(String("Connect aborted early. status=") + String((int)status) +
                    String(" (") + wifiStatusToString(status) + String(")"));
      return false;
    }

    vTaskDelay(pdMS_TO_TICKS(WIFI_CONNECT_POLL_MS));
  }

  wl_status_t status = WiFi.status();
  logWifiBridge(String("Connect timeout after ") + timeoutMs +
                String("ms. status=") + String((int)status) +
                String(" (") + wifiStatusToString(status) + String(")"));
  return status == WL_CONNECTED;
}

void logRecoveryScanResults(int networkCount) {
  if (networkCount < 0) {
    logRecovery(String("WiFi scan failed. code=") + networkCount);
    return;
  }

  if (networkCount == 0) {
    logRecovery("No broadcast SSIDs detected (0 networks).");
    return;
  }

  // Only log the inverter SSID, ignore all other networks
  int8_t bestRssi = -127;
  int bestIdx = -1;
  for (int i = 0; i < networkCount; i++) {
    if (WiFi.SSID(i) == INVERTER_WIFI_SSID) {
      int8_t rssi = (int8_t)WiFi.RSSI(i);
      if (rssi > bestRssi) { bestRssi = rssi; bestIdx = i; }
    }
  }
  if (bestIdx >= 0) {
    memcpy(discoveredBssid, WiFi.BSSID(bestIdx), 6);
    discoveredChannel = (uint8_t)WiFi.channel(bestIdx);
    hasDiscoveredAp = true;
    logRecovery(String("Scan found ") + INVERTER_WIFI_SSID +
                String(" ch=") + discoveredChannel +
                String(" RSSI=") + bestRssi);
  } else {
    hasDiscoveredAp = false;
    logRecovery(String("Scan: ") + networkCount + String(" networks, inverter not visible."));
  }
}

void runRecoveryScanPass() {
  vTaskDelay(pdMS_TO_TICKS(RECOVERY_SCAN_SETTLE_MS));
  logRecovery("Scanning...");
  int networkCount = WiFi.scanNetworks(false, true, false, RECOVERY_SCAN_DWELL_MS);
  logRecoveryScanResults(networkCount);
  WiFi.scanDelete();
}

void triggerPulseSequence() {
  logRecovery("Triggering inverter WiFi wake pulse sequence.");

  digitalWrite(PIN_INVERTER_WIFI_WAKE, LOW);
  delay(PULSE_HIGH_MS);
  digitalWrite(PIN_INVERTER_WIFI_WAKE, HIGH);

  delay(PULSE_GAP_MS);

  digitalWrite(PIN_INVERTER_WIFI_WAKE, LOW);
  delay(PULSE_HIGH_MS);
  digitalWrite(PIN_INVERTER_WIFI_WAKE, HIGH);
}

}  // namespace

void wifiBridgeInit() {
  if (wifiOperationMutex == nullptr) {
    wifiOperationMutex = xSemaphoreCreateMutex();
  }
  // Clean radio state on boot
  WiFi.mode(WIFI_OFF);
  delay(100);
  WiFi.mode(WIFI_STA);
  delay(100);

  pinMode(PIN_INVERTER_WIFI_WAKE, OUTPUT);
  digitalWrite(PIN_INVERTER_WIFI_WAKE, HIGH);
}

/**
 * Ensure WiFi is connected to the inverter's network.
 * If not connected, attempt to connect with a configurable timeout.
 */
bool ensureWifiConnected() {
  return ensureWifiConnectedWithTimeout(WIFI_CONNECT_TIMEOUT_MS);
}

bool ensureWifiConnectedWithTimeout(uint32_t timeoutMs) {
  if (isRecoveryActiveAndLogThrottled()) {
    return false;
  }

  ScopedWifiOperationLock lock(WIFI_LOCK_TIMEOUT_MS);
  if (!lock.acquired()) {
    logWifiBridge("WiFi operation busy; skipping regular connect attempt.");
    return false;
  }

  wl_status_t status = WiFi.status();
  if (status == WL_CONNECTED) {
    return true;
  }

  // If the station is already in-flight, do not reset config with another begin().
  if (status == WL_IDLE_STATUS) {
    logWifiBridge("Connect already in progress; waiting for result.");
  } else {
    startConfiguredWifiConnection();
  }

  return waitForWifiConnection(timeoutMs, true);
}

/**
 * Generic method to fetch data from inverter (GET or POST).
 * Automatically ensures WiFi is connected before making the request.
 */
bool fetchInverterData(const String& method, const String& path, const String& body,
                       String& responseBody, int& httpCode, String& errorMessage) {
  if (isRecoveryActiveAndLogThrottled()) {
    errorMessage = "Recovery measurement is active";
    httpCode = 0;
    return false;
  }

  ScopedWifiOperationLock lock(WIFI_LOCK_TIMEOUT_MS);
  if (!lock.acquired()) {
    errorMessage = "WiFi operation busy";
    httpCode = 0;
    return false;
  }

  wl_status_t status = WiFi.status();
  if (status != WL_CONNECTED) {
    if (status == WL_IDLE_STATUS) {
      logWifiBridge("Connect already in progress; waiting for result.");
    } else {
      startConfiguredWifiConnection();
    }
  }

  // Ensure WiFi is connected before attempting to fetch from inverter
  if (!waitForWifiConnection(WIFI_CONNECT_TIMEOUT_MS, true)) {
    errorMessage = "ESP32 failed to connect to inverter WiFi";
    httpCode = 0;
    return false;
  }

  WiFiClient wifiClient;
  HTTPClient http;
  String url = String("http://") + INVERTER_HOST + path;

  if (!http.begin(wifiClient, url)) {
    errorMessage = String("Failed to initialize HTTP client for ") + path;
    httpCode = 0;
    return false;
  }

  http.setTimeout(WIFI_BRIDGE_HTTP_TIMEOUT_MS);
  int code = 0;

  if (method == "GET") {
    code = http.GET();
  } else if (method == "POST") {
    http.addHeader("Content-Type", "text/plain");
    code = http.POST(body);
  } else {
    errorMessage = String("Unsupported HTTP method: ") + method;
    http.end();
    httpCode = 0;
    return false;
  }

  httpCode = code;
  lastInverterStatusCode = code;

  if (code <= 0) {
    errorMessage = String(method) + String(" ") + path + String(" failed: ") + http.errorToString(code);
    http.end();
    return false;
  }

  responseBody = http.getString();
  http.end();

  logWifiBridge(String(method) + String(" ") + path + String(" success (HTTP ") + code + String(")"));
  return true;
}

bool triggerWifiOffIfConnected() {
  ScopedWifiOperationLock lock(WIFI_LOCK_TIMEOUT_MS);
  if (!lock.acquired()) {
    logRecovery("WiFi operation busy. Cannot send WiFi-off press.");
    return false;
  }

  if (recoveryMeasurementInProgress) {
    logRecovery("Measurement in progress. Skipping WiFi-off press.");
    return false;
  }

  if (WiFi.status() != WL_CONNECTED) {
    logRecovery("WiFi already OFF (not connected). No press sent.");
    return false;
  }

  logRecovery("Sending single press to turn inverter WiFi OFF.");
  digitalWrite(PIN_INVERTER_WIFI_WAKE, LOW);
  delay(PULSE_HIGH_MS);
  digitalWrite(PIN_INVERTER_WIFI_WAKE, HIGH);
  return true;
}

void measureConnectionTime() {
  ScopedWifiOperationLock lock(WIFI_LOCK_TIMEOUT_MS);
  if (!lock.acquired()) {
    logRecovery("WiFi operation busy. Ignoring measurement request.");
    return;
  }

  if (recoveryMeasurementInProgress) {
    logRecovery("Measurement already in progress. Ignoring new request.");
    return;
  }

  RecoveryMeasurementGuard guard;
  triggerPulseSequence();
  unsigned long startMs = millis();

  logRecovery("Measuring connection time after pulse...");

  // Full radio reset for clean state before scan+connect
  WiFi.mode(WIFI_OFF);
  vTaskDelay(pdMS_TO_TICKS(100));
  WiFi.mode(WIFI_STA);
  hasDiscoveredAp = false;

  // Scan to find inverter BSSID+channel — avoids full scan inside WiFi.begin()
  runRecoveryScanPass();

  // Connect using channel+BSSID if found, else auto-scan
  startConfiguredWifiConnection();

  // Poll with per-iteration logging
  uint32_t iteration = 0;
  while (millis() - startMs < RECOVERY_TIMEOUT_MS) {
    iteration++;
    wl_status_t status = WiFi.status();
    unsigned long elapsedMs = millis() - startMs;
    logRecovery(String("Iteration ") + iteration +
                String(" t=") + elapsedMs +
                String("ms status=") + String((int)status) +
                String(" (") + wifiStatusToString(status) + String(")"));

    if (status == WL_CONNECTED) {
      uint8_t ch = WiFi.channel();
      uint8_t* bssid = WiFi.BSSID();
      char bssidStr[18];
      snprintf(bssidStr, sizeof(bssidStr), "%02X:%02X:%02X:%02X:%02X:%02X",
               bssid[0], bssid[1], bssid[2], bssid[3], bssid[4], bssid[5]);
      logRecovery(String("Connected in ") + elapsedMs + "ms. IP=" + WiFi.localIP().toString() +
                  " channel=" + ch + " bssid=" + bssidStr);
      return;
    }

    if (status == WL_CONNECT_FAILED || status == WL_NO_SSID_AVAIL || status == WL_CONNECTION_LOST) {
      logRecovery(String("Retrying after status=") + wifiStatusToString(status));
      hasDiscoveredAp = false;
      startConfiguredWifiConnection();
    }

    vTaskDelay(pdMS_TO_TICKS(WIFI_CONNECT_POLL_MS));
  }

  wl_status_t finalStatus = WiFi.status();
  logRecovery(String("Timeout after ") + RECOVERY_TIMEOUT_MS + "ms. Final status=" +
              String((int)finalStatus) + " (" + wifiStatusToString(finalStatus) + ")");
}
