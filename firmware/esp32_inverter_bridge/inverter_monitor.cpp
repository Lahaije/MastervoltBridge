#include "inverter_monitor.h"

#include "inverter_data.h"
#include "settings.h"
#include "logger.h"
#include "wifi_bridge.h"

namespace {
constexpr const char* HOME_ENDPOINT = "/home";
constexpr bool ENABLE_INVERTER_POLLING = false;
constexpr uint32_t MONITOR_WIFI_CONNECT_TIMEOUT_MS = 5000;
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

void InverterMonitor::runPollingTask() {
  TickType_t lastWakeTime = xTaskGetTickCount();
  const TickType_t intervalTicks = pdMS_TO_TICKS(WIFI_BRIDGE_POLL_INTERVAL_MS);

  while (true) {
    // Ensure WiFi is connected to the inverter
    if (!ensureWifiConnectedWithTimeout(MONITOR_WIFI_CONNECT_TIMEOUT_MS)) {
      appLogger.log("[INVERTER-MONITOR] WiFi not connected within 5s, skipping poll iteration");
      xTaskDelayUntil(&lastWakeTime, intervalTicks);
      continue;
    }

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

          // Log poll result: current power (get_Power) and status
          appLogger.log(String("[INVERTER-MONITOR] Poll #") + successfulPolls + 
                        ": Status=" + parsedData.operatingStatus + 
                        " Power=" + parsedData.instantaneousPower + "W");
        }
      } else {
        if (dataMutex != nullptr && xSemaphoreTake(dataMutex, pdMS_TO_TICKS(5000)) == pdTRUE) {
          failedPolls++;
          xSemaphoreGive(dataMutex);
        }
        appLogger.log("[INVERTER-MONITOR] Failed to parse /home response");
      }
    } else {
      if (dataMutex != nullptr && xSemaphoreTake(dataMutex, pdMS_TO_TICKS(5000)) == pdTRUE) {
        failedPolls++;
        xSemaphoreGive(dataMutex);
      }
      appLogger.log(String("[INVERTER-MONITOR] Failed to fetch /home: ") + errorMessage);
    }

    // Sleep until next poll interval
    xTaskDelayUntil(&lastWakeTime, intervalTicks);
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
