#pragma once

#include <Arduino.h>

enum class WifiModeState {
  STA_CONNECTED,
  STA_CONNECTING,
  AP_MODE,
  DISCONNECTED
};

struct WifiSnapshot {
  WifiModeState mode;
  String ssid;
  String ip;
  int32_t rssi;
  uint8_t clients;
};

class WiFiManagerTiny {
 public:
  bool begin();
  void loop();
  bool connectStation(const String &ssid, const String &password, bool persist);
  void startAccessPoint();
  WifiSnapshot snapshot() const;
  String scanNetworksJson() const;
  bool isCaptivePortalMode() const;

 private:
  bool connectWithTimeout(const String &ssid, const String &password, uint32_t timeoutMs);
  WifiModeState mode_ = WifiModeState::DISCONNECTED;
  uint32_t lastAttemptMs_ = 0;
};

WiFiManagerTiny &wifiManager();
