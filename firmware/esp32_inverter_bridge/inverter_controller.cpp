#include "inverter_controller.h"

#include "inverter_data.h"
#include "inverter_link_state.h"
#include "settings.h"
#include "logger.h"
#include "wifi_bridge.h"

// State-transition logic (linkStateFromStreak, intervalForState) lives here in
// inverter_controller.cpp — it is the monitor's decision-making, not state storage.
// State storage, hook registry, and toString() live in inverter_link_state.cpp.

namespace {
constexpr const char* HOME_ENDPOINT = "/home";
constexpr const char* MULTIPART_BOUNDARY = "FormBoundary7MA4YWxkTrZu0gW";

// Encode a newline-separated "key=value\nkey=value" payload as a
// multipart/form-data body using MULTIPART_BOUNDARY. Each pair becomes one
// form part. Empty or malformed lines are skipped.
static String buildMultipartBody(const String& payload) {
  String body;
  int start = 0;
  while (start < (int)payload.length()) {
    int nl = payload.indexOf('\n', start);
    String pair = (nl < 0) ? payload.substring(start) : payload.substring(start, nl);
    start = (nl < 0) ? (int)payload.length() : nl + 1;
    pair.trim();
    if (pair.length() == 0) continue;
    int eq = pair.indexOf('=');
    if (eq < 0) continue;
    String key   = pair.substring(0, eq);
    String value = pair.substring(eq + 1);
    body += String("--") + MULTIPART_BOUNDARY + "\r\n";
    body += String("Content-Disposition: form-data; name=\"") + key + "\"\r\n";
    body += "\r\n";
    body += value + "\r\n";
  }
  body += String("--") + MULTIPART_BOUNDARY + "--\r\n";
  return body;
}

// Map streak duration to target state.
static InverterLinkState desiredStateFromStreak(uint32_t streakMs) {
  InverterLinkState expectedState = InverterLinkState::RETRYING;
  if (streakMs == 0) {
    expectedState = InverterLinkState::ONLINE;
  } else if (streakMs >= LINK_BACKOFF_TO_DORMANT_MS) {
    expectedState = InverterLinkState::DORMANT;
  } else if (streakMs >= LINK_RETRYING_TO_BACKOFF_MS) {
    expectedState = InverterLinkState::BACKOFF;
  }
  return expectedState;
}

// Return the default poll interval for a given state.
static uint32_t intervalForState(InverterLinkState s) {
  switch (s) {
    case InverterLinkState::BACKOFF:  return LINK_BACKOFF_INTERVAL_MS;
    case InverterLinkState::DORMANT:  return LINK_DORMANT_INTERVAL_MS;
    default:                          return WIFI_BRIDGE_POLL_INTERVAL_MS;
  }
}
}  // namespace

InverterController::InverterController() {
}

void InverterController::initialize() {
  if (isInitialized) {
    return;
  }

  if (dataMutex == nullptr) {
    dataMutex = xSemaphoreCreateMutex();
  }

  // Register one poll-interval hook for all interval-owning states.
  REGISTER_STATE_ENTRY_HOOK(InverterLinkState::ONLINE,  updatePollFrequency);
  REGISTER_STATE_ENTRY_HOOK(InverterLinkState::BACKOFF, updatePollFrequency);
  REGISTER_STATE_ENTRY_HOOK(InverterLinkState::DORMANT, updatePollFrequency);

  // Register transition hooks for ONLINE side effects.
  REGISTER_STATE_CHANGE_HOOK(InverterLinkState::STARTING, InverterLinkState::ONLINE, loadSettingsOnBoot);
  REGISTER_STATE_CHANGE_HOOK(InverterLinkState::BACKOFF,  InverterLinkState::ONLINE, updateAllInverterParam);
  REGISTER_STATE_CHANGE_HOOK(InverterLinkState::DORMANT,  InverterLinkState::ONLINE, updateAllInverterParam);

  // Create polling task only after hooks are registered so they are ready
  // before the task transitions STARTING -> ONLINE.
  if (pollingTaskHandle == nullptr) {
    xTaskCreatePinnedToCore(
      pollingTaskEntry,
      "inverter_controller",
      6144,
      this,
      1,
      &pollingTaskHandle,
      0
    );
  }

  isInitialized = true;
  appLogger.log("[INVERTER-CONTROLLER] Inverter controller initialized");
}

void InverterController::shutdown() {
  if (pollingTaskHandle != nullptr) {
    vTaskDelete(pollingTaskHandle);
    pollingTaskHandle = nullptr;
  }

  if (dataMutex != nullptr) {
    vSemaphoreDelete(dataMutex);
    dataMutex = nullptr;
  }

  isInitialized = false;
  appLogger.log("[INVERTER-CONTROLLER] Inverter controller shut down");
}

