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
    *  Applied  — value validated and POSTed to the inverter. The controller
    *             then invalidates the corresponding cached read-back and
    *             attempts an immediate refresh. If that read-back fails, the
    *             cache remains unknown and the polling task retries it on the
    *             next successful /home telegram.
    *  Deferred — value validated, but could not be applied right now (POST
    *             failed).
   *             desired value is queued; the polling task will retry it on
    *             the next successful /home telegram via the corresponding
    *             per-field applyPending...() helper.
   *             The cached read-back fields are NOT updated.
   *  Rejected — value failed validation (out of range, malformed). Nothing
   *             is queued, nothing is sent.
   */
  enum class SetResult { Applied, Deferred, Rejected };

  /**
   * Set the inverter power limit.
    * On Applied, the queued desired-power flag is cleared, the cached value is
    * invalidated, and an immediate read-back is attempted.
   */
  SetResult setPower(int watts, String& responseBody, int& httpCode, String& errorMessage);

  /**
   * Enable or disable the inverter shadow function.
    * On Applied, the queued desired-shadow flag is cleared, the cached value is
    * invalidated, and an immediate read-back is attempted.
   */
  SetResult setShadow(bool enabled, String& responseBody, int& httpCode, String& errorMessage);

  /**
   * Fetch a specific path from the inverter.
   */
  bool fetchPath(const String& path, String& responseBody, int& httpCode, String& errorMessage);

  /**
   * Current link state (see InverterLinkState above).
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
    * Temporarily override the current poll interval at runtime.
    * Clamped to [100, 300000] ms. The override remains in effect until
    * the next natural link-state transition updates the interval.
   */
  void setPollIntervalMs(uint32_t ms);

  /**
   * Cached shadow function state. Returns true if a value has been successfully
   * read from the inverter at least once since boot, in which case the on/off
   * state is written to *enabledOut*. Returns false when shadowKnown_ is false
   * (never fetched, or invalidated pending a readback).
   */
  bool getShadow(bool& enabledOut);

  /**
   * Cached inverter power limit in watts. Returns true if a value has been
   * successfully read from the inverter at least once since boot, in which
   * case the limit is written to *wattsOut*. Returns false when
   * powerLimitKnown_ is false (never fetched, or invalidated pending a readback).
   */
  bool getPowerLimit(uint16_t& wattsOut);

  /**
   * Returns true if at least one desired shadow / power-limit value is
    * currently queued for delayed apply.
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

  // State hook registered at initialize() time for poll-interval control.
  // Entering ONLINE/BACKOFF/DORMANT calls this function, which selects the
  // appropriate interval from the current state.
  static void updatePollFrequency(InverterLinkState from, InverterLinkState to);

  // Hook callbacks for state-dependent side effects.
  static void loadSettingsOnBoot(InverterLinkState from, InverterLinkState to);
  static void updateAllInverterParam(InverterLinkState from, InverterLinkState to);

  // Fetch shadow/power state live from the inverter and update the in-memory
  // cache. Called independently so only the unknown or invalidated field is
  // re-fetched.
  void fetchAndCacheShadow();
  void fetchAndCachePowerLimit();

  // Called after a successful /home poll to backfill any unknown setting.
  void refreshUnknownSettingsAfterPoll();

  // POST a payload to the inverter's /postoptions form endpoint. Both shadow
  // and power-limit setters share this endpoint; only the payload differs:
  //   power : "enable_mxpower=on\nmaxpower=<W>"
  //   shadow: "enShadow=on" or "enShadow=off"
  // The payload is a newline-separated list of key=value pairs. postOptions()
  // encodes them as multipart/form-data before sending via fetchInverterData().
  bool postOptions(const String& payload, String& responseBody,
                   int& httpCode, String& errorMessage);

  // Called after a successful /home poll to retry any queued setting writes.
  // This is a thin wrapper over the per-field applyPending...() helpers.
  void applyPendingSettings();

  // Retry a queued power-limit write after a successful /home poll.
  // No-ops unless powerLimitDesiredPending_ is set.
  void applyPendingPowerLimit();

  // Retry a queued shadow write after a successful /home poll.
  // No-ops unless shadowDesiredPending_ is set.
  void applyPendingShadow();

  // Helpers to queue a desired value for delayed apply.
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

  // Link-state machine tracking.
  uint32_t failureStartMs = 0;          // millis() when current streak began; 0 if none
  uint32_t currentRetryIntervalMs = WIFI_BRIDGE_POLL_INTERVAL_MS;

  // Cached shadow + power-limit read back from the inverter.
  // *Known = false until first successful read, or after invalidation.
  bool shadowKnown_ = false;
  bool shadowOn_ = false;
  bool powerLimitKnown_ = false;
  uint16_t powerLimitW_ = 0;

  // Desired-state convergence: when an API set call cannot reach the inverter
  // the desired value is queued here. The polling task drains each queued
  // field independently via the corresponding applyPending...() helper on the
  // next successful /home telegram.
  bool          shadowDesiredPending_      = false;
  bool          shadowDesired_             = false;
  bool          powerLimitDesiredPending_  = false;
  uint16_t      powerLimitDesired_         = 0;
};

#endif // INVERTER_CONTROLLER_H
