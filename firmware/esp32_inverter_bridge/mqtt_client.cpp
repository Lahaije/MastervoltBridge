#include "mqtt_client.h"

#include <UIPEthernet.h>
#include <PubSubClient.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include "settings.h"
#include "logger.h"
#include "inverter_controller.h"
#include "mqtt_settings.h"

namespace {
EthernetClient mqttEthClient;
PubSubClient mqttPubSub(mqttEthClient);
SemaphoreHandle_t telemetryMutex = nullptr;

// Throttle how often we run MQTT loop in the ethernet service task.
// Running every 2ms is too aggressive and starves the API server.
unsigned long lastMqttLoopMs = 0;
constexpr unsigned long MQTT_LOOP_INTERVAL_MS = 500;  // Only run MQTT every 500ms

bool parseStrictInt(const String& raw, int& out) {
  String value = raw;
  value.trim();
  if (value.length() == 0) return false;

  int start = 0;
  if (value[0] == '+' || value[0] == '-') {
    if (value.length() == 1) return false;
    start = 1;
  }

  for (int i = start; i < value.length(); i++) {
    if (!isDigit(value[i])) return false;
  }

  out = value.toInt();
  return true;
}

// Publish a single HA discovery config message (retained).
void publishSensorDiscovery(PubSubClient& client, const String& prefix,
                            const String& sensorId, const String& name,
                            const String& unit, const String& deviceClass,
                            const String& stateClass, const String& icon,
                            const String& jsonKey) {
  String topic = "homeassistant/sensor/" + prefix + "/" + sensorId + "/config";
  String stateTopic = prefix + "/telemetry/state";
  String availTopic = prefix + "/status";

  String payload = "{";
  payload += "\"name\":\"" + name + "\"";
  payload += ",\"unique_id\":\"" + prefix + "_sensor_" + sensorId + "\"";
  payload += ",\"state_topic\":\"" + stateTopic + "\"";
  payload += ",\"value_template\":\"{{ value_json." + jsonKey + " }}\"";
  payload += ",\"availability_topic\":\"" + availTopic + "\"";
  payload += ",\"payload_available\":\"online\"";
  payload += ",\"payload_not_available\":\"offline\"";
  if (unit.length() > 0) {
    payload += ",\"unit_of_measurement\":\"" + unit + "\"";
  }
  if (deviceClass.length() > 0) {
    payload += ",\"device_class\":\"" + deviceClass + "\"";
  }
  if (stateClass.length() > 0) {
    payload += ",\"state_class\":\"" + stateClass + "\"";
  }
  if (icon.length() > 0) {
    payload += ",\"icon\":\"" + icon + "\"";
  }
  payload += ",\"device\":{";
  payload += "\"identifiers\":[\"" + prefix + "\"]";
  payload += ",\"name\":\"Mastervolt Bridge\"";
  payload += ",\"manufacturer\":\"Mastervolt\"";
  payload += ",\"model\":\"SOLADIN 1500\"";
  payload += ",\"sw_version\":\"";
  payload += FIRMWARE_VERSION;
  payload += "\"}";
  payload += "}";

  client.publish(topic.c_str(), payload.c_str(), true);
}

void publishNumberDiscovery(PubSubClient& client, const String& prefix,
                            const String& numberId, const String& name,
                            const String& unit, int min, int max, int step,
                            const String& jsonKey) {
  String topic = "homeassistant/number/" + prefix + "/" + numberId + "/config";
  String stateTopic = prefix + "/telemetry/state";
  String cmdTopic = prefix + "/number/" + numberId + "/set";
  String availTopic = prefix + "/status";

  String payload = "{";
  payload += "\"name\":\"" + name + "\"";
  payload += ",\"unique_id\":\"" + prefix + "_number_" + numberId + "\"";
  payload += ",\"state_topic\":\"" + stateTopic + "\"";
  payload += ",\"value_template\":\"{{ value_json." + jsonKey + " }}\"";
  payload += ",\"command_topic\":\"" + cmdTopic + "\"";
  payload += ",\"availability_topic\":\"" + availTopic + "\"";
  payload += ",\"payload_available\":\"online\"";
  payload += ",\"payload_not_available\":\"offline\"";
  if (unit.length() > 0) {
    payload += ",\"unit_of_measurement\":\"" + unit + "\"";
  }
  payload += ",\"min\":" + String(min);
  payload += ",\"max\":" + String(max);
  payload += ",\"step\":" + String(step);
  payload += ",\"mode\":\"slider\"";
  payload += ",\"device\":{";
  payload += "\"identifiers\":[\"" + prefix + "\"]";
  payload += ",\"name\":\"Mastervolt Bridge\"";
  payload += ",\"manufacturer\":\"Mastervolt\"";
  payload += ",\"model\":\"SOLADIN 1500\"";
  payload += ",\"sw_version\":\"";
  payload += FIRMWARE_VERSION;
  payload += "\"}";
  payload += "}";

  client.publish(topic.c_str(), payload.c_str(), true);
}

void publishCombinedTelemetry(PubSubClient& client, const String& prefix,
                             const HomeData& data, bool hasData,
                             uint32_t pollIntervalMs,
                             bool powerLimitKnown, uint16_t powerLimitW) {
  String topic = prefix + "/telemetry/state";

  String payload = "{";
  payload += "\"power_w\":";
  payload += (hasData && data.hasPower) ? String(data.instantaneousPowerW, 1) : "null";
  payload += ",\"total_yield_kwh\":";
  payload += (hasData && data.hasLifetimeEnergy) ? String(data.lifetimeEnergyKwh, 3) : "null";
  payload += ",\"daily_yield_kwh\":";
  payload += (hasData && data.hasDailySessionEnergy) ? String(data.dailySessionEnergyKwh, 3) : "null";
  payload += ",\"poll_interval_s\":" + String(pollIntervalMs / 1000);
  payload += ",\"power_limit_w\":";
  payload += powerLimitKnown ? String(powerLimitW) : "null";
  payload += "}";

  client.publish(topic.c_str(), payload.c_str());
}
}  // namespace

