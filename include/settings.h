#pragma once

#include <Arduino.h>

struct TinyPiSettings {
  String hostname;
  String wifiSsid;
  String wifiPassword;
  String theme;
  String autoBootPage;
  String terminalFg;
  String terminalBg;
  String passwordSalt;
  String passwordHash;
  bool bootLogoEnabled;
  uint8_t oledBrightness;
};

class SettingsManager {
 public:
  bool begin();
  bool load();
  bool save() const;
  void resetToFactory();

  TinyPiSettings &data();
  const TinyPiSettings &data() const;

  String hashPassword(const String &password, const String &salt) const;
  bool verifyPassword(const String &password) const;
  void setPassword(const String &password);

 private:
  TinyPiSettings settings_;
  void applyDefaults();
};

SettingsManager &settingsManager();
