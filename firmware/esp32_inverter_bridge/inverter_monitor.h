#ifndef INVERTER_MONITOR_H
#define INVERTER_MONITOR_H

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>

#include "inverter_data.h"
#include "settings.h"
#include "inverter_link_state.h"

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
   */
  bool setPower(int watts, String& responseBody, int& httpCode, String& errorMessage);

  /**
   * Fetch a specific path from the inverter.
   */
  bool fetchPath(const String& path, String& responseBody, int& httpCode, String& errorMessage);

  /**
   * Current link state (see InverterLinkState above).
   * Written only by the polling task; safe to read from any context.
   */
  InverterLinkState getLinkState();

  /**
   * How long the current failure streak has lasted, in milliseconds.
   * Returns 0 when state is ONLINE or STARTING.
   */
  uint32_t getFailureStreakMs();

  /**
   * The poll/retry interval currently in use, in milliseconds.
   */
  uint32_t getRetryIntervalMs();

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

  // Fired on every link-state transition. All once-per-transition actions
  // are dispatched here. Called from the polling task only.
  //
  // Current bindings:
  //   STARTING -> ONLINE           : fetchAndCacheSettings()
  //   BACKOFF|DORMANT -> ONLINE    : fetchAndCacheSettings()
  void onLinkStateTransition(InverterLinkState from, InverterLinkState to, uint32_t streakMs);

  // Fetch shadow state and power limit live from the inverter and update
  // the in-memory cache. Called on first-ever connection and after recovery
  // from an extended outage.
  void fetchAndCacheSettings();

  // Private state
  TaskHandle_t pollingTaskHandle = nullptr;
  SemaphoreHandle_t dataMutex = nullptr;
  HomeData cachedData;
  unsigned long lastUpdateMs = 0;
  uint32_t successfulPolls = 0;
  uint32_t failedPolls = 0;
  bool isInitialized = false;

  // Link-state machine tracking. The actual state lives in inverter_link_state.cpp
  // as globalInverterState, accessed via setInverterState() and getInverterState().
  // failureStartMs is written only by the polling task; reads from other tasks are atomic.
  uint32_t failureStartMs = 0;          // millis() when current streak began; 0 if none
  uint32_t currentRetryIntervalMs = WIFI_BRIDGE_POLL_INTERVAL_MS;

  // Cached shadow + power-limit read back from the inverter.
  // Protected by dataMutex. *Known = false until first successful read.
  bool shadowKnown_ = false;
  bool shadowOn_ = false;
  bool powerLimitKnown_ = false;
  uint16_t powerLimitW_ = 0;
};

#endif // INVERTER_MONITOR_H
