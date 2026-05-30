#include "ethernet_bridge.h"

#include <Arduino.h>
#include <SPI.h>
#include <UIPEthernet.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "settings.h"
#include "logger.h"
#include "api.h"

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
      vTaskDelay(pdMS_TO_TICKS(ETHERNET_INIT_RETRY_MS));
      continue;
    }

    prevLinkUp = true;

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
    }

    int maintainCode = Ethernet.maintain();
    // Ethernet.maintain() return codes:
    //   0 = DHCP_CHECK_NONE    — nothing to do
    //   1 = DHCP_CHECK_RENEW_FAIL  — renewal attempted, failed
    //   2 = DHCP_CHECK_RENEW_OK    — renewal succeeded
    //   3 = DHCP_CHECK_REBIND_FAIL — rebind attempted, failed
    //   4 = DHCP_CHECK_REBIND_OK   — rebind succeeded
    if (maintainCode == 1) {
      appLogger.log(String("[ETH] DHCP lease renewal failed. IP=") + Ethernet.localIP().toString());
    } else if (maintainCode == 2) {
      appLogger.log(String("[ETH] DHCP lease renewed. IP=") + Ethernet.localIP().toString());
    } else if (maintainCode == 3) {
      appLogger.log(String("[ETH] DHCP rebind failed. IP=") + Ethernet.localIP().toString());
    } else if (maintainCode == 4) {
      appLogger.log(String("[ETH] DHCP lease rebound. IP=") + Ethernet.localIP().toString());
    }

    // Process all available API clients without delay between them.
    while (true) {
      EthernetClient client = apiServer.available();
      if (!client) break;
      handleApiClient(client);
      client.stop();
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
