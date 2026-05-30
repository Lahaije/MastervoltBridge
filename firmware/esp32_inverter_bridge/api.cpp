#include "api.h"

#include "settings.h"
#include "logger.h"
#include "wifi_bridge.h"
#include "inverter_controller.h"
#include "inverter_data.h"
#include "api_helper.h"
#include "web_ui.h"
#include "mqtt_client.h"
#include "mqtt_settings.h"

const ApiEndpointInfo API_ENDPOINTS[API_ENDPOINT_COUNT] = {
  {"GET", "/", "Web UI dashboard"},
  {"GET", "/api", "API discovery and endpoint overview"},
  {"GET", "/api/device", "Stable identity: firmware version, model, MACs, IPs"},
  {"GET", "/api/health", "Bridge diagnostics: link state, operating status, WiFi, debug mode"},
  {"GET", "/api/logs", "Retrieve up to 1000 cached log entries with millisecond timestamps"},
  {"GET", "/api/info", "Real-time telemetry: power, yields, tunables (poll every 5s)"},
  {"POST", "/api/power", String("Set inverter power: JSON body with power field, e.g. {\"power\":1200} (0 <= power <= ") + INVERTER_MAX_POWER_WATTS + "W). Returns 202 if queued for delayed apply."},
  {"POST", "/api/shadow", "Enable or disable inverter shadow function: {\"enabled\":true|false}. Returns 202 if queued for delayed apply."},
  {"POST", "/api/inverter/fetch", "Fetch inverter endpoint: JSON body with url field, e.g. {\"url\":\"/home\"}"},
  {"POST", "/wifi/off", "If bridge WiFi is connected, send a single button press to turn inverter WiFi off"},
  {"GET", "/pulse", "Trigger WiFi module recovery: GPIO pulse sequence to wake inverter WiFi"},
  {"POST", "/api/debug", "Enable or disable debug mode: {\"debug\":true} logs HTTP 200 successes; {\"debug\":false} suppresses them"},
  {"POST", "/api/interval", "Temporarily override current poll interval in ms: {\"interval\":20000} (range 100-300000)"},
  {"GET", "/api/mqtt", "Get current MQTT settings and connection status"},
  {"POST", "/api/mqtt", "Update MQTT settings: {\"broker_ip\":\"...\",\"broker_port\":1883,\"enabled\":true,\"topic_prefix\":\"...\"}"}
};

