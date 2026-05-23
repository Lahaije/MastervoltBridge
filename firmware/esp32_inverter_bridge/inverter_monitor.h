#ifndef INVERTER_MONITOR_H
#define INVERTER_MONITOR_H

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>

#include "inverter_data.h"
#include "settings.h"

/**
 * Link-state machine for the inverter polling loop.
 *
 * Transitions are driven by the failure streak length (no successful /home
 * poll since this many ms). The polling interval per state is in
 * inverter_monitor.cpp (LINK_*_INTERVAL_MS constants).
 *
 *   STARTING  -> ONLINE                : first ever successful poll
 *   ONLINE    -> RETRYING              : poll failed
 *   RETRYING  -> ONLINE                : poll succeeded (no recovery action)
 *   RETRYING  -> BACKOFF               : streak >= LINK_RETRYING_TO_BACKOFF_MS
 *   BACKOFF   -> ONLINE (+MAX reset)   : poll succeeded after long outage
 *   BACKOFF   -> DORMANT               : streak >= LINK_BACKOFF_TO_DORMANT_MS
 *   DORMANT   -> ONLINE (+MAX reset)   : poll succeeded after very long outage
 */
enum class InverterLinkState : uint8_t {
  STARTING = 0,  // boot, no successful poll yet
  ONLINE,        // last poll OK; polling at base interval
  RETRYING,      // streak < short threshold; still at base interval
  BACKOFF,       // streak between short and long thresholds; relaxed interval
  DORMANT,       // streak >= long threshold; max-throttled retries (e.g. overnight)
};

const char* toString(InverterLinkState s);

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

  /**
   * Current link state and how long the bridge has been failing to reach
   * the inverter. failure_streak_ms == 0 when state is ONLINE or STARTING.
   * retry_interval_ms is the interval the polling loop will wait until the
   * next attempt while in the current state.
   */
  InverterLinkState getLinkState();
  uint32_t getFailureStreakMs();
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

  // After a long inverter disconnect, queue a MAX-power command so the
  // polling loop will (re)assert it on recovery. The actual delivery and
  // retry-on-failure is handled by applyPendingPowerCommand(). Idempotent:
  // if the inverter is already at MAX nothing is queued.
  void queueMaxPowerAfterLongDisconnect(uint32_t streakMs);

  // Fired by the polling task whenever the link state changes. All actions
  // that should run once-per-transition (as opposed to once-per-poll) belong
  // here. Called from the polling task only; not thread-safe for arbitrary
  // callers. streakMs is the failure-streak duration at the moment of the
  // transition (always 0 when transitioning into ONLINE).
  //
  // Current bindings:
  //   STARTING -> ONLINE                : (none; cached telemetry now available)
  //   ONLINE   -> RETRYING              : (none; first failure of a new streak)
  //   RETRYING -> BACKOFF               : (none; relaxed retry cadence starts)
  //   BACKOFF  -> DORMANT               : (none; max-throttled cadence starts)
  //   BACKOFF|DORMANT -> ONLINE         : queueMaxPowerAfterLongDisconnect()
  //                                       + applyPendingPowerCommand()
  void onLinkStateTransition(InverterLinkState from, InverterLinkState to, uint32_t streakMs);

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

  // Link-state snapshot. Written only by the polling task; read by API
  // handlers. Reads/writes are protected by pollingConfigMutex (cheap
  // tiny-state lock; reuses an existing mutex to avoid adding another).
  InverterLinkState linkState = InverterLinkState::STARTING;
  uint32_t failureStartMs = 0;     // millis() when current streak began; 0 if none
  uint32_t currentRetryIntervalMs = WIFI_BRIDGE_POLL_INTERVAL_MS;

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
