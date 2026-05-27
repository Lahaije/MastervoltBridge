#ifndef INVERTER_LINK_STATE_H
#define INVERTER_LINK_STATE_H

#include <Arduino.h>

// =============================================================================
// InverterLinkState — five-state FSM driven by poll success / failure streak.
//
//  State      Poll interval      Entry condition
//  ---------  -----------------  --------------------------------------------
//  STARTING   base (15 s)        Boot; no successful poll has occurred yet
//  ONLINE     base (15 s)        Last poll succeeded
//  RETRYING   base (15 s)        Failure streak  < LINK_RETRYING_TO_BACKOFF_MS
//  BACKOFF    LINK_BACKOFF (1 m) Failure streak  5 – 20 min
//  DORMANT    LINK_DORMANT (10m) Failure streak >= LINK_BACKOFF_TO_DORMANT_MS
//
// ---- Transitions and the actions they trigger --------------------------------
//
//  From       To         Poll interval change?   Action
//  --------   --------   ---------------------   ----------------------------
//  STARTING   ONLINE     (stays at base)         fetchAndCacheSettings()
//  ONLINE     RETRYING   (stays at base)         —
//  RETRYING   ONLINE     (stays at base)         —  (quick recovery; settings still valid)
//  RETRYING   BACKOFF    base  -> 1 min          —  (interval change only)
//  BACKOFF    ONLINE     1 min -> base           fetchAndCacheSettings()
//  BACKOFF    DORMANT    1 min -> 10 min         —  (interval change only)
//  DORMANT    ONLINE     10 min -> base          fetchAndCacheSettings()
//
// The poll-interval update is applied unconditionally by intervalForState()
// on every state change. Side-effect actions (settings refresh, etc.) are
// dispatched by InverterMonitor::onLinkStateTransition().
// =============================================================================

enum class InverterLinkState : uint8_t {
  STARTING = 0,  // boot — no successful poll yet
  ONLINE,        // last poll OK; polling at base interval
  RETRYING,      // recent failure, streak < 5 min; still at base interval
  BACKOFF,       // streak 5–20 min; polling every 1 min
  DORMANT,       // streak >= 20 min; polling every 10 min (e.g. overnight)
};

// ---------------------------------------------------------------------------
// Failure-streak thresholds that trigger state transitions
// ---------------------------------------------------------------------------
static constexpr uint32_t LINK_RETRYING_TO_BACKOFF_MS =  5u * 60u * 1000u;  //  5 min
static constexpr uint32_t LINK_BACKOFF_TO_DORMANT_MS  = 20u * 60u * 1000u;  // 20 min

// ---------------------------------------------------------------------------
// Per-state polling intervals (base interval supplied by caller for ONLINE/RETRYING/STARTING)
// ---------------------------------------------------------------------------
static constexpr uint32_t LINK_BACKOFF_INTERVAL_MS =  60u * 1000u;   //  1 min
static constexpr uint32_t LINK_DORMANT_INTERVAL_MS = 600u * 1000u;   // 10 min

// ---------------------------------------------------------------------------
// Free functions
// ---------------------------------------------------------------------------

/** Human-readable name of a state (e.g. "ONLINE"). Never returns nullptr. */
const char* toString(InverterLinkState s);

// Forward declaration to avoid circular include (inverter_monitor.h includes this file)
class InverterMonitor;

// ---------------------------------------------------------------------------
// State getter and setter — global state management
// ---------------------------------------------------------------------------

/** Get the current inverter link state. */
InverterLinkState getInverterState();

/** Set the inverter link state. Centralizes all state mutations. */
void setInverterState(InverterLinkState newState);

// ---------------------------------------------------------------------------
// State-change hooks — extensible callbacks for transitions.
// Hooks allow external code to react to state changes without modifying
// inverter_monitor.cpp. Each hook is called when transitioning from->to.
// ---------------------------------------------------------------------------

/** Hook function signature: called on matching state transition. */
typedef void (*StateChangeHook)(InverterLinkState from, InverterLinkState to, uint32_t streakMs);

/** Register a hook for a specific transition. Returns false if no more slots. */
bool registerStateChangeHook(InverterLinkState from, InverterLinkState to, StateChangeHook callback);

/** Dispatch all registered hooks for a transition. Called from inverter_monitor.cpp. */
void dispatchStateChangeHooks(InverterLinkState from, InverterLinkState to, uint32_t streakMs);

#endif  // INVERTER_LINK_STATE_H
