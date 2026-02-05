#include <Arduino.h>
#include <WiFi.h>
#include <ESP32Ping.h>
#include <ArduinoIoTCloud.h>
#include <Arduino_ConnectionHandler.h>
#include <Preferences.h>
#include <LittleFS.h>
#include "config_manager.h"
#include "logs.h"
#include "notifications.h"
#include "web_server.h"

constexpr uint8_t PIN_POWER = 4;
constexpr uint8_t PIN_LED = 8;

ConfigManager configManager;
LogManager logManager;
NotificationManager notificationManager;
WebServerManager webServer(configManager, logManager);

String pcStatus = "OFF";
bool pcPower = false;

WiFiConnectionHandler ArduinoIoTPreferredConnection("", "");

TaskHandle_t uiTaskHandle;
TaskHandle_t pingTaskHandle;
TaskHandle_t notificationTaskHandle;

unsigned long lastWiFiAttemptMs = 0;
unsigned long wifiBackoffMs = 1000;
unsigned long lastPulseMs = 0;
unsigned long lastStatusBroadcastMs = 0;
unsigned long dailyUptimeMs = 0;
unsigned long lastUptimeTickMs = 0;

void setLedColor(uint8_t r, uint8_t g, uint8_t b) {
  analogWrite(PIN_LED, r);
  analogWrite(PIN_LED, g);
  analogWrite(PIN_LED, b);
}

void updateLed() {
  if (pcStatus == "Error") {
    setLedColor(255, 0, 0);
    return;
  }
  if (configManager.get().maintenanceMode) {
    setLedColor(0, 0, 30);
    return;
  }
  if (WiFi.status() != WL_CONNECTED) {
    static bool blink = false;
    blink = !blink;
    setLedColor(0, 0, blink ? 255 : 0);
    return;
  }
  setLedColor(0, 0, 255);
}

void safeBootCheck() {
  static unsigned long startMs = millis();
  if (WiFi.status() != WL_CONNECTED && millis() - startMs > 600000) {
    WiFi.softAP(configManager.get().deviceName);
  }
}

void reconnectWiFi() {
  if (WiFi.status() == WL_CONNECTED) {
    wifiBackoffMs = 1000;
    return;
  }
  if (millis() - lastWiFiAttemptMs < wifiBackoffMs) {
    return;
  }
  lastWiFiAttemptMs = millis();
  wifiBackoffMs = min(wifiBackoffMs * 2, 60000UL);
  WiFi.begin(configManager.get().wifiSsid, configManager.get().wifiPass);
}

void setPcPower(bool on) {
  if (millis() - lastPulseMs < 2000) {
    return;
  }
  lastPulseMs = millis();
  digitalWrite(PIN_POWER, on ? HIGH : LOW);
  pcPower = on;
}

void handlePingTask(void *param) {
  (void)param;
  int failures = 0;
  for (;;) {
    bool reachable = Ping.ping("192.168.1.10", 1);
    if (!reachable) {
      failures++;
      logManager.logEvent("Ping fallo");
      if (failures >= 3) {
        pcStatus = "OFF";
      }
    } else {
      failures = 0;
      pcStatus = "ON";
    }
    vTaskDelay(pdMS_TO_TICKS(5000));
  }
}

void handleNotificationTask(void *param) {
  (void)param;
  for (;;) {
    NotificationPayload payload;
    payload.message = "Estado PC: " + pcStatus;
    payload.telegramToken = configManager.get().telegramToken;
    payload.telegramChatId = configManager.get().telegramChatId;
    payload.discordWebhook = configManager.get().discordWebhook;
    notificationManager.sendNotification(payload);
    vTaskDelay(pdMS_TO_TICKS(10000));
  }
}

void handleUiTask(void *param) {
  (void)param;
  for (;;) {
    updateLed();
    safeBootCheck();
    reconnectWiFi();
    if (pcStatus == "ON") {
      unsigned long now = millis();
      if (lastUptimeTickMs == 0) {
        lastUptimeTickMs = now;
      }
      dailyUptimeMs += now - lastUptimeTickMs;
      lastUptimeTickMs = now;
    }
    if (millis() - lastStatusBroadcastMs > 2000) {
      StaticJsonDocument<256> doc;
      doc["pcStatus"] = pcStatus;
      doc["dailyUptime"] = dailyUptimeMs / 1000;
      doc["logs"] = logManager.readLogs();
      String payload;
      serializeJson(doc, payload);
      webServer.broadcastStatus(payload);
      lastStatusBroadcastMs = millis();
    }
    vTaskDelay(pdMS_TO_TICKS(200));
  }
}

void setupArduinoIoTCloud() {
  ArduinoCloud.addProperty(pcPower, READWRITE, ON_CHANGE, [](void) {
    if (!configManager.get().maintenanceMode) {
      setPcPower(pcPower);
    }
  });
  ArduinoCloud.addProperty(pcStatus, READ, ON_CHANGE);
  ArduinoCloud.begin(ArduinoIoTPreferredConnection);
}

void setup() {
  Serial.begin(115200);
  pinMode(PIN_POWER, OUTPUT);
  pinMode(PIN_LED, OUTPUT);

  WiFi.setTxPower(WIFI_POWER_8_5dBm);

  configManager.begin();
  logManager.begin();
  notificationManager.begin();

  WiFi.mode(WIFI_STA);
  WiFi.begin(configManager.get().wifiSsid, configManager.get().wifiPass);

  webServer.begin();

  setupArduinoIoTCloud();

  xTaskCreatePinnedToCore(handleUiTask, "uiTask", 4096, nullptr, 1, &uiTaskHandle, 1);
  xTaskCreatePinnedToCore(handlePingTask, "pingTask", 4096, nullptr, 2, &pingTaskHandle, 1);
  xTaskCreatePinnedToCore(handleNotificationTask, "notifyTask", 4096, nullptr, 1, &notificationTaskHandle, 1);
}

void loop() {
  ArduinoCloud.update();
  webServer.loop();
}