void InverterController::pollingTaskEntry(void* param) {
  InverterController* self = static_cast<InverterController*>(param);
  if (self != nullptr) {
    self->runPollingTask();
  }
}

bool InverterController::incrementCounterLocked(uint32_t& counter) {
  if (dataMutex == nullptr) {
    return false;
  }
  if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(DATA_MUTEX_TIMEOUT_MS)) != pdTRUE) {
    if (debugMode) {
      appLogger.log("[INVERTER-CONTROLLER] dataMutex timeout in incrementCounterLocked");
    }
    return false;
  }
  counter++;
  xSemaphoreGive(dataMutex);
  return true;
}

void InverterController::linkStateFromStreak(uint32_t streakMs) {
  InverterLinkState currentState = getInverterState();
  InverterLinkState expectedState = desiredStateFromStreak(streakMs);

  if (currentState == expectedState) {
    return;
  }

  setInverterState(expectedState);

  if (debugMode) {
    appLogger.log(String("[INVERTER-CONTROLLER] State: ") + toString(currentState) +
                  " -> " + toString(expectedState) +
                  "  streak=" + (streakMs / 1000) + "s" +
                  "  interval=" + (currentRetryIntervalMs / 1000) + "s");
  }
}

void InverterController::runPollingTask() {
  failureStartMs = 0;
  // globalInverterState is already STARTING at boot. Initialize the poll interval
  // once from settings; later transitions own any overrides.
  currentRetryIntervalMs = WIFI_BRIDGE_POLL_INTERVAL_MS;

  while (true) {
    bool iterationOk = false;

    if (!WifiConnectionManager::getInstance().ensureConnected()) {
      appLogger.log("[INVERTER-CONTROLLER] No WiFi connection; skipping poll iteration");
    } else {
      String rawResponse;
      String errorMessage;
      int httpCode = 0;

      bool ok = fetchInverterData("GET", HOME_ENDPOINT, "", rawResponse, httpCode, errorMessage);

      if (ok) {
        HomeData parsedData;
        if (parseHomeResponse(rawResponse, parsedData)) {
          if (dataMutex != nullptr && xSemaphoreTake(dataMutex, pdMS_TO_TICKS(DATA_MUTEX_TIMEOUT_MS)) == pdTRUE) {
            cachedData = parsedData;
            lastUpdateMs = millis();
            successfulPolls++;
            xSemaphoreGive(dataMutex);

            appLogger.log(String("[INVERTER-CONTROLLER] Poll #") + successfulPolls +
                          ": Status=" + parsedData.operatingStatus +
                          " Power=" + parsedData.instantaneousPower + "W");
          } else if (dataMutex != nullptr && debugMode) {
            appLogger.log("[INVERTER-CONTROLLER] dataMutex timeout while caching /home poll result");
          }
          iterationOk = true;
        } else {
          incrementCounterLocked(failedPolls);
          appLogger.log("[INVERTER-CONTROLLER] Failed to parse /home response");
        }
      } else {
        incrementCounterLocked(failedPolls);
        appLogger.log(String("[INVERTER-CONTROLLER] Failed to fetch /home: ") + errorMessage);
      }
    }

    if (iterationOk) {
      // Poll succeeded — streak ends. Ensure state is ONLINE.
      uint32_t streakMs = (failureStartMs != 0) ? (millis() - failureStartMs) : 0;
      failureStartMs = 0;
      linkStateFromStreak(0);

      // Drain any deferred shadow/power-limit set requests now that we know
      // the inverter is reachable. See setPower() / setShadow() / applyPendingSettings().
      if (pendingSettings_) {
        applyPendingSettings();
      }
    } else {
      // Poll failed — accumulate streak and check thresholds.
      if (failureStartMs == 0) failureStartMs = millis();
      uint32_t streakMs = millis() - failureStartMs;
      linkStateFromStreak(streakMs);
    }

    vTaskDelay(pdMS_TO_TICKS(currentRetryIntervalMs));
  }
}

bool InverterController::getLatestHomeData(HomeData& dataOut) {
  if (dataMutex == nullptr) {
    appLogger.log("[INVERTER-CONTROLLER] Inverter controller not initialized");
    return false;
  }

  if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(DATA_MUTEX_TIMEOUT_MS)) != pdTRUE) {
    appLogger.log("[INVERTER-CONTROLLER] Failed to acquire data mutex");
    return false;
  }

  if (!cachedData.isValid()) {
    xSemaphoreGive(dataMutex);
    return false;
  }

  dataOut = cachedData;
  xSemaphoreGive(dataMutex);
  return true;
}

