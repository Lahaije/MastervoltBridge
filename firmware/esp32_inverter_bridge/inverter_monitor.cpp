#include "inverter_monitor.h"

#include "inverter_data.h"
#include "inverter_link_state.h"
#include "settings.h"
#include "logger.h"
#include "wifi_bridge.h"

// State-transition logic (linkStateFromStreak, intervalForState) lives here in
// inverter_monitor.cpp — it is the monitor's decision-making, not state storage.
// State storage, hook registry, and toString() live in inverter_link_state.cpp.

namespace {
constexpr const char* HOME_ENDPOINT = "/home";
constexpr bool ENABLE_INVERTER_POLLING = true;
constexpr uint32_t DATA_MUTEX_TIMEOUT_MS = 10;

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

// Return poll interval for a given state. Used by runPollingTask().
static uint32_t intervalForState(InverterLinkState s, uint32_t baseMs) {
  switch (s) {
    case InverterLinkState::BACKOFF:  return LINK_BACKOFF_INTERVAL_MS;
    case InverterLinkState::DORMANT:  return LINK_DORMANT_INTERVAL_MS;
    default:                          return baseMs;
  }
}
}  // namespace

InverterMonitor::InverterMonitor() {
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

  // Register state-entry hook: updates currentRetryIntervalMs on every state change.
  // Uses an entry hook (fires on ANY transition into the target state) so a single
  // registration per interval tier covers all possible predecessor states.
  REGISTER_STATE_ENTRY_HOOK(InverterLinkState::BACKOFF,  applyIntervalForState);
  REGISTER_STATE_ENTRY_HOOK(InverterLinkState::DORMANT,  applyIntervalForState);
  REGISTER_STATE_ENTRY_HOOK(InverterLinkState::ONLINE,   applyIntervalForState);
  REGISTER_STATE_ENTRY_HOOK(InverterLinkState::RETRYING, applyIntervalForState);

  // Register transition hooks for ONLINE side effects.
  REGISTER_STATE_CHANGE_HOOK(InverterLinkState::STARTING, InverterLinkState::ONLINE, loadSettingsOnBoot);
  REGISTER_STATE_CHANGE_HOOK(InverterLinkState::BACKOFF,  InverterLinkState::ONLINE, updateAllInverterParam);
  REGISTER_STATE_CHANGE_HOOK(InverterLinkState::DORMANT,  InverterLinkState::ONLINE, updateAllInverterParam);

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
  if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(DATA_MUTEX_TIMEOUT_MS)) != pdTRUE) {
    if (debugMode) {
      appLogger.log("[INVERTER-MONITOR] dataMutex timeout in incrementCounterLocked");
    }
    return false;
  }
  counter++;
  xSemaphoreGive(dataMutex);
  return true;
}

void InverterMonitor::linkStateFromStreak(uint32_t streakMs) {
  InverterLinkState currentState = getInverterState();
  InverterLinkState expectedState = desiredStateFromStreak(streakMs);

  if (currentState == expectedState) {
    return;
  }

  if (debugMode) {
    appLogger.log(String("[INVERTER-MONITOR] State: ") + toString(currentState) +
                  " -> " + toString(expectedState) +
                  "  streak=" + (streakMs / 1000) + "s" +
                  "  interval=" + (currentRetryIntervalMs / 1000) + "s");
  }
  setInverterState(expectedState);
}

