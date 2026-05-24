#ifndef MDNS_RESPONDER_H
#define MDNS_RESPONDER_H

/**
 * Lightweight mDNS responder for UIPEthernet (ENC28J60).
 *
 * Responds to A-record queries for the configured hostname (.local)
 * so the bridge can be discovered as "mastervolt-bridge.local" on the LAN.
 *
 * Call mdnsBegin() after Ethernet has an IP, and mdnsProcess() from the
 * Ethernet service loop.
 */

void mdnsBegin();
void mdnsProcess();

#endif
