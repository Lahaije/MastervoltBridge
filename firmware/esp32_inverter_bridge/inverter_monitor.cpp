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

bool InverterMonitor::incrementCounterLocked(uint32_t& counter) {
  if (dataMutex == nullptr) return false;
  ScopedDataLock lock(dataMutex, DATA_LOCK_TIMEOUT_MS, "dataMutex");
  if (!lock.acquired()) return false;
  counter++;
  return true;
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

        // After a successful poll, first check whether the auto-reset timer
        // has expired. If it has, queue a MAX request. Then deliver any
        // pending power command in the same iteration.
        checkPowerLimitResetTimer();
        applyPendingPowerCommand();
      } else {
        incrementCounterLocked(failedPolls);
        appLogger.log("[INVERTER-MONITOR] Failed to parse /home response");
      }
    } else {
      incrementCounterLocked(failedPolls);
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

  // --- Update desired state under lock ---
  {
    ScopedPowerStateLock lock(powerStateMutex, POWER_STATE_LOCK_TIMEOUT_MS);
    if (!lock.acquired()) {
      errorMessage = "Power state mutex busy";
      httpCode = 0;
      return false;
    }
    desiredPowerLimit = watts;
    desiredPowerSetAtMs = millis();
    timerTriggeredReset = false;

    // Sub-max requests (re)arm the reset timer; max-power requests don't touch it.
    if (watts < INVERTER_MAX_POWER_WATTS) {
      powerLimitResetAtMs = millis() + ((unsigned long)POWER_LIMIT_RESET_MINUTES * 60UL * 1000UL);
    }
    // NOTE: do NOT pre-mark queued. If we did, the polling task could observe
    // queued=true between our lock release and the HTTP call, and send a
    // duplicate POST /power. Mark queued only on failure below.
  }

  // --- HTTP call OUTSIDE the lock ---
  String payload = String(watts);
  bool ok = fetchInverterData("POST", "/power", payload, responseBody, httpCode, errorMessage, false);

  // --- Commit result under lock ---
  {
    ScopedPowerStateLock lock(powerStateMutex, POWER_STATE_LOCK_TIMEOUT_MS);
    if (lock.acquired()) {
      // Only commit if desired hasn't changed since.
      if (desiredPowerLimit == watts) {
        if (ok) {
          confirmedPowerLimit = watts;
          powerCommandQueued = false;
        } else {
          powerCommandQueued = true;
        }
      }
    }
  }

  if (ok) {
    appLogger.log(String("[INVERTER-MONITOR] Power set to ") + watts + "W (immediate)");
    return true;
  }
  appLogger.log(String("[INVERTER-MONITOR] Power command queued: ") + watts + "W (WiFi unavailable)");
  return false;
}

bool InverterMonitor::isPowerCommandQueued() {
  ScopedPowerStateLock lock(powerStateMutex, POWER_STATE_LOCK_TIMEOUT_MS);
  if (!lock.acquired()) return false;
  return powerCommandQueued;
}

int InverterMonitor::getDesiredPowerLimit() {
  ScopedPowerStateLock lock(powerStateMutex, POWER_STATE_LOCK_TIMEOUT_MS);
  if (!lock.acquired()) return desiredPowerLimit;  // best-effort read
  return desiredPowerLimit;
}

int InverterMonitor::getConfirmedPowerLimit() {
  ScopedPowerStateLock lock(powerStateMutex, POWER_STATE_LOCK_TIMEOUT_MS);
  if (!lock.acquired()) return confirmedPowerLimit;
  return confirmedPowerLimit;
}

unsigned long InverterMonitor::getPowerLimitResetAtMs() {
  ScopedPowerStateLock lock(powerStateMutex, POWER_STATE_LOCK_TIMEOUT_MS);
  if (!lock.acquired()) return powerLimitResetAtMs;
  return powerLimitResetAtMs;
}

void InverterMonitor::applyPendingPowerCommand() {
  // --- Snapshot under lock ---
  int targetWatts;
  bool isTimerTriggered;
  bool shouldExpire = false;
  {
    ScopedPowerStateLock lock(powerStateMutex, POWER_STATE_LOCK_TIMEOUT_MS);
    if (!lock.acquired()) return;
    if (!powerCommandQueued) return;

    // Check 5-minute expiry for user-initiated commands (not timer resets).
    if (!timerTriggeredReset && POWER_COMMAND_EXPIRY_MS > 0 &&
        millis() - desiredPowerSetAtMs > POWER_COMMAND_EXPIRY_MS) {
      shouldExpire = true;
      targetWatts = desiredPowerLimit;  // for logging
    } else {
      targetWatts = desiredPowerLimit;
      isTimerTriggered = timerTriggeredReset;
    }
  }

  if (shouldExpire) {
    appLogger.log(String("[INVERTER-MONITOR] Queued power command expired (") +
                  targetWatts + "W)");
    ScopedPowerStateLock lock(powerStateMutex, POWER_STATE_LOCK_TIMEOUT_MS);
    if (lock.acquired() && powerCommandQueued && !timerTriggeredReset) {
      powerCommandQueued = false;
      desiredPowerLimit = confirmedPowerLimit;
    }
    return;
  }

  // --- HTTP call outside the lock ---
  String responseBody, errorMessage;
  int httpCode = 0;
  String payload = String(targetWatts);
  bool ok = fetchInverterData("POST", "/power", payload, responseBody, httpCode, errorMessage, false);

  // --- Commit result under lock ---
  if (ok) {
    ScopedPowerStateLock lock(powerStateMutex, POWER_STATE_LOCK_TIMEOUT_MS);
    if (lock.acquired()) {
      // Only commit if desired didn't change while we were sending.
      if (desiredPowerLimit == targetWatts) {
        confirmedPowerLimit = targetWatts;
        powerCommandQueued = false;
        if (isTimerTriggered && targetWatts == INVERTER_MAX_POWER_WATTS) {
          powerLimitResetAtMs = 0;
          timerTriggeredReset = false;
        }
      }
    }
    appLogger.log(String("[INVERTER-MONITOR] Queued power command delivered: ") +
                  targetWatts + "W");
  } else {
    appLogger.log(String("[INVERTER-MONITOR] Retry power command failed: ") + errorMessage);
  }
}

