#include "mdns_responder.h"

#include <Arduino.h>
#include <UIPEthernet.h>
#include "utility/Enc28J60Network.h"

#include "settings.h"
#include "logger.h"

// --- mDNS Announcements ---
// UIPEthernet's uIP stack cannot RECEIVE multicast (drops packets not addressed
// to our IP). But we CAN SEND to the multicast address. So we periodically
// broadcast unsolicited mDNS announcements. Clients cache these and resolve
// "mastervolt-bridge.local" without needing to query us.

static const IPAddress MDNS_MULTICAST_IP(224, 0, 0, 251);
static const uint16_t MDNS_PORT = 5353;
static const uint32_t MDNS_ANNOUNCE_INTERVAL_MS = 60000;  // 60s (TTL=120s)
static const uint32_t MDNS_STARTUP_DELAY_MS = 2000;       // delay between startup announcements
static const uint8_t  MDNS_STARTUP_COUNT = 3;             // rapid announcements at boot

static EthernetUDP mdnsUdp;
static bool mdnsStarted = false;
static unsigned long mdnsLastAnnounceMs = 0;
static uint8_t mdnsStartupAnnouncements = 0;
static unsigned long mdnsAnnouncementsSent = 0;

// --- NBNS (NetBIOS Name Service) ---
// Windows queries via broadcast UDP on port 137. uIP receives broadcast packets
// (dest 255.255.255.255) so we CAN respond to these queries.

static const uint16_t NBNS_PORT = 137;
static EthernetUDP nbnsUdp;
static bool nbnsStarted = false;
static unsigned long nbnsQueriesReceived = 0;
static unsigned long nbnsResponsesSent = 0;

// Debug stats
static unsigned long lastStatsLogMs = 0;

// --- Helper: build DNS name label ---
static int writeDnsLabel(uint8_t* buf, const char* label) {
  int len = strlen(label);
  buf[0] = (uint8_t)len;
  memcpy(buf + 1, label, len);
  return len + 1;
}

// --- Helper: build mDNS A-record announcement packet ---
static int buildMdnsAnnouncement(uint8_t* buf, IPAddress ip) {
  int rLen = 0;

  // DNS Header
  buf[rLen++] = 0x00; buf[rLen++] = 0x00;  // Transaction ID (0 for mDNS)
  buf[rLen++] = 0x84; buf[rLen++] = 0x00;  // Flags: response, authoritative
  buf[rLen++] = 0x00; buf[rLen++] = 0x00;  // Questions: 0
  buf[rLen++] = 0x00; buf[rLen++] = 0x01;  // Answers: 1
  buf[rLen++] = 0x00; buf[rLen++] = 0x00;  // Authority: 0
  buf[rLen++] = 0x00; buf[rLen++] = 0x00;  // Additional: 0

  // Answer: name
  rLen += writeDnsLabel(buf + rLen, MDNS_HOSTNAME);
  rLen += writeDnsLabel(buf + rLen, "local");
  buf[rLen++] = 0x00;  // name terminator

  // Type: A (1)
  buf[rLen++] = 0x00; buf[rLen++] = 0x01;
  // Class: IN (1) with cache-flush bit set
  buf[rLen++] = 0x80; buf[rLen++] = 0x01;
  // TTL: 120 seconds
  buf[rLen++] = 0x00; buf[rLen++] = 0x00;
  buf[rLen++] = 0x00; buf[rLen++] = 0x78;
  // Data length: 4 (IPv4)
  buf[rLen++] = 0x00; buf[rLen++] = 0x04;
  // IP address
  buf[rLen++] = ip[0];
  buf[rLen++] = ip[1];
  buf[rLen++] = ip[2];
  buf[rLen++] = ip[3];

  return rLen;
}

