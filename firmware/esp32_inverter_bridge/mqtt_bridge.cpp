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
const char* NVS_KEY_USER = "user";
const char* NVS_KEY_PASS = "pass";

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
#if MQTT_BRIDGE_DISABLED
  // Compile-time kill switch. UIPEthernet has shown corruption when MQTT
  // operations (probe scan, frequent connect retries) run alongside
  // Ethernet.maintain() and the API server. Bisecting suspected MQTT as the
  // root cause of the ENC28J60 wedge that left the bridge unreachable
  // overnight. Keep MQTT compiled out until the bridge has been verified
  // stable on LAN; then re-enable in phases.
  haEnabled_ = false;
  configured_ = false;
  appLogger.log("[MQTT] compiled out (MQTT_BRIDGE_DISABLED=1); skipping init");
  return;
#endif
  Preferences prefs;
  prefs.begin(NVS_NAMESPACE, true);
  uint32_t stored = prefs.getUInt(NVS_KEY_BROKER, 0);
  haEnabled_ = prefs.getBool(NVS_KEY_HA_EN, HA_MQTT_ENABLED_DEFAULT);
  user_ = prefs.getString(NVS_KEY_USER, "");
  password_ = prefs.getString(NVS_KEY_PASS, "");
  prefs.end();

  if (stored != 0) {
    brokerIp_ = IPAddress(stored);
    configured_ = true;
  }

  appLogger.log(String("[MQTT] init ha_enabled=") + (haEnabled_ ? "true" : "false") +
                " broker=" + (configured_ ? brokerIp_.toString() : String("<unset>")) +
                " user=" + (user_.length() ? user_ : String("<none>")) +
                " pass_len=" + password_.length());

  mqttClient.setCallback(messageCallback);
  // Buffer must hold the largest single discovery payload + topic.
  // power_limit discovery JSON is ~510 bytes; topic ~60 bytes; with MQTT header overhead
  // we need >=600. Use 1024 for safety and to allow future fields.
  mqttClient.setBufferSize(1024);
  mqttClient.setSocketTimeout(5);  // seconds; 1s was too aggressive and starved I/O
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
#if MQTT_BRIDGE_DISABLED
  if (enabled) {
    appLogger.log("[MQTT] setHaEnabled(true) ignored - MQTT compiled out");
    return;
  }
#endif
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

void MqttBridge::setCredentials(const String& user, const String& password) {
  user_ = user;
  password_ = password;

  Preferences prefs;
  prefs.begin(NVS_NAMESPACE, false);
  prefs.putString(NVS_KEY_USER, user);
  prefs.putString(NVS_KEY_PASS, password);
  prefs.end();

  appLogger.log(String("[MQTT] credentials updated user=") +
                (user.length() ? user : String("<none>")) +
                " pass_len=" + password.length());

  // Drop any active session so the new creds are used on the next reconnect.
  if (mqttClient.connected()) {
    mqttClient.disconnect();
  }
  lastConnectAttemptMs_ = 0;  // trigger immediate retry
}

void MqttBridge::setUser(const String& user) {
  user_ = user;
  Preferences prefs;
  prefs.begin(NVS_NAMESPACE, false);
  prefs.putString(NVS_KEY_USER, user);
  prefs.end();
  appLogger.log(String("[MQTT] user updated user=") +
                (user.length() ? user : String("<none>")));
  if (mqttClient.connected()) {
    mqttClient.disconnect();
  }
  lastConnectAttemptMs_ = 0;
}

