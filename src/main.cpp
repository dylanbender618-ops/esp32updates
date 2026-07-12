#include <Arduino.h>

#include "api.h"
#include "filesystem.h"
#include "oled.h"
#include "settings.h"
#include "terminal.h"
#include "wifi_manager.h"

void setup() {
  Serial.begin(115200);
  delay(200);

  if (!filesystemManager().begin()) {
    Serial.println("[TinyPiOS] LittleFS mount failed");
  }

  settingsManager().begin();

  oledManager().begin();
  if (settingsManager().data().bootLogoEnabled) {
    oledManager().showBootSequence();
  }
  oledManager().setBrightness(settingsManager().data().oledBrightness);

  wifiManager().begin();
  terminalManager().begin();
  apiServer().begin();

  Serial.println("[TinyPiOS] Boot complete");
}

void loop() {
  wifiManager().loop();
  oledManager().tick();
  apiServer().loop();
}
