#include "api.h"

#include <ArduinoJson.h>
#include <AsyncJson.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#include <Update.h>
#include <esp_system.h>

#include "editor.h"
#include "filesystem.h"
#include "oled.h"
#include "settings.h"
#include "terminal.h"
#include "wifi.h"

namespace {
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");
constexpr size_t kUploadLimit = 1024 * 1024;
constexpr size_t kOtaLimit = 3 * 1024 * 1024;

String modeString(WifiModeState mode) {
  switch (mode) {
    case WifiModeState::STA_CONNECTED:
      return "sta_connected";
    case WifiModeState::STA_CONNECTING:
      return "sta_connecting";
    case WifiModeState::AP_MODE:
      return "ap_mode";
    default:
      return "disconnected";
  }
}

String formatUptime(uint32_t seconds) {
  uint32_t h = seconds / 3600;
  uint32_t m = (seconds % 3600) / 60;
  uint32_t s = seconds % 60;
  char buf[16];
  snprintf(buf, sizeof(buf), "%02lu:%02lu:%02lu", static_cast<unsigned long>(h),
           static_cast<unsigned long>(m), static_cast<unsigned long>(s));
  return String(buf);
}
}  // namespace

bool ApiServer::begin() {
  setupWebSocket();
  setupRoutes();
  server.begin();
  return true;
}

void ApiServer::loop() {
  ws.cleanupClients();
  if (millis() - lastBroadcastMs_ > 1000) {
    broadcastStatus();
    lastBroadcastMs_ = millis();
  }
}

bool ApiServer::validateSession(const String &token) const {
  if (token.isEmpty()) return false;
  auto it = sessionsExpiry_.find(token);
  if (it == sessionsExpiry_.end()) return false;
  if (millis() > it->second) {
    sessionsExpiry_.erase(token);
    sessionsUser_.erase(token);
    return false;
  }
  return true;
}

String ApiServer::userForSession(const String &token) const {
  auto it = sessionsUser_.find(token);
  return it == sessionsUser_.end() ? "guest" : it->second;
}

String ApiServer::makeSession(const String &user) {
  uint32_t a = esp_random();
  uint32_t b = esp_random();
  char token[25];
  snprintf(token, sizeof(token), "%08lx%08lx", static_cast<unsigned long>(a), static_cast<unsigned long>(b));
  String sid(token);
  sessionsExpiry_[sid] = millis() + (1000UL * 60UL * 60UL * 24UL);
  sessionsUser_[sid] = user;
  return sid;
}

String ApiServer::readCookieValue(const String &cookieHeader, const String &name) const {
  String pattern = name + "=";
  int start = cookieHeader.indexOf(pattern);
  if (start < 0) return "";
  start += pattern.length();
  int end = cookieHeader.indexOf(';', start);
  if (end < 0) end = cookieHeader.length();
  return cookieHeader.substring(start, end);
}

String ApiServer::htmlEscape(const String &in) const {
  String out;
  out.reserve(in.length() + 16);
  for (size_t i = 0; i < in.length(); ++i) {
    char c = in[i];
    switch (c) {
      case '&':
        out += "&amp;";
        break;
      case '<':
        out += "&lt;";
        break;
      case '>':
        out += "&gt;";
        break;
      case '"':
        out += "&quot;";
        break;
      case '\'':
        out += "&#39;";
        break;
      default:
        out += c;
    }
  }
  return out;
}

bool ApiServer::isAuthenticated(AsyncWebServerRequest *request) const {
  if (!request->hasHeader("Cookie")) return false;
  String sid = readCookieValue(request->header("Cookie"), "TPSID");
  return validateSession(sid);
}

void ApiServer::sendJson(AsyncWebServerRequest *request, int code, const String &json) const {
  AsyncWebServerResponse *res = request->beginResponse(code, "application/json", json);
  res->addHeader("Cache-Control", "no-store");
  request->send(res);
}

