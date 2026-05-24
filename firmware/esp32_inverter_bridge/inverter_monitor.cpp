#include "inverter_monitor.h"

#include "inverter_data.h"
#include "settings.h"
#include "logger.h"
#include "wifi_bridge.h"
#include "lock_guard.h"

namespace {
constexpr const char* HOME_ENDPOINT = "/home";
constexpr uint32_t POWER_STATE_LOCK_TIMEOUT_MS = 1000;
constexpr uint32_t DATA_LOCK_TIMEOUT_MS = 5000;
constexpr uint32_t POLLING_CONFIG_LOCK_TIMEOUT_MS = 1000;

// Aliases for clarity at call sites. All three obey the lock hierarchy
// enforced by lock_guard.h: POLLING_CONFIG < DATA < POWER_STATE < WIFI.
using ScopedPowerStateLock    = ScopedLock<LockRank::POWER_STATE>;
using ScopedDataLock          = ScopedLock<LockRank::DATA>;
using ScopedPollingConfigLock = ScopedLock<LockRank::POLLING_CONFIG>;

// ---------------------------------------------------------------------------
// Link-state thresholds and per-state polling intervals.
//
// The polling loop occupies one InverterLinkState at a time, derived from
// the length of the current failure streak. The state determines the next
// retry interval and is exposed via /api/health for the dashboard.
//
// Edit these to tune retry aggressiveness vs. log/GPIO noise.
// ---------------------------------------------------------------------------
static constexpr uint32_t LINK_RETRYING_TO_BACKOFF_MS =  5u * 60u * 1000u;  // 5 min
static constexpr uint32_t LINK_BACKOFF_TO_DORMANT_MS  = 20u * 60u * 1000u;  // 20 min
static constexpr uint32_t LINK_BACKOFF_INTERVAL_MS    =        60u * 1000u; // 1 min
static constexpr uint32_t LINK_DORMANT_INTERVAL_MS    =      600u * 1000u;  // 10 min

// Decide which link state matches a given failure-streak length.
// streakMs == 0 means "no failure streak", which the caller should map to
// ONLINE (or STARTING on first boot).
static InverterLinkState linkStateFromStreak(uint32_t streakMs) {
  if (streakMs == 0) return InverterLinkState::ONLINE;
  if (streakMs >= LINK_BACKOFF_TO_DORMANT_MS) return InverterLinkState::DORMANT;
  if (streakMs >= LINK_RETRYING_TO_BACKOFF_MS) return InverterLinkState::BACKOFF;
  return InverterLinkState::RETRYING;
}

static uint32_t intervalForState(InverterLinkState s, uint32_t baseIntervalMs) {
  switch (s) {
    case InverterLinkState::BACKOFF: return LINK_BACKOFF_INTERVAL_MS;
    case InverterLinkState::DORMANT: return LINK_DORMANT_INTERVAL_MS;
    case InverterLinkState::STARTING:
    case InverterLinkState::ONLINE:
    case InverterLinkState::RETRYING:
    default:
      return baseIntervalMs;
  }
}

}  // namespace

