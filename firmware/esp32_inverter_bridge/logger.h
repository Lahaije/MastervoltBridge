#ifndef LOGGER_H
#define LOGGER_H

#include <Arduino.h>

constexpr int MAX_LOG_ENTRIES = 1000;

struct LogEntry {
  unsigned long timestamp;  // milliseconds since boot
  String message;
};

class Logger {
private:
  LogEntry logs[MAX_LOG_ENTRIES];
  int logCount = 0;

public:
  Logger() {
    // Initialize serial during construction
    Serial.begin(115200);
    delay(100);  // Give serial time to initialize
  }

  void log(const char* message) {
    unsigned long elapsedMs = millis();
    
    // Print to serial immediately
    Serial.print("[");
    Serial.print(elapsedMs);
    Serial.print("ms] ");
    Serial.println(message);
    
    // Store in log buffer if not full
    if (logCount < MAX_LOG_ENTRIES) {
      logs[logCount].timestamp = elapsedMs;
      logs[logCount].message = String(message);
      logCount++;
    } else {
      // Shift logs and add new one at the end (circular buffer behavior)
      for (int i = 0; i < MAX_LOG_ENTRIES - 1; i++) {
        logs[i] = logs[i + 1];
      }
      logs[MAX_LOG_ENTRIES - 1].timestamp = elapsedMs;
      logs[MAX_LOG_ENTRIES - 1].message = String(message);
    }
  }

  void log(const String& message) {
    log(message.c_str());
  }

  int getLogCount() const {
    return logCount;
  }

  const LogEntry& getLogEntry(int index) const {
    return logs[index];
  }
};

// Global logger instance
extern Logger appLogger;

#endif // LOGGER_H