String ApiServer::buildSystemJson() const {
  DynamicJsonDocument doc(2048);
  WifiSnapshot wifi = wifiManager().snapshot();
  FsUsage fs = filesystemManager().usage();

  doc["hostname"] = settingsManager().data().hostname;
  doc["ip"] = wifi.ip;
  doc["wifiMode"] = modeString(wifi.mode);
  doc["ssid"] = wifi.ssid;
  doc["rssi"] = wifi.rssi;
  doc["clients"] = wifi.clients;
  doc["cpuFreqMHz"] = ESP.getCpuFreqMHz();
  doc["freeHeap"] = ESP.getFreeHeap();
  doc["flashSize"] = ESP.getFlashChipSize();
  doc["flashSpeed"] = ESP.getFlashChipSpeed();
  doc["fsUsed"] = static_cast<uint32_t>(fs.usedBytes);
  doc["fsTotal"] = static_cast<uint32_t>(fs.totalBytes);
  doc["uptimeSec"] = millis() / 1000;
  doc["uptime"] = formatUptime(millis() / 1000);
  doc["time"] = millis() / 1000;
  doc["oledStatus"] = oledManager().statusText();
  doc["page"] = currentPage_;

  String out;
  serializeJson(doc, out);
  return out;
}

void ApiServer::broadcastStatus() {
  DynamicJsonDocument doc(3072);
  DynamicJsonDocument payload(2048);
  deserializeJson(payload, buildSystemJson());
  doc["type"] = "status";
  doc["payload"] = payload.as<JsonVariant>();
  String out;
  serializeJson(doc, out);
  ws.textAll(out);
}

void ApiServer::setupWebSocket() {
  ws.onEvent([this](AsyncWebSocket *serverWs, AsyncWebSocketClient *client, AwsEventType type, void *arg,
                    uint8_t *data, size_t len) {
    (void)serverWs;
    (void)arg;
    if (type == WS_EVT_CONNECT) {
      client->text(String("{\"type\":\"hello\",\"id\":") + client->id() + "}");
      return;
    }

    if (type == WS_EVT_DATA) {
      AwsFrameInfo *info = reinterpret_cast<AwsFrameInfo *>(arg);
      if (!info->final || info->index != 0 || info->len != len || info->opcode != WS_TEXT) {
        return;
      }
      String payload;
      payload.reserve(len);
      for (size_t i = 0; i < len; ++i) payload += static_cast<char>(data[i]);
      DynamicJsonDocument doc(512);
      if (deserializeJson(doc, payload)) return;

      String action = doc["action"] | "";
      if (action == "setPage") {
        currentPage_ = doc["page"] | "dashboard";
        oledManager().setCurrentPage(currentPage_);
      }
    }
  });
  server.addHandler(&ws);
}

