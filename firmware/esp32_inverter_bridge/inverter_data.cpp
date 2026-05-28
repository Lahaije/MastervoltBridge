#include "inverter_data.h"

#include "inverter_controller.h"
#include "logger.h"

namespace {

String normalizeDecimalString(const String& input) {
  String trimmed = input;
  trimmed.trim();

  if (trimmed.length() == 0) {
    return "";
  }

  int pos = 0;
  bool isNegative = false;
  if (trimmed[pos] == '+' || trimmed[pos] == '-') {
    isNegative = (trimmed[pos] == '-');
    pos++;
    if (pos >= (int)trimmed.length()) {
      return input;
    }
  }

  int dotIndex = -1;
  bool hasDigit = false;
  for (int i = pos; i < (int)trimmed.length(); i++) {
    char c = trimmed[i];
    if (c == '.') {
      if (dotIndex >= 0) {
        return input;
      }
      dotIndex = i;
      continue;
    }
    if (!isDigit(c)) {
      return input;
    }
    hasDigit = true;
  }

  if (!hasDigit) {
    return input;
  }

  const int integerStart = pos;
  const int integerEnd = (dotIndex >= 0) ? dotIndex : (int)trimmed.length();
  const int fractionStart = (dotIndex >= 0) ? dotIndex + 1 : -1;

  int firstNonZero = integerStart;
  while (firstNonZero < integerEnd && trimmed[firstNonZero] == '0') {
    firstNonZero++;
  }

  String normalized;
  if (isNegative) {
    normalized += '-';
  }

  // Keep a single zero for values like 0000.123 or 0000.
  if (firstNonZero >= integerEnd) {
    normalized += '0';
  } else {
    normalized += trimmed.substring(firstNonZero, integerEnd);
  }

  if (dotIndex >= 0) {
    normalized += '.';
    if (fractionStart < (int)trimmed.length()) {
      normalized += trimmed.substring(fractionStart);
    }
  }

  return normalized;
}

}  // namespace

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
  dataOut.lifetimeEnergy = normalizeDecimalString(extractLine(6));
  dataOut.dailySessionEnergy = normalizeDecimalString(extractLine(7));

  return dataOut.isValid();
}
