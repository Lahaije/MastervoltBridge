#include "api.h"

#include "settings.h"
#include "logger.h"
#include "wifi_bridge.h"
#include "inverter_monitor.h"
#include "inverter_data.h"
#include "api_helper.h"

const ApiEndpointInfo API_ENDPOINTS[API_ENDPOINT_COUNT] = {
  {"GET", "/", "API discovery and endpoint overview"},
  {"GET", "/api/version", "Firmware version currently running on this ESP"},
  {"GET", "/api/health", "Bridge connectivity state: WiFi, Ethernet, inverter host, IPs"},
  {"GET", "/api/logs", "Retrieve up to 1000 cached log entries with millisecond timestamps"},
  {"GET", "/api/info", "Latest cached inverter /home telemetry: status, mode, power, energy (runtime-configurable poll interval)"},
  {"POST", "/api/polling", "Set monitor polling interval in seconds: JSON body with seconds field, e.g. {\"seconds\":3} (1 <= seconds <= 3600)"},
  {"POST", "/api/power", String("Set inverter power: JSON body with power field, e.g. {\"power\":1200} (0 <= power <= ") + INVERTER_MAX_POWER_WATTS + "W)"},
  {"POST", "/api/inverter/fetch", "Fetch inverter endpoint: JSON body with url field, e.g. {\"url\":\"/home\"}"},
  {"POST", "/wifi/off", "If bridge WiFi is connected, send a single button press to turn inverter WiFi off"},
  {"GET", "/pulse", "Trigger WiFi module recovery: GPIO pulse sequence to wake inverter WiFi"},
  {"POST", "/api/debug", "Enable or disable debug mode: {\"debug\":true} logs HTTP 200 successes; {\"debug\":false} suppresses them"}
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

  // Read request body if Content-Length > 0.
  // client.setTimeout() only covers Stream read calls (readStringUntil, readBytes, etc.),
  // NOT this manual polling loop. Without an explicit deadline a client that sends
  // Content-Length: N but stalls before delivering all bytes would block the single
  // Ethernet service task indefinitely. We therefore cap total body-read time to
  // API_CLIENT_TIMEOUT_MS so a slow or malicious client cannot monopolize the task.
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
    sendHttpResponse(client, 200, "application/json", buildApiDiscoveryJson());
    return;
  }

  if (method == "GET" && path == "/api/version") {
    String response = JsonBuilder()
      .addString("firmware_version", String(FIRMWARE_VERSION))
      .build();
    sendHttpResponse(client, 200, "application/json", response);
    return;
  }

  if (method == "GET" && path == "/api/health") {
    sendHttpResponse(client, 200, "application/json", buildHealthJson());
    return;
  }

  if (method == "GET" && path == "/api/logs") {
    sendLogsResponse(client);
    return;
  }

  if (method == "GET" && path == "/pulse") {
    // forceWifiReconnect() pulses the wake GPIO and then runs a fresh connect
    // attempt using the next alternating path, so the resulting
    // [WIFI-CONNECT] log line captures real-world performance.
    bool ok = forceWifiReconnect();
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

  if (method == "POST" && path == "/api/polling") {
    String rawSeconds = getJsonValueByKey(body, "seconds");
    if (rawSeconds.length() == 0) {
      sendHttpResponse(client, 400, "application/json", buildErrorJson("body must contain seconds value"));
      return;
    }

    int requestedSeconds = 0;
    if (!parseStringToInt(rawSeconds, requestedSeconds)) {
      sendHttpResponse(client, 400, "application/json", buildErrorJson("seconds must be an integer value"));
      return;
    }

    uint32_t appliedMs = 0;
    String err;
    bool ok = InverterMonitor::getInstance().setPollingIntervalSeconds((uint32_t)requestedSeconds, appliedMs, err);
    if (!ok) {
      sendHttpResponse(client, 400, "application/json", buildErrorJson(err));
      return;
    }

    String response = JsonBuilder()
      .addNumber("poll_interval_seconds", String(appliedMs / 1000UL))
      .addNumber("poll_interval_ms", String(appliedMs))
      .build();
    sendHttpResponse(client, 200, "application/json", response);
    return;
  }
  
  if (method == "GET" && path == "/api/info") {
    // Retrieve latest cached inverter telemetry from polling task
    HomeData inverterData = getInverterData();
    if (!inverterData.isValid()) {
      // Polling hasn't completed yet or WiFi connection failed
      sendHttpResponse(client, 502, "application/json", buildErrorJson("No inverter telemetry data available yet"));
      return;
    }

    unsigned long lastUpdateMs = InverterMonitor::getInstance().getLastUpdateMs();
    sendHttpResponse(client, 200, "application/json", buildInfoJson(inverterData, lastUpdateMs));
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

    // Send power command to inverter
    String inverterResponse;
    String errorMsg;
    int inverterHttpCode = 0;
    bool ok = InverterMonitor::getInstance().setPower(requestedPower, inverterResponse, inverterHttpCode, errorMsg);
    if (!ok) {
      // Command could not be delivered immediately.
      // If queued (WiFi down), return 202; if validation failed, return 502.
      if (InverterMonitor::getInstance().isPowerCommandQueued()) {
        String response = JsonBuilder()
          .addNumber("requested_power_watts", String(requestedPower))
          .addString("status", "queued")
          .addString("message", "Inverter unreachable; command queued for retry")
          .build();
        sendHttpResponse(client, 202, "application/json", response);
      } else {
        sendHttpResponse(client, 502, "application/json", buildErrorJson(errorMsg));
      }
      return;
    }

    String response = JsonBuilder()
      .addNumber("requested_power_watts", String(requestedPower))
      .addNumber("inverter_http_status", String(inverterHttpCode))
      .addString("inverter_response", inverterResponse)
      .build();
    sendHttpResponse(client, 200, "application/json", response);
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
    bool ok = InverterMonitor::getInstance().fetchPath(inverterPath, inverterBody, code, err);
    if (!ok) {
      // Inverter communication failed
      sendHttpResponse(client, 502, "application/json", buildErrorJson(err));
      return;
    }

    sendHttpResponse(client, 200, "text/plain", inverterBody);
    return;
  }

  if (method != "GET" && method != "POST") {
    sendHttpResponse(client, 405, "application/json", buildErrorJson("only GET and POST supported"));
    return;
  }

  // No endpoint matched the request
  sendHttpResponse(client, 404, "application/json", buildErrorJson("endpoint not found"));
}