void ApiServer::setupRoutes() {
  server.serveStatic("/", LittleFS, "/web/").setDefaultFile("index.html");

  server.on("/api/session", HTTP_GET, [this](AsyncWebServerRequest *request) {
    bool ok = isAuthenticated(request);
    sendJson(request, 200, String("{\"authenticated\":") + (ok ? "true" : "false") + "}");
  });

  auto loginHandler = new AsyncCallbackJsonWebHandler("/api/login", [this](AsyncWebServerRequest *request, JsonVariant &json) {
    JsonObject body = json.as<JsonObject>();
    String username = body["username"] | "admin";
    String password = body["password"] | "";

    if (!settingsManager().verifyPassword(password)) {
      sendJson(request, 401, "{\"ok\":false,\"error\":\"Invalid credentials\"}");
      return;
    }

    String sid = makeSession(username);
    oledManager().setCurrentUser(username);
    AsyncWebServerResponse *res = request->beginResponse(200, "application/json", "{\"ok\":true}");
    res->addHeader("Set-Cookie", "TPSID=" + sid + "; Path=/; HttpOnly; SameSite=Lax");
    request->send(res);
  });
  server.addHandler(loginHandler);

  server.on("/api/logout", HTTP_POST, [this](AsyncWebServerRequest *request) {
    AsyncWebServerResponse *res = request->beginResponse(200, "application/json", "{\"ok\":true}");
    res->addHeader("Set-Cookie", "TPSID=; Path=/; Max-Age=0");
    request->send(res);
  });

  server.on("/api/system", HTTP_GET, [this](AsyncWebServerRequest *request) {
    if (!isAuthenticated(request)) {
      sendJson(request, 401, "{\"error\":\"auth required\"}");
      return;
    }
    if (request->hasParam("page")) {
      currentPage_ = request->getParam("page")->value();
      oledManager().setCurrentPage(currentPage_);
    }
    sendJson(request, 200, buildSystemJson());
  });

  server.on("/api/files", HTTP_GET, [this](AsyncWebServerRequest *request) {
    if (!isAuthenticated(request)) {
      sendJson(request, 401, "{\"error\":\"auth required\"}");
      return;
    }
    String path = request->hasParam("path") ? request->getParam("path")->value() : "/";
    if (!filesystemManager().isValidPath(filesystemManager().normalizePath(path))) {
      sendJson(request, 400, "{\"error\":\"invalid path\"}");
      return;
    }
    sendJson(request, 200, filesystemManager().listJson(path));
  });

  auto filesMutateHandler = new AsyncCallbackJsonWebHandler("/api/files", [this](AsyncWebServerRequest *request, JsonVariant &json) {
    if (!isAuthenticated(request)) {
      sendJson(request, 401, "{\"error\":\"auth required\"}");
      return;
    }
    JsonObject body = json.as<JsonObject>();
    String action = body["action"] | "";
    String path = body["path"] | "";
    String target = body["target"] | "";
    bool ok = false;

    if (action == "mkdir") {
      ok = filesystemManager().mkdir(path);
    } else if (action == "delete") {
      ok = filesystemManager().removePath(path);
    } else if (action == "rename") {
      ok = filesystemManager().renamePath(path, target);
    } else if (action == "copy") {
      ok = filesystemManager().copyPath(path, target);
    } else if (action == "create") {
      ok = filesystemManager().writeFile(path, body["content"] | "");
    }

    sendJson(request, ok ? 200 : 400, String("{\"ok\":") + (ok ? "true" : "false") + "}");
  });
  filesMutateHandler->setMethod(HTTP_POST);
  server.addHandler(filesMutateHandler);

  server.on("/api/download", HTTP_GET, [this](AsyncWebServerRequest *request) {
    if (!isAuthenticated(request)) {
      sendJson(request, 401, "{\"error\":\"auth required\"}");
      return;
    }
    if (!request->hasParam("path")) {
      sendJson(request, 400, "{\"error\":\"missing path\"}");
      return;
    }
    String path = filesystemManager().normalizePath(request->getParam("path")->value());
    if (!filesystemManager().isValidPath(path) || !LittleFS.exists(path)) {
      sendJson(request, 404, "{\"error\":\"not found\"}");
      return;
    }
    request->send(LittleFS, path, "application/octet-stream", true);
  });

  server.on(
      "/api/upload", HTTP_POST,
      [this](AsyncWebServerRequest *request) {
        if (!isAuthenticated(request)) {
          sendJson(request, 401, "{\"error\":\"auth required\"}");
          return;
        }
        sendJson(request, 200, "{\"ok\":true}");
      },
      [this](AsyncWebServerRequest *request, const String &filename, size_t index, uint8_t *data, size_t len,
             bool final) {
        if (!isAuthenticated(request)) return;
        String target = "/" + filename;
        if (request->hasParam("path", true)) {
          target = filesystemManager().normalizePath(request->getParam("path", true)->value() + "/" + filename);
        }
        if (!filesystemManager().isValidPath(target) || (index + len) > kUploadLimit) return;

        if (index == 0) {
          request->_tempFile = LittleFS.open(target, "w");
        }
        if (request->_tempFile) {
          request->_tempFile.write(data, len);
        }
        if (final && request->_tempFile) {
          request->_tempFile.close();
        }
      });

  auto terminalHandler = new AsyncCallbackJsonWebHandler("/api/terminal", [this](AsyncWebServerRequest *request, JsonVariant &json) {
    if (!isAuthenticated(request)) {
      sendJson(request, 401, "{\"error\":\"auth required\"}");
      return;
    }
    String sid = readCookieValue(request->header("Cookie"), "TPSID");
    String user = userForSession(sid);
    String command = json["command"] | "";
    TerminalResponse tr = terminalManager().execute(command, user);

    DynamicJsonDocument doc(1536);
    doc["ok"] = true;
    doc["clear"] = tr.clearScreen;
    doc["output"] = htmlEscape(tr.output);
    doc["cwd"] = terminalManager().cwd();
    String out;
    serializeJson(doc, out);

    sendJson(request, 200, out);

    DynamicJsonDocument wsDoc(1536);
    wsDoc["type"] = "terminal";
    wsDoc["command"] = command;
    wsDoc["output"] = tr.output;
    String wsOut;
    serializeJson(wsDoc, wsOut);
    ws.textAll(wsOut);
  });
  server.addHandler(terminalHandler);

  auto editorSaveHandler = new AsyncCallbackJsonWebHandler("/api/editor/save", [this](AsyncWebServerRequest *request, JsonVariant &json) {
    if (!isAuthenticated(request)) {
      sendJson(request, 401, "{\"error\":\"auth required\"}");
      return;
    }
    String path = json["path"] | "";
    String content = json["content"] | "";
    bool ok = editorManager().save(path, content);
    sendJson(request, ok ? 200 : 400, String("{\"ok\":") + (ok ? "true" : "false") + "}");
  });
  server.addHandler(editorSaveHandler);

  server.on("/api/editor/load", HTTP_GET, [this](AsyncWebServerRequest *request) {
    if (!isAuthenticated(request)) {
      sendJson(request, 401, "{\"error\":\"auth required\"}");
      return;
    }
    if (!request->hasParam("path")) {
      sendJson(request, 400, "{\"error\":\"missing path\"}");
      return;
    }
    String path = request->getParam("path")->value();
    DynamicJsonDocument doc(4096);
    doc["path"] = path;
    doc["content"] = editorManager().load(path);
    String out;
    serializeJson(doc, out);
    sendJson(request, 200, out);
  });

  server.on("/api/settings", HTTP_GET, [this](AsyncWebServerRequest *request) {
    if (!isAuthenticated(request)) {
      sendJson(request, 401, "{\"error\":\"auth required\"}");
      return;
    }

    const TinyPiSettings &cfg = settingsManager().data();
    DynamicJsonDocument doc(1024);
    doc["hostname"] = cfg.hostname;
    doc["theme"] = cfg.theme;
    doc["autoBootPage"] = cfg.autoBootPage;
    doc["terminalFg"] = cfg.terminalFg;
    doc["terminalBg"] = cfg.terminalBg;
    doc["bootLogoEnabled"] = cfg.bootLogoEnabled;
    doc["oledBrightness"] = cfg.oledBrightness;
    doc["wifiSsid"] = cfg.wifiSsid;

    String out;
    serializeJson(doc, out);
    sendJson(request, 200, out);
  });

  auto settingsPost = new AsyncCallbackJsonWebHandler("/api/settings", [this](AsyncWebServerRequest *request, JsonVariant &json) {
    if (!isAuthenticated(request)) {
      sendJson(request, 401, "{\"error\":\"auth required\"}");
      return;
    }

    TinyPiSettings &cfg = settingsManager().data();
    JsonObject obj = json.as<JsonObject>();
    if (obj["factoryReset"].as<bool>()) {
      settingsManager().resetToFactory();
      sendJson(request, 200, "{"ok":true}");
      return;
    }
    if (obj.containsKey("hostname")) cfg.hostname = obj["hostname"].as<String>();
    if (obj.containsKey("theme")) cfg.theme = obj["theme"].as<String>();
    if (obj.containsKey("autoBootPage")) cfg.autoBootPage = obj["autoBootPage"].as<String>();
    if (obj.containsKey("terminalFg")) cfg.terminalFg = obj["terminalFg"].as<String>();
    if (obj.containsKey("terminalBg")) cfg.terminalBg = obj["terminalBg"].as<String>();
    if (obj.containsKey("bootLogoEnabled")) cfg.bootLogoEnabled = obj["bootLogoEnabled"].as<bool>();
    if (obj.containsKey("oledBrightness")) cfg.oledBrightness = obj["oledBrightness"].as<uint8_t>();
    if (obj.containsKey("password") && obj["password"].as<String>().length() >= 4) {
      settingsManager().setPassword(obj["password"].as<String>());
    }

    oledManager().setBrightness(cfg.oledBrightness);
    bool ok = settingsManager().save();
    sendJson(request, ok ? 200 : 500, String("{\"ok\":") + (ok ? "true" : "false") + "}");
  });
  server.addHandler(settingsPost);

  server.on("/api/wifi", HTTP_GET, [this](AsyncWebServerRequest *request) {
    if (!isAuthenticated(request) && !wifiManager().isCaptivePortalMode()) {
      sendJson(request, 401, "{\"error\":\"auth required\"}");
      return;
    }
    DynamicJsonDocument doc(2048);
    WifiSnapshot s = wifiManager().snapshot();
    doc["mode"] = modeString(s.mode);
    doc["ssid"] = s.ssid;
    doc["ip"] = s.ip;
    doc["rssi"] = s.rssi;
    if (request->hasParam("scan")) {
      DynamicJsonDocument scanDoc(4096);
      deserializeJson(scanDoc, wifiManager().scanNetworksJson());
      doc["scan"] = scanDoc["networks"];
    }
    String out;
    serializeJson(doc, out);
    sendJson(request, 200, out);
  });

  auto wifiSetHandler = new AsyncCallbackJsonWebHandler("/api/wifi", [this](AsyncWebServerRequest *request, JsonVariant &json) {
    JsonObject body = json.as<JsonObject>();
    String ssid = body["ssid"] | "";
    String password = body["password"] | "";
    if (ssid.isEmpty()) {
      sendJson(request, 400, "{\"ok\":false,\"error\":\"ssid required\"}");
      return;
    }
    bool ok = wifiManager().connectStation(ssid, password, true);
    sendJson(request, ok ? 200 : 400, String("{\"ok\":") + (ok ? "true" : "false") + "}");
  });
  server.addHandler(wifiSetHandler);

  server.on("/api/reboot", HTTP_POST, [this](AsyncWebServerRequest *request) {
    if (!isAuthenticated(request)) {
      sendJson(request, 401, "{\"error\":\"auth required\"}");
      return;
    }
    sendJson(request, 200, "{\"ok\":true,\"message\":\"rebooting\"}");
    delay(250);
    ESP.restart();
  });

  server.on(
      "/api/ota", HTTP_POST,
      [this](AsyncWebServerRequest *request) {
        if (!isAuthenticated(request)) {
          sendJson(request, 401, "{\"error\":\"auth required\"}");
          return;
        }
        bool ok = !Update.hasError();
        sendJson(request, ok ? 200 : 500, String("{\"ok\":") + (ok ? "true" : "false") + "}");
        if (ok) {
          delay(600);
          ESP.restart();
        }
      },
      [this](AsyncWebServerRequest *request, const String &filename, size_t index, uint8_t *data, size_t len,
             bool final) {
        if (!isAuthenticated(request)) return;
        if (index == 0) {
          if (!Update.begin(kOtaLimit)) {
            return;
          }
        }
        if (index + len > kOtaLimit) {
          Update.abort();
          return;
        }
        Update.write(data, len);
        if (final) {
          Update.end(true);
        }
      });

  server.onNotFound([this](AsyncWebServerRequest *request) {
    if (wifiManager().isCaptivePortalMode() && request->method() == HTTP_GET) {
      request->redirect("/");
      return;
    }
    sendJson(request, 404, "{\"error\":\"not found\"}");
  });
}

ApiServer &apiServer() {
  static ApiServer api;
  return api;
}