unsigned long InverterController::getLastUpdateMs() {
  if (dataMutex == nullptr) {
    return 0;
  }
  if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(DATA_MUTEX_TIMEOUT_MS)) != pdTRUE) {
    return 0;
  }
  unsigned long result = lastUpdateMs;
  xSemaphoreGive(dataMutex);
  return result;
}

bool InverterController::getShadow(bool& enabledOut) {
  if (dataMutex == nullptr) return false;
  if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(DATA_MUTEX_TIMEOUT_MS)) != pdTRUE) {
    return false;
  }
  bool known = shadowKnown_;
  bool value = shadowOn_;
  xSemaphoreGive(dataMutex);
  if (!known) return false;
  enabledOut = value;
  return true;
}

bool InverterController::getPowerLimit(uint16_t& wattsOut) {
  if (dataMutex == nullptr) return false;
  if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(DATA_MUTEX_TIMEOUT_MS)) != pdTRUE) {
    return false;
  }
  bool known = powerLimitKnown_;
  uint16_t value = powerLimitW_;
  xSemaphoreGive(dataMutex);
  if (!known) return false;
  wattsOut = value;
  return true;
}

InverterLinkState InverterController::getLinkState() {
  return getInverterState();
}

uint32_t InverterController::getFailureStreakMs() {
  uint32_t startMs = failureStartMs;
  if (startMs == 0) return 0;
  uint32_t now = millis();
  return (now >= startMs) ? (now - startMs) : 0;
}

uint32_t InverterController::getRetryIntervalMs() {
  return currentRetryIntervalMs;
}

void InverterController::setPollIntervalMs(uint32_t ms) {
  if (ms < 100) ms = 100;
  if (ms > 300000) ms = 300000;
  currentRetryIntervalMs = ms;
  appLogger.log(String("[INVERTER-CONTROLLER] Temporary poll interval override set to ") + ms + "ms");
}

void InverterController::updatePollFrequency(InverterLinkState /*from*/,
                                          InverterLinkState to) {
  getInstance().currentRetryIntervalMs = intervalForState(to);
}

// -----------------------------------------------------------------------------
// Transition hook: first-ever ONLINE after boot.
void InverterController::loadSettingsOnBoot(InverterLinkState /*from*/,
                                         InverterLinkState /*to*/) {
  appLogger.log("[INVERTER-CONTROLLER] First connection; fetching shadow + power limit");
  getInstance().fetchAndCacheSettings();
}

// Transition hook: ONLINE recovery after prolonged outage.
void InverterController::updateAllInverterParam(InverterLinkState from,
                                             InverterLinkState /*to*/) {
  appLogger.log(String("[INVERTER-CONTROLLER] Recovered from ") + toString(from) +
                "; refreshing shadow + power limit");
  getInstance().fetchAndCacheSettings();
}

void InverterController::fetchAndCacheSettings() {
  // Shadow: GET /shadow -> "<0|1>\n<interval>\n"
  {
    String body, err;
    int code = 0;
    if (fetchInverterData("GET", "/shadow", "", body, code, err) && code == 200) {
      body.trim();
      bool enabled = body.length() > 0 && body[0] == '1';
      if (dataMutex != nullptr && xSemaphoreTake(dataMutex, pdMS_TO_TICKS(DATA_MUTEX_TIMEOUT_MS)) == pdTRUE) {
        shadowOn_ = enabled;
        shadowKnown_ = true;
        xSemaphoreGive(dataMutex);
      } else if (dataMutex != nullptr && debugMode) {
        appLogger.log("[INVERTER-CONTROLLER] dataMutex timeout while caching /shadow state");
      }
      appLogger.log(String("[INVERTER-CONTROLLER] Shadow read: ") + (enabled ? "ON" : "OFF"));
    } else {
      appLogger.log(String("[INVERTER-CONTROLLER] Failed to read shadow: ") + err);
    }
  }
  // Power limit: GET /power -> "<watts>\n"
  {
    String body, err;
    int code = 0;
    if (fetchInverterData("GET", "/power", "", body, code, err) && code == 200) {
      body.trim();
      int watts = body.toInt();
      if (watts >= 0 && watts <= INVERTER_MAX_POWER_WATTS) {
        uint16_t w = static_cast<uint16_t>(watts);
        if (dataMutex != nullptr && xSemaphoreTake(dataMutex, pdMS_TO_TICKS(DATA_MUTEX_TIMEOUT_MS)) == pdTRUE) {
          powerLimitW_ = w;
          powerLimitKnown_ = true;
          xSemaphoreGive(dataMutex);
        } else if (dataMutex != nullptr && debugMode) {
          appLogger.log("[INVERTER-CONTROLLER] dataMutex timeout while caching /power limit");
        }
        appLogger.log(String("[INVERTER-CONTROLLER] Power limit read: ") + watts + "W");
      } else {
        appLogger.log(String("[INVERTER-CONTROLLER] Invalid /power response: '") + body + "'");
      }
    } else {
      appLogger.log(String("[INVERTER-CONTROLLER] Failed to read power limit: ") + err);
    }
  }
}

