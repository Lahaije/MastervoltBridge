#include <Arduino.h>
#include "settings.h"
#include "logger.h"
#include "wifi_bridge.h"
#include "inverter_monitor.h"
#include "ethernet_bridge.h"


void setup() {

  // Keep WiFi and Ethernet stack separated: WiFi for inverter, ENC28J60 for HA network.
  ethernetBridgeInit();
  wifiBridgeInit();
  
  // Initialize inverter monitor for inverter telemetry and requests
  InverterMonitor::getInstance().initialize();
}

void loop() {
  // Delay-based pacing for simpler and predictable responsiveness.
  delay(MAIN_LOOP_SLEEP_MS);
}