void InverterMonitor::checkPowerLimitResetTimer() {
  // The timer arms a pending command; the actual send happens via
  // applyPendingPowerCommand. This avoids a race where the timer's HTTP
  // could fire AFTER a concurrent setPower(N<MAX) and leave the inverter
  // at MAX while our state thinks it's at N.
  ScopedPowerStateLock lock(powerStateMutex, POWER_STATE_LOCK_TIMEOUT_MS);
  if (!lock.acquired()) return;
  if (powerLimitResetAtMs == 0 || millis() < powerLimitResetAtMs) return;

  // Timer fired.
  if (confirmedPowerLimit == INVERTER_MAX_POWER_WATTS &&
      desiredPowerLimit == INVERTER_MAX_POWER_WATTS) {
    // Already at max — just clear the timer.
    powerLimitResetAtMs = 0;
    return;
  }

  appLogger.log(String("[INVERTER-MONITOR] Power limit timer expired. Queuing reset to ") +
                INVERTER_MAX_POWER_WATTS + "W");

  desiredPowerLimit = INVERTER_MAX_POWER_WATTS;
  desiredPowerSetAtMs = millis();
  timerTriggeredReset = true;
  powerCommandQueued = true;
  // applyPendingPowerCommand runs immediately after this in the polling loop
  // and will send the actual POST /power.
}

void InverterMonitor::queueMaxPowerAfterLongDisconnect(uint32_t streakMs) {
  ScopedPowerStateLock lock(powerStateMutex, POWER_STATE_LOCK_TIMEOUT_MS);
  if (!lock.acquired()) return;

  // If a user-initiated command is currently queued (e.g. someone POSTed
  // /api/power while we were disconnected), respect it; the user's intent
  // wins over the defensive max-reset.
  if (powerCommandQueued && !timerTriggeredReset) {
    appLogger.log(String("[INVERTER-MONITOR] Recovery after ") + (streakMs / 1000) +
                  "s: user power command still queued, not overriding with MAX");
    return;
  }

  // Already known to be at MAX with no pending change — nothing to do.
  if (confirmedPowerLimit == INVERTER_MAX_POWER_WATTS &&
      desiredPowerLimit == INVERTER_MAX_POWER_WATTS &&
      !powerCommandQueued) {
    return;
  }

  appLogger.log(String("[INVERTER-MONITOR] Recovery after ") + (streakMs / 1000) +
                "s: queuing MAX power reset (inverter state unknown)");

  desiredPowerLimit = INVERTER_MAX_POWER_WATTS;
  desiredPowerSetAtMs = millis();
  timerTriggeredReset = true;   // exempt from POWER_COMMAND_EXPIRY_MS so it keeps retrying
  powerCommandQueued = true;
  powerLimitResetAtMs = 0;      // clear any pending auto-reset; we're resetting now
}

void InverterMonitor::onLinkStateTransition(InverterLinkState from,
                                            InverterLinkState to,
                                            uint32_t streakMs) {
  // Single dispatch point for every link-state transition. Add new bindings
  // here; the polling loop and any callers stay untouched.
  //
  // Recovery from an extended outage: the inverter may have rebooted or its
  // power-limit reset timer may have elapsed while we were unreachable, and
  // there is no read-back endpoint for the current limit. Defensively force
  // a MAX-power reset; applyPendingPowerCommand() delivers it on this
  // iteration and the queued command persists across polls until accepted.
  const bool wasOutage =
      (from == InverterLinkState::BACKOFF || from == InverterLinkState::DORMANT);
  if (to == InverterLinkState::ONLINE && wasOutage) {
    queueMaxPowerAfterLongDisconnect(streakMs);
    applyPendingPowerCommand();
    return;
  }

  // STARTING -> ONLINE, ONLINE -> RETRYING, RETRYING -> BACKOFF,
  // BACKOFF -> DORMANT: no bound action today. The transition is logged
  // (when debugMode is on) by runPollingTask() before calling this method.
}

bool InverterMonitor::fetchPath(const String& path, String& responseBody, int& httpCode, String& errorMessage) {
  if (path.length() == 0 || path[0] != '/') {
    errorMessage = "path must start with '/'";
    return false;
  }

  return fetchInverterData("GET", path, "", responseBody, httpCode, errorMessage, false);
}
