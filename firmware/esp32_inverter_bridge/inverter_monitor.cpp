#include "inverter_monitor.h"

#include "inverter_data.h"
#include "settings.h"
#include "logger.h"
#include "wifi_bridge.h"

namespace {
constexpr const char* HOME_ENDPOINT = "/home";
constexpr bool ENABLE_INVERTER_POLLING = true;
constexpr uint32_t POWER_STATE_LOCK_TIMEOUT_MS = 1000;

// RAII lock guard for power-state mutex. Held briefly only.
class ScopedPowerStateLock {
public:
  ScopedPowerStateLock(SemaphoreHandle_t mtx, uint32_t timeoutMs)
      : mutex_(mtx),
        acquired_(mtx != nullptr && xSemaphoreTake(mtx, pdMS_TO_TICKS(timeoutMs)) == pdTRUE) {}
  ~ScopedPowerStateLock() {
    if (acquired_ && mutex_ != nullptr) xSemaphoreGive(mutex_);
  }
  bool acquired() const { return acquired_; }
private:
  SemaphoreHandle_t mutex_;
  bool acquired_;
};

// ---------------------------------------------------------------------------
// Stepped retry backoff for inverter unavailability (e.g. overnight).
// When the inverter stops responding the poll interval is relaxed in stages
// to avoid flooding the log buffer and hammering the GPIO button all night.
// On the next successful poll the interval resets to normal automatically.
// Edit this table to tune retry aggressiveness vs. log/GPIO noise.
// ---------------------------------------------------------------------------
struct BackoffStage {
  uint32_t after_failure_ms;  // Enter this stage once failure streak >= this duration
  uint32_t interval_ms;       // Poll/retry interval while in this stage
};

static const BackoffStage BACKOFF_STAGES[] = {
  {  5 * 60000u,  60000u },  //  5 – 20 min : every  1 min
  { 20 * 60000u, 600000u },  // 20+    min  : every 10 min
};
static constexpr size_t BACKOFF_STAGE_COUNT = sizeof(BACKOFF_STAGES) / sizeof(BACKOFF_STAGES[0]);

// A failure streak this long counts as a "long disconnect". On recovery the
// monitor will force a power-limit reset to MAX because the inverter may have
// rebooted / forgotten our last setting, and we cannot read the current limit
// back from it. Matches the first backoff threshold so any time we entered
// reduced polling, we re-arm to MAX on the next successful poll.
static constexpr uint32_t LONG_DISCONNECT_THRESHOLD_MS = 5u * 60u * 1000u;

static uint32_t getBackoffIntervalMs(uint32_t failedForMs, uint32_t baseIntervalMs) {
  if (failedForMs < BACKOFF_STAGES[0].after_failure_ms) {
    return baseIntervalMs;
  }
  // Walk stages in reverse; return the interval of the last threshold reached.
  for (int i = static_cast<int>(BACKOFF_STAGE_COUNT) - 1; i >= 0; i--) {
    if (failedForMs >= BACKOFF_STAGES[i].after_failure_ms) {
      return BACKOFF_STAGES[i].interval_ms;
    }
  }
  return baseIntervalMs;
}

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

  if (powerStateMutex == nullptr) {
    powerStateMutex = xSemaphoreCreateMutex();
  }

  if (pollingConfigMutex == nullptr) {
    pollingConfigMutex = xSemaphoreCreateMutex();
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
  uint32_t failureStartMs = 0;  // millis() when current failure streak began; 0 = no streak
  uint32_t lastIntervalMs = getPollingIntervalMs();

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

    // Update failure streak tracking and compute next sleep interval.
    if (iterationOk) {
      if (failureStartMs != 0) {
        uint32_t streakMs = millis() - failureStartMs;
        appLogger.log(String("[INVERTER-MONITOR] Inverter recovered after ") +
                      (streakMs / 1000) + "s; resuming normal poll interval");
        if (streakMs >= LONG_DISCONNECT_THRESHOLD_MS) {
          // Long disconnect: inverter state is unknown (it may have rebooted
          // or its power-limit timeout may have expired during the outage).
          // Force a MAX-power reset and let applyPendingPowerCommand deliver
          // it on this iteration / retry on subsequent polls until accepted.
          queueMaxPowerAfterLongDisconnect(streakMs);
          applyPendingPowerCommand();
        }
        failureStartMs = 0;
      }
      lastIntervalMs = getPollingIntervalMs();
    } else {
      if (failureStartMs == 0) {
        failureStartMs = millis();
      }
      uint32_t nextIntervalMs = getBackoffIntervalMs(millis() - failureStartMs, getPollingIntervalMs());
      if (nextIntervalMs != lastIntervalMs) {
        appLogger.log(String("[INVERTER-MONITOR] Backoff: retry interval -> ") +
                      (nextIntervalMs / 1000) + "s");
        lastIntervalMs = nextIntervalMs;
      }
    }

    vTaskDelay(pdMS_TO_TICKS(lastIntervalMs));
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

bool InverterMonitor::setPollingIntervalSeconds(uint32_t seconds, uint32_t& appliedMs, String& errorMessage) {
  if (seconds < 1 || seconds > 3600) {
    errorMessage = "seconds must satisfy 1 <= seconds <= 3600";
    return false;
  }

  if (pollingConfigMutex == nullptr) {
    errorMessage = "polling config mutex not initialized";
    return false;
  }

  if (xSemaphoreTake(pollingConfigMutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
    errorMessage = "polling config mutex busy";
    return false;
  }

  pollingIntervalMs = seconds * 1000UL;
  appliedMs = pollingIntervalMs;

  xSemaphoreGive(pollingConfigMutex);
  appLogger.log(String("[INVERTER-MONITOR] Poll interval updated to ") + seconds + "s");
  return true;
}

uint32_t InverterMonitor::getPollingIntervalMs() {
  if (pollingConfigMutex == nullptr) {
    return WIFI_BRIDGE_POLL_INTERVAL_MS;
  }

  if (xSemaphoreTake(pollingConfigMutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
    return pollingIntervalMs;
  }

  uint32_t result = pollingIntervalMs;
  xSemaphoreGive(pollingConfigMutex);
  return result;
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

bool InverterMonitor::fetchPath(const String& path, String& responseBody, int& httpCode, String& errorMessage) {
  if (path.length() == 0 || path[0] != '/') {
    errorMessage = "path must start with '/'";
    return false;
  }

  return fetchInverterData("GET", path, "", responseBody, httpCode, errorMessage, false);
}