void InverterMonitor::runPollingTask() {
  failureStartMs = 0;
  // globalInverterState is already STARTING at boot; set interval explicitly
  // since no state transition fires at boot (STARTING -> STARTING is a no-op).
  currentRetryIntervalMs = intervalForState(InverterLinkState::STARTING, WIFI_BRIDGE_POLL_INTERVAL_MS);

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
          if (dataMutex != nullptr && xSemaphoreTake(dataMutex, pdMS_TO_TICKS(DATA_MUTEX_TIMEOUT_MS)) == pdTRUE) {
            cachedData = parsedData;
            lastUpdateMs = millis();
            successfulPolls++;
            xSemaphoreGive(dataMutex);

            appLogger.log(String("[INVERTER-MONITOR] Poll #") + successfulPolls +
                          ": Status=" + parsedData.operatingStatus +
                          " Power=" + parsedData.instantaneousPower + "W");
          } else if (dataMutex != nullptr && debugMode) {
            appLogger.log("[INVERTER-MONITOR] dataMutex timeout while caching /home poll result");
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

    if (iterationOk) {
      // Poll succeeded — streak ends. Ensure state is ONLINE.
      uint32_t streakMs = (failureStartMs != 0) ? (millis() - failureStartMs) : 0;
      failureStartMs = 0;
      linkStateFromStreak(0);
    } else {
      // Poll failed — accumulate streak and check thresholds.
      if (failureStartMs == 0) failureStartMs = millis();
      uint32_t streakMs = millis() - failureStartMs;
      linkStateFromStreak(streakMs);
    }

    vTaskDelay(pdMS_TO_TICKS(currentRetryIntervalMs));
  }
}

bool InverterMonitor::getLatestHomeData(HomeData& dataOut) {
  if (dataMutex == nullptr) {
    appLogger.log("[INVERTER-MONITOR] Inverter monitor not initialized");
    return false;
  }

  if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(DATA_MUTEX_TIMEOUT_MS)) != pdTRUE) {
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
  if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(DATA_MUTEX_TIMEOUT_MS)) != pdTRUE) {
    return 0;
  }
  unsigned long result = lastUpdateMs;
  xSemaphoreGive(dataMutex);
  return result;
}

InverterLinkState InverterMonitor::getLinkState() {
  return getInverterState();
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
// applyIntervalForState — state-entry hook registered at initialize().
//
// Fires on every state transition (entry hook: any -> targetState).
// Updates currentRetryIntervalMs to the interval appropriate for the new state.
// This is the ONLY place currentRetryIntervalMs is written after boot.
//
//  to          interval
//  ---------   --------------------------
//  BACKOFF     LINK_BACKOFF_INTERVAL_MS  (1 min)
//  DORMANT     LINK_DORMANT_INTERVAL_MS  (10 min)
//  other       WIFI_BRIDGE_POLL_INTERVAL_MS (base, 15 s)
// -----------------------------------------------------------------------------
void InverterMonitor::applyIntervalForState(InverterLinkState /*from*/,
                                            InverterLinkState to) {
  getInstance().currentRetryIntervalMs = intervalForState(to, WIFI_BRIDGE_POLL_INTERVAL_MS);
}

// -----------------------------------------------------------------------------
// Transition hook: first-ever ONLINE after boot.
void InverterMonitor::loadSettingsOnBoot(InverterLinkState /*from*/,
                                         InverterLinkState /*to*/) {
  appLogger.log("[INVERTER-MONITOR] First connection; fetching shadow + power limit");
  getInstance().fetchAndCacheSettings();
}

// Transition hook: ONLINE recovery after prolonged outage.
void InverterMonitor::updateAllInverterParam(InverterLinkState from,
                                             InverterLinkState /*to*/) {
  appLogger.log(String("[INVERTER-MONITOR] Recovered from ") + toString(from) +
                "; refreshing shadow + power limit");
  getInstance().fetchAndCacheSettings();
}

void InverterMonitor::fetchAndCacheSettings() {
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
        appLogger.log("[INVERTER-MONITOR] dataMutex timeout while caching /shadow state");
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
        if (dataMutex != nullptr && xSemaphoreTake(dataMutex, pdMS_TO_TICKS(DATA_MUTEX_TIMEOUT_MS)) == pdTRUE) {
          powerLimitW_ = w;
          powerLimitKnown_ = true;
          xSemaphoreGive(dataMutex);
        } else if (dataMutex != nullptr && debugMode) {
          appLogger.log("[INVERTER-MONITOR] dataMutex timeout while caching /power limit");
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
