#include "lock_guard.h"

namespace lock_hierarchy_detail {

namespace {

// Small per-task tracker. The firmware has ~4 long-lived tasks (Arduino main,
// API server, polling, WiFi connection worker) plus occasional system tasks.
// Slots are claimed on first use and freed when a task drops its last lock,
// keeping the table small and lookup O(N).
constexpr size_t MAX_TRACKED_TASKS = 8;

struct TaskRanks {
  TaskHandle_t task;
  uint8_t held;
};

TaskRanks gTable[MAX_TRACKED_TASKS] = {};
portMUX_TYPE gTableMux = portMUX_INITIALIZER_UNLOCKED;

// Caller must hold gTableMux.
TaskRanks* findSlot(TaskHandle_t t) {
  for (size_t i = 0; i < MAX_TRACKED_TASKS; ++i) {
    if (gTable[i].task == t) return &gTable[i];
  }
  return nullptr;
}

// Caller must hold gTableMux.
TaskRanks* findOrInsertSlot(TaskHandle_t t) {
  if (auto* s = findSlot(t)) return s;
  for (size_t i = 0; i < MAX_TRACKED_TASKS; ++i) {
    if (gTable[i].task == nullptr) {
      gTable[i].task = t;
      gTable[i].held = 0;
      return &gTable[i];
    }
  }
  return nullptr;  // table full; tracking silently disabled for this task
}

}  // namespace

uint8_t peekHeldRanks() {
  TaskHandle_t t = xTaskGetCurrentTaskHandle();
  uint8_t held = 0;
  portENTER_CRITICAL(&gTableMux);
  if (auto* s = findSlot(t)) held = s->held;
  portEXIT_CRITICAL(&gTableMux);
  return held;
}

void recordHeld(LockRank r) {
  TaskHandle_t t = xTaskGetCurrentTaskHandle();
  const uint8_t bit = static_cast<uint8_t>(1u << static_cast<uint8_t>(r));
  portENTER_CRITICAL(&gTableMux);
  if (auto* s = findOrInsertSlot(t)) s->held |= bit;
  portEXIT_CRITICAL(&gTableMux);
}

void recordReleased(LockRank r) {
  TaskHandle_t t = xTaskGetCurrentTaskHandle();
  const uint8_t bit = static_cast<uint8_t>(1u << static_cast<uint8_t>(r));
  portENTER_CRITICAL(&gTableMux);
  if (auto* s = findSlot(t)) {
    s->held &= ~bit;
    if (s->held == 0) s->task = nullptr;  // free slot for another task
  }
  portEXIT_CRITICAL(&gTableMux);
}

}  // namespace lock_hierarchy_detail