// -----------------------------------------------------------------------------
// Setter implementations: setPower / setShadow both POST to the inverter's
// /postoptions form endpoint via postOptions(). The payload differs per
// setter — power uses enable_mxpower + maxpower; shadow uses enShadow only.
// Partial submissions are accepted by the inverter (fields not present are
// left unchanged), so there is no need to read the other field's state first.
//
// Flow:
//   1. Validate input. Out-of-range -> Rejected.
//   2. POST the field-specific payload to /postoptions. On HTTP failure,
//      queue the desired value and return Deferred.
//   3. On success, re-read both values live from the inverter (cache
//      refresh). Clear that field's pending flag if it now matches. Return
//      Applied.
//
// Step lifecycle for the queue (clearing the master flag after retry) lives
// in applyPendingSettings(), called from the polling task on every successful
// /home telegram.
// -----------------------------------------------------------------------------

InverterController::SetResult InverterController::setPower(int watts, String& responseBody,
                                                    int& httpCode, String& errorMessage) {
  // 1. Validation
  if (watts < 0 || watts > INVERTER_MAX_POWER_WATTS) {
    errorMessage = String("Invalid power value: ") + watts +
                   String("W. Must satisfy 0 <= power <= ") + INVERTER_MAX_POWER_WATTS + "W";
    httpCode = 0;
    appLogger.log(String("[INVERTER-CONTROLLER] ") + errorMessage);
    return SetResult::Rejected;
  }

  // 2. POST power-only payload.
  String payload = String("enable_mxpower=on\nmaxpower=") + watts;
  if (!postOptions(payload, responseBody, httpCode, errorMessage)) {
    queuePowerLimitDesired(static_cast<uint16_t>(watts));
    appLogger.log(String("[INVERTER-CONTROLLER] setPower POST failed, deferred: ") + errorMessage);
    return SetResult::Deferred;
  }

  // 3. Refresh cache from inverter; clear pending if it now matches.
  fetchAndCacheSettings();
  uint16_t readback = 0;
  if (getPowerLimit(readback) && readback == static_cast<uint16_t>(watts)) {
    if (dataMutex != nullptr && xSemaphoreTake(dataMutex, pdMS_TO_TICKS(DATA_MUTEX_TIMEOUT_MS)) == pdTRUE) {
      powerLimitDesiredPending_ = false;
      pendingSettings_ = shadowDesiredPending_;
      xSemaphoreGive(dataMutex);
    }
  }
  return SetResult::Applied;
}

InverterController::SetResult InverterController::setShadow(bool enabled, String& responseBody,
                                                     int& httpCode, String& errorMessage) {
  // 1. Validation — caller has already coerced to bool, nothing to reject.

  // 2. POST shadow-only payload.
  String payload = String("enShadow=") + (enabled ? "on" : "off");
  if (!postOptions(payload, responseBody, httpCode, errorMessage)) {
    queueShadowDesired(enabled);
    appLogger.log(String("[INVERTER-CONTROLLER] setShadow POST failed, deferred: ") + errorMessage);
    return SetResult::Deferred;
  }

  // 3. Refresh cache from inverter; clear pending if it now matches.
  fetchAndCacheSettings();
  bool readback = false;
  if (getShadow(readback) && readback == enabled) {
    if (dataMutex != nullptr && xSemaphoreTake(dataMutex, pdMS_TO_TICKS(DATA_MUTEX_TIMEOUT_MS)) == pdTRUE) {
      shadowDesiredPending_ = false;
      pendingSettings_ = powerLimitDesiredPending_;
      xSemaphoreGive(dataMutex);
    }
  }
  return SetResult::Applied;
}

bool InverterController::postOptions(const String& payload, String& responseBody,
                                  int& httpCode, String& errorMessage) {
  String body = buildMultipartBody(payload);
  String contentType = String("multipart/form-data; boundary=") + MULTIPART_BOUNDARY;
  return fetchInverterData("POST", "/postoptions", body, responseBody, httpCode, errorMessage,
                           contentType.c_str());
}

