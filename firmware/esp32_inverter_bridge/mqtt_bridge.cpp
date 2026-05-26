#include "mqtt_bridge.h"

#include <UIPEthernet.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <Preferences.h>

#include "settings.h"
#include "logger.h"
#include "inverter_monitor.h"
#include "inverter_data.h"
#include "wifi_bridge.h"

namespace {
EthernetClient mqttEthClient;
PubSubClient mqttClient(mqttEthClient);

// Topic prefixes (built from MQTT_DEVICE_ID)
const String STATE_TOPIC = String("mastervolt/") + MQTT_DEVICE_ID + "/state";
const String STATE_POWER_LIMIT_TOPIC = String("mastervolt/") + MQTT_DEVICE_ID + "/state/power_limit";
const String STATE_SHADOW_TOPIC = String("mastervolt/") + MQTT_DEVICE_ID + "/state/shadow";
const String AVAIL_TOPIC = String("mastervolt/") + MQTT_DEVICE_ID + "/availability";
const String CMD_POWER_TOPIC = String("mastervolt/") + MQTT_DEVICE_ID + "/cmd/power";
const String CMD_POLLING_TOPIC = String("mastervolt/") + MQTT_DEVICE_ID + "/cmd/polling";
const String CMD_SHADOW_TOPIC = String("mastervolt/") + MQTT_DEVICE_ID + "/cmd/shadow";

const char* NVS_NAMESPACE = "mqtt";
const char* NVS_KEY_BROKER = "broker_ip";
const char* NVS_KEY_HA_EN = "ha_en";

// Per-host probe socket (reused for stored-broker check and /24 scan)
EthernetClient probeClient;

bool isZeroIp(const IPAddress& ip) {
  return ip[0] == 0 && ip[1] == 0 && ip[2] == 0 && ip[3] == 0;
}

// Helper: build HA discovery topic for a given component/object_id
String discoveryTopic(const char* component, const char* objectId) {
  return String(MQTT_DISCOVERY_PREFIX) + "/" + component + "/" + MQTT_DEVICE_ID + "/" + objectId + "/config";
}

// Helper: device JSON block (shared across all discovery messages)
String deviceJson() {
  String mac = String(ETH_MAC[0], HEX) + ":" + String(ETH_MAC[1], HEX) + ":" +
               String(ETH_MAC[2], HEX) + ":" + String(ETH_MAC[3], HEX) + ":" +
               String(ETH_MAC[4], HEX) + ":" + String(ETH_MAC[5], HEX);
  return String("\"dev\":{\"ids\":[\"") + MQTT_DEVICE_ID + "\"]," +
         "\"name\":\"Mastervolt SOLADIN 1500\"," +
         "\"mf\":\"Mastervolt\"," +
         "\"mdl\":\"SOLADIN 1500\"," +
         "\"sw\":\"" + FIRMWARE_VERSION + "\"," +
         "\"connections\":[[\"mac\",\"" + mac + "\"]]}";
}

String availabilityJson() {
  return String("\"avty_t\":\"") + AVAIL_TOPIC + "\",\"pl_avail\":\"online\",\"pl_not_avail\":\"offline\"";
}

}  // namespace

MqttBridge::MqttBridge() : brokerIp_(0, 0, 0, 0) {}

void MqttBridge::messageCallback(char* topic, byte* payload, unsigned int length) {
  MqttBridge::getInstance().onMessage(topic, payload, length);
}

void MqttBridge::initialize() {
  Preferences prefs;
  prefs.begin(NVS_NAMESPACE, true);
  uint32_t stored = prefs.getUInt(NVS_KEY_BROKER, 0);
  haEnabled_ = prefs.getBool(NVS_KEY_HA_EN, HA_MQTT_ENABLED_DEFAULT);
  prefs.end();

  if (stored != 0) {
    brokerIp_ = IPAddress(stored);
    configured_ = true;
  }

  appLogger.log(String("[MQTT] init ha_enabled=") + (haEnabled_ ? "true" : "false") +
                " broker=" + (configured_ ? brokerIp_.toString() : String("<unset>")));

  mqttClient.setCallback(messageCallback);
  mqttClient.setBufferSize(512);
  mqttClient.setSocketTimeout(1);  // keep blocking calls short
}

