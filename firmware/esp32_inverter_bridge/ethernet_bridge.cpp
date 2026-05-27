#include "ethernet_bridge.h"

#include <Arduino.h>
#include <SPI.h>
#include <UIPEthernet.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "settings.h"
#include "logger.h"
#include "api.h"
#include "mqtt_bridge.h"

namespace {
TaskHandle_t ethernetTaskHandle = nullptr;
bool apiServerStarted = false;
bool prevLinkUp = false;

void initEthernetHardware() {
  // ESP32 requires explicit SPI pin assignment before UIPEthernet initialises.
  SPI.begin(PIN_ETH_SCK, PIN_ETH_MISO, PIN_ETH_MOSI, PIN_ETH_CS);
  Ethernet.init(PIN_ETH_CS);
  appLogger.log("[ETH] ENC28J60 hardware initialized. Waiting for cable...");
}

bool tryAcquireDhcp() {
  appLogger.log("[ETH] Cable detected. Attempting DHCP...");
  int dhcpOk = Ethernet.begin(ETH_MAC);
  
  // If DHCP fails, return false to caller.
  if (dhcpOk == 0) {
    appLogger.log("[ETH] DHCP failed.");
    return false;
  }
  String msg = String("[ETH] DHCP OK. IP=") + Ethernet.localIP().toString();
  appLogger.log(msg);
  return true;
}

void ethernetBridgeTask(void* param) {
  (void)param;

  // Worker task: runs continuously and checks Ethernet state on a fixed schedule.
  appLogger.log("ESP32 inverter bridge booting...");
  initEthernetHardware();

  // Liveness recovery state. Used to recover from two UIPEthernet failure
  // modes that the overnight run hit:
  //   1. ENC28J60 silently wedges and keeps reporting LinkOFF.
  //   2. Link reports up but RX is dead / DHCP lease silently lost.
  uint32_t lastLinkUpMs = millis();
  uint32_t lastApiClientMs = millis();

  // Main Ethernet service loop.
  while (true) {
    bool linkUp = (Ethernet.linkStatus() == LinkON);

    // If link is down: log disconnect edge once and wait before retry.
    if (!linkUp) {
      if (prevLinkUp) {
        appLogger.log("[ETH] Cable disconnected.");
        apiServerStarted = false;
      }
      prevLinkUp = false;
      // Recovery: if the link has stayed down for too long, re-init the
      // ENC28J60. Handles the case where the chip is wedged and pretends
      // LinkOFF even though the cable is plugged in.
      if (millis() - lastLinkUpMs >= ETHERNET_NO_LINK_RECOVERY_MS) {
        appLogger.log("[ETH] No link for too long; re-initialising ENC28J60.");
        initEthernetHardware();
        lastLinkUpMs = millis();
        lastApiClientMs = millis();
      }
      vTaskDelay(pdMS_TO_TICKS(ETHERNET_INIT_RETRY_MS));
      continue;
    }

    prevLinkUp = true;
    lastLinkUpMs = millis();

    // Link is up and API server not started yet: acquire DHCP and start API listener.
    if (!apiServerStarted) {
      if (!tryAcquireDhcp()) {
        // Retry is scheduled here by waiting, then looping back to call tryAcquireDhcp() again.
        vTaskDelay(pdMS_TO_TICKS(ETHERNET_INIT_RETRY_MS));
        continue;
      }
      apiServer.begin();
      String apiMsg = String("[API] Listening on Ethernet port ") + String(API_PORT) +
                      String(" (IP=") + Ethernet.localIP().toString() + String(")");
      appLogger.log(apiMsg);
      apiServerStarted = true;

      // Initialize MQTT after Ethernet is ready and trigger broker probe/scan.
      MqttBridge& mqtt = MqttBridge::getInstance();
      mqtt.initialize();
      mqtt.onIpAcquired();
    }

    int maintainCode = Ethernet.maintain();
    // DHCP renew/rebind status reporting.
    if (maintainCode == 1 || maintainCode == 3) {
      appLogger.log(String("[ETH] DHCP lease renewed. IP=") + Ethernet.localIP().toString());
      MqttBridge::getInstance().onIpAcquired();
    } else if (maintainCode == 2 || maintainCode == 4) {
      appLogger.log(String("[ETH] DHCP lease rebind. IP=") + Ethernet.localIP().toString());
      MqttBridge::getInstance().onIpAcquired();
    }

    // Process all available API clients without delay between them.
    while (true) {
      EthernetClient client = apiServer.available();
      if (!client) break;
      handleApiClient(client);
      client.stop();
      lastApiClientMs = millis();
    }

    // Service MQTT connection (connect/reconnect, publish, receive)
    MqttBridge::getInstance().loop();

    // Recovery: link claims up but nothing is reaching us. The ENC28J60 RX
    // path may be wedged or the DHCP lease may have been quietly revoked by
    // the router. Force a hardware re-init + fresh DHCP.
    if (apiServerStarted &&
        millis() - lastApiClientMs >= ETHERNET_NO_ACTIVITY_RECOVERY_MS) {
      appLogger.log("[ETH] No API activity for too long; forcing re-init.");
      initEthernetHardware();
      apiServerStarted = false;
      prevLinkUp = false;
      lastLinkUpMs = millis();
      lastApiClientMs = millis();
    }

    vTaskDelay(pdMS_TO_TICKS(ETHERNET_SERVICE_INTERVAL_MS));
  }
}
}

void ethernetBridgeInit() {
  // If task already exists, do nothing.
  if (ethernetTaskHandle != nullptr) {
    return;
  }

  xTaskCreatePinnedToCore(
    ethernetBridgeTask,
    "ethernet_bridge",
    6144,
    nullptr,
    1,
    &ethernetTaskHandle,
    1
  );
}
