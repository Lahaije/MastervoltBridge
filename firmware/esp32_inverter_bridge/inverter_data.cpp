#include "inverter_data.h"

#include "inverter_controller.h"
#include "logger.h"

bool HomeData::isValid() const {
  return operatingStatus.length() > 0;
}

void HomeData::clear() {
  operatingStatus = "";
  errorAlarmCode = "";
  operatingMode = "";
  inverterModel = "";
  inverterMacAddress = "";
  instantaneousPower = "";
  lifetimeEnergy = "";
  dailySessionEnergy = "";
}

HomeData getInverterData() {
  HomeData data;
  InverterController::getInstance().getLatestHomeData(data);
  return data;
}

bool parseHomeResponse(const String& rawResponse, HomeData& dataOut) {
  dataOut.clear();

  // The /home response is 8 newline-delimited lines
  // Split by newline and populate HomeData fields
  
  int lines[9] = {0};  // Start positions of lines 0-8
  int lineCount = 0;

  // Find all line starts
  lines[0] = 0;
  lineCount = 1;

  for (int i = 0; i < (int)rawResponse.length() && lineCount < 9; i++) {
    if (rawResponse[i] == '\n') {
      lines[lineCount] = i + 1;
      lineCount++;
    }
  }

  // Extract the 8 fields
  auto extractLine = [&](int lineNum) -> String {
    if (lineNum >= lineCount) {
      return "";
    }

    int start = lines[lineNum];
    int end = start;

    // Find the end of this line (either \n or end of string)
    while (end < (int)rawResponse.length() && rawResponse[end] != '\n') {
      end++;
    }

    // Trim whitespace
    while (start < end && (rawResponse[start] == ' ' || rawResponse[start] == '\r')) {
      start++;
    }
    while (end > start && (rawResponse[end - 1] == ' ' || rawResponse[end - 1] == '\r')) {
      end--;
    }

    if (end > start) {
      return rawResponse.substring(start, end);
    }
    return "";
  };

  if (lineCount < 8) {
    appLogger.log(String("[INVERTER-DATA] Expected 8 lines but got ") + lineCount);
    return false;
  }

  dataOut.operatingStatus = extractLine(0);
  dataOut.errorAlarmCode = extractLine(1);
  dataOut.operatingMode = extractLine(2);
  dataOut.inverterModel = extractLine(3);
  dataOut.inverterMacAddress = extractLine(4);
  dataOut.instantaneousPower = extractLine(5);
  dataOut.lifetimeEnergy = extractLine(6);
  dataOut.dailySessionEnergy = extractLine(7);

  return dataOut.isValid();
}
