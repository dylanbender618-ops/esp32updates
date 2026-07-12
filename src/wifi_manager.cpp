#include "wifi_manager.h"

#include <ArduinoJson.h>
#include <WiFi.h>

#include "settings.h"

namespace {
constexpr const char *kApSsid = "TinyPiOS";
constexpr const char *kApPassword = "tinypios123";
}  // namespace

bool WiFiManagerTiny::begin() {
  WiFi.mode(WIFI_MODE_NULL);
  WiFi.setHostname(settingsManager().data().hostname.c_str());

  const TinyPiSettings &cfg = settingsManager().data();
  if (!cfg.wifiSsid.isEmpty() && connectWithTimeout(cfg.wifiSsid, cfg.wifiPassword, 20000)) {
    mode_ = WifiModeState::STA_CONNECTED;
    return true;
  }

  startAccessPoint();
  return false;
}

void WiFiManagerTiny::loop() {
  if (mode_ == WifiModeState::STA_CONNECTED && WiFi.status() != WL_CONNECTED) {
    mode_ = WifiModeState::DISCONNECTED;
    lastAttemptMs_ = millis();
  }

  if (mode_ == WifiModeState::DISCONNECTED && millis() - lastAttemptMs_ > 8000) {
    const TinyPiSettings &cfg = settingsManager().data();
    if (!cfg.wifiSsid.isEmpty() && connectWithTimeout(cfg.wifiSsid, cfg.wifiPassword, 12000)) {
      mode_ = WifiModeState::STA_CONNECTED;
      return;
    }
    startAccessPoint();
  }
}

bool WiFiManagerTiny::connectStation(const String &ssid, const String &password, bool persist) {
  if (connectWithTimeout(ssid, password, 20000)) {
    mode_ = WifiModeState::STA_CONNECTED;
    if (persist) {
      TinyPiSettings &cfg = settingsManager().data();
      cfg.wifiSsid = ssid;
      cfg.wifiPassword = password;
      settingsManager().save();
    }
    return true;
  }
  startAccessPoint();
  return false;
}

void WiFiManagerTiny::startAccessPoint() {
  WiFi.mode(WIFI_AP);
  WiFi.softAP(kApSsid, kApPassword);
  mode_ = WifiModeState::AP_MODE;
}

WifiSnapshot WiFiManagerTiny::snapshot() const {
  WifiSnapshot s{};
  s.mode = mode_;
  if (mode_ == WifiModeState::STA_CONNECTED && WiFi.status() == WL_CONNECTED) {
    s.ssid = WiFi.SSID();
    s.ip = WiFi.localIP().toString();
    s.rssi = WiFi.RSSI();
    s.clients = 0;
  } else if (mode_ == WifiModeState::AP_MODE) {
    s.ssid = WiFi.softAPSSID();
    s.ip = WiFi.softAPIP().toString();
    s.rssi = 0;
    s.clients = static_cast<uint8_t>(WiFi.softAPgetStationNum());
  } else {
    s.ssid = "";
    s.ip = "0.0.0.0";
    s.rssi = 0;
    s.clients = 0;
  }
  return s;
}

String WiFiManagerTiny::scanNetworksJson() const {
  int found = WiFi.scanNetworks();
  DynamicJsonDocument doc(4096);
  JsonArray arr = doc.createNestedArray("networks");
  for (int i = 0; i < found; ++i) {
    JsonObject net = arr.createNestedObject();
    net["ssid"] = WiFi.SSID(i);
    net["rssi"] = WiFi.RSSI(i);
    net["enc"] = WiFi.encryptionType(i);
    net["channel"] = WiFi.channel(i);
  }
  doc["count"] = found;
  String out;
  serializeJson(doc, out);
  return out;
}

bool WiFiManagerTiny::isCaptivePortalMode() const {
  return mode_ == WifiModeState::AP_MODE;
}

bool WiFiManagerTiny::connectWithTimeout(const String &ssid, const String &password, uint32_t timeoutMs) {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid.c_str(), password.c_str());
  mode_ = WifiModeState::STA_CONNECTING;

  uint32_t start = millis();
  while (millis() - start < timeoutMs) {
    if (WiFi.status() == WL_CONNECTED) {
      return true;
    }
    delay(150);
  }
  WiFi.disconnect(true, true);
  return false;
}

WiFiManagerTiny &wifiManager() {
  static WiFiManagerTiny manager;
  return manager;
}
