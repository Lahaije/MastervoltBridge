#ifndef INVERTER_LINK_STATE_H
#define INVERTER_LINK_STATE_H

#include <Arduino.h>

// =============================================================================
// InverterLinkState — five-state FSM driven by poll success / failure streak.
//
//  State      Poll interval           Entry condition
//  ---------  ----------------------  --------------------------------------------
//  STARTING   default (20 s)          Boot; no successful poll has occurred yet
//  ONLINE     current / default       Last poll succeeded
//  RETRYING   current                 Failure streak  < LINK_RETRYING_TO_BACKOFF_MS
//  BACKOFF    LINK_BACKOFF (1 m)      Failure streak  5 – 20 min
//  DORMANT    LINK_DORMANT (10 m)     Failure streak >= LINK_BACKOFF_TO_DORMANT_MS
//
// ---- Transitions and the actions they trigger --------------------------------
//
//  From       To         Poll interval change?            Action
//  --------   --------   -------------------------------   ----------------------------
//  STARTING   ONLINE     preserves current/default        fetchAndCacheSettings()
//  ONLINE     RETRYING   preserves current                —
//  RETRYING   ONLINE     preserves current                —  (quick recovery; settings still valid)
//  RETRYING   BACKOFF    current -> 1 min                —  (interval change only)
//  BACKOFF    ONLINE     1 min -> default                fetchAndCacheSettings()
//  BACKOFF    DORMANT    1 min -> 10 min                 —  (interval change only)
//  DORMANT    ONLINE     10 min -> default               fetchAndCacheSettings()
//
// The poll interval is initialized once at boot, can be temporarily overridden
// by the API, and is then overwritten only by the state transitions listed
// above. Side-effect actions (settings refresh, etc.) are dispatched by
// InverterController transition hooks.
// =============================================================================

enum class InverterLinkState : uint8_t {
  STARTING = 0,  // boot — no successful poll yet
  ONLINE,        // last poll OK; polling at current/default interval
  RETRYING,      // recent failure, streak < 5 min; preserves current interval
  BACKOFF,       // streak 5–20 min; polling every 1 min
  DORMANT,       // streak >= 20 min; polling every 10 min (e.g. overnight)
};

// ---------------------------------------------------------------------------
// Free functions
// ---------------------------------------------------------------------------

/** Human-readable name of a state (e.g. "ONLINE"). Never returns nullptr. */
const char* toString(InverterLinkState s);

// Forward declaration to avoid circular include (inverter_controller.h includes this file)
class InverterController;

// ---------------------------------------------------------------------------
// State getter and setter — global state management
// ---------------------------------------------------------------------------

/** Get the current inverter link state. */
InverterLinkState getInverterState();

/** Set the inverter link state and dispatch hooks if the state changed. */
void setInverterState(InverterLinkState newState);

// ---------------------------------------------------------------------------
// State-change hooks — extensible callbacks for transitions.
// Two hook types are supported:
//
//  1. Transition hook  — fires only on a specific from -> to pair.
//     Register with registerStateChangeHook(from, to, cb).
//
//  2. Entry hook       — fires whenever ANY transition enters a target state,
//                        regardless of where it came from.
//     Register with registerStateEntryHook(to, cb).
//
// Both types share the same callback signature. All registered hooks for a
// given transition are dispatched by dispatchStateChangeHooks().
// ---------------------------------------------------------------------------

/** Shared callback signature for both hook types. */
typedef void (*StateChangeHook)(InverterLinkState from, InverterLinkState to);

/** Register a hook for a specific from -> to transition. Returns false if no slots remain. */
bool registerStateChangeHook(InverterLinkState from,
                             InverterLinkState to,
                             StateChangeHook callback,
                             const char* hookName = nullptr);

/** Register a hook that fires on ANY transition into targetState. Returns false if no slots remain. */
bool registerStateEntryHook(InverterLinkState targetState,
                            StateChangeHook callback,
                            const char* hookName = nullptr);

/** Dispatch all matching hooks for a transition. Called from inverter_controller.cpp. */
void dispatchStateChangeHooks(InverterLinkState from, InverterLinkState to);

// Compile-time helpers: function name is stringized at compile time.
#define REGISTER_STATE_CHANGE_HOOK(fromState, toState, hookFn) \
  registerStateChangeHook((fromState), (toState), (hookFn), #hookFn)

#define REGISTER_STATE_ENTRY_HOOK(targetState, hookFn) \
  registerStateEntryHook((targetState), (hookFn), #hookFn)

#endif  // INVERTER_LINK_STATE_H
