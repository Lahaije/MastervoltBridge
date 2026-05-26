#ifndef MQTT_BRIDGE_H
#define MQTT_BRIDGE_H

#include <Arduino.h>
#include <IPAddress.h>

/**
 * MqttBridge: Manages MQTT connection to Home Assistant broker.
 * Publishes HA MQTT discovery config on connect, then periodically
 * publishes inverter state. Subscribes to command topics for
 * power limit, polling interval, and shadow function.
 *
 * The broker IP is stored in ESP32 NVS (non-volatile storage) and
 * can be configured at runtime via the /api/mqtt REST endpoint.
 */
class MqttBridge {
public:
  static MqttBridge& getInstance() {
    static MqttBridge instance;
    return instance;
  }

  /**
   * Initialize MQTT. Loads broker IP from NVS.
   * Call after Ethernet is up.
   */
  void initialize();

  /**
   * Call periodically from the Ethernet task to maintain connection
   * and publish state updates.
   */
  void loop();

  /**
   * Set the MQTT broker IP and persist to NVS. Pass 0.0.0.0 to clear.
   * Returns true if accepted and saved.
   */
  bool setBrokerIp(const IPAddress& ip);

  /**
   * Get the currently configured broker IP. Returns 0.0.0.0 when not set.
   */
  IPAddress getBrokerIp() const;

  /**
   * Returns true if currently connected to the broker.
   */
  bool isConnected() const;

  /**
   * Returns true when HA MQTT integration is enabled.
   */
  bool isHaEnabled() const { return haEnabled_; }

  /**
   * Enable or disable HA MQTT integration. Persisted to NVS.
   * Disabling closes any active broker connection and suppresses publishing.
   */
  void setHaEnabled(bool enabled);

  /**
   * Set MQTT username/password (persisted to NVS). Empty strings clear them.
   * If currently connected, the existing session is dropped so the new creds
   * are used on the next reconnect.
   */
  void setCredentials(const String& user, const String& password);
  void setUser(const String& user);
  void setPassword(const String& password);

  /** Returns the stored MQTT username (may be empty). */
  String getUser() const { return user_; }

  /** Returns true if a non-empty username is configured. */
  bool hasCredentials() const { return user_.length() > 0; }

  /**
   * Returns true if a broker discovery scan is currently in progress.
   */
  bool isScanning() const { return scanState_ != ScanState::Idle; }

  /**
   * Called by the Ethernet task every time a fresh IP lease is obtained
   * (initial DHCP, lease renew, lease rebind, cable replug). Triggers a
   * probe of the stored broker; if unreachable, starts a /24 scan to
   * auto-discover an MQTT broker on the LAN.
   */
  void onIpAcquired();

  /**
   * Force a state publish (called after every successful inverter poll).
   * No-op when HA is disabled, broker IP is unset, or not connected.
   */
  void publishState();

  /**
   * Publish the cached power-limit value on its dedicated state topic.
   * Called after a successful setPower() and once per MQTT (re)connect.
   * No-op when HA disabled, broker unset, not connected, or value unknown.
   */
  void publishPowerLimit();

  /**
   * Publish the cached shadow on/off value on its dedicated state topic.
   * Called after a successful setShadow() and once per MQTT (re)connect.
   * No-op when HA disabled, broker unset, not connected, or value unknown.
   */
  void publishShadow();

private:
  MqttBridge();
  ~MqttBridge() = default;
  MqttBridge(const MqttBridge&) = delete;
  MqttBridge& operator=(const MqttBridge&) = delete;

  enum class ScanState : uint8_t {
    Idle,
    ProbeStored,   // try TCP-connecting to the previously stored broker
    ScanRunning    // walking the /24 subnet looking for an MQTT broker
  };

  void connect();
  void publishDiscovery();
  void publishAvailability(bool online);
  void onMessage(const char* topic, byte* payload, unsigned int length);
  void persistBrokerIp(const IPAddress& ip);
  bool probeBroker(const IPAddress& ip);  // TCP-connect probe with short timeout
  void serviceScan();                      // state machine step, called from loop()

  static void messageCallback(char* topic, byte* payload, unsigned int length);

  IPAddress brokerIp_;
  String user_;
  String password_;
  bool haEnabled_ = false;
  bool configured_ = false;       // broker IP is non-zero
  bool discoveryPublished_ = false;
  unsigned long lastConnectAttemptMs_ = 0;
  unsigned long lastPublishMs_ = 0;

  ScanState scanState_ = ScanState::Idle;
  uint8_t scanHostOctet_ = 0;     // next .X host to probe in /24 scan
  unsigned long scanStartedMs_ = 0;
  uint8_t storedProbeAttempts_ = 0;  // retry counter for stored broker probe
};

#endif // MQTT_BRIDGE_H
