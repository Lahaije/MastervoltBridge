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
  int head = 0;   // Index where next entry will be written
  int count = 0;  // Number of entries currently stored (0..MAX_LOG_ENTRIES)

public:
  Logger() {}

  void init() {
    Serial.begin(115200);
    delay(100);
  }

  void log(const char* message) {
    unsigned long elapsedMs = millis();

    Serial.print("[");
    Serial.print(elapsedMs);
    Serial.print("ms] ");
    Serial.println(message);

    // Write at head position (overwrites oldest when full)
    logs[head].timestamp = elapsedMs;
    logs[head].message = String(message);
    head = (head + 1) % MAX_LOG_ENTRIES;
    if (count < MAX_LOG_ENTRIES) {
      count++;
    }
  }

  void log(const String& message) {
    log(message.c_str());
  }

  int getLogCount() const {
    return count;
  }

  /**
   * Get log entry by logical index (0 = oldest).
   * Caller must ensure 0 <= index < getLogCount().
   */
  const LogEntry& getLogEntry(int index) const {
    // Oldest entry starts at head when full, or at 0 when not yet full
    int start = (count < MAX_LOG_ENTRIES) ? 0 : head;
    return logs[(start + index) % MAX_LOG_ENTRIES];
  }
};

// Global logger instance
extern Logger appLogger;

#endif // LOGGER_H
