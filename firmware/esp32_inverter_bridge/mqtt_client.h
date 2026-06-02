#ifndef MQTT_CLIENT_H
#define MQTT_CLIENT_H

#include <Arduino.h>
#include "inverter_data.h"
#include "mqtt_settings.h"

/**
 * MqttClient: Manages MQTT connection to Home Assistant broker.
 * Publishes inverter telemetry and subscribes to power limit commands.
 * Uses the Ethernet interface (ENC28J60 via UIPEthernet).
 *
 * IMPORTANT: All network I/O must happen in the ethernet service task.
 * publishTelemetry() only queues data; actual MQTT writes happen in loop().
 */
class MqttClient {
public:
  static MqttClient& getInstance() {
    static MqttClient instance;
    return instance;
  }

  /**
   * Initialize MQTT client. Loads settings from NVS and attempts connection.
   * Must be called after Ethernet is up.
   */
  void initialize();

  /**
   * Call periodically from the ethernet service task to maintain MQTT connection,
   * process incoming messages, and flush any queued telemetry.
   * NOT thread-safe — must only be called from the ethernet task.
   */
  void loop();

  /**
   * Queue telemetry data for publishing on next loop() call.
   * Thread-safe — can be called from any task (e.g. inverter controller).
   */
  void publishTelemetry(const HomeData& data, uint32_t pollIntervalMs, uint16_t powerLimitW, bool powerLimitKnown);

  /**
   * Reload settings from NVS and reconnect if changed.
   */
  void applySettings(const MqttSettings& settings);

  /**
   * Returns true if currently connected to the MQTT broker.
   */
  bool isConnected();

  /**
   * Get current settings (for API/UI display).
   */
  MqttSettings getSettings();

private:
  MqttClient();
  ~MqttClient() = default;
  MqttClient(const MqttClient&) = delete;
  MqttClient& operator=(const MqttClient&) = delete;

  void connect();
  void publishDiscovery();
  void publishAvailability(bool online);
  void flushPendingTelemetry();
  static void mqttCallback(char* topic, byte* payload, unsigned int length);

  MqttSettings settings_;
  bool initialized_ = false;
  bool discoveryPublished_ = false;
  unsigned long lastConnectAttemptMs_ = 0;
  static constexpr unsigned long RECONNECT_INTERVAL_MS = 30000;

  // Pending telemetry (set from any task, flushed in ethernet task)
  volatile bool pendingTelemetry_ = false;
  HomeData pendingData_;
  uint32_t pendingPollIntervalMs_ = 0;
  uint16_t pendingPowerLimitW_ = 0;
  bool pendingPowerLimitKnown_ = false;
};

#endif // MQTT_CLIENT_H
