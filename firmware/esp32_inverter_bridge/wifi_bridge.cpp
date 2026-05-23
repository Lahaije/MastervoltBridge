#include "wifi_bridge.h"

#include <WiFi.h>
#include <HTTPClient.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>

#include "settings.h"
#include "logger.h"

namespace {

constexpr uint32_t WIFI_CONNECT_POLL_MS = 250;
constexpr uint32_t WIFI_LOCK_TIMEOUT_MS = 50;
constexpr uint32_t CONNECT_TIMEOUT_MS = 7000;
constexpr uint32_t SCAN_SETTLE_MS = 100;

// Retry settings for ensureConnected(): pulse once, then try connecting up to
// MAX_CONNECT_RETRIES times (alternating dwell/auto) before returning failure.
// This avoids re-pulsing the inverter button on every attempt, which could
// toggle the WiFi back OFF.
constexpr int MAX_CONNECT_RETRIES = 3;
constexpr uint32_t RETRY_PAUSE_MS = 500;

// Dwell path: short scan dwell, hint fallback enabled.
constexpr uint32_t DWELL_SCAN_DWELL_MS = 200;
constexpr bool DWELL_USE_HINT_FALLBACK = true;

// Auto path: longer scan dwell, no hint fallback (auto-discovery only).
constexpr uint32_t AUTO_SCAN_DWELL_MS = 500;
constexpr bool AUTO_USE_HINT_FALLBACK = false;

SemaphoreHandle_t wifiOperationMutex = nullptr;

// Discovered AP BSSID+channel from scan during the current connect attempt.
uint8_t discoveredBssid[6] = {0};
uint8_t discoveredChannel = 0;
bool hasDiscoveredAp = false;

// Optional configured AP hint from settings (expected inverter channels+BSSID).
uint8_t configuredHintBssid[6] = {0};
uint8_t hintChannelIndex = 0;
bool hasConfiguredHint = false;

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

void logWifiBridge(const String& message) {
  appLogger.log(String("[WIFI-BRIDGE] ") + message);
}

void logConnect(const String& message) {
  appLogger.log(String("[WIFI-CONNECT] ") + message);
}

void powerDownWifiRadio() {
  WiFi.mode(WIFI_OFF);
  hasDiscoveredAp = false;
}

void powerUpWifiRadioForConnect() {
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

bool ensureWifiConnectedOrPowerDown() {
  if (isWifiConnected()) {
    return true;
  }
  powerDownWifiRadio();
  logWifiBridge("WiFi not connected; radio powered down.");
  return false;
}

bool acquireWifiLock(ScopedWifiOperationLock& lock) {
  if (!lock.acquired()) {
    logWifiBridge("WiFi operation busy; lock not acquired.");
    return false;
  }

  return true;
}

bool acquireConnectedWifi(ScopedWifiOperationLock& lock) {
  if (!acquireWifiLock(lock)) {
    return false;
  }

  return ensureWifiConnectedOrPowerDown();
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
  int networkCount = WiFi.scanNetworks(false, true, false, dwellMs);
  hasDiscoveredAp = false;
  if (networkCount > 0) {
    int8_t bestRssi = -127;
    int bestIdx = -1;
    for (int i = 0; i < networkCount; i++) {
      if (WiFi.SSID(i) == INVERTER_WIFI_SSID) {
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
// Brings the radio to STA mode, scans for the inverter SSID, then attempts
// to connect using the discovered channel/BSSID (with optional configured-hint
// fallback). Returns true on success, false on timeout.
bool runConnectPath(const char* pathName, uint32_t scanDwellMs, bool useHintFallback) {
  unsigned long startMs = millis();
  logConnect(String("start path=") + pathName +
             " scan_dwell_ms=" + scanDwellMs +
             " hint_fallback=" + (useHintFallback ? "1" : "0"));

  // Ensure radio is up for scan/connect. We power it down explicitly on
  // failures and disconnected states elsewhere.
  powerUpWifiRadioForConnect();
  runScanPass(scanDwellMs);
  startWifiBegin(useHintFallback);

  while (millis() - startMs < CONNECT_TIMEOUT_MS) {
    wl_status_t status = WiFi.status();
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

// ---------------------------------------------------------------------------
// Connect strategy functions (named so they appear in logs for A/B analysis).
// File-local; the manager below is the only caller.
// ---------------------------------------------------------------------------

bool connectWifiDwell() {
  return runConnectPath("dwell", DWELL_SCAN_DWELL_MS, DWELL_USE_HINT_FALLBACK);
}

bool connectWifiAuto() {
  return runConnectPath("auto", AUTO_SCAN_DWELL_MS, AUTO_USE_HINT_FALLBACK);
}

bool connectWifi(bool useDwellPath) {
  return useDwellPath ? connectWifiDwell() : connectWifiAuto();
}

}  // namespace

// ---------------------------------------------------------------------------
// WifiConnectionManager: owns alternating connect-path state and exposes a
// single "give me a working WiFi connection" entry point used by callers.
// ---------------------------------------------------------------------------

WifiConnectionManager& WifiConnectionManager::getInstance() {
  static WifiConnectionManager instance;
  return instance;
}

bool WifiConnectionManager::ensureConnected() {
  ScopedWifiOperationLock lock(WIFI_LOCK_TIMEOUT_MS);
  if (!acquireWifiLock(lock)) {
    return false;
  }
  if (isWifiConnected()) {
    return true;
  }
  // Inverter WiFi is typically asleep; wake it with a double-press pulse
  // (OFF → ON). We pulse once, then retry the connect up to
  // MAX_CONNECT_RETRIES times WITHOUT pulsing again — the inverter may
  // simply need more time to become scannable after the initial wake.
  // Re-pulsing on every retry risks toggling the inverter WiFi back OFF.
  triggerPulseSequence();

  for (int attempt = 0; attempt < MAX_CONNECT_RETRIES; attempt++) {
    if (connectUsingNextPath()) {
      return true;
    }
    // Brief pause before the next scan/connect attempt (no pulse).
    vTaskDelay(pdMS_TO_TICKS(RETRY_PAUSE_MS));
  }
  return false;
}

bool WifiConnectionManager::forceReconnect() {
  ScopedWifiOperationLock lock(WIFI_LOCK_TIMEOUT_MS);
  if (!acquireWifiLock(lock)) {
    return false;
  }
  triggerPulseSequence();
  return connectUsingNextPath();
}

bool WifiConnectionManager::connectUsingNextPath() {
  bool useDwell = nextUseDwell_;
  nextUseDwell_ = !nextUseDwell_;
  return connectWifi(useDwell);
}

// ---------------------------------------------------------------------------
// Public WiFi bridge API.
// ---------------------------------------------------------------------------

void wifiBridgeInit() {
  if (wifiOperationMutex == nullptr) {
    wifiOperationMutex = xSemaphoreCreateMutex();
  }
  initializeConfiguredHint();

  // Boot with the radio off; the first connect attempt powers it up.
  WiFi.mode(WIFI_OFF);

  pinMode(PIN_INVERTER_WIFI_WAKE, OUTPUT);
  digitalWrite(PIN_INVERTER_WIFI_WAKE, HIGH);
}

bool fetchInverterData(const String& method, const String& path, const String& body,
                       String& responseBody, int& httpCode, String& errorMessage) {
  // Make sure WiFi is up before we try to talk to the inverter. The manager
  // releases the WiFi-operation lock once the (re)connect attempt completes,
  // so we can re-acquire it below for the HTTP exchange.
  if (!WifiConnectionManager::getInstance().ensureConnected()) {
    errorMessage = "ESP32 failed to connect to inverter WiFi";
    httpCode = 0;
    return false;
  }

  ScopedWifiOperationLock lock(WIFI_LOCK_TIMEOUT_MS);
  if (!acquireConnectedWifi(lock)) {
    errorMessage = lock.acquired()
      ? "WiFi dropped before HTTP request"
      : "WiFi operation busy";
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
    errorMessage = method + ' ' + path + " failed: " + http.errorToString(code);
    http.end();
    powerDownWifiRadio();
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
  ScopedWifiOperationLock lock(WIFI_LOCK_TIMEOUT_MS);
  if (!acquireConnectedWifi(lock)) {
    return false;
  }

  logWifiBridge("Sending single press to turn inverter WiFi OFF.");
  pressInverterWifiButtonOnce();
  return true;
}
