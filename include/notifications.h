#pragma once

#include <Arduino.h>
#include <WiFiClientSecure.h>

struct NotificationPayload {
  String message;
  String telegramToken;
  String telegramChatId;
  String discordWebhook;
};

class NotificationManager {
 public:
  void begin() { lastSentMs = 0; }

  bool canSend() {
    return (millis() - lastSentMs) > rateLimitMs;
  }

  void sendNotification(const NotificationPayload &payload) {
    if (!canSend()) {
      return;
    }
    lastSentMs = millis();

    if (payload.telegramToken.length() > 0 && payload.telegramChatId.length() > 0) {
      sendTelegram(payload);
    }
    if (payload.discordWebhook.length() > 0) {
      sendDiscord(payload);
    }
  }

 private:
  void sendTelegram(const NotificationPayload &payload) {
    WiFiClientSecure client;
    client.setInsecure();
    String host = "api.telegram.org";
    if (!client.connect(host.c_str(), 443)) {
      client.stop();
      return;
    }

    String url = "/bot" + payload.telegramToken + "/sendMessage";
    String body = "chat_id=" + payload.telegramChatId + "&text=" + urlencode(payload.message);

    client.print(String("POST ") + url + " HTTP/1.1\r\n" +
                 "Host: " + host + "\r\n" +
                 "Content-Type: application/x-www-form-urlencoded\r\n" +
                 "Content-Length: " + String(body.length()) + "\r\n\r\n" +
                 body);
    delay(10);
    client.stop();
  }

  void sendDiscord(const NotificationPayload &payload) {
    WiFiClientSecure client;
    client.setInsecure();
    String host = payload.discordWebhook;
    host.replace("https://", "");
    int slash = host.indexOf('/');
    String path = "/";
    if (slash > 0) {
      path += host.substring(slash + 1);
      host = host.substring(0, slash);
    }
    if (!client.connect(host.c_str(), 443)) {
      client.stop();
      return;
    }

    String body = String("{\"content\":\"") + escapeJson(payload.message) + "\"}";

    client.print(String("POST ") + path + " HTTP/1.1\r\n" +
                 "Host: " + host + "\r\n" +
                 "Content-Type: application/json\r\n" +
                 "Content-Length: " + String(body.length()) + "\r\n\r\n" +
                 body);
    delay(10);
    client.stop();
  }

  String escapeJson(const String &input) {
    String output;
    for (size_t i = 0; i < input.length(); i++) {
      char c = input[i];
      if (c == '"' || c == '\\') {
        output += '\\';
      }
      output += c;
    }
    return output;
  }

  String urlencode(const String &input) {
    String encoded;
    char hex[4];
    for (size_t i = 0; i < input.length(); i++) {
      char c = input[i];
      if (isalnum(static_cast<unsigned char>(c)) || c == '-' || c == '_' || c == '.' || c == '~') {
        encoded += c;
      } else {
        snprintf(hex, sizeof(hex), "%%%02X", static_cast<unsigned char>(c));
        encoded += hex;
      }
    }
    return encoded;
  }

  unsigned long lastSentMs = 0;
  const unsigned long rateLimitMs = 10000;
};
