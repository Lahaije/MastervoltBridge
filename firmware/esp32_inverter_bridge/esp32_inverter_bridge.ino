#include <Arduino.h>
#include "settings.h"
#include "logger.h"
#include "wifi_bridge.h"
#include "inverter_controller.h"
#include "ethernet_bridge.h"


void setup() {
  appLogger.init();

  // Keep WiFi and Ethernet stack separated: WiFi for inverter, ENC28J60 for HA network.
  ethernetBridgeInit();
  wifiBridgeInit();
  
  // Initialize inverter controller for inverter telemetry and requests
  InverterController::getInstance().initialize();

  // All hardware and tasks are initialized; disable debug-level logging for
  // routine HTTP 200 success events to keep the log buffer clean.
  debugMode = false;
}

void loop() {
  // Delay-based pacing for simpler and predictable responsiveness.
  delay(MAIN_LOOP_SLEEP_MS);
}
