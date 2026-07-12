#include "oled.h"

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <WiFi.h>
#include <Wire.h>

#include "wifi_manager.h"

namespace {
constexpr int kScreenWidth = 128;
constexpr int kScreenHeight = 64;
constexpr int kOledReset = -1;
constexpr uint8_t kOledAddress = 0x3C;
Adafruit_SSD1306 display(kScreenWidth, kScreenHeight, &Wire, kOledReset);

String modeToString(WifiModeState mode) {
  switch (mode) {
    case WifiModeState::STA_CONNECTED:
      return "STA";
    case WifiModeState::STA_CONNECTING:
      return "Connecting";
    case WifiModeState::AP_MODE:
      return "AP";
    default:
      return "Offline";
  }
}
}  // namespace

bool OledManager::begin() {
  Wire.begin(23, 22);
  if (!display.begin(SSD1306_SWITCHCAPVCC, kOledAddress)) {
    return false;
  }
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.display();
  return true;
}

void OledManager::showBootSequence() {
  display.clearDisplay();
  display.setTextSize(2);
  display.setCursor(10, 6);
  display.print("TinyPiOS");
  display.setTextSize(1);
  display.setCursor(20, 28);
  display.print("Booting...");
  display.drawRect(8, 46, 112, 12, SSD1306_WHITE);
  display.display();

  for (int i = 0; i <= 100; i += 4) {
    display.fillRect(10, 48, i, 8, SSD1306_WHITE);
    display.display();
    delay(40);
  }
}

void OledManager::setCurrentPage(const String &page) { page_ = page; }
void OledManager::setCurrentUser(const String &user) { user_ = user; }
void OledManager::setStatusMessage(const String &message) { status_ = message; }
void OledManager::setBrightness(uint8_t value) { brightness_ = value; }

void OledManager::tick() {
  if (millis() - lastDrawMs_ < 1000) {
    return;
  }
  lastDrawMs_ = millis();

  WifiSnapshot w = wifiManager().snapshot();
  display.clearDisplay();
  display.setTextSize(1);

  display.setCursor(0, 0);
  display.printf("TinyPiOS [%s]", modeToString(w.mode).c_str());

  display.setCursor(0, 10);
  display.print("IP:");
  display.print(w.ip);

  display.setCursor(0, 20);
  display.print("Page:");
  display.print(page_.substring(0, 10));

  display.setCursor(0, 30);
  display.print("User:");
  display.print(user_.substring(0, 10));

  display.setCursor(0, 40);
  display.printf("Heap:%uKB", static_cast<unsigned>(ESP.getFreeHeap() / 1024));

  uint32_t uptime = millis() / 1000;
  display.setCursor(0, 50);
  display.printf("Up:%02lu:%02lu", static_cast<unsigned long>((uptime / 60) % 60),
                 static_cast<unsigned long>(uptime % 60));

  display.display();
  (void)brightness_;
}

String OledManager::statusText() const {
  return status_;
}

void OledManager::clear() {
  display.clearDisplay();
  display.display();
}

void OledManager::printLine(const String &text) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("TinyPiOS Message");
  display.drawLine(0, 9, 127, 9, SSD1306_WHITE);
  display.setCursor(0, 16);
  display.print(text.substring(0, 80));
  display.display();
}

OledManager &oledManager() {
  static OledManager manager;
  return manager;
}