void MqttBridge::persistBrokerIp(const IPAddress& ip) {
  Preferences prefs;
  prefs.begin(NVS_NAMESPACE, false);
  prefs.putUInt(NVS_KEY_BROKER, (uint32_t)ip);
  prefs.end();
}

bool MqttBridge::setBrokerIp(const IPAddress& ip) {
  persistBrokerIp(ip);
  brokerIp_ = ip;
  configured_ = !isZeroIp(ip);
  discoveryPublished_ = false;

  if (mqttClient.connected()) {
    mqttClient.disconnect();
  }

  if (configured_) {
    appLogger.log(String("[MQTT] Broker set to ") + ip.toString());
  } else {
    appLogger.log("[MQTT] Broker cleared");
  }
  return true;
}

void MqttBridge::setHaEnabled(bool enabled) {
  if (haEnabled_ == enabled) return;
  haEnabled_ = enabled;

  Preferences prefs;
  prefs.begin(NVS_NAMESPACE, false);
  prefs.putBool(NVS_KEY_HA_EN, enabled);
  prefs.end();

  appLogger.log(String("[MQTT] HA integration ") + (enabled ? "ENABLED" : "DISABLED"));

  if (!enabled) {
    if (mqttClient.connected()) {
      publishAvailability(false);
      mqttClient.disconnect();
    }
    scanState_ = ScanState::Idle;
  } else if (Ethernet.linkStatus() == LinkON) {
    // Re-probe / scan immediately so the user gets fast feedback.
    onIpAcquired();
  }
}

IPAddress MqttBridge::getBrokerIp() const {
  return brokerIp_;
}

bool MqttBridge::isConnected() const {
  return mqttClient.connected();
}

void MqttBridge::loop() {
  if (!haEnabled_) return;

  // If a scan is active, service one step per loop iteration.
  if (scanState_ != ScanState::Idle) {
    serviceScan();
    return;  // don't attempt MQTT connect during scan
  }

  if (!configured_) return;  // no broker known, nothing to do until /api/mqtt or next IP event

  if (!mqttClient.connected()) {
    unsigned long now = millis();
    if (now - lastConnectAttemptMs_ >= MQTT_RECONNECT_INTERVAL_MS) {
      lastConnectAttemptMs_ = now;
      connect();
    }
    return;
  }

  mqttClient.loop();
  // Note: state publishing is driven by InverterMonitor on every successful
  // poll (see inverter_monitor.cpp). No periodic publish here on purpose -
  // we only post when fresh data arrives from the inverter.
}

bool MqttBridge::probeBroker(const IPAddress& ip) {
  if (isZeroIp(ip)) return false;
  // PubSubClient::setSocketTimeout above caps this to ~1s. We do a raw TCP
  // connect here rather than a full MQTT CONNECT to keep the per-host cost low.
  bool ok = probeClient.connect(ip, MQTT_PORT);
  if (ok) probeClient.stop();
  return ok;
}

void MqttBridge::onIpAcquired() {
  if (!haEnabled_) return;

  scanStartedMs_ = millis();
  if (configured_) {
    appLogger.log(String("[MQTT] IP acquired; probing stored broker ") + brokerIp_.toString());
    scanState_ = ScanState::ProbeStored;
  } else {
    appLogger.log("[MQTT] IP acquired; no broker stored, starting /24 scan");
    scanState_ = ScanState::ScanRunning;
    scanHostOctet_ = 1;
  }
}

