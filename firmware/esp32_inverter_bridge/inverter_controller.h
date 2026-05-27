#ifndef INVERTER_CONTROLLER_H
#define INVERTER_CONTROLLER_H

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>

#include "inverter_data.h"
#include "settings.h"
#include "inverter_link_state.h"

/**
 * InverterController: Manages polling of the inverter's /home endpoint,
 * caches the latest response as a parsed HomeData object, and provides
 * HTTP request functions for interacting with the inverter.
 * 
 * This module handles all inverter requests and telemetry collection,
 * separating these concerns from WiFi connectivity management.
 */
class InverterController {
public:
  static InverterController& getInstance() {
    static InverterController instance;
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
   * Result of a setShadow / setPower call.
   *
   *  Applied  — value validated, POSTed to inverter, and re-read from
   *             inverter (cache updated). The readback value should match
   *             the desired value but the caller can verify via getShadow()
   *             / getPowerLimit() if needed.
   *  Deferred — value validated, but could not be applied right now (POST
   *             failed, or the *other* shared field is not yet cached so the
   *             combined /postoptions form cannot be built safely). The
   *             desired value is queued; the polling task will retry it on
   *             the next successful /home telegram via applyPendingSettings().
   *             The cached read-back fields are NOT updated.
   *  Rejected — value failed validation (out of range, malformed). Nothing
   *             is queued, nothing is sent.
   */
  enum class SetResult { Applied, Deferred, Rejected };

  /**
   * Set the inverter power limit.
   * On Applied, the cached value is refreshed from the inverter and any
   * pending desired-power flag is cleared if it now matches.
   */
  SetResult setPower(int watts, String& responseBody, int& httpCode, String& errorMessage);

  /**
   * Enable or disable the inverter shadow function.
   * On Applied, the cached value is refreshed from the inverter and any
   * pending desired-shadow flag is cleared if it now matches.
   */
  SetResult setShadow(bool enabled, String& responseBody, int& httpCode, String& errorMessage);

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

  /**
   * Get the base poll interval (used for ONLINE/RETRYING/STARTING states).
   */
  uint32_t getBasePollIntervalMs();

  /**
   * Set the base poll interval at runtime. Clamped to [5000, 300000] ms.
   * Takes effect on the next state-entry hook fire or immediately if
   * the current state uses the base interval.
   */
  void setBasePollIntervalMs(uint32_t ms);

  /**
   * Cached shadow function state. Returns true if a value has been read from
   * the inverter at least once since boot (cache populated by
   * fetchAndCacheSettings()), in which case the on/off state is written to
   * *enabledOut*. Returns false when no value has ever been cached or when
   * the data mutex could not be acquired.
   */
  bool getShadow(bool& enabledOut);

  /**
   * Cached inverter power limit in watts. Returns true if a value has been
   * read from the inverter at least once since boot, in which case the limit
   * is written to *wattsOut*. Returns false when no value has ever been
   * cached or when the data mutex could not be acquired.
   */
  bool getPowerLimit(uint16_t& wattsOut);

  /**
   * Returns true if at least one desired shadow / power-limit value is
   * currently queued for delayed apply (see applyPendingSettings()).
   */
  bool hasPendingSettings();

private:
  InverterController();
  ~InverterController() = default;

  // Prevent copying
  InverterController(const InverterController&) = delete;
  InverterController& operator=(const InverterController&) = delete;

  // FreeRTOS task for polling
  static void pollingTaskEntry(void* param);

  // Polling task implementation
  void runPollingTask();

  // Reconcile global link state from streak duration.
  // streakMs determines the target state and is used for transition logging.
  // 0 forces ONLINE after successful reads.
  void linkStateFromStreak(uint32_t streakMs);

  // Increment a counter (e.g. successfulPolls / failedPolls) under dataMutex.
  // Returns false if the mutex could not be acquired within the configured timeout.
  bool incrementCounterLocked(uint32_t& counter);

  // State-entry hook registered at initialize() time.
  // Updates currentRetryIntervalMs whenever the FSM enters a new state.
  static void applyIntervalForState(InverterLinkState from, InverterLinkState to);

  // Hook callbacks for state-dependent side effects.
  static void loadSettingsOnBoot(InverterLinkState from, InverterLinkState to);
  static void updateAllInverterParam(InverterLinkState from, InverterLinkState to);

  // Fetch shadow state and power limit live from the inverter and update
  // the in-memory cache. Called on first-ever connection and after recovery
  // from an extended outage.
  void fetchAndCacheSettings();

  // POST a payload to the inverter's /postoptions form endpoint. Both shadow
  // and power-limit setters share this endpoint; only the payload differs:
  //   power : "enable_mxpower=on\nmaxpower=<W>"
  //   shadow: "enShadow=on" or "enShadow=off"
  // The payload is a newline-separated list of key=value pairs. postOptions()
  // encodes them as multipart/form-data before sending via fetchInverterData().
  bool postOptions(const String& payload, String& responseBody,
                   int& httpCode, String& errorMessage);

  // Reconciliation loop. Called from runPollingTask() after a successful
  // /home telegram when pendingSettings_ is set. Recomputes the desired
  // composite state, posts if it differs from the cached read-back, re-reads
  // both values from the inverter, and clears the pending flag on full match.
  void applyPendingSettings();

  // Helpers to queue a desired value under dataMutex; also raise pendingSettings_.
  void queueShadowDesired(bool enabled);
  void queuePowerLimitDesired(uint16_t watts);

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
  uint32_t basePollIntervalMs_ = WIFI_BRIDGE_POLL_INTERVAL_MS;  // runtime-adjustable base

  // Cached shadow + power-limit read back from the inverter.
  // Protected by dataMutex. *Known = false until first successful read.
  bool shadowKnown_ = false;
  bool shadowOn_ = false;
  bool powerLimitKnown_ = false;
  uint16_t powerLimitW_ = 0;

  // Desired-state convergence: when an API set call cannot reach the inverter
  // (or cannot safely build the combined /postoptions form because the *other*
  // shared field is still unknown), the desired value is queued here and the
  // master flag is raised. The polling task drains it via applyPendingSettings()
  // on the next successful /home telegram. Protected by dataMutex.
  // pendingSettings_ is the master "anything to do?" flag — equivalent to
  // (shadowDesiredPending_ || powerLimitDesiredPending_) but stored explicitly
  // to keep the polling task's per-poll check O(1) without taking the mutex.
  volatile bool pendingSettings_           = false;
  bool          shadowDesiredPending_      = false;
  bool          shadowDesired_             = false;
  bool          powerLimitDesiredPending_  = false;
  uint16_t      powerLimitDesired_         = 0;
};

#endif // INVERTER_CONTROLLER_H
