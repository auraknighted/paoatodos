#pragma once

#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include <AsyncElegantOTA.h>
#include <DNSServer.h>
#include <ESP32SSDP.h>
#include <WiFi.h>
#include "config_manager.h"
#include "logs.h"

class WebServerManager {
 public:
  WebServerManager(ConfigManager &configManager, LogManager &logManager)
      : server(80), websocket("/ws"), configManager(configManager), logManager(logManager) {}

  void begin() {
    dnsServer.start(53, "configurar.me", WiFi.softAPIP());
    setupRoutes();
    AsyncElegantOTA.begin(&server);
    server.addHandler(&websocket);
    server.begin();
    setupSSDP();
  }

  void loop() {
    dnsServer.processNextRequest();
  }

  void broadcastStatus(const String &payload) {
    websocket.textAll(payload);
  }

  AsyncWebSocket &getWebsocket() { return websocket; }

 private:
  void setupRoutes() {
    server.on("/", HTTP_GET, [&](AsyncWebServerRequest *request) {
      request->send(LittleFS, "/index.html", "text/html");
    });

    server.on("/api/config", HTTP_GET, [&](AsyncWebServerRequest *request) {
      StaticJsonDocument<512> doc;
      const Config &config = configManager.get();
      doc["maintenanceMode"] = config.maintenanceMode;
      doc["powerRecoveryEnabled"] = config.powerRecoveryEnabled;
      doc["schedulesEnabled"] = config.schedulesEnabled;
      doc["ntpOffset"] = config.ntpOffset;
      String response;
      serializeJson(doc, response);
      request->send(200, "application/json", response);
    });

    server.on("/api/config", HTTP_POST, [&](AsyncWebServerRequest *request) {
      bool changed = false;
      if (request->hasParam("maintenanceMode", true)) {
        configManager.getMutable().maintenanceMode = request->getParam("maintenanceMode", true)->value() == "1";
        changed = true;
      }
      if (request->hasParam("powerRecoveryEnabled", true)) {
        configManager.getMutable().powerRecoveryEnabled = request->getParam("powerRecoveryEnabled", true)->value() == "1";
        changed = true;
      }
      if (request->hasParam("schedulesEnabled", true)) {
        configManager.getMutable().schedulesEnabled = request->getParam("schedulesEnabled", true)->value() == "1";
        changed = true;
      }
      if (request->hasParam("ntpOffset", true)) {
        configManager.getMutable().ntpOffset = request->getParam("ntpOffset", true)->value().toInt();
        changed = true;
      }
      if (changed) {
        configManager.save();
      }
      request->send(200, "application/json", "{\"status\":\"ok\"}");
    });

    server.on("/api/logs", HTTP_GET, [&](AsyncWebServerRequest *request) {
      String logs = logManager.readLogs();
      request->send(200, "text/plain", logs);
    });

    server.on("/api/manual", HTTP_GET, [&](AsyncWebServerRequest *request) {
      String manual = String("<h2>Manual Zenith-PC-Control</h2>")
                      + "<p>Alexa + Arduino IoT Cloud: usa pcPower y pcStatus.</p>"
                      + "<p>Telegram: crea un bot con BotFather y copia token/chatId.</p>"
                      + "<p>Discord: crea un webhook en tu canal y pega la URL.</p>";
      request->send(200, "text/html", manual);
    });

    server.on("/api/backup", HTTP_GET, [&](AsyncWebServerRequest *request) {
      String payload;
      if (!configManager.backup(payload)) {
        request->send(500, "application/json", "{\"error\":\"backup_failed\"}");
        return;
      }
      request->send(200, "application/json", payload);
    });

    server.on("/api/restore", HTTP_POST, [&](AsyncWebServerRequest *request) {
      if (!request->hasParam("payload", true)) {
        request->send(400, "application/json", "{\"error\":\"missing_payload\"}");
        return;
      }
      String payload = request->getParam("payload", true)->value();
      if (!configManager.restore(payload)) {
        request->send(500, "application/json", "{\"error\":\"restore_failed\"}");
        return;
      }
      request->send(200, "application/json", "{\"status\":\"ok\"}");
    });
  }

  void setupSSDP() {
    SSDP.setSchemaURL("/description.xml");
    SSDP.setHTTPPort(80);
    SSDP.setName(configManager.get().deviceName);
    SSDP.setSerialNumber("ZPC-01");
    SSDP.setURL("/");
    SSDP.setDeviceType("urn:schemas-upnp-org:device:Basic:1");
    SSDP.setManufacturer("Zenith");
    SSDP.setModelName("Zenith-PC-Control");
    SSDP.begin();
  }

  AsyncWebServer server;
  AsyncWebSocket websocket;
  DNSServer dnsServer;
  ConfigManager &configManager;
  LogManager &logManager;
};
