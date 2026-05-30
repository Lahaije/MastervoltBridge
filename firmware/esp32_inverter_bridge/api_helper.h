#ifndef API_HELPER_H
#define API_HELPER_H

#include <Arduino.h>
#include <UIPEthernet.h>

struct HomeData;

/**
 * API endpoint metadata structure.
 * Used by both api.h (endpoint definitions) and api_helper.cpp (discovery response).
 */
struct ApiEndpointInfo {
  const char* method;
  const char* path;
  String description;
};

// Single source of truth for all API endpoints (defined in api.h)
constexpr size_t API_ENDPOINT_COUNT = 15;
extern const ApiEndpointInfo API_ENDPOINTS[API_ENDPOINT_COUNT];

/**
 * Escape a string for safe JSON inclusion.
 * Handles quotes, backslashes, newlines, and carriage returns.
 */
String jsonEscape(const String& input);

/**
 * Simple JSON builder with automatic comma and escaping handling.
 * Usage: JsonBuilder().addString("key", "value").addNumber("count", "42").build()
 */
class JsonBuilder {
private:
  String json;
  bool needsComma;

public:
  JsonBuilder() : needsComma(false) { json = "{"; }

  /**
   * Add a string field (automatically escaped).
   */
  JsonBuilder& addString(const String& key, const String& value) {
    if (needsComma) json += ",";
    json += "\"" + key + "\":\"" + jsonEscape(value) + "\"";
    needsComma = true;
    return *this;
  }

  /**
   * Add a numeric field (unescaped, as-is).
   * If check_non_zero is true and the numeric value is exactly zero,
   * emits JSON null instead of 0.
   */
  JsonBuilder& addNumber(const String& key, const String& value, bool check_non_zero = false) {
    String normalized = value;
    normalized.trim();

    bool emitNull = false;
    if (check_non_zero && normalized.length() > 0) {
      int pos = 0;
      if (normalized[0] == '+' || normalized[0] == '-') {
        pos = 1;
      }

      bool seenDigit = false;
      bool seenDot = false;
      bool allDigitsZero = true;
      bool isValidSimpleNumber = (pos < (int)normalized.length());

      for (int i = pos; i < (int)normalized.length() && isValidSimpleNumber; i++) {
        char c = normalized[i];
        if (c == '.') {
          if (seenDot) {
            isValidSimpleNumber = false;
          } else {
            seenDot = true;
          }
          continue;
        }

        if (!isDigit(c)) {
          isValidSimpleNumber = false;
          continue;
        }

        seenDigit = true;
        if (c != '0') {
          allDigitsZero = false;
        }
      }

      if (isValidSimpleNumber && seenDigit && allDigitsZero) {
        emitNull = true;
      }
    }

    if (needsComma) json += ",";
    if (emitNull) {
      json += "\"" + key + "\":null";
    } else {
      json += "\"" + key + "\":" + normalized;
    }
    needsComma = true;
    return *this;
  }

  /**
   * Add a boolean field.
   */
  JsonBuilder& addBool(const String& key, bool value) {
    if (needsComma) json += ",";
    json += "\"" + key + "\":" + (value ? "true" : "false");
    needsComma = true;
    return *this;
  }

  /**
   * Add a JSON null field.
   */
  JsonBuilder& addNull(const String& key) {
    if (needsComma) json += ",";
    json += "\"" + key + "\":null";
    needsComma = true;
    return *this;
  }

  /**
   * Add power limit field: fetches live from controller, emits watts if known or null.
   */
  JsonBuilder& addPowerLimit();

  /**
   * Add shadow enabled field: fetches live from controller, emits boolean if known or null.
   */
  JsonBuilder& addShadow();

  /**
   * Finalize and return the JSON object.
   */
  String build() {
    json += "}";
    return json;
  }
};

/**
 * Build error response JSON with automatic escaping.
 */
inline String buildErrorJson(const String& errorMessage) {
  return JsonBuilder().addString("error", errorMessage).build();
}

/**
 * Send an HTTP response with status code, content type, and body.
 * Handles status text lookup and header formatting.
 */
void sendHttpResponse(EthernetClient& client, int code, const char* contentType, const String& body);

/**
 * Parse a string as an integer (optional leading sign, then digits only).
 * Returns true if successfully parsed, false otherwise.
 */
bool parseStringToInt(const String& input, int& valueOut);

/**
 * Extract a JSON value by key from request body.
 * JSON-only format: {"key":"value"} or {"key":123}
 * Returns the value as a String, or empty String if key not found.
 */
String getJsonValueByKey(const String& body, const String& key);

/**
 * Parse inverter URL from request body.
 * Expects JSON with a double-quoted key: {"url":"/home"}
 * Returns true if successfully parsed, false otherwise.
 */
bool parseFetchUrlFromBody(const String& body, String& urlOut);

/**
 * Build JSON response for /api/info endpoint.
 * Real-time telemetry: power, yields, tunables (shadow, power limit, poll interval).
 */
String buildInfoJson(const HomeData& data);

/**
 * Build JSON response for /api/health endpoint.
 * Bridge diagnostics: link state, operating status, WiFi connectivity, debug mode.
 */
String buildHealthJson(const HomeData& data);

/**
 * Build JSON response for /api/device endpoint.
 * Stable identity: firmware version, model, MACs, IPs, hosts.
 */
String buildDeviceJson(const HomeData& data);

/**
 * Stream the /api/logs response directly to the client.
 * Avoids building the (potentially >60 KB) body as a single Arduino String,
 * which would silently fail mid-build under heap fragmentation and produce
 * malformed JSON. Writes headers (no Content-Length, framed by
 * Connection: close) and the JSON body one entry at a time.
 */
void sendLogsResponse(EthernetClient& client);

/**
 * Build JSON response for / (API discovery) endpoint.
 * Lists all available API endpoints with methods, paths, and descriptions.
 */
String buildApiDiscoveryJson();

/**
 * Send a PROGMEM-backed HTML page directly to the client in bounded chunks.
 * Zero heap allocation for the payload — reads from flash and writes in
 * fixed-size batches with connectivity checks to avoid dead-write crashes.
 */
void sendFlashHtmlResponse(EthernetClient& client, const char* flashData, size_t len);

#endif // API_HELPER_H