// --- Helper: encode NetBIOS name (first-level encoding) ---
// NetBIOS names are 16 bytes padded with spaces, then each byte is split into
// two nibbles + 'A' to make a 32-byte encoded name.
static void encodeNetbiosName(const char* name, uint8_t* encoded) {
  uint8_t raw[16];
  memset(raw, 0x20, 16);  // pad with spaces
  int len = strlen(name);
  if (len > 15) len = 15;
  for (int i = 0; i < len; i++) {
    raw[i] = toupper((unsigned char)name[i]);
  }
  // 16th byte = 0x00 for workstation service
  raw[15] = 0x00;

  for (int i = 0; i < 16; i++) {
    encoded[i * 2]     = 'A' + ((raw[i] >> 4) & 0x0F);
    encoded[i * 2 + 1] = 'A' + (raw[i] & 0x0F);
  }
}

// --- Helper: check if NBNS query matches our name ---
static bool nbnsNameMatches(const uint8_t* packet, int offset, int len) {
  // NBNS name format: <0x20><32 encoded bytes><0x00>
  if (offset + 34 > len) return false;
  if (packet[offset] != 0x20) return false;  // label length must be 32

  uint8_t ourEncoded[32];
  encodeNetbiosName(NBNS_NAME, ourEncoded);

  // Compare (case-insensitive already handled by encoding)
  return memcmp(packet + offset + 1, ourEncoded, 32) == 0;
}

// ===== Public API =====

void mdnsBegin() {
  if (mdnsStarted) return;

  // Enable multicast frame reception on ENC28J60 (needed for sending multicast)
  Enc28J60Network::enableMulticast();

  if (mdnsUdp.begin(MDNS_PORT)) {
    mdnsStarted = true;
    appLogger.log(String("[MDNS] Announcement mode active, hostname=") + MDNS_HOSTNAME + ".local");
  } else {
    appLogger.log("[MDNS] ERROR: Failed to bind UDP port 5353");
  }

  // Start NBNS responder
  if (nbnsUdp.begin(NBNS_PORT)) {
    nbnsStarted = true;
    appLogger.log(String("[NBNS] Listening on UDP port 137, name=") + NBNS_NAME);
  } else {
    appLogger.log("[NBNS] ERROR: Failed to bind UDP port 137");
  }
}

