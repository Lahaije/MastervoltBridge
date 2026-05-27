#include "inverter_link_state.h"

// Hook registry
namespace {
  constexpr size_t MAX_STATE_CHANGE_HOOKS = 8;

  struct StateChangeBinding {
    InverterLinkState from;
    InverterLinkState to;
    StateChangeHook callback;
  };

  StateChangeBinding hooks[MAX_STATE_CHANGE_HOOKS];
  size_t hookCount = 0;
}  // namespace

InverterLinkState linkStateFromStreak(uint32_t streakMs) {
  if (streakMs == 0)                           return InverterLinkState::ONLINE;
  if (streakMs >= LINK_BACKOFF_TO_DORMANT_MS)  return InverterLinkState::DORMANT;
  if (streakMs >= LINK_RETRYING_TO_BACKOFF_MS) return InverterLinkState::BACKOFF;
  return InverterLinkState::RETRYING;
}

uint32_t intervalForState(InverterLinkState s, uint32_t baseMs) {
  switch (s) {
    case InverterLinkState::BACKOFF:  return LINK_BACKOFF_INTERVAL_MS;
    case InverterLinkState::DORMANT:  return LINK_DORMANT_INTERVAL_MS;
    default:                          return baseMs;
  }
}

const char* toString(InverterLinkState s) {
  switch (s) {
    case InverterLinkState::STARTING: return "STARTING";
    case InverterLinkState::ONLINE:   return "ONLINE";
    case InverterLinkState::RETRYING: return "RETRYING";
    case InverterLinkState::BACKOFF:  return "BACKOFF";
    case InverterLinkState::DORMANT:  return "DORMANT";
  }
  return "UNKNOWN";
}

bool registerStateChangeHook(InverterLinkState from, InverterLinkState to, StateChangeHook callback) {
  if (hookCount >= MAX_STATE_CHANGE_HOOKS) {
    return false;  // No more space in hook registry
  }
  if (!callback) {
    return false;  // Null callback
  }
  hooks[hookCount].from = from;
  hooks[hookCount].to = to;
  hooks[hookCount].callback = callback;
  hookCount++;
  return true;
}

void dispatchStateChangeHooks(InverterLinkState from, InverterLinkState to, uint32_t streakMs) {
  for (size_t i = 0; i < hookCount; i++) {
    if (hooks[i].from == from && hooks[i].to == to && hooks[i].callback) {
      hooks[i].callback(from, to, streakMs);
    }
  }
}
