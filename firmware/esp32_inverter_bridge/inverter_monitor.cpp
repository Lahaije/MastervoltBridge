#include "inverter_monitor.h"

#include "inverter_data.h"
#include "inverter_link_state.h"
#include "settings.h"
#include "logger.h"
#include "wifi_bridge.h"

// linkStateFromStreak(), intervalForState(), toString() live in inverter_link_state.cpp

namespace {
constexpr const char* HOME_ENDPOINT = "/home";
constexpr bool ENABLE_INVERTER_POLLING = true;
}  // namespace

InverterMonitor::InverterMonitor() {
}

InverterMonitor::~InverterMonitor() {
  shutdown();
}

void InverterMonitor::initialize() {
  if (isInitialized) {
    return;
  }

  if (dataMutex == nullptr) {
    dataMutex = xSemaphoreCreateMutex();
  }

  if (ENABLE_INVERTER_POLLING && pollingTaskHandle == nullptr) {
    xTaskCreatePinnedToCore(
      pollingTaskEntry,
      "inverter_monitor",
      6144,
      this,
      1,
      &pollingTaskHandle,
      0
    );
  }

  isInitialized = true;
  if (ENABLE_INVERTER_POLLING) {
    appLogger.log("[INVERTER-MONITOR] Inverter monitor initialized");
  } else {
    appLogger.log("[INVERTER-MONITOR] Inverter polling disabled (measurement mode)");
  }
}

void InverterMonitor::shutdown() {
  if (pollingTaskHandle != nullptr) {
    vTaskDelete(pollingTaskHandle);
    pollingTaskHandle = nullptr;
  }

  if (dataMutex != nullptr) {
    vSemaphoreDelete(dataMutex);
    dataMutex = nullptr;
  }

  isInitialized = false;
  appLogger.log("[INVERTER-MONITOR] Inverter monitor shut down");
}

void InverterMonitor::pollingTaskEntry(void* param) {
  InverterMonitor* self = static_cast<InverterMonitor*>(param);
  if (self != nullptr) {
    self->runPollingTask();
  }
}

bool InverterMonitor::incrementCounterLocked(uint32_t& counter) {
  if (dataMutex == nullptr) {
    return false;
  }
  if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(5000)) != pdTRUE) {
    return false;
  }
  counter++;
  xSemaphoreGive(dataMutex);
  return true;
}

void InverterMonitor::runPollingTask() {
  failureStartMs = 0;
  linkState = InverterLinkState::STARTING;
  currentRetryIntervalMs = WIFI_BRIDGE_POLL_INTERVAL_MS;

  while (true) {
    bool iterationOk = false;

    if (!WifiConnectionManager::getInstance().ensureConnected()) {
      appLogger.log("[INVERTER-MONITOR] No WiFi connection; skipping poll iteration");
    } else {
      String rawResponse;
      String errorMessage;
      int httpCode = 0;

      bool ok = fetchInverterData("GET", HOME_ENDPOINT, "", rawResponse, httpCode, errorMessage);

      if (ok) {
        HomeData parsedData;
        if (parseHomeResponse(rawResponse, parsedData)) {
          if (dataMutex != nullptr && xSemaphoreTake(dataMutex, pdMS_TO_TICKS(5000)) == pdTRUE) {
            cachedData = parsedData;
            lastUpdateMs = millis();
            successfulPolls++;
            xSemaphoreGive(dataMutex);

            appLogger.log(String("[INVERTER-MONITOR] Poll #") + successfulPolls +
                          ": Status=" + parsedData.operatingStatus +
                          " Power=" + parsedData.instantaneousPower + "W");
          }
          iterationOk = true;
        } else {
          incrementCounterLocked(failedPolls);
          appLogger.log("[INVERTER-MONITOR] Failed to parse /home response");
        }
      } else {
        incrementCounterLocked(failedPolls);
        appLogger.log(String("[INVERTER-MONITOR] Failed to fetch /home: ") + errorMessage);
      }
    }

    // -------------------------------------------------------------------------
    // Link-state machine: evaluate next state from poll outcome
    // -------------------------------------------------------------------------
    InverterLinkState prevState = linkState;
    InverterLinkState newState;
    uint32_t streakMs = 0;

    if (iterationOk) {
      streakMs = (failureStartMs != 0) ? (millis() - failureStartMs) : 0;
      newState = InverterLinkState::ONLINE;
      failureStartMs = 0;
    } else {
      if (failureStartMs == 0) failureStartMs = millis();
      streakMs = millis() - failureStartMs;
      newState = linkStateFromStreak(streakMs);  // see inverter_link_state.h
    }

    // ---- Apply polling-interval change on state change --------------------
    // intervalForState() maps BACKOFF->1 min, DORMANT->10 min, else baseMs.
    // This is the only place currentRetryIntervalMs is mutated.
    uint32_t nextIntervalMs = intervalForState(newState, WIFI_BRIDGE_POLL_INTERVAL_MS);
    currentRetryIntervalMs = nextIntervalMs;

    if (newState != prevState) {
      linkState = newState;
      appLogger.log(String("[INVERTER-MONITOR] State: ") + toString(prevState) +
                    " -> " + toString(newState) +
                    "  streak=" + (streakMs / 1000) + "s" +
                    "  interval=" + (nextIntervalMs / 1000) + "s");

      // ---- Dispatch on-transition actions (see onLinkStateTransition) -----
      onLinkStateTransition(prevState, newState, streakMs);
    }

    vTaskDelay(pdMS_TO_TICKS(nextIntervalMs));
  }
}

