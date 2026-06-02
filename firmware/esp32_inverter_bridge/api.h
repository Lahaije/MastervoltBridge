#ifndef API_H
#define API_H

#include <UIPEthernet.h>

void handleApiClient(EthernetClient& client);

// Handler functions for individual endpoints
void handleGetRoot(EthernetClient& client);
void handleGetConfig(EthernetClient& client);
void handleGetWebUiCss(EthernetClient& client);
void handleGetWebUiJs(EthernetClient& client);
void handleGetApiDiscovery(EthernetClient& client);
void handleGetHealth(EthernetClient& client);
void handleGetDevice(EthernetClient& client);
void handleGetLogs(EthernetClient& client);
void handleGetPulse(EthernetClient& client);
void handlePostWifiOff(EthernetClient& client);
void handlePostDebug(EthernetClient& client, const String& body);
void handlePostInterval(EthernetClient& client, const String& body);
void handleGetInfo(EthernetClient& client);
void handlePostPower(EthernetClient& client, const String& body);
void handlePostShadow(EthernetClient& client, const String& body);
void handlePostInverterFetch(EthernetClient& client, const String& body);
void handleGetMqttSettings(EthernetClient& client);
void handlePostMqttSettings(EthernetClient& client, const String& body);

#endif // API_H
