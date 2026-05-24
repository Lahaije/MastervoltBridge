#include "mdns_responder.h"

#include <Arduino.h>
#include <UIPEthernet.h>

#include "settings.h"
#include "logger.h"

// mDNS constants
static const IPAddress MDNS_MULTICAST_IP(224, 0, 0, 251);
static const uint16_t MDNS_PORT = 5353;

// Hostname is defined in settings.h (MDNS_HOSTNAME)

static EthernetUDP mdnsUdp;
static bool mdnsStarted = false;

// Debug counters
static unsigned long mdnsPacketsReceived = 0;
static unsigned long mdnsQueriesForUs = 0;
static unsigned long mdnsResponsesSent = 0;
static unsigned long mdnsLastLogMs = 0;

// Build a DNS name label from a C-string (length-prefixed format).
// Returns number of bytes written.
static int writeDnsLabel(uint8_t* buf, const char* label) {
  int len = strlen(label);
  buf[0] = (uint8_t)len;
  memcpy(buf + 1, label, len);
  return len + 1;
}

// Extract the queried name as a readable string for debug logging
static String extractQueryName(const uint8_t* packet, int offset, int packetLen) {
  String name;
  while (offset < packetLen && packet[offset] != 0) {
    uint8_t labelLen = packet[offset];
    if (offset + 1 + labelLen > packetLen) break;
    if (name.length() > 0) name += ".";
    for (int i = 0; i < labelLen; i++) {
      name += (char)packet[offset + 1 + i];
    }
    offset += 1 + labelLen;
  }
  return name;
}

// Check if a DNS query name matches our hostname.local
static bool matchesOurName(const uint8_t* packet, int offset, int packetLen) {
  // Expected: <len>mastervolt-bridge<len>local<0>
  int hostnameLen = strlen(MDNS_HOSTNAME);

  // Check bounds
  if (offset + 1 + hostnameLen + 1 + 5 + 1 > packetLen) return false;

  // Check hostname label
  if (packet[offset] != hostnameLen) return false;
  if (memcmp(packet + offset + 1, MDNS_HOSTNAME, hostnameLen) != 0) return false;
  offset += 1 + hostnameLen;

  // Check "local" label
  if (packet[offset] != 5) return false;
  if (memcmp(packet + offset + 1, "local", 5) != 0) return false;
  offset += 6;

  // Check null terminator
  if (packet[offset] != 0) return false;

  return true;
}

void mdnsBegin() {
  if (mdnsStarted) return;

  // UIPEthernet (ENC28J60) does not have a public enableMulticast() API.
  // We rely on the network switch flooding multicast to all ports (common for
  // link-local multicast 224.0.0.x) or the ENC28J60 passing frames through.
  if (mdnsUdp.begin(MDNS_PORT)) {
    mdnsStarted = true;
    appLogger.log(String("[MDNS] Listening on UDP port 5353, hostname=") + MDNS_HOSTNAME + ".local");
  } else {
    appLogger.log("[MDNS] ERROR: Failed to bind UDP port 5353");
  }
}

void mdnsProcess() {
  if (!mdnsStarted) return;

  // Periodic status log every 60s for debugging
  unsigned long now = millis();
  if (now - mdnsLastLogMs > 60000) {
    mdnsLastLogMs = now;
    appLogger.log(String("[MDNS] Stats: pkts=") + String(mdnsPacketsReceived) +
                  " queries_for_us=" + String(mdnsQueriesForUs) +
                  " responses=" + String(mdnsResponsesSent));
  }

  int packetSize = mdnsUdp.parsePacket();
  if (packetSize <= 0) return;

  mdnsPacketsReceived++;

  // Read the packet (mDNS packets are small, typically < 512 bytes)
  uint8_t packet[512];
  int len = mdnsUdp.read(packet, sizeof(packet));

  if (len < 12) {
    appLogger.log(String("[MDNS] Pkt too short: ") + String(len) + " bytes");
    return;
  }

  // Parse DNS header
  uint16_t flags = (packet[2] << 8) | packet[3];
  bool isQuery = (flags & 0x8000) == 0;

  if (!isQuery) return;  // Ignore responses

  uint16_t qdCount = (packet[4] << 8) | packet[5];
  if (qdCount == 0) return;

  // Log first few received queries for debugging
  if (mdnsPacketsReceived <= 5) {
    String qname = extractQueryName(packet, 12, len);
    appLogger.log(String("[MDNS] Query #") + String(mdnsPacketsReceived) +
                  ": name=" + qname + " len=" + String(len));
  }

  // Parse the first question (offset 12 = after header)
  int qOffset = 12;

  if (!matchesOurName(packet, qOffset, len)) return;

  // Skip past the name to get QTYPE and QCLASS
  int nameEnd = qOffset;
  while (nameEnd < len && packet[nameEnd] != 0) {
    nameEnd += 1 + packet[nameEnd];
  }
  nameEnd++;  // skip null terminator

  if (nameEnd + 4 > len) return;

  uint16_t qtype = (packet[nameEnd] << 8) | packet[nameEnd + 1];

  // Only respond to A record queries (type 1) or ANY (type 255)
  if (qtype != 1 && qtype != 255) {
    appLogger.log(String("[MDNS] Matched name but qtype=") + String(qtype) + " (ignoring)");
    return;
  }

  mdnsQueriesForUs++;
  IPAddress myIP = Ethernet.localIP();
  appLogger.log(String("[MDNS] Responding to query for ") + MDNS_HOSTNAME +
                ".local -> " + myIP.toString());

  // Build mDNS response
  uint8_t response[256];
  int rLen = 0;

  // DNS Header
  response[rLen++] = 0x00; response[rLen++] = 0x00;  // Transaction ID (0 for mDNS)
  response[rLen++] = 0x84; response[rLen++] = 0x00;  // Flags: response, authoritative
  response[rLen++] = 0x00; response[rLen++] = 0x00;  // Questions: 0
  response[rLen++] = 0x00; response[rLen++] = 0x01;  // Answers: 1
  response[rLen++] = 0x00; response[rLen++] = 0x00;  // Authority: 0
  response[rLen++] = 0x00; response[rLen++] = 0x00;  // Additional: 0

  // Answer: name
  rLen += writeDnsLabel(response + rLen, MDNS_HOSTNAME);
  rLen += writeDnsLabel(response + rLen, "local");
  response[rLen++] = 0x00;  // name terminator

  // Type: A (1)
  response[rLen++] = 0x00; response[rLen++] = 0x01;
  // Class: IN (1) with cache-flush bit set
  response[rLen++] = 0x80; response[rLen++] = 0x01;
  // TTL: 120 seconds
  response[rLen++] = 0x00; response[rLen++] = 0x00;
  response[rLen++] = 0x00; response[rLen++] = 0x78;
  // Data length: 4 (IPv4)
  response[rLen++] = 0x00; response[rLen++] = 0x04;
  // IP address
  response[rLen++] = myIP[0];
  response[rLen++] = myIP[1];
  response[rLen++] = myIP[2];
  response[rLen++] = myIP[3];

  // Send response to multicast address
  int result = mdnsUdp.beginPacket(MDNS_MULTICAST_IP, MDNS_PORT);
  if (result != 1) {
    appLogger.log(String("[MDNS] beginPacket failed: ") + String(result));
    return;
  }
  mdnsUdp.write(response, rLen);
  result = mdnsUdp.endPacket();
  if (result != 1) {
    appLogger.log(String("[MDNS] endPacket failed: ") + String(result));
    return;
  }

  mdnsResponsesSent++;
}