void mdnsProcess() {
  unsigned long now = millis();

  // --- Periodic stats log every 60s ---
  if (now - lastStatsLogMs > 60000) {
    lastStatsLogMs = now;
    appLogger.log(String("[DISCOVERY] mDNS announcements=") + String(mdnsAnnouncementsSent) +
                  " NBNS queries=" + String(nbnsQueriesReceived) +
                  " responses=" + String(nbnsResponsesSent));
  }

  // --- mDNS unsolicited announcements ---
  if (mdnsStarted) {
    bool shouldAnnounce = false;

    if (mdnsStartupAnnouncements < MDNS_STARTUP_COUNT) {
      // Rapid startup announcements
      if (now - mdnsLastAnnounceMs >= MDNS_STARTUP_DELAY_MS) {
        shouldAnnounce = true;
        mdnsStartupAnnouncements++;
      }
    } else {
      // Periodic steady-state announcements
      if (now - mdnsLastAnnounceMs >= MDNS_ANNOUNCE_INTERVAL_MS) {
        shouldAnnounce = true;
      }
    }

    if (shouldAnnounce) {
      IPAddress myIP = Ethernet.localIP();
      if (myIP[0] != 0) {  // Only announce if we have an IP
        uint8_t pkt[128];
        int pktLen = buildMdnsAnnouncement(pkt, myIP);

        if (mdnsUdp.beginPacket(MDNS_MULTICAST_IP, MDNS_PORT) == 1) {
          mdnsUdp.write(pkt, pktLen);
          if (mdnsUdp.endPacket() == 1) {
            mdnsAnnouncementsSent++;
            mdnsLastAnnounceMs = now;
            if (mdnsAnnouncementsSent <= 3) {
              appLogger.log(String("[MDNS] Announced ") + MDNS_HOSTNAME +
                            ".local -> " + myIP.toString());
            }
          } else {
            appLogger.log("[MDNS] endPacket failed");
            mdnsLastAnnounceMs = now;  // avoid retry flood
          }
        } else {
          appLogger.log("[MDNS] beginPacket failed");
          mdnsLastAnnounceMs = now;
        }
      }
    }
  }

  // --- NBNS query processing ---
  if (nbnsStarted) {
    int packetSize = nbnsUdp.parsePacket();
    if (packetSize > 0) {
      uint8_t packet[256];
      int len = nbnsUdp.read(packet, sizeof(packet));

      // NBNS header: 12 bytes minimum
      if (len >= 12) {
        uint16_t txnId = (packet[0] << 8) | packet[1];
        uint16_t flags = (packet[2] << 8) | packet[3];
        uint16_t qdCount = (packet[4] << 8) | packet[5];

        // Check: is this a query (opcode=0, QR=0)?
        bool isQuery = (flags & 0x8000) == 0;
        uint8_t opcode = (flags >> 11) & 0x0F;

        if (isQuery && opcode == 0 && qdCount > 0) {
          nbnsQueriesReceived++;

          // Question starts at offset 12
          if (nbnsNameMatches(packet, 12, len)) {
            // Build NBNS response
            IPAddress myIP = Ethernet.localIP();
            IPAddress remoteIP = nbnsUdp.remoteIP();
            uint16_t remotePort = nbnsUdp.remotePort();

            uint8_t resp[62];
            int rLen = 0;

            // Header
            resp[rLen++] = packet[0]; resp[rLen++] = packet[1];  // Transaction ID (echo)
            resp[rLen++] = 0x85; resp[rLen++] = 0x00;  // Flags: response, authoritative, recursion desired
            resp[rLen++] = 0x00; resp[rLen++] = 0x00;  // Questions: 0
            resp[rLen++] = 0x00; resp[rLen++] = 0x01;  // Answers: 1
            resp[rLen++] = 0x00; resp[rLen++] = 0x00;  // Authority: 0
            resp[rLen++] = 0x00; resp[rLen++] = 0x00;  // Additional: 0

            // Answer name (same format as query)
            resp[rLen++] = 0x20;  // label length = 32
            uint8_t ourEncoded[32];
            encodeNetbiosName(NBNS_NAME, ourEncoded);
            memcpy(resp + rLen, ourEncoded, 32);
            rLen += 32;
            resp[rLen++] = 0x00;  // name terminator

            // Type: NB (0x0020)
            resp[rLen++] = 0x00; resp[rLen++] = 0x20;
            // Class: IN (0x0001)
            resp[rLen++] = 0x00; resp[rLen++] = 0x01;
            // TTL: 300 seconds (5 min)
            resp[rLen++] = 0x00; resp[rLen++] = 0x00;
            resp[rLen++] = 0x01; resp[rLen++] = 0x2C;
            // Data length: 6 (2 flags + 4 IP)
            resp[rLen++] = 0x00; resp[rLen++] = 0x06;
            // NB flags: B-node, unique
            resp[rLen++] = 0x00; resp[rLen++] = 0x00;
            // IP address
            resp[rLen++] = myIP[0];
            resp[rLen++] = myIP[1];
            resp[rLen++] = myIP[2];
            resp[rLen++] = myIP[3];

            // Send response back to querier
            if (nbnsUdp.beginPacket(remoteIP, remotePort) == 1) {
              nbnsUdp.write(resp, rLen);
              if (nbnsUdp.endPacket() == 1) {
                nbnsResponsesSent++;
                if (nbnsResponsesSent <= 3) {
                  appLogger.log(String("[NBNS] Responded to ") + remoteIP.toString() +
                                " -> " + myIP.toString());
                }
              }
            }
          }
        }
      }
    }
  }
}