MqttClient::MqttClient() {
}

void MqttClient::initialize() {
  if (initialized_) return;

  if (telemetryMutex == nullptr) {
    telemetryMutex = xSemaphoreCreateMutex();
  }

  settings_ = loadMqttSettings();

  if (!settings_.enabled) {
    appLogger.log("[MQTT] Disabled in settings");
    initialized_ = true;
    return;
  }

  IPAddress brokerIp;
  if (!brokerIp.fromString(settings_.brokerIp)) {
    appLogger.log("[MQTT] Invalid broker IP: " + settings_.brokerIp);
    initialized_ = true;
    return;
  }

  mqttPubSub.setServer(brokerIp, settings_.brokerPort);
  mqttPubSub.setCallback(mqttCallback);
  mqttPubSub.setBufferSize(1024);

  // Set very short socket timeout to prevent blocking the ethernet service loop.
  // UIPEthernet's connect() uses this for TCP handshake timeout.
  mqttEthClient.setTimeout(250);
  mqttPubSub.setSocketTimeout(1);  // 1 second max for MQTT protocol operations

  initialized_ = true;
  appLogger.log("[MQTT] Initialized. Broker: " + settings_.brokerIp + ":" + String(settings_.brokerPort));
}

void MqttClient::loop() {
  if (!initialized_ || !settings_.enabled) return;

  // Rate-limit MQTT processing to avoid starving the API server.
  // The ethernet service loop runs every 2ms; MQTT only needs ~500ms cadence.
  unsigned long now = millis();
  if (now - lastMqttLoopMs < MQTT_LOOP_INTERVAL_MS) {
    return;
  }
  lastMqttLoopMs = now;

  if (mqttPubSub.connected()) {
    mqttPubSub.loop();
    flushPendingTelemetry();
    return;
  }

  // Throttle reconnect attempts
  if (now - lastConnectAttemptMs_ < RECONNECT_INTERVAL_MS && lastConnectAttemptMs_ != 0) {
    return;
  }

  connect();
}