void MqttBridge::serviceScan() {
  IPAddress myIp = Ethernet.localIP();
  if (isZeroIp(myIp)) {
    // No link yet; abort scan, will be restarted on next IP event.
    scanState_ = ScanState::Idle;
    return;
  }

  if (scanState_ == ScanState::ProbeStored) {
    if (probeBroker(brokerIp_)) {
      appLogger.log("[MQTT] Stored broker reachable; keeping it.");
      scanState_ = ScanState::Idle;
      lastConnectAttemptMs_ = 0;  // trigger immediate MQTT connect
      return;
    }
    appLogger.log("[MQTT] Stored broker unreachable; starting /24 scan");
    scanState_ = ScanState::ScanRunning;
    scanHostOctet_ = 1;
    return;
  }

  // ScanRunning: probe a single host per call so we don't starve the API server.
  // Skip our own IP and network/broadcast (.0 / .255 handled by loop bounds).
  while (scanHostOctet_ <= 254 && scanHostOctet_ == myIp[3]) {
    scanHostOctet_++;
  }
  if (scanHostOctet_ > 254) {
    unsigned long elapsed = millis() - scanStartedMs_;
    appLogger.log(String("[MQTT] /24 scan complete in ") + elapsed + "ms - no broker found");
    // No broker discovered: clear stored IP per spec ("make the IP false").
    brokerIp_ = IPAddress(0, 0, 0, 0);
    configured_ = false;
    persistBrokerIp(brokerIp_);
    scanState_ = ScanState::Idle;
    return;
  }

  IPAddress candidate(myIp[0], myIp[1], myIp[2], scanHostOctet_);
  if (probeBroker(candidate)) {
    unsigned long elapsed = millis() - scanStartedMs_;
    appLogger.log(String("[MQTT] Broker discovered at ") + candidate.toString() +
                  " after " + elapsed + "ms scan");
    brokerIp_ = candidate;
    configured_ = true;
    discoveryPublished_ = false;
    persistBrokerIp(brokerIp_);
    scanState_ = ScanState::Idle;
    lastConnectAttemptMs_ = 0;  // trigger immediate MQTT connect
    return;
  }
  scanHostOctet_++;
}

void MqttBridge::connect() {
  mqttClient.setServer(brokerIp_, MQTT_PORT);

  String willTopic = AVAIL_TOPIC;
  bool connected = mqttClient.connect(
    MQTT_CLIENT_ID,
    willTopic.c_str(),  // LWT topic
    0,                  // QoS
    true,               // retain
    "offline"           // LWT payload
  );

  if (connected) {
    appLogger.log("[MQTT] Connected to broker");
    publishAvailability(true);

    if (!discoveryPublished_) {
      publishDiscovery();
      discoveryPublished_ = true;
    }

    // Subscribe to command topics
    mqttClient.subscribe(CMD_POWER_TOPIC.c_str());
    mqttClient.subscribe(CMD_POLLING_TOPIC.c_str());
    mqttClient.subscribe(CMD_SHADOW_TOPIC.c_str());

    // Publish initial state + cached shadow/power_limit mirrors so HA picks
    // up the bridge's view of these write-only inverter settings immediately
    // (cache + dual-topic pattern).
    publishState();
    publishPowerLimit();
    publishShadow();
  } else {
    appLogger.log(String("[MQTT] Connect failed, rc=") + mqttClient.state());
  }
}

void MqttBridge::publishAvailability(bool online) {
  mqttClient.publish(AVAIL_TOPIC.c_str(), online ? "online" : "offline", true);
}

void MqttBridge::publishPowerLimit() {
  if (!haEnabled_ || !configured_ || !mqttClient.connected()) return;
  uint16_t w;
  if (!InverterMonitor::getInstance().getCachedPowerLimit(w)) {
    // Cache empty (no set since boot, no NVS record) - leave HA value as 'unknown'
    // until the first successful setPower.
    return;
  }
  mqttClient.publish(STATE_POWER_LIMIT_TOPIC.c_str(), String(w).c_str(), true);
}

void MqttBridge::publishShadow() {
  if (!haEnabled_ || !configured_ || !mqttClient.connected()) return;
  bool on;
  if (!InverterMonitor::getInstance().getCachedShadow(on)) {
    return;
  }
  mqttClient.publish(STATE_SHADOW_TOPIC.c_str(), on ? "ON" : "OFF", true);
}

