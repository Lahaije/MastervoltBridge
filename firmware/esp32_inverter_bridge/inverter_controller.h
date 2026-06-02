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
 * Polls the inverter, caches the latest HomeData, and applies control requests.
 */
class InverterController {
public:
  static InverterController& getInstance() {
    static InverterController instance;
    return instance;
  }

  /** Initialize the controller and start the polling task. */
  void initialize();

  /** Stop the polling task. */
  void shutdown();

  /** Return the latest cached HomeData snapshot. */
  bool getLatestHomeData(HomeData& dataOut);

  /** Return the timestamp of the last successful poll, or 0 if none. */
  unsigned long getLastUpdateMs();

  /** Result of setPower / setShadow. */
  enum class SetResult { Applied, Deferred, Rejected };

  /** Set the inverter power limit. */
  SetResult setPower(int watts, String& responseBody, int& httpCode, String& errorMessage);

  /** Enable or disable the inverter shadow function. */
  SetResult setShadow(bool enabled, String& responseBody, int& httpCode, String& errorMessage);

  /** Fetch a specific path from the inverter. */
  bool fetchPath(const String& path, String& responseBody, int& httpCode, String& errorMessage);

  /** Current link state. */
  InverterLinkState getLinkState();

  /** Return the current failure streak in milliseconds. */
  uint32_t getFailureStreakMs();

  /** Return the current poll interval in milliseconds. */
  uint32_t getRetryIntervalMs();

  /** Override the current poll interval at runtime. */
  void setPollIntervalMs(uint32_t ms);

  /** Return the cached shadow function state if known. */
  bool getShadow(bool& enabledOut);

  /** Return the cached inverter power limit if known. */
  bool getPowerLimit(uint16_t& wattsOut);

  /** Return true if a deferred shadow or power-limit update is queued. */
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
  void linkStateFromStreak(uint32_t streakMs);

  // Increment a counter under dataMutex.
  bool incrementCounterLocked(uint32_t& counter);

  // State hook for poll-interval control.
  static void updatePollFrequency(InverterLinkState from, InverterLinkState to);

  // State-dependent hooks.
  static void loadSettingsOnBoot(InverterLinkState from, InverterLinkState to);
  static void updateAllInverterParam(InverterLinkState from, InverterLinkState to);

  // Refresh the in-memory shadow/power cache.
  void fetchAndCacheShadow();
  void fetchAndCachePowerLimit();

  // Backfill unknown settings after a successful /home poll.
  void refreshUnknownSettingsAfterPoll();

  // POST a payload to the inverter's /postoptions form endpoint.
  bool postOptions(const String& payload, String& responseBody,
                   int& httpCode, String& errorMessage);

  // Retry queued setting writes after a successful /home poll.
  void applyPendingSettings();

  // Retry a queued power-limit write after a successful /home poll.
  void applyPendingPowerLimit();

  // Retry a queued shadow write after a successful /home poll.
  void applyPendingShadow();

  // Queue a desired value for delayed apply.
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
  uint32_t failureStartMs = 0;
  uint32_t currentRetryIntervalMs = WIFI_BRIDGE_POLL_INTERVAL_MS;

  // Cached shadow and power-limit values.
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
