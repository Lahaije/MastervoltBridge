#include "mqtt_settings.h"
#include <Preferences.h>

static const char* NVS_NAMESPACE = "mqtt";
static const char* KEY_BROKER_IP = "broker_ip";
static const char* KEY_BROKER_PORT = "broker_port";
static const char* KEY_ENABLED = "enabled";
static const char* KEY_TOPIC_PREFIX = "topic_prefix";

MqttSettings loadMqttSettings() {
  MqttSettings settings;
  Preferences prefs;

  if (prefs.begin(NVS_NAMESPACE, true)) {  // read-only
    settings.brokerIp = prefs.getString(KEY_BROKER_IP, MQTT_DEFAULT_BROKER_IP);
    settings.brokerPort = prefs.getUShort(KEY_BROKER_PORT, MQTT_DEFAULT_BROKER_PORT);
    settings.enabled = prefs.getBool(KEY_ENABLED, MQTT_DEFAULT_ENABLED);
    settings.topicPrefix = prefs.getString(KEY_TOPIC_PREFIX, MQTT_DEFAULT_TOPIC_PREFIX);
    prefs.end();
  } else {
    settings.brokerIp = MQTT_DEFAULT_BROKER_IP;
    settings.brokerPort = MQTT_DEFAULT_BROKER_PORT;
    settings.enabled = MQTT_DEFAULT_ENABLED;
    settings.topicPrefix = MQTT_DEFAULT_TOPIC_PREFIX;
  }

  return settings;
}

bool saveMqttSettings(const MqttSettings& settings) {
  Preferences prefs;

  if (!prefs.begin(NVS_NAMESPACE, false)) {  // read-write
    return false;
  }

  prefs.putString(KEY_BROKER_IP, settings.brokerIp);
  prefs.putUShort(KEY_BROKER_PORT, settings.brokerPort);
  prefs.putBool(KEY_ENABLED, settings.enabled);
  prefs.putString(KEY_TOPIC_PREFIX, settings.topicPrefix);
  prefs.end();

  return true;
}