void MqttBridge::publishState() {
  if (!haEnabled_ || !configured_ || !mqttClient.connected()) return;

  InverterMonitor& mon = InverterMonitor::getInstance();
  HomeData data;
  bool valid = mon.getLatestHomeData(data);

  String payload;
  if (valid) {
    payload = String("{\"power\":") + data.instantaneousPower +
              ",\"total_yield\":" + data.lifetimeEnergy +
              ",\"daily_yield\":" + data.dailySessionEnergy +
              ",\"status\":" + data.operatingStatus +
              ",\"wifi_connected\":" + (WiFi.status() == WL_CONNECTED ? "true" : "false") +
              "}";
  } else {
    payload = "{\"power\":null,\"total_yield\":null,\"daily_yield\":null,\"status\":null,\"wifi_connected\":false}";
  }

  mqttClient.publish(STATE_TOPIC.c_str(), payload.c_str(), true);
}

void MqttBridge::publishDiscovery() {
  String dev = deviceJson();
  String avty = availabilityJson();
  String stTopic = STATE_TOPIC;

  // --- Sensor: Power ---
  {
    String topic = discoveryTopic("sensor", "power");
    String cfg = String("{\"name\":\"Power\",\"uniq_id\":\"") + MQTT_DEVICE_ID + "_power\"," +
                 "\"stat_t\":\"" + stTopic + "\"," +
                 "\"val_tpl\":\"{{value_json.power}}\"," +
                 "\"dev_cla\":\"power\",\"stat_cla\":\"measurement\",\"unit_of_meas\":\"W\"," +
                 avty + "," + dev + "}";
    mqttClient.publish(topic.c_str(), cfg.c_str(), true);
  }

  // --- Sensor: Total Yield ---
  {
    String topic = discoveryTopic("sensor", "total_yield");
    String cfg = String("{\"name\":\"Total Yield\",\"uniq_id\":\"") + MQTT_DEVICE_ID + "_total_yield\"," +
                 "\"stat_t\":\"" + stTopic + "\"," +
                 "\"val_tpl\":\"{{value_json.total_yield}}\"," +
                 "\"dev_cla\":\"energy\",\"stat_cla\":\"total_increasing\",\"unit_of_meas\":\"kWh\"," +
                 avty + "," + dev + "}";
    mqttClient.publish(topic.c_str(), cfg.c_str(), true);
  }

  // --- Sensor: Daily Yield ---
  {
    String topic = discoveryTopic("sensor", "daily_yield");
    String cfg = String("{\"name\":\"Daily Yield\",\"uniq_id\":\"") + MQTT_DEVICE_ID + "_daily_yield\"," +
                 "\"stat_t\":\"" + stTopic + "\"," +
                 "\"val_tpl\":\"{{value_json.daily_yield}}\"," +
                 "\"dev_cla\":\"energy\",\"stat_cla\":\"total_increasing\",\"unit_of_meas\":\"kWh\"," +
                 avty + "," + dev + "}";
    mqttClient.publish(topic.c_str(), cfg.c_str(), true);
  }

  // --- Binary Sensor: WiFi Connected ---
  {
    String topic = discoveryTopic("binary_sensor", "wifi_connected");
    String cfg = String("{\"name\":\"Inverter WiFi\",\"uniq_id\":\"") + MQTT_DEVICE_ID + "_wifi\"," +
                 "\"stat_t\":\"" + stTopic + "\"," +
                 "\"val_tpl\":\"{{value_json.wifi_connected}}\"," +
                 "\"dev_cla\":\"connectivity\",\"pl_on\":\"true\",\"pl_off\":\"false\"," +
                 avty + "," + dev + "}";
    mqttClient.publish(topic.c_str(), cfg.c_str(), true);
  }

  // --- Number: Power Limit ---
  // Dual-topic: cmd_t for writes, stat_t for the bridge's cached mirror so HA
  // shows the value the bridge believes the inverter is set to (the inverter
  // does not report power_limit via /home).
  {
    String topic = discoveryTopic("number", "power_limit");
    String cfg = String("{\"name\":\"Power Limit\",\"uniq_id\":\"") + MQTT_DEVICE_ID + "_power_limit\"," +
                 "\"cmd_t\":\"" + CMD_POWER_TOPIC + "\"," +
                 "\"stat_t\":\"" + STATE_POWER_LIMIT_TOPIC + "\"," +
                 "\"min\":0,\"max\":" + INVERTER_MAX_POWER_WATTS + ",\"step\":25," +
                 "\"unit_of_meas\":\"W\",\"icon\":\"mdi:solar-power\"," +
                 avty + "," + dev + "}";
    mqttClient.publish(topic.c_str(), cfg.c_str(), true);
  }

  // --- Number: Polling Interval ---
  {
    String topic = discoveryTopic("number", "polling_interval");
    String cfg = String("{\"name\":\"Polling Interval\",\"uniq_id\":\"") + MQTT_DEVICE_ID + "_polling\"," +
                 "\"cmd_t\":\"" + CMD_POLLING_TOPIC + "\"," +
                 "\"min\":1,\"max\":300,\"step\":1," +
                 "\"unit_of_meas\":\"s\",\"icon\":\"mdi:timer-outline\"," +
                 avty + "," + dev + "}";
    mqttClient.publish(topic.c_str(), cfg.c_str(), true);
  }

  // --- Switch: Shadow Function ---
  // Dual-topic: cmd_t for writes, stat_t for the cached mirror.
  {
    String topic = discoveryTopic("switch", "shadow");
    String cfg = String("{\"name\":\"Shadow Function\",\"uniq_id\":\"") + MQTT_DEVICE_ID + "_shadow\"," +
                 "\"cmd_t\":\"" + CMD_SHADOW_TOPIC + "\"," +
                 "\"stat_t\":\"" + STATE_SHADOW_TOPIC + "\"," +
                 "\"pl_on\":\"ON\",\"pl_off\":\"OFF\"," +
                 "\"stat_on\":\"ON\",\"stat_off\":\"OFF\"," +
                 "\"icon\":\"mdi:weather-partly-cloudy\"," +
                 avty + "," + dev + "}";
    mqttClient.publish(topic.c_str(), cfg.c_str(), true);
  }

  // --- Button: Wake Pulse ---
  {
    String topic = discoveryTopic("button", "wake_pulse");
    String cmdTopic = String("mastervolt/") + MQTT_DEVICE_ID + "/cmd/pulse";
    String cfg = String("{\"name\":\"Wake Pulse\",\"uniq_id\":\"") + MQTT_DEVICE_ID + "_pulse\"," +
                 "\"cmd_t\":\"" + cmdTopic + "\"," +
                 "\"icon\":\"mdi:pulse\"," +
                 avty + "," + dev + "}";
    mqttClient.publish(topic.c_str(), cfg.c_str(), true);
    mqttClient.subscribe(cmdTopic.c_str());
  }

  appLogger.log("[MQTT] HA discovery published (7 entities)");
}