void MqttClient::connect() {
  lastConnectAttemptMs_ = millis();

  // Stop any lingering connection before reconnecting
  mqttEthClient.stop();

  String clientId = "mv-bridge-" + String(ETH_MAC[4], HEX) + String(ETH_MAC[5], HEX);
  String willTopic = settings_.topicPrefix + "/status";

  appLogger.log("[MQTT] Connecting to " + settings_.brokerIp + ":" + String(settings_.brokerPort) + "...");

  bool connected;
  if (settings_.username.length() > 0) {
    connected = mqttPubSub.connect(
      clientId.c_str(),
      settings_.username.c_str(),
      settings_.password.c_str(),
      willTopic.c_str(),  // will topic
      0,                  // will QoS
      true,               // will retain
      "offline"           // will message
    );
  } else {
    connected = mqttPubSub.connect(
      clientId.c_str(),
      willTopic.c_str(),  // will topic
      0,                  // will QoS
      true,               // will retain
      "offline"           // will message
    );
  }

  if (connected) {
    appLogger.log("[MQTT] Connected");
    publishAvailability(true);
    publishDiscovery();

    // Subscribe to command topics
    String cmdTopicPower = settings_.topicPrefix + "/number/power_limit/set";
    mqttPubSub.subscribe(cmdTopicPower.c_str());
    appLogger.log("[MQTT] Subscribed to " + cmdTopicPower);
    String cmdTopicPoll = settings_.topicPrefix + "/number/poll_interval/set";
    mqttPubSub.subscribe(cmdTopicPoll.c_str());
    appLogger.log("[MQTT] Subscribed to " + cmdTopicPoll);
  } else {
    appLogger.log("[MQTT] Connection failed, rc=" + String(mqttPubSub.state()));
  }
}

void MqttClient::publishDiscovery() {
  String prefix = settings_.topicPrefix;

  // Sensor: power
  publishSensorDiscovery(mqttPubSub, prefix, "power", "Power", "W", "power", "measurement", "", "power_w");

  // Sensor: total yield
  publishSensorDiscovery(mqttPubSub, prefix, "total_yield", "Total Yield", "kWh", "energy", "total_increasing", "", "total_yield_kwh");

  // Sensor: daily yield
  publishSensorDiscovery(mqttPubSub, prefix, "daily_yield", "Daily Yield", "kWh", "energy", "total_increasing", "", "daily_yield_kwh");

  // Sensor: poll interval
  publishSensorDiscovery(mqttPubSub, prefix, "poll_interval", "Poll Interval", "s", "", "", "mdi:timer-sand", "poll_interval_s");

  // Number: power limit (slider)
  publishNumberDiscovery(mqttPubSub, prefix, "power_limit", "Power Limit", "W", 0, INVERTER_MAX_POWER_WATTS, 1, "power_limit_w");

  // Number: poll interval (slider/input, allows changing poll interval from HA)
  // Range: 1s to 300s, step 1s
  publishNumberDiscovery(mqttPubSub, prefix, "poll_interval", "Poll Interval", "s", 1, 300, 1, "poll_interval_s");

  appLogger.log("[MQTT] Discovery published");
}

void MqttClient::publishAvailability(bool online) {
  String topic = settings_.topicPrefix + "/status";
  mqttPubSub.publish(topic.c_str(), online ? "online" : "offline", true);
}

void MqttClient::publishTelemetry(const HomeData& data) {
  if (!initialized_ || !settings_.enabled) return;
  if (telemetryMutex == nullptr) return;

  // Queue telemetry for publishing in the ethernet service task.
  // Protected by a mutex because producer and consumer are on different cores.
  if (xSemaphoreTake(telemetryMutex, pdMS_TO_TICKS(5)) != pdTRUE) {
    if (debugMode) {
      appLogger.log("[MQTT] telemetry mutex timeout while queuing");
    }
    return;
  }
  pendingData_ = data;
  pendingTelemetry_ = true;
  xSemaphoreGive(telemetryMutex);
}

void MqttClient::flushPendingTelemetry() {
  if (!pendingTelemetry_ || !mqttPubSub.connected()) return;

  if (telemetryMutex == nullptr) return;

  HomeData snapshotData;

  if (xSemaphoreTake(telemetryMutex, pdMS_TO_TICKS(5)) != pdTRUE) {
    if (debugMode) {
      appLogger.log("[MQTT] telemetry mutex timeout while flushing");
    }
    return;
  }

  if (!pendingTelemetry_) {
    xSemaphoreGive(telemetryMutex);
    return;
  }

  snapshotData = pendingData_;
  pendingTelemetry_ = false;
  xSemaphoreGive(telemetryMutex);

  // Read current control/state values from the single source of truth.
  // MQTT transports data; InverterController owns these values.
  uint32_t currentPollIntervalMs = InverterController::getInstance().getRetryIntervalMs();
  uint16_t currentPowerLimitW = 0;
  bool currentPowerLimitKnown = InverterController::getInstance().getPowerLimit(currentPowerLimitW);

  publishCombinedTelemetry(mqttPubSub, settings_.topicPrefix,
                           snapshotData, snapshotData.isValid(),
                           currentPollIntervalMs,
                           currentPowerLimitKnown, currentPowerLimitW);
}