void MqttBridge::setPassword(const String& password) {
  password_ = password;
  Preferences prefs;
  prefs.begin(NVS_NAMESPACE, false);
  prefs.putString(NVS_KEY_PASS, password);
  prefs.end();
  appLogger.log(String("[MQTT] password updated pass_len=") + password.length());
  if (mqttClient.connected()) {
    mqttClient.disconnect();
  }
  lastConnectAttemptMs_ = 0;
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
  if (!haEnabled_) {
    appLogger.log("[MQTT] IP acquired; HA disabled, no scan");
    return;
  }

  scanStartedMs_ = millis();
  storedProbeAttempts_ = 0;
  if (configured_) {
    // Skip the TCP probe entirely for stored brokers. probeClient.stop() was
    // observed to corrupt UIPEthernet's socket table and crash the next
    // Ethernet.maintain() (StoreProhibited in UIPEthernetClass::tick()).
    // Let mqttClient.connect() itself act as the reachability test; its
    // retry cadence is governed by MQTT_RECONNECT_INTERVAL_MS.
    appLogger.log(String("[MQTT] IP acquired; will connect directly to stored broker ") +
                  brokerIp_.toString());
    scanState_ = ScanState::Idle;
    lastConnectAttemptMs_ = 0;  // trigger immediate MQTT connect
  } else {
    appLogger.log("[MQTT] IP acquired; HA enabled but no broker stored - starting /24 scan");
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
    storedProbeAttempts_++;
    appLogger.log(String("[MQTT] Probe attempt ") + storedProbeAttempts_ + "/5 to stored broker " +
                  brokerIp_.toString());
    if (probeBroker(brokerIp_)) {
      appLogger.log(String("[MQTT] Stored broker reachable on attempt ") + storedProbeAttempts_ +
                    "; keeping it.");
      scanState_ = ScanState::Idle;
      // Force a short cool-down before the real MQTT CONNECT so UIPEthernet
      // can fully release the probe socket (closing it requires processing
      // a few RX packets). Otherwise the subsequent connect races with the
      // FIN_WAIT/TIME_WAIT cleanup and can crash PubSubClient.
      lastConnectAttemptMs_ = millis() - (MQTT_RECONNECT_INTERVAL_MS - 500);  // ~500ms cool-down
      return;
    }
    // First TCP connect after a fresh DHCP lease often fails (ARP not warm,
    // UIPEthernet quirks). Retry several times before falling back to /24 scan.
    if (storedProbeAttempts_ < 5) {
      appLogger.log(String("[MQTT] Stored broker probe failed (attempt ") + storedProbeAttempts_ +
                    "/5); will retry next loop iteration");
      return;  // stay in ProbeStored state
    }
    appLogger.log("[MQTT] Stored broker unreachable after 5 attempts; starting /24 scan");
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
  // Per-iteration progress log so scan is clearly visible in /api/logs and serial.
  {
    unsigned long elapsed = millis() - scanStartedMs_;
    appLogger.log(String("[MQTT] /24 scan: probing ") + candidate.toString() +
                  " (" + scanHostOctet_ + "/254, " + elapsed + "ms elapsed)");
  }
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
  appLogger.log(String("[MQTT] connect() -> ") + brokerIp_.toString() + ":" + MQTT_PORT +
                " user=" + (user_.length() ? user_ : String("<anon>")));
  mqttClient.setServer(brokerIp_, MQTT_PORT);

  String willTopic = AVAIL_TOPIC;
  const char* userPtr = user_.length() ? user_.c_str() : nullptr;
  const char* passPtr = (user_.length() && password_.length()) ? password_.c_str() : nullptr;
  bool connected = mqttClient.connect(
    MQTT_CLIENT_ID,
    userPtr,            // username (nullptr => anonymous)
    passPtr,            // password
    willTopic.c_str(),  // LWT topic
    0,                  // QoS
    true,               // retain
    "offline"           // LWT payload
  );
  appLogger.log(String("[MQTT] connect() returned connected=") + (connected ? "true" : "false") +
                " state=" + mqttClient.state());

  if (connected) {
    appLogger.log("[MQTT] Connected to broker");
    publishAvailability(true);
    appLogger.log("[MQTT] availability sent");

    if (!discoveryPublished_) {
      publishDiscovery();
      discoveryPublished_ = true;
      appLogger.log("[MQTT] discovery sent");
    }

    // Subscribe to command topics
    mqttClient.subscribe(CMD_POWER_TOPIC.c_str());
    mqttClient.subscribe(CMD_POLLING_TOPIC.c_str());
    mqttClient.subscribe(CMD_SHADOW_TOPIC.c_str());
    appLogger.log("[MQTT] subscriptions sent");

    // Publish initial state + cached shadow/power_limit mirrors so HA picks
    // up the bridge's view of these write-only inverter settings immediately
    // (cache + dual-topic pattern).
    publishState();
    publishPowerLimit();
    publishShadow();
    appLogger.log("[MQTT] initial publishes done");
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
  // Helper lambda: publish + log + return ok flag. Bail on first failure so a
  // crash trace clearly identifies the offending entity.
  auto pub = [&](const char* label, const String& topic, const String& cfg) {
    appLogger.log(String("[MQTT] discovery ") + label + " topic_len=" + topic.length() +
                  " cfg_len=" + cfg.length());
    bool ok = mqttClient.publish(topic.c_str(), cfg.c_str(), true);
    if (!ok) {
      appLogger.log(String("[MQTT] discovery ") + label + " PUBLISH FAILED (buffer too small?)");
    }
    return ok;
  };

  // --- Sensor: Power ---
  {
    String topic = discoveryTopic("sensor", "power");
    String cfg = String("{\"name\":\"Power\",\"uniq_id\":\"") + MQTT_DEVICE_ID + "_power\"," +
                 "\"stat_t\":\"" + stTopic + "\"," +
                 "\"val_tpl\":\"{{value_json.power}}\"," +
                 "\"dev_cla\":\"power\",\"stat_cla\":\"measurement\",\"unit_of_meas\":\"W\"," +
                 avty + "," + dev + "}";
    if (!pub("power", topic, cfg)) return;
  }

  // --- Sensor: Total Yield ---
  {
    String topic = discoveryTopic("sensor", "total_yield");
    String cfg = String("{\"name\":\"Total Yield\",\"uniq_id\":\"") + MQTT_DEVICE_ID + "_total_yield\"," +
                 "\"stat_t\":\"" + stTopic + "\"," +
                 "\"val_tpl\":\"{{value_json.total_yield}}\"," +
                 "\"dev_cla\":\"energy\",\"stat_cla\":\"total_increasing\",\"unit_of_meas\":\"kWh\"," +
                 avty + "," + dev + "}";
    if (!pub("total_yield", topic, cfg)) return;
  }

  // --- Sensor: Daily Yield ---
  {
    String topic = discoveryTopic("sensor", "daily_yield");
    String cfg = String("{\"name\":\"Daily Yield\",\"uniq_id\":\"") + MQTT_DEVICE_ID + "_daily_yield\"," +
                 "\"stat_t\":\"" + stTopic + "\"," +
                 "\"val_tpl\":\"{{value_json.daily_yield}}\"," +
                 "\"dev_cla\":\"energy\",\"stat_cla\":\"total_increasing\",\"unit_of_meas\":\"kWh\"," +
                 avty + "," + dev + "}";
    if (!pub("daily_yield", topic, cfg)) return;
  }

  // --- Binary Sensor: WiFi Connected ---
  {
    String topic = discoveryTopic("binary_sensor", "wifi_connected");
    String cfg = String("{\"name\":\"Inverter WiFi\",\"uniq_id\":\"") + MQTT_DEVICE_ID + "_wifi\"," +
                 "\"stat_t\":\"" + stTopic + "\"," +
                 "\"val_tpl\":\"{{value_json.wifi_connected}}\"," +
                 "\"dev_cla\":\"connectivity\",\"pl_on\":\"true\",\"pl_off\":\"false\"," +
                 avty + "," + dev + "}";
    if (!pub("wifi_connected", topic, cfg)) return;
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
    if (!pub("power_limit", topic, cfg)) return;
  }

  // --- Number: Polling Interval ---
  {
    String topic = discoveryTopic("number", "polling_interval");
    String cfg = String("{\"name\":\"Polling Interval\",\"uniq_id\":\"") + MQTT_DEVICE_ID + "_polling\"," +
                 "\"cmd_t\":\"" + CMD_POLLING_TOPIC + "\"," +
                 "\"min\":1,\"max\":300,\"step\":1," +
                 "\"unit_of_meas\":\"s\",\"icon\":\"mdi:timer-outline\"," +
                 avty + "," + dev + "}";
    if (!pub("polling_interval", topic, cfg)) return;
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
    if (!pub("shadow", topic, cfg)) return;
  }

  // --- Button: Wake Pulse ---
  {
    String topic = discoveryTopic("button", "wake_pulse");
    String cmdTopic = String("mastervolt/") + MQTT_DEVICE_ID + "/cmd/pulse";
    String cfg = String("{\"name\":\"Wake Pulse\",\"uniq_id\":\"") + MQTT_DEVICE_ID + "_pulse\"," +
                 "\"cmd_t\":\"" + cmdTopic + "\"," +
                 "\"icon\":\"mdi:pulse\"," +
                 avty + "," + dev + "}";
    if (!pub("wake_pulse", topic, cfg)) return;
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
      uint32_t appliedMs = 0;
      String err;
      bool ok = InverterMonitor::getInstance().setPollingIntervalSeconds(
        (uint32_t)seconds, appliedMs, err);
      if (!ok) {
        appLogger.log(String("[MQTT] setPollingIntervalSeconds failed: ") + err);
      }
    } else {
      appLogger.log(String("[MQTT] Polling interval out of range (1..3600): ") + seconds);
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