bool InverterMonitor::getLatestHomeData(HomeData& dataOut) {
  if (dataMutex == nullptr) {
    appLogger.log("[INVERTER-MONITOR] Inverter monitor not initialized");
    return false;
  }

  if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(5000)) != pdTRUE) {
    appLogger.log("[INVERTER-MONITOR] Failed to acquire data mutex");
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

unsigned long InverterMonitor::getLastUpdateMs() {
  if (dataMutex == nullptr) {
    return 0;
  }
  if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(5000)) != pdTRUE) {
    return 0;
  }
  unsigned long result = lastUpdateMs;
  xSemaphoreGive(dataMutex);
  return result;
}

InverterLinkState InverterMonitor::getLinkState() {
  return linkState;
}

uint32_t InverterMonitor::getFailureStreakMs() {
  uint32_t startMs = failureStartMs;
  if (startMs == 0) return 0;
  uint32_t now = millis();
  return (now >= startMs) ? (now - startMs) : 0;
}

uint32_t InverterMonitor::getRetryIntervalMs() {
  return currentRetryIntervalMs;
}

// -----------------------------------------------------------------------------
// onLinkStateTransition — canonical list of all on-change side-effects.
//
//  from         to         Action
//  ----------   --------   ---------------------------------------------------
//  STARTING  -> ONLINE   : fetchAndCacheSettings()  (first-ever connection)
//  ONLINE    -> RETRYING : (none — first failure, wait and see)
//  RETRYING  -> ONLINE   : (none — quick recovery; settings still valid)
//  RETRYING  -> BACKOFF  : (none — interval change already applied above)
//  BACKOFF   -> ONLINE   : fetchAndCacheSettings()  (recovery from outage)
//  BACKOFF   -> DORMANT  : (none — interval change already applied above)
//  DORMANT   -> ONLINE   : fetchAndCacheSettings()  (recovery from outage)
// -----------------------------------------------------------------------------
void InverterMonitor::onLinkStateTransition(InverterLinkState from,
                                            InverterLinkState to,
                                            uint32_t streakMs) {
  if (to == InverterLinkState::ONLINE) {
    if (from == InverterLinkState::STARTING) {
      appLogger.log("[INVERTER-MONITOR] First connection; fetching shadow + power limit");
      fetchAndCacheSettings();
    } else if (from == InverterLinkState::BACKOFF || from == InverterLinkState::DORMANT) {
      appLogger.log(String("[INVERTER-MONITOR] Recovered after ") +
                    (streakMs / 1000) + "s outage; refreshing shadow + power limit");
      fetchAndCacheSettings();
    }
    // RETRYING -> ONLINE: quick recovery, no action needed
  }
  // All other transitions: interval change was already handled by intervalForState().

  // Dispatch registered hooks (optional; allows external code to react)
  dispatchStateChangeHooks(from, to, streakMs);
}

void InverterMonitor::fetchAndCacheSettings() {
  // Shadow: GET /shadow -> "<0|1>\n<interval>\n"
  {
    String body, err;
    int code = 0;
    if (fetchInverterData("GET", "/shadow", "", body, code, err) && code == 200) {
      body.trim();
      bool enabled = body.length() > 0 && body[0] == '1';
      if (dataMutex != nullptr && xSemaphoreTake(dataMutex, pdMS_TO_TICKS(5000)) == pdTRUE) {
        shadowOn_ = enabled;
        shadowKnown_ = true;
        xSemaphoreGive(dataMutex);
      }
      appLogger.log(String("[INVERTER-MONITOR] Shadow read: ") + (enabled ? "ON" : "OFF"));
    } else {
      appLogger.log(String("[INVERTER-MONITOR] Failed to read shadow: ") + err);
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
        if (dataMutex != nullptr && xSemaphoreTake(dataMutex, pdMS_TO_TICKS(5000)) == pdTRUE) {
          powerLimitW_ = w;
          powerLimitKnown_ = true;
          xSemaphoreGive(dataMutex);
        }
        appLogger.log(String("[INVERTER-MONITOR] Power limit read: ") + watts + "W");
      } else {
        appLogger.log(String("[INVERTER-MONITOR] Invalid /power response: '") + body + "'");
      }
    } else {
      appLogger.log(String("[INVERTER-MONITOR] Failed to read power limit: ") + err);
    }
  }
}

bool InverterMonitor::setPower(int watts, String& responseBody, int& httpCode, String& errorMessage) {
  // Safety check before inverter command: enforce hardware range
  if (watts < 0 || watts > INVERTER_MAX_POWER_WATTS) {
    errorMessage = String("Invalid power value: ") + watts + 
                   String("W. Must satisfy 0 <= power <= ") + INVERTER_MAX_POWER_WATTS + "W";
    httpCode = 0;
    appLogger.log(String("[INVERTER-MONITOR] ") + errorMessage);
    return false;
  }

  String payload = String(watts);
  return fetchInverterData("POST", "/power", payload, responseBody, httpCode, errorMessage);
}

bool InverterMonitor::fetchPath(const String& path, String& responseBody, int& httpCode, String& errorMessage) {
  if (path.length() == 0 || path[0] != '/') {
    errorMessage = "path must start with '/'";
    return false;
  }

  return fetchInverterData("GET", path, "", responseBody, httpCode, errorMessage);
}
