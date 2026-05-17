#ifndef INVERTER_DATA_H
#define INVERTER_DATA_H

#include <Arduino.h>

/**
 * HomeData: Represents the parsed response from the inverter's /home endpoint.
 * 
 * The /home endpoint returns 8 newline-delimited fields containing live telemetry
 * data about the inverter's operational state, model info, and power metrics.
 * 
 * Format: Line 1\nLine 2\n...\nLine 8\n
 * 
 * All raw fields are stored as String; helper methods provide typed access.
 */
struct HomeData {
  // Raw fields from /home endpoint
  String operatingStatus;
  String errorAlarmCode;
  String operatingMode;
  String inverterModel;
  String inverterMacAddress;
  String instantaneousPower;
  String lifetimeEnergy;
  String dailySessionEnergy;

  /**
   * Check if this struct has valid data (at least one field populated).
   */
  bool isValid() const;

  /**
   * Clear all fields.
   */
  void clear();
};

/**
 * Fetch the latest inverter /home telemetry data.
 * 
 * This function retrieves data from the inverter monitor's cache (20s polling interval).
 * For real-time data, consider fetching directly if latency is critical.
 * 
 * Returns: HomeData object with parsed fields, or empty if unavailable.
 */
HomeData getInverterData();

/**
 * Parse the raw /home response into a HomeData struct.
 * Expects 8 newline-delimited fields.
 * Returns true if parsing succeeded and data is valid.
 */
bool parseHomeResponse(const String& rawResponse, HomeData& dataOut);

#endif // INVERTER_DATA_H
