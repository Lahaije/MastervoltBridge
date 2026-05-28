#include "api_helper.h"

#include <math.h>

#include <WiFi.h>
#include "settings.h"
#include "logger.h"
#include "inverter_data.h"
#include "inverter_controller.h"

namespace {

String formatKwh(float value) {
  return String(value, 3);
}

}  // namespace

String jsonEscape(const String& input) {
  String out;
  out.reserve(input.length() + 8);
  for (size_t i = 0; i < input.length(); i++) {
    char c = input[i];
    if (c == '"' || c == '\\') {
      out += '\\';
      out += c;
    } else if (c == '\n') {
      out += "\\n";
    } else if (c == '\r') {
      out += "\\r";
    } else {
      out += c;
    }
  }
  return out;
}

void sendHttpResponse(EthernetClient& client, int code, const char* contentType, const String& body) {
  String statusText = "OK";
  if (code == 202) statusText = "Accepted";
  else if (code == 400) statusText = "Bad Request";
  else if (code == 404) statusText = "Not Found";
  else if (code == 405) statusText = "Method Not Allowed";
  else if (code == 408) statusText = "Request Timeout";
  else if (code == 500) statusText = "Internal Server Error";
  else if (code == 502) statusText = "Bad Gateway";

  client.print("HTTP/1.1 ");
  client.print(code);
  client.print(" ");
  client.println(statusText);
  client.println("Connection: close");
  client.print("Content-Type: ");
  client.println(contentType);
  client.print("Content-Length: ");
  client.println(body.length());
  client.println();
  client.print(body);
}

bool parseStringToInt(const String& input, int& valueOut) {
  String trimmed = input;
  trimmed.trim();

  if (trimmed.length() == 0) {
    return false;
  }

  // Allow optional leading sign
  size_t start = 0;
  if (trimmed[0] == '-' || trimmed[0] == '+') {
    start = 1;
    if (trimmed.length() == 1) return false;  // sign with no digits
  }

  // Validate remaining characters are digits
  for (size_t i = start; i < trimmed.length(); i++) {
    if (!isDigit(trimmed[i])) return false;
  }

  valueOut = trimmed.toInt();
  return true;
}

String getJsonValueByKey(const String& body, const String& key) {
  String normalized = body;
  normalized.trim();
  if (normalized.length() == 0) {
    return "";
  }

  // JSON-style payloads only: {"key":"value"} or {"key":123}
  String quotedKey = String("\"") + key + "\"";
  int keyIndex = normalized.indexOf(quotedKey);
  if (keyIndex < 0) {
    return "";
  }

  int colonIndex = normalized.indexOf(':', keyIndex + quotedKey.length());
  if (colonIndex < 0) {
    return "";
  }

  int i = colonIndex + 1;
  while (i < normalized.length() && isspace((unsigned char)normalized[i])) {
    i++;
  }

  // Quoted string value
  if (i < normalized.length() && normalized[i] == '"') {
    int valueEnd = normalized.indexOf('"', i + 1);
    if (valueEnd > i + 1) {
      String result = normalized.substring(i + 1, valueEnd);
      result.trim();
      return result;
    }
    return "";
  }

  // Unquoted numeric value
  int valueEnd = i;
  while (valueEnd < normalized.length() &&
         normalized[valueEnd] != ',' &&
         normalized[valueEnd] != '}' &&
         !isspace((unsigned char)normalized[valueEnd])) {
    valueEnd++;
  }

  if (valueEnd > i) {
    String result = normalized.substring(i, valueEnd);
    result.trim();
    return result;
  }

  return "";
}

bool parseFetchUrlFromBody(const String& body, String& urlOut) {
  urlOut = getJsonValueByKey(body, "url");
  return urlOut.length() > 0;
}

String buildInfoJson(const HomeData& data, unsigned long lastUpdateMs) {
  JsonBuilder json;
  const float powerW = (data.hasPower && isfinite(data.instantaneousPowerW))
                         ? data.instantaneousPowerW
                         : 0.0f;
  json
    .addNumber("last_update_ms", String(lastUpdateMs))
    .addString("operating_status", data.operatingStatus)
    .addString("error_alarm_code", data.errorAlarmCode)
    .addString("operating_mode", data.operatingMode)
    .addString("inverter_model", data.inverterModel)
    .addString("inverter_mac_address", data.inverterMacAddress)
    .addNumber("power", String(powerW, 3))
    .addString("inverter_link_state", String(toString(InverterController::getInstance().getLinkState())))
    .addNumber("failure_streak_s", String(InverterController::getInstance().getFailureStreakMs() / 1000UL))
    .addNumber("poll_interval_ms", String(InverterController::getInstance().getRetryIntervalMs()))
    .addPowerLimit()
    .addShadow();

  if (data.hasLifetimeEnergy && isfinite(data.lifetimeEnergyKwh)) {
    json.addNumber("total_yield", formatKwh(data.lifetimeEnergyKwh), true);
  } else {
    json.addNull("total_yield");
  }

  if (data.hasDailySessionEnergy && isfinite(data.dailySessionEnergyKwh)) {
    json.addNumber("daily_yield", formatKwh(data.dailySessionEnergyKwh), true);
  } else {
    json.addNull("daily_yield");
  }

  return json.build();
}

