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
   * Set polling interval in seconds for the monitor loop.
   * Valid range: 1..3600 seconds.
   */
  bool setPollingIntervalSeconds(uint32_t seconds, uint32_t& appliedMs, String& errorMessage);

  /**
   * Get current polling interval in milliseconds.
   */
  uint32_t getPollingIntervalMs();

  /**
   * Set the inverter power limit.
   * If the inverter is reachable, sets immediately and returns true.
   * If unreachable, queues the command for retry and returns false
   * (caller should check isCommandQueued()).
   */
  bool setPower(int watts, String& responseBody, int& httpCode, String& errorMessage);

  /**
   * Returns true if a power command is queued and awaiting delivery.
   */
  bool isPowerCommandQueued();

  /**
   * Get current power limit state.
   */
  int getDesiredPowerLimit();
  int getConfirmedPowerLimit();
  unsigned long getPowerLimitResetAtMs();

  /**
   * Fetch a specific path from the inverter.
   */
  bool fetchPath(const String& path, String& responseBody, int& httpCode, String& errorMessage);

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

  // Polling interval runtime config.
  SemaphoreHandle_t pollingConfigMutex = nullptr;
  uint32_t pollingIntervalMs = WIFI_BRIDGE_POLL_INTERVAL_MS;

  // Power limit state machine. All fields below are protected by powerStateMutex.
  // Held briefly only to read/update fields (never across HTTP calls).
  SemaphoreHandle_t powerStateMutex = nullptr;
  int desiredPowerLimit = INVERTER_MAX_POWER_WATTS;
  int confirmedPowerLimit = INVERTER_MAX_POWER_WATTS;
  unsigned long desiredPowerSetAtMs = 0;        // when the user command was received
  unsigned long powerLimitResetAtMs = 0;        // absolute ms deadline; 0 = no timer active
  bool powerCommandQueued = false;              // true = desired != confirmed, awaiting delivery
  bool timerTriggeredReset = false;             // true = reset was triggered by timer (never expires)

  // Apply pending power command if needed (called from polling loop).
  void applyPendingPowerCommand();

  // Check and handle power-limit reset timer (called from polling loop).
  void checkPowerLimitResetTimer();
};

#endif // INVERTER_MONITOR_H
