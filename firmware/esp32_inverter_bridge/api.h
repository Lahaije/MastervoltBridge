#ifndef API_H
#define API_H

#include <Arduino.h>
#include <UIPEthernet.h>
#include "settings.h"
#include "wifi_bridge.h"
#include "inverter_monitor.h"
#include "inverter_data.h"
#include "api_helper.h"

const ApiEndpointInfo API_ENDPOINTS[API_ENDPOINT_COUNT] = {
  {"GET", "/", "API discovery and endpoint overview"},
  {"GET", "/api/health", "Bridge connectivity state: WiFi, Ethernet, inverter host, IPs"},
  {"GET", "/api/logs", "Retrieve up to 1000 cached log entries with millisecond timestamps"},
  {"GET", "/api/info", "Latest cached inverter /home telemetry: status, mode, power, energy (20s poll interval)"},
  {"POST", "/api/power", "Set inverter power: JSON body with power field, e.g. {\"power\":1200} (requires 0 < power < configured max)"},
  {"POST", "/api/inverter/fetch", "Fetch inverter endpoint: JSON body with url field, e.g. {\"url\":\"/home\"}"},
  {"GET", "/pulse", "Trigger WiFi module recovery: GPIO pulse sequence to wake inverter WiFi"}
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

  // Read request body if Content-Length > 0
  String body;
  if (contentLength > 0) {
    body.reserve(contentLength);
    while ((int)body.length() < contentLength && client.connected()) {
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

  if (method == "GET" && path == "/api/health") {
    sendHttpResponse(client, 200, "application/json", buildHealthJson());
    return;
  }

  if (method == "GET" && path == "/api/logs") {
    sendHttpResponse(client, 200, "application/json", buildLogsJson());
    return;
  }

  if (method == "GET" && path == "/pulse") {
    // Temporary test to see what the exact wifi behaviour of the inverter after a pulse is send.
    // Need to be edited in the final firmware, 
    measureConnectionTime();
    sendHttpResponse(client, 200, "application/json", "{\"status\":\"pulse_complete\"}");
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

    if (requestedPower <= 0 || requestedPower >= INVERTER_MAX_POWER_WATTS) {
      String msg = String("power must satisfy 0 < power < ") + INVERTER_MAX_POWER_WATTS;
      sendHttpResponse(client, 400, "application/json", buildErrorJson(msg));
      return;
    }

    // Send power command to inverter
    String inverterResponse;
    String errorMsg;
    int inverterHttpCode = 0;
    bool ok = InverterMonitor::getInstance().setPower(requestedPower, inverterResponse, inverterHttpCode, errorMsg);
    if (!ok) {
      // Inverter communication failed (WiFi down or HTTP error)
      sendHttpResponse(client, 502, "application/json", buildErrorJson(errorMsg));
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

#endif // API_H
