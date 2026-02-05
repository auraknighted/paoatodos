#pragma once

#include <Arduino.h>
#include <LittleFS.h>

class LogManager {
 public:
  bool begin() {
    if (!LittleFS.begin(true)) {
      return false;
    }
    return true;
  }

  void logEvent(const String &message) {
    rotateIfNeeded(message.length());
    File file = LittleFS.open(logPath, "a");
    if (!file) {
      return;
    }
    file.println(message);
    file.close();
  }

  String readLogs() {
    File file = LittleFS.open(logPath, "r");
    if (!file) {
      return String();
    }
    String content = file.readString();
    file.close();
    return content;
  }

  String readOldLogs() {
    File file = LittleFS.open(oldPath, "r");
    if (!file) {
      return String();
    }
    String content = file.readString();
    file.close();
    return content;
  }

 private:
  void rotateIfNeeded(size_t incoming) {
    if (!LittleFS.exists(logPath)) {
      return;
    }
    File file = LittleFS.open(logPath, "r");
    if (!file) {
      return;
    }
    size_t size = file.size();
    file.close();
    if (size + incoming < maxSize) {
      return;
    }
    if (LittleFS.exists(oldPath)) {
      LittleFS.remove(oldPath);
    }
    LittleFS.rename(logPath, oldPath);
  }

  const char *logPath = "/logs.txt";
  const char *oldPath = "/logs_old.txt";
  const size_t maxSize = 50 * 1024;
};
