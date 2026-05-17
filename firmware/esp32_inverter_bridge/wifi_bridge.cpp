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
constexpr uint32_t RECOVERY_TIMEOUT_MS = 30000;
constexpr uint32_t RECOVERY_SCAN_DWELL_MS = 1000;
constexpr uint32_t RECOVERY_SCAN_SETTLE_MS = 200;
constexpr uint32_t RECOVERY_BUSY_LOG_INTERVAL_MS = 5000;

SemaphoreHandle_t wifiOperationMutex = nullptr;
bool recoveryMeasurementInProgress = false;
unsigned long lastRecoveryBusyLogMs = 0;

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
  if (strlen(INVERTER_WIFI_PASSWORD) == 0) {
    WiFi.begin(INVERTER_WIFI_SSID);
  } else {
    WiFi.begin(INVERTER_WIFI_SSID, INVERTER_WIFI_PASSWORD);
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

  bool inverterSsidFound = false;
  for (int i = 0; i < networkCount; i++) {
    String ssid = WiFi.SSID(i);
    if (ssid == INVERTER_WIFI_SSID) {
      inverterSsidFound = true;
    }
    if (ssid.length() == 0) {
      ssid = "<hidden>";
    }
    logRecovery(String(ssid) + String(" (RSSI=") + WiFi.RSSI(i) + String(")"));
  }

  if (inverterSsidFound) {
    logRecovery("Target inverter SSID is visible in scan.");
  } else {
    logRecovery("Target inverter SSID not visible in scan results.");
  }
}

void runRecoveryScanPass() {
  WiFi.disconnect(true, true);
  vTaskDelay(pdMS_TO_TICKS(RECOVERY_SCAN_SETTLE_MS));

  logRecovery("Scanning visible networks...");
  int networkCount = WiFi.scanNetworks(false, true, false, RECOVERY_SCAN_DWELL_MS);
  logRecoveryScanResults(networkCount);
  WiFi.scanDelete();
}

}  // namespace

void wifiBridgeInit() {
  if (wifiOperationMutex == nullptr) {
    wifiOperationMutex = xSemaphoreCreateMutex();
  }
  pinMode(PIN_INVERTER_WIFI_WAKE, OUTPUT);
  digitalWrite(PIN_INVERTER_WIFI_WAKE, HIGH);
}

/**
 * Ensure WiFi is connected to the inverter's network.
 * If not connected, attempt to connect with a configurable timeout.
 */
bool ensureWifiConnected() {
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

  return waitForWifiConnection(WIFI_CONNECT_TIMEOUT_MS, true);
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

/**
 * Trigger the inverter WiFi wake pulse sequence for recovery.
 */
void wifiBridgeTriggerPulseSequence() {
  logRecovery("Triggering inverter WiFi wake pulse sequence.");

  digitalWrite(PIN_INVERTER_WIFI_WAKE, LOW);
  delay(PULSE_HIGH_MS);
  digitalWrite(PIN_INVERTER_WIFI_WAKE, HIGH);

  delay(PULSE_GAP_MS);

  digitalWrite(PIN_INVERTER_WIFI_WAKE, LOW);
  delay(PULSE_HIGH_MS);
  digitalWrite(PIN_INVERTER_WIFI_WAKE, HIGH);
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
  wifiBridgeTriggerPulseSequence();
  unsigned long startMs = millis();

  logRecovery("Measuring connection time after pulse...");
  WiFi.mode(WIFI_STA);

  while (millis() - startMs < RECOVERY_TIMEOUT_MS) {
    runRecoveryScanPass();
    startConfiguredWifiConnection();

    unsigned long elapsedMs = millis() - startMs;
    if (elapsedMs >= RECOVERY_TIMEOUT_MS) {
      break;
    }
    uint32_t remainingMs = RECOVERY_TIMEOUT_MS - elapsedMs;
    if (waitForWifiConnection(remainingMs, false)) {
      unsigned long elapsed = millis() - startMs;
      logRecovery(String("Connected in ") + elapsed + "ms. IP=" + WiFi.localIP().toString());
      return;
    }
  }

  logRecovery("Failed to connect within 30s.");
}