void handleApiClient(EthernetClient& client) {
  client.setTimeout(API_CLIENT_TIMEOUT_MS);

  // Parse HTTP request line to extract method and path
  String requestLine = client.readStringUntil('\n');
  requestLine.trim();
  if (requestLine.length() == 0) {
    sendHttpResponse(client, 400, "application/json", buildErrorJson("empty request"));
    return;
  }

  // Extract method and path from "METHOD /path HTTP/1.1"
  int firstSpace = requestLine.indexOf(' ');
  int secondSpace = requestLine.indexOf(' ', firstSpace + 1);
  if (firstSpace < 0 || secondSpace < 0) {
    sendHttpResponse(client, 400, "application/json", buildErrorJson("malformed request line"));
    return;
  }

  String method = requestLine.substring(0, firstSpace);
  String path = requestLine.substring(firstSpace + 1, secondSpace);

  appLogger.log("[API] " + method + " " + path);

  // Parse HTTP headers to extract Content-Length
  int contentLength = 0;
  while (client.connected()) {
    String headerLine = client.readStringUntil('\n');
    headerLine.trim();
    if (headerLine.length() == 0) {
      break;  // End of headers
    }

    String lower = headerLine;
    lower.toLowerCase();
    if (lower.startsWith("content-length:")) {
      String value = headerLine.substring(headerLine.indexOf(':') + 1);
      value.trim();
      contentLength = value.toInt();
    }
  }

  // Read request body if Content-Length > 0. Enforces a deadline to prevent stalls.
  String body;
  if (contentLength > 0) {
    body.reserve(contentLength);
    unsigned long bodyReadStart = millis();
    while ((int)body.length() < contentLength && client.connected()) {
      if (millis() - bodyReadStart > API_CLIENT_TIMEOUT_MS) {
        sendHttpResponse(client, 408, "application/json", buildErrorJson("request body read timeout"));
        return;
      }
      if (client.available()) {
        body += (char)client.read();
      } else {
        delay(1);
      }
    }
  }

  if (method == "GET" && path == "/") {
    sendFlashHtmlResponse(client, WEB_UI_HTML, WEB_UI_HTML_LEN);
    return;
  }

  if (method == "GET" && path == "/api") {
    sendHttpResponse(client, 200, "application/json", buildApiDiscoveryJson());
    return;
  }

  if (method == "GET" && path == "/api/health") {
    HomeData inverterData = getInverterData();
    sendHttpResponse(client, 200, "application/json", buildHealthJson(inverterData));
    return;
  }

  if (method == "GET" && path == "/api/device") {
    HomeData inverterData = getInverterData();
    sendHttpResponse(client, 200, "application/json", buildDeviceJson(inverterData));
    return;
  }

  if (method == "GET" && path == "/api/logs") {
    sendLogsResponse(client);
    return;
  }

  if (method == "GET" && path == "/pulse") {
    bool ok = WifiConnectionManager::getInstance().forceReconnect();
    String response = JsonBuilder().addBool("reconnected", ok).build();
    sendHttpResponse(client, 200, "application/json", response);
    return;
  }

  if (method == "POST" && path == "/wifi/off") {
    bool pressed = triggerWifiOffIfConnected();
    String response = JsonBuilder().addBool("pressed", pressed).build();
    sendHttpResponse(client, 200, "application/json", response);
    return;
  }

  if (method == "POST" && path == "/api/debug") {
    String value = getJsonValueByKey(body, "debug");
    if (value != "true" && value != "false") {
      sendHttpResponse(client, 400, "application/json",
                       buildErrorJson("body must contain debug field: {\"debug\":true} or {\"debug\":false}"));
      return;
    }
    debugMode = (value == "true");
    appLogger.log(String("[API] debug mode ") + (debugMode ? "enabled" : "disabled"));
    String response = JsonBuilder().addBool("debug", debugMode).build();
    sendHttpResponse(client, 200, "application/json", response);
    return;
  }

  if (method == "POST" && path == "/api/interval") {
    String rawInterval = getJsonValueByKey(body, "interval");
    if (rawInterval.length() == 0) {
      sendHttpResponse(client, 400, "application/json", buildErrorJson("body must contain interval field (milliseconds)"));
      return;
    }
    int intervalMs = 0;
    if (!parseStringToInt(rawInterval, intervalMs) || intervalMs < 100 || intervalMs > 300000) {
      sendHttpResponse(client, 400, "application/json", buildErrorJson("interval must be 100-300000 ms"));
      return;
    }
    InverterController::getInstance().setPollIntervalMs((uint32_t)intervalMs);
    String response = JsonBuilder()
      .addNumber("poll_interval_ms", String(InverterController::getInstance().getRetryIntervalMs()))
      .build();
    sendHttpResponse(client, 200, "application/json", response);
    return;
  }
  
  if (method == "GET" && path == "/api/info") {
    // Return cached telemetry; yield fields are null until first successful parse.
    HomeData inverterData = getInverterData();
    sendHttpResponse(client, 200, "application/json", buildInfoJson(inverterData));
    return;
  }

  if (method == "POST" && path == "/api/power") {
    // Extract power value from JSON body
    String rawPower = getJsonValueByKey(body, "power");
    if (rawPower.length() == 0) {
      sendHttpResponse(client, 400, "application/json", buildErrorJson("body must contain power value"));
      return;
    }

    // Convert to int and validate
    int requestedPower;
    if (!parseStringToInt(rawPower, requestedPower)) {
      sendHttpResponse(client, 400, "application/json", buildErrorJson("power must be an integer value in watts"));
      return;
    }

    if (requestedPower < 0 || requestedPower > INVERTER_MAX_POWER_WATTS) {
      String msg = String("power must satisfy 0 <= power <= ") + INVERTER_MAX_POWER_WATTS;
      sendHttpResponse(client, 400, "application/json", buildErrorJson(msg));
      return;
    }

    // Send power command to inverter (combined /postoptions form).
    String inverterResponse;
    String errorMsg;
    int inverterHttpCode = 0;
    InverterController::SetResult result = InverterController::getInstance().setPower(
        requestedPower, inverterResponse, inverterHttpCode, errorMsg);

    if (result == InverterController::SetResult::Rejected) {
      sendHttpResponse(client, 400, "application/json", buildErrorJson(errorMsg));
      return;
    }
    if (result == InverterController::SetResult::Deferred) {
      String resp = JsonBuilder()
        .addBool("deferred", true)
        .addNumber("desired_power_watts", String(requestedPower))
        .addString("reason", errorMsg)
        .build();
      sendHttpResponse(client, 202, "application/json", resp);
      return;
    }

    // Applied: include live read-back from the cache (refreshed inside setPower).
    JsonBuilder rb;
    rb.addNumber("requested_power_watts", String(requestedPower))
      .addBool("applied", true)
      .addNumber("inverter_http_status", String(inverterHttpCode))
      .addString("inverter_response", inverterResponse);
    uint16_t readback = 0;
    if (InverterController::getInstance().getPowerLimit(readback)) {
      rb.addNumber("readback_power_watts", String(readback));
    } else {
      rb.addNull("readback_power_watts");
    }
    sendHttpResponse(client, 200, "application/json", rb.build());
    return;
  }

  if (method == "POST" && path == "/api/shadow") {
    // Extract enabled flag from JSON body
    String rawEnabled = getJsonValueByKey(body, "enabled");
    if (rawEnabled.length() == 0) {
      sendHttpResponse(client, 400, "application/json", buildErrorJson("body must contain enabled boolean"));
      return;
    }
    rawEnabled.toLowerCase();
    bool enabled;
    if (rawEnabled == "true" || rawEnabled == "1") {
      enabled = true;
    } else if (rawEnabled == "false" || rawEnabled == "0") {
      enabled = false;
    } else {
      sendHttpResponse(client, 400, "application/json", buildErrorJson("enabled must be a boolean"));
      return;
    }

    String inverterResponse;
    String errorMsg;
    int inverterHttpCode = 0;
    InverterController::SetResult result = InverterController::getInstance().setShadow(
        enabled, inverterResponse, inverterHttpCode, errorMsg);

    if (result == InverterController::SetResult::Rejected) {
      sendHttpResponse(client, 400, "application/json", buildErrorJson(errorMsg));
      return;
    }
    if (result == InverterController::SetResult::Deferred) {
      String resp = JsonBuilder()
        .addBool("deferred", true)
        .addBool("desired_shadow", enabled)
        .addString("reason", errorMsg)
        .build();
      sendHttpResponse(client, 202, "application/json", resp);
      return;
    }

    JsonBuilder rb;
    rb.addBool("requested_shadow", enabled)
      .addBool("applied", true)
      .addNumber("inverter_http_status", String(inverterHttpCode))
      .addString("inverter_response", inverterResponse);
    bool readback = false;
    if (InverterController::getInstance().getShadow(readback)) {
      rb.addBool("readback_shadow", readback);
    } else {
      rb.addNull("readback_shadow");
    }
    sendHttpResponse(client, 200, "application/json", rb.build());
    return;
  }

  if (method == "POST" && path == "/api/inverter/fetch") {
    // Parse inverter URL from JSON request body (e.g. {"url":"/home"})
    String inverterPath;
    if (!parseFetchUrlFromBody(body, inverterPath)) {
      sendHttpResponse(client, 400, "application/json", buildErrorJson("body must contain JSON url field, e.g. {\"url\":\"/home\"}"));
      return;
    }

    // Validate that URL starts with "/"
    if (!inverterPath.startsWith("/")) {
      sendHttpResponse(client, 400, "application/json", buildErrorJson("url must start with '/'"));
      return;
    }

    // Fetch the inverter endpoint and return raw response
    String inverterBody;
    String err;
    int code = 0;
    bool ok = InverterController::getInstance().fetchPath(inverterPath, inverterBody, code, err);
    if (!ok) {
      // Inverter communication failed
      sendHttpResponse(client, 502, "application/json", buildErrorJson(err));
      return;
    }

    sendHttpResponse(client, 200, "text/plain", inverterBody);
    return;
  }

  if (method == "GET" && path == "/api/mqtt") {
    MqttSettings ms = MqttClient::getInstance().getSettings();
    String response = JsonBuilder()
      .addString("broker_ip", ms.brokerIp)
      .addNumber("broker_port", String(ms.brokerPort))
      .addBool("enabled", ms.enabled)
      .addString("topic_prefix", ms.topicPrefix)
      .addString("username", ms.username)
      .addBool("has_password", ms.password.length() > 0)
      .addBool("connected", MqttClient::getInstance().isConnected())
      .build();
    sendHttpResponse(client, 200, "application/json", response);
    return;
  }

  if (method == "POST" && path == "/api/mqtt") {
    MqttSettings ms = MqttClient::getInstance().getSettings();

    // Parse optional fields from JSON body
    String rawIp = getJsonValueByKey(body, "broker_ip");
    if (rawIp.length() > 0) {
      // Validate IP format
      IPAddress testIp;
      if (!testIp.fromString(rawIp)) {
        sendHttpResponse(client, 400, "application/json", buildErrorJson("broker_ip must be a valid IP address"));
        return;
      }
      ms.brokerIp = rawIp;
    }

    String rawPort = getJsonValueByKey(body, "broker_port");
    if (rawPort.length() > 0) {
      int port = 0;
      if (!parseStringToInt(rawPort, port) || port < 1 || port > 65535) {
        sendHttpResponse(client, 400, "application/json", buildErrorJson("broker_port must be 1-65535"));
        return;
      }
      ms.brokerPort = (uint16_t)port;
    }

    String rawEnabled = getJsonValueByKey(body, "enabled");
    if (rawEnabled.length() > 0) {
      rawEnabled.toLowerCase();
      if (rawEnabled == "true" || rawEnabled == "1") {
        ms.enabled = true;
      } else if (rawEnabled == "false" || rawEnabled == "0") {
        ms.enabled = false;
      } else {
        sendHttpResponse(client, 400, "application/json", buildErrorJson("enabled must be a boolean"));
        return;
      }
    }

    String rawPrefix = getJsonValueByKey(body, "topic_prefix");
    if (rawPrefix.length() > 0) {
      if (rawPrefix.length() > 64) {
        sendHttpResponse(client, 400, "application/json", buildErrorJson("topic_prefix too long (max 64 chars)"));
        return;
      }
      ms.topicPrefix = rawPrefix;
    }

    String rawUsername = getJsonValueByKey(body, "username");
    if (rawUsername.length() > 0 || body.indexOf("\"username\"") >= 0) {
      ms.username = rawUsername;
    }

    String rawPassword = getJsonValueByKey(body, "password");
    if (rawPassword.length() > 0 || body.indexOf("\"password\"") >= 0) {
      ms.password = rawPassword;
    }

    // Save to NVS
    if (!saveMqttSettings(ms)) {
      sendHttpResponse(client, 500, "application/json", buildErrorJson("failed to save MQTT settings to flash"));
      return;
    }

    // Apply to running client
    MqttClient::getInstance().applySettings(ms);

    String response = JsonBuilder()
      .addString("broker_ip", ms.brokerIp)
      .addNumber("broker_port", String(ms.brokerPort))
      .addBool("enabled", ms.enabled)
      .addString("topic_prefix", ms.topicPrefix)
      .addString("username", ms.username)
      .addBool("has_password", ms.password.length() > 0)
      .addBool("connected", MqttClient::getInstance().isConnected())
      .build();
    sendHttpResponse(client, 200, "application/json", response);
    return;
  }

  if (method != "GET" && method != "POST") {
    sendHttpResponse(client, 405, "application/json", buildErrorJson("only GET and POST supported"));
    return;
  }

  // No endpoint matched the request
  sendHttpResponse(client, 404, "application/json", buildErrorJson("endpoint not found"));
}
