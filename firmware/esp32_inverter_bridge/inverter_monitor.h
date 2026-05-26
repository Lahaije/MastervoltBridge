#ifndef INVERTER_MONITOR_H
#define INVERTER_MONITOR_H

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>

#include "inverter_data.h"
#include "settings.h"

/**
 * InverterMonitor: Manages polling of the inverter's /home endpoint,
 * caches the latest response as a parsed HomeData object, and provides
 * HTTP request functions for interacting with the inverter.
 * 
 * This module handles all inverter requests and telemetry collection,
 * separating these concerns from WiFi connectivity management.
 */
class InverterMonitor {
public:
  static InverterMonitor& getInstance() {
    static InverterMonitor instance;
    return instance;
  }

  /**
   * Initialize the home monitor and start the polling task.
   * Must be called once during system initialization.
   */
  void initialize();

  /**
   * Stop the polling task.
   */
  void shutdown();

  /**
   * Get the latest cached HomeData.
   * Returns true if valid data is available, false otherwise.
   * The returned data is a snapshot and won't change until next poll.
   */
  bool getLatestHomeData(HomeData& dataOut);

  /**
   * Get the timestamp (in milliseconds) when the last successful poll occurred.
   * Returns 0 if no valid data has been cached yet.
   */
  unsigned long getLastUpdateMs();

  /**
   * Set the inverter power limit.
   * On success (HTTP 200), the value is cached in NVS and a state update is
   * pushed to MQTT for the HA "Power Limit" entity (cache + dual-topic).
   */
  bool setPower(int watts, String& responseBody, int& httpCode, String& errorMessage);

  /**
   * Enable or disable the inverter "shadow function" (MPPT shadow-tracking
   * mode). On success the value is cached in NVS and a state update is
   * pushed to MQTT for the HA "Shadow Function" entity.
   * Inverter endpoint: POST /shadow with body "1" (on) or "0" (off).
   */
  bool setShadow(bool enabled, String& responseBody, int& httpCode, String& errorMessage);

  /**
   * Fetch a specific path from the inverter.
   */
  bool fetchPath(const String& path, String& responseBody, int& httpCode, String& errorMessage);

  /**
   * Cached "shadow" value reflecting the last successful set. Returns true
   * when a value is known (set since boot or restored from NVS), false when
   * unknown. The actual on/off state is written to *enabledOut*.
   */
  bool getCachedShadow(bool& enabledOut) const;

  /**
   * Cached power-limit value reflecting the last successful set. Returns
   * true when a value is known, false when unknown.
   */
  bool getCachedPowerLimit(uint16_t& wattsOut) const;

  /**
   * Set polling interval in seconds for the monitor loop.
   * Valid range: 1..3600 seconds. Persisted to NVS so the value survives
   * reboots. On success, *appliedMs* receives the new interval in ms.
   */
  bool setPollingIntervalSeconds(uint32_t seconds, uint32_t& appliedMs, String& errorMessage);

  /**
   * Get current polling interval in milliseconds.
   */
  uint32_t getPollingIntervalMs();

private:
  InverterMonitor();
  ~InverterMonitor();

  // Prevent copying
  InverterMonitor(const InverterMonitor&) = delete;
  InverterMonitor& operator=(const InverterMonitor&) = delete;

  // FreeRTOS task for polling
  static void pollingTaskEntry(void* param);

  // Polling task implementation
  void runPollingTask();

  // Increment a counter (e.g. successfulPolls / failedPolls) under dataMutex.
  // Returns false if the mutex could not be acquired within 5 s.
  bool incrementCounterLocked(uint32_t& counter);

  // Private state
  TaskHandle_t pollingTaskHandle = nullptr;
  SemaphoreHandle_t dataMutex = nullptr;
  HomeData cachedData;
  unsigned long lastUpdateMs = 0;
  uint32_t successfulPolls = 0;
  uint32_t failedPolls = 0;
  bool isInitialized = false;

  // Polling interval runtime config. Protected by pollingConfigMutex.
  // Persisted to NVS so user-set value survives reboots.
  SemaphoreHandle_t pollingConfigMutex = nullptr;
  uint32_t pollingIntervalMs = WIFI_BRIDGE_POLL_INTERVAL_MS;

  // Cached "shadow" values for HA cache+dual-topic pattern. These reflect the
  // last successfully commanded value (NOT a readback from the inverter -
  // /home does not report shadow/power_limit). Persisted to NVS so they
  // survive reboots; published on every successful set and on MQTT reconnect.
  bool shadowKnown_ = false;
  bool shadowOn_ = false;
  bool powerLimitKnown_ = false;
  uint16_t powerLimitW_ = 0;

  void loadCachedSettingsFromNvs();
  void persistShadow(bool enabled);
  void persistPowerLimit(uint16_t watts);
};

#endif // INVERTER_MONITOR_H
