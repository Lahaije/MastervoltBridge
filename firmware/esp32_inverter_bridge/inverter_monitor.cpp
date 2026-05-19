#include "inverter_monitor.h"

#include "inverter_data.h"
#include "settings.h"
#include "logger.h"
#include "wifi_bridge.h"

namespace {
constexpr const char* HOME_ENDPOINT = "/home";
constexpr bool ENABLE_INVERTER_POLLING = true;

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
  {           0,  20000u },  //  0 –  5 min : every 20 s   (normal rate, matches WIFI_BRIDGE_POLL_INTERVAL_MS)
  {  5 * 60000u,  60000u },  //  5 – 20 min : every  1 min
  { 20 * 60000u, 600000u },  // 20+    min  : every 10 min
};
static constexpr size_t BACKOFF_STAGE_COUNT = sizeof(BACKOFF_STAGES) / sizeof(BACKOFF_STAGES[0]);

static uint32_t getBackoffIntervalMs(uint32_t failedForMs) {
  // Walk stages in reverse; return the interval of the last threshold reached.
  for (int i = static_cast<int>(BACKOFF_STAGE_COUNT) - 1; i >= 0; i--) {
    if (failedForMs >= BACKOFF_STAGES[i].after_failure_ms) {
      return BACKOFF_STAGES[i].interval_ms;
    }
  }
  return BACKOFF_STAGES[0].interval_ms;
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
  uint32_t failureStartMs = 0;  // millis() when current failure streak began; 0 = no streak
  uint32_t lastIntervalMs = WIFI_BRIDGE_POLL_INTERVAL_MS;

  while (true) {
    bool iterationOk = false;

    // Obtain a WiFi connection via the connection manager. The manager
    // returns immediately if WiFi is already up, or invokes the next
    // alternating connect path (dwell/auto) if WiFi is down.
    if (!WifiConnectionManager::getInstance().ensureConnected()) {
      appLogger.log("[INVERTER-MONITOR] No WiFi connection; skipping poll iteration");
    } else {
      // Fetch the latest /home response
      String rawResponse;
      String errorMessage;
      int httpCode = 0;

      bool ok = fetchInverterData("GET", HOME_ENDPOINT, "", rawResponse, httpCode, errorMessage);

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
        } else {
          incrementCounterLocked(failedPolls);
          appLogger.log("[INVERTER-MONITOR] Failed to parse /home response");
        }
      } else {
        incrementCounterLocked(failedPolls);
        appLogger.log(String("[INVERTER-MONITOR] Failed to fetch /home: ") + errorMessage);
      }
    }

    // Update failure streak tracking and compute next sleep interval.
    if (iterationOk) {
      if (failureStartMs != 0) {
        appLogger.log("[INVERTER-MONITOR] Inverter recovered; resuming normal poll interval");
        failureStartMs = 0;
        lastIntervalMs = WIFI_BRIDGE_POLL_INTERVAL_MS;
      }
    } else {
      if (failureStartMs == 0) {
        failureStartMs = millis();
      }
      uint32_t nextIntervalMs = getBackoffIntervalMs(millis() - failureStartMs);
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
