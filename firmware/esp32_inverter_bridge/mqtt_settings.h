#ifndef MQTT_SETTINGS_H
#define MQTT_SETTINGS_H

#include <Arduino.h>

/**
 * MQTT settings stored in NVS (Non-Volatile Storage).
 * Persists across power cycles.
 */
struct MqttSettings {
  String brokerIp;
  uint16_t brokerPort;
  bool enabled;
  String topicPrefix;
};

/**
 * Load MQTT settings from NVS. Returns defaults if never saved.
 */
MqttSettings loadMqttSettings();

/**
 * Save MQTT settings to NVS.
 * Returns true on success.
 */
bool saveMqttSettings(const MqttSettings& settings);

// Default values
static const char* MQTT_DEFAULT_BROKER_IP = "192.168.1.23";
static const uint16_t MQTT_DEFAULT_BROKER_PORT = 1883;
static const bool MQTT_DEFAULT_ENABLED = true;
static const char* MQTT_DEFAULT_TOPIC_PREFIX = "mastervolt_bridge";

#endif // MQTT_SETTINGS_H
