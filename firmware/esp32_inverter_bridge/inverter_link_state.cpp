#include "inverter_link_state.h"

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
