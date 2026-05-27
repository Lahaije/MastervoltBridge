#include "inverter_link_state.h"

#include "logger.h"
#include "settings.h"

// ---------------------------------------------------------------------------
// Global state variable
// ---------------------------------------------------------------------------
static InverterLinkState globalInverterState = InverterLinkState::STARTING;

// ---------------------------------------------------------------------------
// State getter and setter
// ---------------------------------------------------------------------------
InverterLinkState getInverterState() {
  return globalInverterState;
}

void setInverterState(InverterLinkState newState) {
  InverterLinkState prev = globalInverterState;
  if (newState == prev) return;
  globalInverterState = newState;
  dispatchStateChangeHooks(prev, newState);
}

// Hook registries
namespace {
  constexpr size_t MAX_STATE_CHANGE_HOOKS = 8;
  constexpr size_t MAX_STATE_ENTRY_HOOKS  = 8;

  // Type 1: fires only on a specific from -> to transition
  struct StateChangeBinding {
    InverterLinkState from;
    InverterLinkState to;
    StateChangeHook callback;
    const char* hookName;
  };

  // Type 2: fires on any transition into a target state
  struct StateEntryBinding {
    InverterLinkState targetState;
    StateChangeHook callback;
    const char* hookName;
  };

  StateChangeBinding transitionHooks[MAX_STATE_CHANGE_HOOKS];
  size_t transitionHookCount = 0;

  StateEntryBinding entryHooks[MAX_STATE_ENTRY_HOOKS];
  size_t entryHookCount = 0;
}  // namespace

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

bool registerStateChangeHook(InverterLinkState from,
                             InverterLinkState to,
                             StateChangeHook callback,
                             const char* hookName) {
  if (!callback || transitionHookCount >= MAX_STATE_CHANGE_HOOKS) return false;
  transitionHooks[transitionHookCount++] = { from, to, callback, hookName };
  return true;
}

bool registerStateEntryHook(InverterLinkState targetState,
                            StateChangeHook callback,
                            const char* hookName) {
  if (!callback || entryHookCount >= MAX_STATE_ENTRY_HOOKS) return false;
  entryHooks[entryHookCount++] = { targetState, callback, hookName };
  return true;
}

void dispatchStateChangeHooks(InverterLinkState from, InverterLinkState to) {
  // Type 1: exact from -> to match
  for (size_t i = 0; i < transitionHookCount; i++) {
    if (transitionHooks[i].from == from && transitionHooks[i].to == to) {
      if (debugMode) {
        const char* hookName = transitionHooks[i].hookName ? transitionHooks[i].hookName : "<unnamed-transition-hook>";
        appLogger.log(String("[INVERTER-LINK-STATE] Hook fired: ") + hookName +
                      "  transition=" + toString(from) + "->" + toString(to));
      }
      transitionHooks[i].callback(from, to);
    }
  }
  // Type 2: any transition entering targetState
  for (size_t i = 0; i < entryHookCount; i++) {
    if (entryHooks[i].targetState == to) {
      if (debugMode) {
        const char* hookName = entryHooks[i].hookName ? entryHooks[i].hookName : "<unnamed-entry-hook>";
        appLogger.log(String("[INVERTER-LINK-STATE] Hook fired: ") + hookName +
                      "  transition=" + toString(from) + "->" + toString(to));
      }
      entryHooks[i].callback(from, to);
    }
  }
}