JsonBuilder& JsonBuilder::addPowerLimit() {
  uint16_t watts = 0;
  const bool known = InverterController::getInstance().getPowerLimit(watts);
  if (needsComma) json += ",";
  json += "\"power_limit_watts\":";
  if (known) {
    json += String(watts);
  } else {
    json += "null";
  }
  needsComma = true;
  return *this;
}

JsonBuilder& JsonBuilder::addShadow() {
  bool enabled = false;
  const bool known = InverterController::getInstance().getShadow(enabled);
  if (needsComma) json += ",";
  json += "\"shadow_enabled\":";
  if (known) {
    json += (enabled ? "true" : "false");
  } else {
    json += "null";
  }
  needsComma = true;
  return *this;
}

String buildHealthJson() {
  return JsonBuilder()
    .addBool("wifi_connected", WiFi.status() == WL_CONNECTED)
    .addString("wifi_ssid", String(INVERTER_WIFI_SSID))
    .addString("wifi_ip", WiFi.localIP().toString())
    .addString("ethernet_ip", Ethernet.localIP().toString())
    .addString("inverter_host", String(INVERTER_HOST))
    .addNumber("last_inverter_status", String(lastInverterStatusCode))
    .addBool("debug_mode", debugMode)
    .build();
}

void sendLogsResponse(EthernetClient& client) {
  // Snapshot count before iterating.
  const int count = appLogger.getLogCount();

  // Headers: omit Content-Length and use Connection: close framing so we can
  // stream entries without precomputing the body size.
  client.print(F("HTTP/1.1 200 OK\r\n"));
  client.print(F("Connection: close\r\n"));
  client.print(F("Content-Type: application/json\r\n"));
  client.print(F("\r\n"));

  // Batch writes to reduce small packets.
  static const size_t BUF_SIZE = 1024;
  char buf[BUF_SIZE];
  size_t pos = 0;
  bool aborted = false;

  // Flush buffer to client; aborts on disconnect.
  auto flush = [&]() {
    if (pos > 0) {
      if (!client.connected()) {
        aborted = true;
        pos = 0;
        return;
      }
      client.write((const uint8_t*)buf, pos);
      pos = 0;
      delay(1);
    }
  };

  // Helper lambda: append data to buffer, flushing as needed.
  auto append = [&](const char* data, size_t len) {
    while (len > 0 && !aborted) {
      size_t space = BUF_SIZE - pos;
      size_t chunk = (len < space) ? len : space;
      memcpy(buf + pos, data, chunk);
      pos += chunk;
      data += chunk;
      len -= chunk;
      if (pos >= BUF_SIZE) {
        flush();
      }
    }
  };

  auto appendStr = [&](const String& s) {
    append(s.c_str(), s.length());
  };

  const char* header = "{\"total_entries\":";
  append(header, strlen(header));
  String countStr = String(count);
  appendStr(countStr);
  const char* entriesOpen = ",\"entries\":[";
  append(entriesOpen, strlen(entriesOpen));

  for (int i = 0; i < count && !aborted; i++) {
    if (i > 0) {
      append(",", 1);
    }
    const LogEntry& entry = appLogger.getLogEntry(i);
    String obj = JsonBuilder()
      .addNumber("timestamp_ms", String(entry.timestamp))
      .addString("message", entry.message)
      .build();
    appendStr(obj);
  }

  if (!aborted) {
    append("]}", 2);
    flush();
  }
}

String buildApiDiscoveryJson() {
  String json = "{";
  json += "\"service\":\"esp32-inverter-bridge\",";
  json += "\"endpoints\":[";

  constexpr size_t endpointCount = sizeof(API_ENDPOINTS) / sizeof(API_ENDPOINTS[0]);
  for (size_t i = 0; i < endpointCount; i++) {
    if (i > 0) {
      json += ",";
    }

    // Use JsonBuilder for each endpoint object
    json += JsonBuilder()
      .addString("method", API_ENDPOINTS[i].method)
      .addString("path", API_ENDPOINTS[i].path)
      .addString("description", API_ENDPOINTS[i].description)
      .build();
  }

  json += "]";
  json += "}";
  return json;
}

void sendFlashHtmlResponse(EthernetClient& client, const char* flashData, size_t len) {
  client.print(F("HTTP/1.1 200 OK\r\n"));
  client.print(F("Connection: close\r\n"));
  client.print(F("Content-Type: text/html\r\n"));
  client.print(F("Content-Length: "));
  client.print(len);
  client.print(F("\r\n\r\n"));

  // Write payload in bounded chunks directly from flash.
  static const size_t CHUNK = 512;
  char buf[CHUNK];
  size_t offset = 0;
  while (offset < len) {
    if (!client.connected()) return;  // abort cleanly on disconnect
    size_t remaining = len - offset;
    size_t n = (remaining < CHUNK) ? remaining : CHUNK;
    memcpy_P(buf, flashData + offset, n);
    client.write((const uint8_t*)buf, n);
    offset += n;
    delay(1);
  }
}
