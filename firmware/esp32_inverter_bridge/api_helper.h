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
  const char* description;
};

// Single source of truth for all API endpoints (defined in api.h)
constexpr size_t API_ENDPOINT_COUNT = 7;
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
   */
  JsonBuilder& addNumber(const String& key, const String& value) {
    if (needsComma) json += ",";
    json += "\"" + key + "\":" + value;
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
 * JSON-only format:
 * - {"url":"/home"} or {'url':'/home'}
 * Returns true if successfully parsed, false otherwise.
 */
bool parseFetchUrlFromBody(const String& body, String& urlOut);

/**
 * Build JSON response for /api/info endpoint.
 * Includes latest cached inverter telemetry: status, mode, power, yields.
 */
String buildInfoJson(const HomeData& data, unsigned long lastUpdateMs);

/**
 * Build JSON response for /api/health endpoint.
 * Includes WiFi status, IPs, and last inverter HTTP status code.
 */
String buildHealthJson();

/**
 * Build JSON response for /api/logs endpoint.
 * Contains up to 1000 log entries from the circular buffer.
 */
String buildLogsJson();

/**
 * Build JSON response for / (API discovery) endpoint.
 * Lists all available API endpoints with methods, paths, and descriptions.
 */
String buildApiDiscoveryJson();

#endif // API_HELPER_H
