#pragma once

#include <Arduino.h>
#include <map>

class ApiServer {
 public:
  bool begin();
  void loop();

 private:
  bool validateSession(const String &token) const;
  String userForSession(const String &token) const;
  String makeSession(const String &user);
  String readCookieValue(const String &cookieHeader, const String &name) const;
  String htmlEscape(const String &in) const;
  bool isAuthenticated(class AsyncWebServerRequest *request) const;
  void sendJson(class AsyncWebServerRequest *request, int code, const String &json) const;
  void setupRoutes();
  void setupWebSocket();
  String buildSystemJson() const;
  void broadcastStatus();

  mutable std::map<String, uint32_t> sessionsExpiry_;
  mutable std::map<String, String> sessionsUser_;
  String currentPage_ = "dashboard";
  uint32_t lastBroadcastMs_ = 0;
};

ApiServer &apiServer();