void MqttBridge::onMessage(const char* topic, byte* payload, unsigned int length) {
  String topicStr(topic);
  String value;
  value.reserve(length);
  for (unsigned int i = 0; i < length; i++) {
    value += (char)payload[i];
  }

  appLogger.log(String("[MQTT] Received: ") + topicStr + " = " + value);

  if (topicStr == CMD_POWER_TOPIC) {
    int watts = value.toInt();
    if (watts >= 0 && watts <= INVERTER_MAX_POWER_WATTS) {
      String resp, err;
      int code = 0;
      InverterMonitor::getInstance().setPower(watts, resp, code, err);
    }
  } else if (topicStr == CMD_POLLING_TOPIC) {
    int seconds = value.toInt();
    if (seconds >= 1 && seconds <= 3600) {
      // TODO: wire to InverterMonitor::setPollingInterval when available
      appLogger.log(String("[MQTT] Polling interval requested: ") + seconds + "s");
    }
  } else if (topicStr == CMD_SHADOW_TOPIC) {
    bool enabled = (value == "ON" || value == "on" || value == "true" || value == "1");
    String resp, err;
    int code = 0;
    InverterMonitor::getInstance().setShadow(enabled, resp, code, err);
  } else if (topicStr.endsWith("/cmd/pulse")) {
    WifiConnectionManager::getInstance().forceReconnect();
  }
}
