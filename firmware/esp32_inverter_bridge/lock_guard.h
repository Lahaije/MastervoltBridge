#ifndef LOCK_GUARD_H
#define LOCK_GUARD_H

#include <Arduino.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#include "logger.h"

// ---------------------------------------------------------------------------
// Lock hierarchy.
//
// Every mutex in the firmware is tagged with a `LockRank`. The `ScopedLock<R>`
// RAII guard enforces, at runtime per task, that locks are acquired in
// ascending rank order and never recursively. Violations are logged loudly
// via `appLogger` (the guard still proceeds with acquisition so we don't
// introduce new failure modes); see docs/LOCKING_MODEL.md.
//
// To add a new mutex:
//   1. Add a value to `LockRank` in strictly ascending acquisition order.
//   2. Wrap acquisitions in `ScopedLock<LockRank::YOUR_RANK> lk(handle, ms, "name")`.
//   3. Update docs/LOCKING_MODEL.md.
// ---------------------------------------------------------------------------

enum class LockRank : uint8_t {
  POLLING_CONFIG = 0,  // pollingConfigMutex   (inverter_monitor.cpp)
  DATA           = 1,  // dataMutex            (inverter_monitor.cpp)
  POWER_STATE    = 2,  // powerStateMutex      (inverter_monitor.cpp)
  WIFI_OPERATION = 3,  // wifiOperationMutex   (wifi_bridge.cpp)
  // Highest rank wins: WiFi ops are held across HTTP and may legitimately
  // be acquired while business-state locks are NOT held.
};

namespace lock_hierarchy_detail {

// Returns the currently-held rank bitmap for the calling task.
uint8_t peekHeldRanks();

// Marks `r` as held by the calling task.
void recordHeld(LockRank r);

// Clears `r` from the calling task's held set.
void recordReleased(LockRank r);

}  // namespace lock_hierarchy_detail

template <LockRank R>
class ScopedLock {
public:
  ScopedLock(SemaphoreHandle_t mutex, uint32_t timeoutMs, const char* name = "")
      : mutex_(mutex), name_(name), acquired_(false) {
    // Check hierarchy BEFORE blocking on the mutex. This both surfaces the
    // bug in logs and avoids self-deadlock if the violation would have
    // re-acquired the same non-recursive mutex on the same task.
    const uint8_t held = lock_hierarchy_detail::peekHeldRanks();
    const uint8_t rankBit = static_cast<uint8_t>(1u << static_cast<uint8_t>(R));
    // Forbidden = any bit already held with rank >= R (i.e. equal or higher).
    const uint8_t forbidden = held & ~static_cast<uint8_t>(rankBit - 1u);
    if (forbidden != 0) {
      appLogger.log(String("[LOCK-HIERARCHY] VIOLATION: acquiring ") + name_ +
                    " (rank " + static_cast<int>(R) +
                    ") while held bitmap=0x" + String(held, HEX) +
                    " by task " + pcTaskGetName(nullptr));
    }

    if (mutex_ == nullptr) return;
    if (xSemaphoreTake(mutex_, pdMS_TO_TICKS(timeoutMs)) != pdTRUE) return;
    acquired_ = true;
    lock_hierarchy_detail::recordHeld(R);
  }

  ~ScopedLock() {
    if (acquired_) {
      lock_hierarchy_detail::recordReleased(R);
      xSemaphoreGive(mutex_);
    }
  }

  ScopedLock(const ScopedLock&) = delete;
  ScopedLock& operator=(const ScopedLock&) = delete;

  bool acquired() const { return acquired_; }

private:
  SemaphoreHandle_t mutex_;
  const char* name_;
  bool acquired_;
};

// Assert (log-only) that the calling task does NOT currently hold any of the
// business-state locks. Use at entry of code that performs network I/O or
// other potentially blocking calls; pairs with rule "do not hold any state
// lock while doing HTTP".
inline void assertNoStateLocksHeld(const char* callsite) {
  using namespace lock_hierarchy_detail;
  const uint8_t held = peekHeldRanks();
  constexpr uint8_t kStateMask =
      static_cast<uint8_t>(1u << static_cast<uint8_t>(LockRank::POLLING_CONFIG)) |
      static_cast<uint8_t>(1u << static_cast<uint8_t>(LockRank::DATA)) |
      static_cast<uint8_t>(1u << static_cast<uint8_t>(LockRank::POWER_STATE));
  if ((held & kStateMask) != 0) {
    appLogger.log(String("[LOCK-HIERARCHY] VIOLATION: state lock held at ") +
                  callsite + " (bitmap=0x" + String(held, HEX) +
                  ", task=" + pcTaskGetName(nullptr) + ")");
  }
}

#endif  // LOCK_GUARD_H
