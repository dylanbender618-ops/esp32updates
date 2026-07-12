#pragma once

#include <Arduino.h>

class OledManager {
 public:
  bool begin();
  void showBootSequence();
  void setCurrentPage(const String &page);
  void setCurrentUser(const String &user);
  void setStatusMessage(const String &message);
  void setBrightness(uint8_t value);
  void tick();
  String statusText() const;
  void clear();
  void printLine(const String &text);

 private:
  String page_ = "dashboard";
  String user_ = "guest";
  String status_ = "Ready";
  uint8_t brightness_ = 255;
  uint32_t lastDrawMs_ = 0;
};

OledManager &oledManager();
