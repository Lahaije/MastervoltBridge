#include "api_helper.h"

#include <WiFi.h>
#include "settings.h"
#include "logger.h"
#include "inverter_data.h"
#include "inverter_monitor.h"
#include "mqtt_bridge.h"

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
  if (code == 400) statusText = "Bad Request";
  else if (code == 404) statusText = "Not Found";
  else if (code == 405) statusText = "Method Not Allowed";
  else if (code == 502) statusText = "Bad Gateway";
  else if (code == 500) statusText = "Internal Server Error";

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

String buildInfoJson(const HomeData& data, unsigned long lastUpdateMs, bool dataValid) {
  // Pull the bridge's cached "shadow" / "power_limit" values (HA cache +
  // dual-topic pattern; inverter does not report these via /home).
  InverterMonitor& mon = InverterMonitor::getInstance();
  uint16_t pwrLimitW = 0;
  bool pwrLimitKnown = mon.getCachedPowerLimit(pwrLimitW);
  bool shadowOn = false;
  bool shadowKnown = mon.getCachedShadow(shadowOn);

  return JsonBuilder()
    .addBool("telemetry_valid", dataValid)
    .addNumber("last_update_ms", String(lastUpdateMs))
    .addString("operating_status", dataValid ? data.operatingStatus : String(""))
    .addString("error_alarm_code", dataValid ? data.errorAlarmCode : String(""))
    .addString("operating_mode", dataValid ? data.operatingMode : String(""))
    .addString("inverter_model", dataValid ? data.inverterModel : String(""))
    .addString("inverter_mac_address", dataValid ? data.inverterMacAddress : String(""))
    .addString("power", dataValid ? data.instantaneousPower : String(""))
    .addString("total_yield", dataValid ? data.lifetimeEnergy : String(""))
    .addString("daily_yield", dataValid ? data.dailySessionEnergy : String(""))
    // Cached write-only settings. *_known=false means the bridge has never
    // seen a successful set since the last NVS erase; the value field is
    // meaningless in that case.
    .addBool("power_limit_known", pwrLimitKnown)
    .addNumber("power_limit_watts", String(pwrLimitKnown ? pwrLimitW : 0))
    .addBool("shadow_known", shadowKnown)
    .addBool("shadow", shadowKnown && shadowOn)
    .addNumber("polling_interval_s", String(mon.getPollingIntervalMs() / 1000UL))
    .build();
}

String buildHealthJson() {
  MqttBridge& mqtt = MqttBridge::getInstance();
  IPAddress broker = mqtt.getBrokerIp();
  bool brokerSet = !(broker[0] == 0 && broker[1] == 0 && broker[2] == 0 && broker[3] == 0);
  return JsonBuilder()
    .addBool("wifi_connected", WiFi.status() == WL_CONNECTED)
    .addString("wifi_ssid", String(INVERTER_WIFI_SSID))
    .addString("wifi_ip", WiFi.localIP().toString())
    .addString("ethernet_ip", Ethernet.localIP().toString())
    .addString("inverter_host", String(INVERTER_HOST))
    .addNumber("last_inverter_status", String(lastInverterStatusCode))
    .addBool("debug_mode", debugMode)
    .addBool("ha_mqtt_enabled", mqtt.isHaEnabled())
    .addString("mqtt_broker", brokerSet ? broker.toString() : String(""))
    .addBool("mqtt_connected", mqtt.isConnected())
    .addBool("mqtt_scanning", mqtt.isScanning())
    .build();
}

void sendLogsResponse(EthernetClient& client) {
  // Snapshot count up-front; the buffer is appended to from another task and
  // could grow between the loop's iterations.
  const int count = appLogger.getLogCount();

  // Headers: omit Content-Length and use Connection: close framing so we can
  // stream entries without precomputing the body size.
  client.print(F("HTTP/1.1 200 OK\r\n"));
  client.print(F("Connection: close\r\n"));
  client.print(F("Content-Type: application/json\r\n"));
  client.print(F("\r\n"));

  // Use a stack buffer to batch entries per TCP write, reducing the number
  // of small packets sent over the ENC28J60 (major throughput bottleneck).
  // Buffer must be small enough to fit in the ENC28J60's 8KB packet memory
  // alongside receive buffers. After each flush we yield to let the UIP
  // TCP/IP stack process incoming ACKs, preventing write-stalls.
  static const size_t BUF_SIZE = 1024;
  char buf[BUF_SIZE];
  size_t pos = 0;
  bool aborted = false;

  // Helper lambda: flush buffer to client when full or at end.
  // Checks client connectivity first to avoid writing to a dead connection
  // (which crashes the ENC28J60/UIP stack).
  auto flush = [&]() {
    if (pos > 0) {
      if (!client.connected()) {
        aborted = true;
        pos = 0;
        return;
      }
      client.write((const uint8_t*)buf, pos);
      pos = 0;
      // Yield to allow UIP stack to process ACKs from receiver.
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
