#include "settings.h"

#include <ArduinoJson.h>
#include <LittleFS.h>
#include <esp_system.h>
#include <mbedtls/sha256.h>

namespace {
constexpr const char *kSettingsPath = "/config/settings.json";

String randomSalt() {
  uint32_t a = esp_random();
  uint32_t b = esp_random();
  char buf[17];
  snprintf(buf, sizeof(buf), "%08lx%08lx", static_cast<unsigned long>(a), static_cast<unsigned long>(b));
  return String(buf);
}

String sha256Hex(const String &input) {
  uint8_t hash[32];
  mbedtls_sha256_context ctx;
  mbedtls_sha256_init(&ctx);
  mbedtls_sha256_starts_ret(&ctx, 0);
  mbedtls_sha256_update_ret(&ctx, reinterpret_cast<const unsigned char *>(input.c_str()), input.length());
  mbedtls_sha256_finish_ret(&ctx, hash);
  mbedtls_sha256_free(&ctx);

  char out[65];
  for (size_t i = 0; i < sizeof(hash); ++i) {
    sprintf(out + (i * 2), "%02x", hash[i]);
  }
  out[64] = '\0';
  return String(out);
}
}  // namespace

bool SettingsManager::begin() {
  applyDefaults();
  if (!LittleFS.exists("/config")) {
    LittleFS.mkdir("/config");
  }
  if (!load()) {
    save();
  }
  return true;
}

bool SettingsManager::load() {
  if (!LittleFS.exists(kSettingsPath)) {
    return false;
  }

  File f = LittleFS.open(kSettingsPath, "r");
  if (!f) {
    return false;
  }

  StaticJsonDocument<1024> doc;
  auto err = deserializeJson(doc, f);
  f.close();
  if (err) {
    return false;
  }

  settings_.hostname = doc["hostname"] | settings_.hostname;
  settings_.wifiSsid = doc["wifiSsid"] | "";
  settings_.wifiPassword = doc["wifiPassword"] | "";
  settings_.theme = doc["theme"] | settings_.theme;
  settings_.autoBootPage = doc["autoBootPage"] | settings_.autoBootPage;
  settings_.terminalFg = doc["terminalFg"] | settings_.terminalFg;
  settings_.terminalBg = doc["terminalBg"] | settings_.terminalBg;
  settings_.passwordSalt = doc["passwordSalt"] | settings_.passwordSalt;
  settings_.passwordHash = doc["passwordHash"] | settings_.passwordHash;
  settings_.bootLogoEnabled = doc["bootLogoEnabled"] | settings_.bootLogoEnabled;
  settings_.oledBrightness = doc["oledBrightness"] | settings_.oledBrightness;

  if (settings_.passwordSalt.isEmpty() || settings_.passwordHash.isEmpty()) {
    settings_.passwordSalt = randomSalt();
    setPassword("tinypios");
    save();
  }

  return true;
}

bool SettingsManager::save() const {
  if (!LittleFS.exists("/config")) {
    LittleFS.mkdir("/config");
  }

  File f = LittleFS.open(kSettingsPath, "w");
  if (!f) {
    return false;
  }

  StaticJsonDocument<1024> doc;
  doc["hostname"] = settings_.hostname;
  doc["wifiSsid"] = settings_.wifiSsid;
  doc["wifiPassword"] = settings_.wifiPassword;
  doc["theme"] = settings_.theme;
  doc["autoBootPage"] = settings_.autoBootPage;
  doc["terminalFg"] = settings_.terminalFg;
  doc["terminalBg"] = settings_.terminalBg;
  doc["passwordSalt"] = settings_.passwordSalt;
  doc["passwordHash"] = settings_.passwordHash;
  doc["bootLogoEnabled"] = settings_.bootLogoEnabled;
  doc["oledBrightness"] = settings_.oledBrightness;

  bool ok = serializeJsonPretty(doc, f) > 0;
  f.close();
  return ok;
}

void SettingsManager::resetToFactory() {
  applyDefaults();
  settings_.passwordSalt = randomSalt();
  setPassword("tinypios");
  save();
}

TinyPiSettings &SettingsManager::data() { return settings_; }
const TinyPiSettings &SettingsManager::data() const { return settings_; }

String SettingsManager::hashPassword(const String &password, const String &salt) const {
  return sha256Hex(salt + ":" + password + ":TinyPiOS");
}

bool SettingsManager::verifyPassword(const String &password) const {
  return hashPassword(password, settings_.passwordSalt) == settings_.passwordHash;
}

void SettingsManager::setPassword(const String &password) {
  if (settings_.passwordSalt.isEmpty()) {
    settings_.passwordSalt = randomSalt();
  }
  settings_.passwordHash = hashPassword(password, settings_.passwordSalt);
}

void SettingsManager::applyDefaults() {
  settings_.hostname = "tinypios";
  settings_.wifiSsid = "";
  settings_.wifiPassword = "";
  settings_.theme = "dark";
  settings_.autoBootPage = "dashboard";
  settings_.terminalFg = "#d9fdd3";
  settings_.terminalBg = "#0d1117";
  settings_.passwordSalt = randomSalt();
  settings_.passwordHash = hashPassword("tinypios", settings_.passwordSalt);
  settings_.bootLogoEnabled = true;
  settings_.oledBrightness = 255;
}

SettingsManager &settingsManager() {
  static SettingsManager manager;
  return manager;
}