const char* toString(InverterLinkState s) {
  switch (s) {
    case InverterLinkState::STARTING: return "STARTING";
    case InverterLinkState::ONLINE:   return "ONLINE";
    case InverterLinkState::RETRYING: return "RETRYING";
    case InverterLinkState::BACKOFF:  return "BACKOFF";
    case InverterLinkState::DORMANT:  return "DORMANT";
  }
  return "UNKNOWN";
}

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

  if (powerStateMutex == nullptr) {
    powerStateMutex = xSemaphoreCreateMutex();
  }

  if (pollingConfigMutex == nullptr) {
    pollingConfigMutex = xSemaphoreCreateMutex();
  }

  if (pollingTaskHandle == nullptr) {
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
  appLogger.log("[INVERTER-MONITOR] Inverter monitor initialized");
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

  if (powerStateMutex != nullptr) {
    vSemaphoreDelete(powerStateMutex);
    powerStateMutex = nullptr;
  }

  if (pollingConfigMutex != nullptr) {
    vSemaphoreDelete(pollingConfigMutex);
    pollingConfigMutex = nullptr;
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

void InverterMonitor::runPollingTask() {
  // failureStartMs and linkState are members so the API layer can read them.
  // The polling task is the sole writer.
  failureStartMs = 0;
  linkState = InverterLinkState::STARTING;
  currentRetryIntervalMs = getPollingIntervalMs();

  while (true) {
    bool iterationOk = false;

    // fetchInverterData(waitForConnection=true) blocks until the connection
    // worker has established WiFi, then performs the HTTP request.
    String rawResponse;
    String errorMessage;
    int httpCode = 0;

    bool ok = fetchInverterData("GET", HOME_ENDPOINT, "", rawResponse, httpCode, errorMessage, true);

    if (ok) {
      // Parse the response into HomeData
      HomeData parsedData;
      if (parseHomeResponse(rawResponse, parsedData)) {
        // Update cached data with mutex protection
        {
          ScopedDataLock lock(dataMutex, DATA_LOCK_TIMEOUT_MS, "dataMutex");
          if (lock.acquired()) {
            cachedData = parsedData;
            lastUpdateMs = millis();
            successfulPolls++;
          }
        }
        appLogger.log(String("[INVERTER-MONITOR] Poll #") + successfulPolls +
                      ": Status=" + parsedData.operatingStatus +
                      " Power=" + parsedData.instantaneousPower + "W");
        iterationOk = true;

        // Deliver any pending power command after a successful poll.
        applyPendingPowerCommand();
      } else {
        appLogger.log("[INVERTER-MONITOR] Failed to parse /home response");
      }
    } else {
      appLogger.log(String("[INVERTER-MONITOR] Failed to fetch /home: ") + errorMessage);
    }

    // ---- Link state transition + recovery action ----
    InverterLinkState prevState = linkState;
    InverterLinkState newState;
    uint32_t streakMs = 0;

    if (iterationOk) {
      streakMs = (failureStartMs != 0) ? (millis() - failureStartMs) : 0;
      newState = InverterLinkState::ONLINE;
      failureStartMs = 0;
    } else {
      if (failureStartMs == 0) {
        failureStartMs = millis();
      }
      streakMs = millis() - failureStartMs;
      newState = linkStateFromStreak(streakMs);
    }

    uint32_t baseIntervalMs = getPollingIntervalMs();
    uint32_t nextIntervalMs = intervalForState(newState, baseIntervalMs);

    if (newState != prevState) {
      linkState = newState;
      currentRetryIntervalMs = nextIntervalMs;
      if (debugMode) {
        appLogger.log(String("[INVERTER-MONITOR] Link state: ") + toString(prevState) +
                      " -> " + toString(newState) +
                      " (streak=" + (streakMs / 1000) + "s, interval=" +
                      (nextIntervalMs / 1000) + "s)");
      }
      // Fire the single transition event. All once-per-transition behaviour
      // lives in onLinkStateTransition() so this loop stays a pure dispatcher.
      onLinkStateTransition(prevState, newState, streakMs);
    } else if (nextIntervalMs != currentRetryIntervalMs) {
      // Same state but the base interval may have been retuned at runtime.
      currentRetryIntervalMs = nextIntervalMs;
    }

    vTaskDelay(pdMS_TO_TICKS(nextIntervalMs));
  }
}

bool InverterMonitor::getLatestHomeData(HomeData& dataOut) {
  if (dataMutex == nullptr) {
    appLogger.log("[INVERTER-MONITOR] Inverter monitor not initialized");
    return false;
  }

  ScopedDataLock lock(dataMutex, DATA_LOCK_TIMEOUT_MS, "dataMutex");
  if (!lock.acquired()) {
    appLogger.log("[INVERTER-MONITOR] Failed to acquire data mutex");
    return false;
  }

  if (!cachedData.isValid()) return false;
  dataOut = cachedData;
  return true;
}

unsigned long InverterMonitor::getLastUpdateMs() {
  if (dataMutex == nullptr) return 0;
  ScopedDataLock lock(dataMutex, DATA_LOCK_TIMEOUT_MS, "dataMutex");
  if (!lock.acquired()) return 0;
  return lastUpdateMs;
}

bool InverterMonitor::setPollingIntervalSeconds(uint32_t seconds, uint32_t& appliedMs, String& errorMessage) {
  if (seconds < 1 || seconds > 3600) {
    errorMessage = "seconds must satisfy 1 <= seconds <= 3600";
    return false;
  }

  if (pollingConfigMutex == nullptr) {
    errorMessage = "polling config mutex not initialized";
    return false;
  }

  ScopedPollingConfigLock lock(pollingConfigMutex, POLLING_CONFIG_LOCK_TIMEOUT_MS, "pollingConfigMutex");
  if (!lock.acquired()) {
    errorMessage = "polling config mutex busy";
    return false;
  }

  pollingIntervalMs = seconds * 1000UL;
  appliedMs = pollingIntervalMs;
  appLogger.log(String("[INVERTER-MONITOR] Poll interval updated to ") + seconds + "s");
  return true;
}

uint32_t InverterMonitor::getPollingIntervalMs() {
  if (pollingConfigMutex == nullptr) return WIFI_BRIDGE_POLL_INTERVAL_MS;
  ScopedPollingConfigLock lock(pollingConfigMutex, POLLING_CONFIG_LOCK_TIMEOUT_MS, "pollingConfigMutex");
  // Best-effort read if the lock is contended; pollingIntervalMs is a 32-bit
  // value whose torn read would just yield a slightly stale interval.
  if (!lock.acquired()) return pollingIntervalMs;
  return pollingIntervalMs;
}

InverterLinkState InverterMonitor::getLinkState() {
  // linkState is written only by the polling task. Reading a uint8_t enum
  // value is atomic on ESP32; no lock needed.
  return linkState;
}

uint32_t InverterMonitor::getFailureStreakMs() {
  uint32_t startMs = failureStartMs;  // atomic read of uint32_t on ESP32
  if (startMs == 0) return 0;
  uint32_t now = millis();
  return (now >= startMs) ? (now - startMs) : 0;
}

uint32_t InverterMonitor::getRetryIntervalMs() {
  return currentRetryIntervalMs;
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

  // --- HTTP call (no lock held) ---
  String payload = String(watts);
  bool ok = fetchInverterData("POST", "/postoptions", payload, responseBody, httpCode, errorMessage, false);

  if (ok) {
    appLogger.log(String("[INVERTER-MONITOR] Power set to ") + watts + "W (immediate)");
    // Re-read the actual value from the inverter to confirm.
    refreshPowerLimit();
    return true;
  }

  // WiFi unavailable — queue the command for retry.
  {
    ScopedPowerStateLock lock(powerStateMutex, POWER_STATE_LOCK_TIMEOUT_MS);
    if (lock.acquired()) {
      queuedPowerWatts = watts;
      queuedAtMs = millis();
    }
  }
  appLogger.log(String("[INVERTER-MONITOR] Power command queued: ") + watts + "W (WiFi unavailable)");
  return false;
}

bool InverterMonitor::isPowerCommandQueued() {
  ScopedPowerStateLock lock(powerStateMutex, POWER_STATE_LOCK_TIMEOUT_MS);
  if (!lock.acquired()) return false;
  return queuedPowerWatts >= 0;
}

int InverterMonitor::getCachedPowerLimit() {
  ScopedPowerStateLock lock(powerStateMutex, POWER_STATE_LOCK_TIMEOUT_MS);
  if (!lock.acquired()) return cachedPowerLimit;  // best-effort
  return cachedPowerLimit;
}

void InverterMonitor::applyPendingPowerCommand() {
  // --- Snapshot under lock ---
  int targetWatts;
  {
    ScopedPowerStateLock lock(powerStateMutex, POWER_STATE_LOCK_TIMEOUT_MS);
    if (!lock.acquired()) return;
    if (queuedPowerWatts < 0) return;  // nothing queued

    // Check 5-minute expiry for queued commands.
    if (POWER_COMMAND_EXPIRY_MS > 0 &&
        millis() - queuedAtMs > POWER_COMMAND_EXPIRY_MS) {
      appLogger.log(String("[INVERTER-MONITOR] Queued power command expired (") +
                    queuedPowerWatts + "W)");
      queuedPowerWatts = -1;
      return;
    }
    targetWatts = queuedPowerWatts;
  }

  // --- HTTP call outside the lock ---
  String responseBody, errorMessage;
  int httpCode = 0;
  String payload = String(targetWatts);
  bool ok = fetchInverterData("POST", "/power", payload, responseBody, httpCode, errorMessage, false);

  if (ok) {
    {
      ScopedPowerStateLock lock(powerStateMutex, POWER_STATE_LOCK_TIMEOUT_MS);
      if (lock.acquired()) {
        // Only clear if it hasn't been replaced by a newer command.
        if (queuedPowerWatts == targetWatts) {
          queuedPowerWatts = -1;
        }
      }
    }
    appLogger.log(String("[INVERTER-MONITOR] Queued power command delivered: ") +
                  targetWatts + "W");
    refreshPowerLimit();
  } else {
    appLogger.log(String("[INVERTER-MONITOR] Retry power command failed: ") + errorMessage);
  }
}

void InverterMonitor::refreshPowerLimit() {
  String errorMessage;
  int watts = fetchPowerLimit(errorMessage);
  if (watts >= 0) {
    ScopedPowerStateLock lock(powerStateMutex, POWER_STATE_LOCK_TIMEOUT_MS);
    if (lock.acquired()) {
      cachedPowerLimit = watts;
    }
    if (debugMode) {
      appLogger.log(String("[INVERTER-MONITOR] Power limit read: ") + watts + "W");
    }
  } else {
    appLogger.log(String("[INVERTER-MONITOR] Failed to read power limit: ") + errorMessage);
  }
}

void InverterMonitor::onLinkStateTransition(InverterLinkState from,
                                            InverterLinkState to,
                                            uint32_t streakMs) {
  if (to == InverterLinkState::ONLINE) {
    if (from == InverterLinkState::STARTING) {
      // First ever successful poll — read the real power limit from inverter.
      refreshPowerLimit();
    } else if (from == InverterLinkState::BACKOFF || from == InverterLinkState::DORMANT) {
      // Recovery after extended outage — re-read power limit (may have changed
      // if inverter rebooted) and deliver any queued user command.
      refreshPowerLimit();
      applyPendingPowerCommand();
    }
  }
}

int InverterMonitor::fetchPowerLimit(String& errorMessage) {
  String responseBody;
  int httpCode = 0;
  bool ok = fetchInverterData("GET", "/power", "", responseBody, httpCode, errorMessage, false);
  if (!ok) {
    return -1;
  }
  responseBody.trim();
  int watts = responseBody.toInt();
  if (watts <= 0 && responseBody != "0") {
    errorMessage = "invalid /power response: " + responseBody;
    return -1;
  }
  return watts;
}
