#include "inverter_controller.h"

#include "inverter_data.h"
#include "inverter_link_state.h"
#include "settings.h"
#include "logger.h"
#include "wifi_bridge.h"

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

  // Create polling task.
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
                          " Power=" + (parsedData.hasPower ? String(parsedData.instantaneousPowerW, 1) : "?") + "W");
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
      // the inverter is reachable.
      applyPendingSettings();

      // Backfill any unknown cached settings.
      refreshUnknownSettingsAfterPoll();
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
  if (!shadowKnown_) return false;
  enabledOut = shadowOn_;
  return true;
}

bool InverterController::getPowerLimit(uint16_t& wattsOut) {
  if (!powerLimitKnown_) return false;
  wattsOut = powerLimitW_;
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
  appLogger.log("[INVERTER-CONTROLLER] First connection; invalidating shadow + power limit cache");
  getInstance().shadowKnown_ = false;
  getInstance().powerLimitKnown_ = false;
}

// Transition hook: ONLINE recovery after prolonged outage.
void InverterController::updateAllInverterParam(InverterLinkState from,
                                             InverterLinkState /*to*/) {
  appLogger.log(String("[INVERTER-CONTROLLER] Recovered from ") + toString(from) +
                "; invalidating shadow + power limit cache");
  getInstance().shadowKnown_ = false;
  getInstance().powerLimitKnown_ = false;
}

void InverterController::fetchAndCacheShadow() {
  // Shadow: GET /shadow -> "<0|1>\n<interval>\n"
  String body, err;
  int code = 0;
  if (fetchInverterData("GET", "/shadow", "", body, code, err) && code == 200) {
    body.trim();
    bool enabled = body.length() > 0 && body[0] == '1';
    shadowOn_ = enabled;
    shadowKnown_ = true;
    appLogger.log(String("[INVERTER-CONTROLLER] Shadow read: ") + (enabled ? "ON" : "OFF"));
  } else {
    appLogger.log(String("[INVERTER-CONTROLLER] Failed to read shadow: ") + err);
  }
}

void InverterController::fetchAndCachePowerLimit() {
  // Power limit: GET /power -> "<watts>\n"
  String body, err;
  int code = 0;
  if (fetchInverterData("GET", "/power", "", body, code, err) && code == 200) {
    body.trim();
    int watts = body.toInt();
    if (watts >= 0 && watts <= INVERTER_MAX_POWER_WATTS) {
      uint16_t w = static_cast<uint16_t>(watts);
      powerLimitW_ = w;
      powerLimitKnown_ = true;
      appLogger.log(String("[INVERTER-CONTROLLER] Power limit read: ") + watts + "W");
    } else {
      appLogger.log(String("[INVERTER-CONTROLLER] Invalid /power response: '") + body + "'");
    }
  } else {
    appLogger.log(String("[INVERTER-CONTROLLER] Failed to read power limit: ") + err);
  }
}

void InverterController::refreshUnknownSettingsAfterPoll() {
  if (!powerLimitKnown_) {
    if (debugMode) appLogger.log("[INVERTER-CONTROLLER] Power limit unknown; refreshing /power");
    fetchAndCachePowerLimit();
  }
  if (!shadowKnown_) {
    if (debugMode) appLogger.log("[INVERTER-CONTROLLER] Shadow state unknown; refreshing /shadow");
    fetchAndCacheShadow();
  }
}

// -----------------------------------------------------------------------------
// setPower / setShadow: POST a field-specific payload to /postoptions.
// On failure, queue the desired value (Deferred). On success, invalidate
// the Known cache flag and attempt a readback (Applied).
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

  // 3. POST succeeded: clear pending flag, invalidate cache, attempt readback.
  powerLimitDesiredPending_ = false;
  powerLimitKnown_ = false;
  fetchAndCachePowerLimit();
  return SetResult::Applied;
}

InverterController::SetResult InverterController::setShadow(bool enabled, String& responseBody,
                                                     int& httpCode, String& errorMessage) {
  // 2. POST shadow-only payload.
  String payload = String("enShadow=") + (enabled ? "on" : "off");
  if (!postOptions(payload, responseBody, httpCode, errorMessage)) {
    queueShadowDesired(enabled);
    appLogger.log(String("[INVERTER-CONTROLLER] setShadow POST failed, deferred: ") + errorMessage);
    return SetResult::Deferred;
  }

  // 3. POST succeeded: clear pending flag, invalidate cache, attempt readback.
  shadowDesiredPending_ = false;
  shadowKnown_ = false;
  fetchAndCacheShadow();
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
  shadowDesired_ = enabled;
  shadowDesiredPending_ = true;
  appLogger.log(String("[INVERTER-CONTROLLER] Queued desired shadow=") + (enabled ? "ON" : "OFF"));
}

void InverterController::queuePowerLimitDesired(uint16_t watts) {
  powerLimitDesired_ = watts;
  powerLimitDesiredPending_ = true;
  appLogger.log(String("[INVERTER-CONTROLLER] Queued desired power_limit=") + watts + "W");
}

bool InverterController::hasPendingSettings() {
  return shadowDesiredPending_ || powerLimitDesiredPending_;
}

void InverterController::applyPendingSettings() {
  applyPendingPowerLimit();
  applyPendingShadow();
}

// -----------------------------------------------------------------------------
// applyPendingPowerLimit / applyPendingShadow — retry queued writes after a
// successful /home poll. Each no-ops unless its own field is queued.
// -----------------------------------------------------------------------------
void InverterController::applyPendingPowerLimit() {
  if (!powerLimitDesiredPending_) {
    return;
  }

  String resp, err;
  int code = 0;
  String payload = String("enable_mxpower=on\nmaxpower=") + powerLimitDesired_;
  appLogger.log(String("[INVERTER-CONTROLLER] applyPendingPowerLimit: posting power=") + powerLimitDesired_ + "W");
  if (!postOptions(payload, resp, code, err)) {
    appLogger.log(String("[INVERTER-CONTROLLER] applyPendingPowerLimit POST failed: ") + err);
    return;
  }

  powerLimitDesiredPending_ = false;
  powerLimitKnown_ = false;
  fetchAndCachePowerLimit();
  appLogger.log("[INVERTER-CONTROLLER] applyPendingPowerLimit: applied, readback attempted");
}

void InverterController::applyPendingShadow() {
  if (!shadowDesiredPending_) {
    return;
  }

  String resp, err;
  int code = 0;
  String payload = String("enShadow=") + (shadowDesired_ ? "on" : "off");
  appLogger.log(String("[INVERTER-CONTROLLER] applyPendingShadow: posting shadow=") +
                (shadowDesired_ ? "ON" : "OFF"));
  if (!postOptions(payload, resp, code, err)) {
    appLogger.log(String("[INVERTER-CONTROLLER] applyPendingShadow POST failed: ") + err);
    return;
  }

  shadowDesiredPending_ = false;
  shadowKnown_ = false;
  fetchAndCacheShadow();
  appLogger.log("[INVERTER-CONTROLLER] applyPendingShadow: applied, readback attempted");
}

bool InverterController::fetchPath(const String& path, String& responseBody, int& httpCode, String& errorMessage) {
  if (path.length() == 0 || path[0] != '/') {
    errorMessage = "path must start with '/'";
    return false;
  }

  return fetchInverterData("GET", path, "", responseBody, httpCode, errorMessage);
}