void MqttClient::applySettings(const MqttSettings& settings) {
  settings_ = settings;

  // Disconnect current session if connected
  if (mqttPubSub.connected()) {
    publishAvailability(false);
    mqttPubSub.disconnect();
  }
  mqttEthClient.stop();

  lastConnectAttemptMs_ = 0;

  if (!settings_.enabled) {
    appLogger.log("[MQTT] Disabled");
    return;
  }

  IPAddress brokerIp;
  if (!brokerIp.fromString(settings_.brokerIp)) {
    appLogger.log("[MQTT] Invalid broker IP: " + settings_.brokerIp);
    return;
  }

  mqttPubSub.setServer(brokerIp, settings_.brokerPort);
  appLogger.log("[MQTT] Settings updated. Broker: " + settings_.brokerIp + ":" + String(settings_.brokerPort));
}

bool MqttClient::isConnected() {
  return mqttPubSub.connected();
}

MqttSettings MqttClient::getSettings() {
  return settings_;
}

void MqttClient::mqttCallback(char* topic, byte* payload, unsigned int length) {
  MqttClient& self = getInstance();
  String cmdTopicPower = self.settings_.topicPrefix + "/number/power_limit/set";
  String cmdTopicPoll = self.settings_.topicPrefix + "/number/poll_interval/set";

  String topicStr(topic);
  String value;
  value.reserve(length);
  for (unsigned int i = 0; i < length; i++) {
    value += (char)payload[i];
  }

  // Power limit set
  if (topicStr == cmdTopicPower) {
    int watts = 0;
    if (!parseStrictInt(value, watts)) {
      appLogger.log("[MQTT] Power limit command invalid payload: " + value);
      return;
    }
    if (watts < 0 || watts > INVERTER_MAX_POWER_WATTS) {
      appLogger.log("[MQTT] Power limit command out of range: " + value);
      return;
    }
    appLogger.log("[MQTT] Power limit command received: " + String(watts) + "W");
    String responseBody;
    String errorMsg;
    int httpCode = 0;
    InverterController::SetResult result = InverterController::getInstance().setPower(
        watts, responseBody, httpCode, errorMsg);
    if (result == InverterController::SetResult::Applied) {
      appLogger.log("[MQTT] Power limit applied: " + String(watts) + "W");
      HomeData latestData;
      bool hasLatestData = InverterController::getInstance().getLatestHomeData(latestData);
      uint32_t pollIntervalMs = InverterController::getInstance().getRetryIntervalMs();
      uint16_t powerLimitW = 0;
      bool powerLimitKnown = InverterController::getInstance().getPowerLimit(powerLimitW);
      publishCombinedTelemetry(mqttPubSub, self.settings_.topicPrefix,
                               latestData, hasLatestData,
                               pollIntervalMs,
                               powerLimitKnown, powerLimitW);
    } else if (result == InverterController::SetResult::Deferred) {
      appLogger.log("[MQTT] Power limit deferred: " + String(watts) + "W");
    } else {
      appLogger.log("[MQTT] Power limit rejected: " + errorMsg);
    }
    return;
  }

  // Poll interval set
  if (topicStr == cmdTopicPoll) {
    int seconds = 0;
    if (!parseStrictInt(value, seconds)) {
      appLogger.log("[MQTT] Poll interval command invalid payload: " + value);
      return;
    }
    if (seconds <= 0) {
      appLogger.log("[MQTT] Poll interval command must be > 0: " + value);
      return;
    }
    uint32_t intervalMs = seconds * 1000;
    appLogger.log("[MQTT] Poll interval command received: " + String(seconds) + "s");
    InverterController::getInstance().setPollIntervalMs(intervalMs);
    HomeData latestData;
    bool hasLatestData = InverterController::getInstance().getLatestHomeData(latestData);
    uint32_t pollIntervalMs = InverterController::getInstance().getRetryIntervalMs();
    uint16_t powerLimitW = 0;
    bool powerLimitKnown = InverterController::getInstance().getPowerLimit(powerLimitW);
    publishCombinedTelemetry(mqttPubSub, self.settings_.topicPrefix,
                             latestData, hasLatestData,
                             pollIntervalMs,
                             powerLimitKnown, powerLimitW);
    return;
  }
}