void InverterController::queueShadowDesired(bool enabled) {
  if (dataMutex == nullptr) return;
  if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(DATA_MUTEX_TIMEOUT_MS)) != pdTRUE) return;
  shadowDesired_ = enabled;
  shadowDesiredPending_ = true;
  pendingSettings_ = true;
  xSemaphoreGive(dataMutex);
  appLogger.log(String("[INVERTER-CONTROLLER] Queued desired shadow=") + (enabled ? "ON" : "OFF"));
}

void InverterController::queuePowerLimitDesired(uint16_t watts) {
  if (dataMutex == nullptr) return;
  if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(DATA_MUTEX_TIMEOUT_MS)) != pdTRUE) return;
  powerLimitDesired_ = watts;
  powerLimitDesiredPending_ = true;
  pendingSettings_ = true;
  xSemaphoreGive(dataMutex);
  appLogger.log(String("[INVERTER-CONTROLLER] Queued desired power_limit=") + watts + "W");
}

bool InverterController::hasPendingSettings() {
  return pendingSettings_;  // volatile bool, atomic read on ESP32
}

// -----------------------------------------------------------------------------
// applyPendingSettings — called from the polling task after a successful
// /home telegram when pendingSettings_ is set. See the lifecycle comment on
// setPower() above.
// -----------------------------------------------------------------------------
void InverterController::applyPendingSettings() {
  // Snapshot pending desired state under the mutex.
  bool sPending = false, sDesired = false;
  bool pPending = false;
  uint16_t pDesired = 0;
  if (dataMutex == nullptr) return;
  if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(DATA_MUTEX_TIMEOUT_MS)) != pdTRUE) return;
  sPending = shadowDesiredPending_;
  sDesired = shadowDesired_;
  pPending = powerLimitDesiredPending_;
  pDesired = powerLimitDesired_;
  xSemaphoreGive(dataMutex);

  if (!sPending && !pPending) {
    // Master flag was raised but no desired values queued: clear and exit.
    pendingSettings_ = false;
    return;
  }

  // POST each pending field independently via /postoptions. Partial submissions
  // are accepted by the inverter; the omitted field is left unchanged. If a
  // POST fails, keep the flag raised and retry on the next successful poll.
  if (pPending) {
    String resp, err;
    int code = 0;
    String payload = String("enable_mxpower=on\nmaxpower=") + pDesired;
    appLogger.log(String("[INVERTER-CONTROLLER] applyPendingSettings: posting power=") + pDesired + "W");
    if (!postOptions(payload, resp, code, err)) {
      appLogger.log(String("[INVERTER-CONTROLLER] applyPendingSettings power POST failed: ") + err);
      return;  // keep flags raised, retry next poll
    }
  }
  if (sPending) {
    String resp, err;
    int code = 0;
    String payload = String("enShadow=") + (sDesired ? "on" : "off");
    appLogger.log(String("[INVERTER-CONTROLLER] applyPendingSettings: posting shadow=") +
                  (sDesired ? "ON" : "OFF"));
    if (!postOptions(payload, resp, code, err)) {
      appLogger.log(String("[INVERTER-CONTROLLER] applyPendingSettings shadow POST failed: ") + err);
      return;  // keep flags raised, retry next poll
    }
  }

  // Re-read live from inverter to verify; updates shadowKnown_/powerLimitKnown_.
  fetchAndCacheSettings();

  // Check convergence and clear flags.
  if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(DATA_MUTEX_TIMEOUT_MS)) != pdTRUE) return;
  bool allMatch = true;
  if (sPending && (!shadowKnown_ || shadowOn_ != sDesired))      allMatch = false;
  if (pPending && (!powerLimitKnown_ || powerLimitW_ != pDesired)) allMatch = false;
  if (allMatch) {
    if (sPending) shadowDesiredPending_ = false;
    if (pPending) powerLimitDesiredPending_ = false;
    pendingSettings_ = shadowDesiredPending_ || powerLimitDesiredPending_;
    appLogger.log("[INVERTER-CONTROLLER] applyPendingSettings: converged, flags cleared");
  } else {
    appLogger.log("[INVERTER-CONTROLLER] applyPendingSettings: mismatch persists, will retry");
  }
  xSemaphoreGive(dataMutex);
}

bool InverterController::fetchPath(const String& path, String& responseBody, int& httpCode, String& errorMessage) {
  if (path.length() == 0 || path[0] != '/') {
    errorMessage = "path must start with '/'";
    return false;
  }

  return fetchInverterData("GET", path, "", responseBody, httpCode, errorMessage);
}
