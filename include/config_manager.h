#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <Preferences.h>

struct Config {
  char wifiSsid[32];
  char wifiPass[64];
  char deviceName[32];
  char telegramToken[128];
  char telegramChatId[32];
  char discordWebhook[196];
  char wolMacList[256];
  bool maintenanceMode;
  bool powerRecoveryEnabled;
  bool schedulesEnabled;
  int ntpOffset;
};

class ConfigManager {
 public:
  bool begin() {
    if (!LittleFS.begin(true)) {
      return false;
    }
    return load();
  }

  const Config &get() const { return config; }
  Config &getMutable() { return config; }

  bool load() {
    if (!LittleFS.exists(configPath)) {
      setDefaults();
      return save();
    }

    File file = LittleFS.open(configPath, "r");
    if (!file) {
      setDefaults();
      return false;
    }

    StaticJsonDocument<1024> doc;
    DeserializationError err = deserializeJson(doc, file);
    file.close();
    if (err) {
      setDefaults();
      return false;
    }

    strlcpy(config.wifiSsid, doc["wifiSsid"] | "", sizeof(config.wifiSsid));
    strlcpy(config.wifiPass, doc["wifiPass"] | "", sizeof(config.wifiPass));
    strlcpy(config.deviceName, doc["deviceName"] | "Zenith-PC-Control", sizeof(config.deviceName));
    strlcpy(config.telegramToken, doc["telegramToken"] | "", sizeof(config.telegramToken));
    strlcpy(config.telegramChatId, doc["telegramChatId"] | "", sizeof(config.telegramChatId));
    strlcpy(config.discordWebhook, doc["discordWebhook"] | "", sizeof(config.discordWebhook));
    strlcpy(config.wolMacList, doc["wolMacList"] | "", sizeof(config.wolMacList));
    config.maintenanceMode = doc["maintenanceMode"] | false;
    config.powerRecoveryEnabled = doc["powerRecoveryEnabled"] | true;
    config.schedulesEnabled = doc["schedulesEnabled"] | true;
    config.ntpOffset = doc["ntpOffset"] | 0;
    return true;
  }

  bool save() {
    File file = LittleFS.open(configPath, "w");
    if (!file) {
      return false;
    }

    StaticJsonDocument<1024> doc;
    doc["wifiSsid"] = config.wifiSsid;
    doc["wifiPass"] = config.wifiPass;
    doc["deviceName"] = config.deviceName;
    doc["telegramToken"] = config.telegramToken;
    doc["telegramChatId"] = config.telegramChatId;
    doc["discordWebhook"] = config.discordWebhook;
    doc["wolMacList"] = config.wolMacList;
    doc["maintenanceMode"] = config.maintenanceMode;
    doc["powerRecoveryEnabled"] = config.powerRecoveryEnabled;
    doc["schedulesEnabled"] = config.schedulesEnabled;
    doc["ntpOffset"] = config.ntpOffset;

    if (serializeJson(doc, file) == 0) {
      file.close();
      return false;
    }
    file.close();
    return true;
  }

  bool backup(String &out) {
    if (!LittleFS.exists(configPath)) {
      return false;
    }
    File file = LittleFS.open(configPath, "r");
    if (!file) {
      return false;
    }
    out = file.readString();
    file.close();
    return true;
  }

  bool restore(const String &payload) {
    File file = LittleFS.open(configPath, "w");
    if (!file) {
      return false;
    }
    file.print(payload);
    file.close();
    return load();
  }

  bool validateWebhook(const String &url) {
    return url.startsWith("https://") && url.length() < 190;
  }

  bool validateIp(const String &ip) {
    int parts = 0;
    int acc = 0;
    int digits = 0;
    for (size_t i = 0; i < ip.length(); i++) {
      char c = ip[i];
      if (c == '.') {
        if (digits == 0 || acc > 255) {
          return false;
        }
        parts++;
        acc = 0;
        digits = 0;
        continue;
      }
      if (!isdigit(static_cast<unsigned char>(c))) {
        return false;
      }
      acc = acc * 10 + (c - '0');
      digits++;
    }
    return parts == 3 && digits > 0 && acc <= 255;
  }

  bool validateMac(const String &mac) {
    if (mac.length() != 17) {
      return false;
    }
    for (int i = 0; i < 17; i++) {
      if ((i + 1) % 3 == 0) {
        if (mac[i] != ':') {
          return false;
        }
      } else {
        if (!isxdigit(static_cast<unsigned char>(mac[i]))) {
          return false;
        }
      }
    }
    return true;
  }

 private:
  void setDefaults() {
    memset(&config, 0, sizeof(config));
    strlcpy(config.deviceName, "Zenith-PC-Control", sizeof(config.deviceName));
    config.maintenanceMode = false;
    config.powerRecoveryEnabled = true;
    config.schedulesEnabled = true;
    config.ntpOffset = 0;
  }

  Config config{};
  const char *configPath = "/config.json";
};
