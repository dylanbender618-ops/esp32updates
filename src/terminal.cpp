#include "terminal.h"

#include <ArduinoJson.h>
#include <LittleFS.h>

#include "filesystem.h"
#include "oled.h"
#include "settings.h"
#include "wifi_manager.h"

namespace {
constexpr uint8_t kBuiltinLedPin = 2;
}

void TerminalManager::begin() {
  history_.clear();
  currentDir_ = "/";
}

String TerminalManager::prompt() const { return "TinyPiOS $"; }
String TerminalManager::cwd() const { return currentDir_; }

void TerminalManager::pushHistory(const String &line) {
  if (line.isEmpty()) return;
  history_.push_back(line);
  if (history_.size() > 120) {
    history_.erase(history_.begin());
  }
}

String TerminalManager::resolvePath(const String &input) const {
  if (input.isEmpty()) return currentDir_;
  if (input.startsWith("/")) return filesystemManager().normalizePath(input);
  if (currentDir_ == "/") return filesystemManager().normalizePath("/" + input);
  return filesystemManager().normalizePath(currentDir_ + "/" + input);
}

String TerminalManager::listDir(const String &path) const {
  File dir = LittleFS.open(path, "r");
  if (!dir || !dir.isDirectory()) {
    return "Not a directory.";
  }
  String out;
  File item = dir.openNextFile();
  while (item) {
    out += String(item.isDirectory() ? "d " : "f ") + String(item.name()) +
           (item.isDirectory() ? "" : " (" + String(item.size()) + "B)") + "\n";
    item = dir.openNextFile();
  }
  return out.isEmpty() ? "(empty)" : out;
}

String TerminalManager::treeDir(const String &path, int depth, const String &prefix) const {
  if (depth < 0) return "";
  File dir = LittleFS.open(path, "r");
  if (!dir || !dir.isDirectory()) return "";
  String out;
  File item = dir.openNextFile();
  while (item) {
    String name = String(item.name());
    out += prefix + "|- " + name + "\n";
    if (item.isDirectory()) {
      out += treeDir(name, depth - 1, prefix + "|  ");
    }
    item = dir.openNextFile();
  }
  return out;
}

TerminalResponse TerminalManager::execute(const String &commandLine, const String &user) {
  TerminalResponse res;
  String line = commandLine;
  line.trim();
  if (line.isEmpty()) {
    res.output = "";
    return res;
  }

  pushHistory(line);

  int idx = line.indexOf(' ');
  String cmd = idx < 0 ? line : line.substring(0, idx);
  String args = idx < 0 ? "" : line.substring(idx + 1);
  cmd.toLowerCase();

  if (cmd == "help") {
    res.output = "Commands: help clear pwd ls tree cd mkdir touch rm mv cp cat nano echo hostname date time uptime mem flash wifi reboot restart shutdown oled led temp about version whoami passwd history";
  } else if (cmd == "clear") {
    res.clearScreen = true;
    res.output = "";
  } else if (cmd == "pwd") {
    res.output = currentDir_;
  } else if (cmd == "ls") {
    res.output = listDir(currentDir_);
  } else if (cmd == "tree") {
    String tree = treeDir(currentDir_, 5, "");
    res.output = tree.isEmpty() ? "(empty)" : tree;
  } else if (cmd == "cd") {
    String target = resolvePath(args);
    if (filesystemManager().isDirectory(target)) {
      currentDir_ = target;
      res.output = "Changed directory to " + currentDir_;
    } else {
      res.output = "Directory not found.";
    }
  } else if (cmd == "mkdir") {
    String target = resolvePath(args);
    res.output = filesystemManager().mkdir(target) ? "Directory created." : "Unable to create directory.";
  } else if (cmd == "touch") {
    String target = resolvePath(args);
    res.output = filesystemManager().writeFile(target, "") ? "File created." : "Unable to create file.";
  } else if (cmd == "rm") {
    String target = resolvePath(args);
    res.output = filesystemManager().removePath(target) ? "Removed." : "Unable to remove path.";
  } else if (cmd == "mv") {
    int split = args.indexOf(' ');
    if (split < 0) {
      res.output = "Usage: mv <source> <target>";
    } else {
      String src = resolvePath(args.substring(0, split));
      String dst = resolvePath(args.substring(split + 1));
      res.output = filesystemManager().renamePath(src, dst) ? "Moved." : "Move failed.";
    }
  } else if (cmd == "cp") {
    int split = args.indexOf(' ');
    if (split < 0) {
      res.output = "Usage: cp <source> <target>";
    } else {
      String src = resolvePath(args.substring(0, split));
      String dst = resolvePath(args.substring(split + 1));
      res.output = filesystemManager().copyPath(src, dst) ? "Copied." : "Copy failed.";
    }
  } else if (cmd == "cat") {
    String target = resolvePath(args);
    String content = filesystemManager().readFile(target);
    res.output = content.isEmpty() ? "(empty or missing file)" : content;
  } else if (cmd == "nano") {
    res.output = "nano is browser-based in TinyPiOS. Open the editor panel instead.";
  } else if (cmd == "echo") {
    res.output = args;
  } else if (cmd == "hostname") {
    if (args.isEmpty()) {
      res.output = settingsManager().data().hostname;
    } else {
      settingsManager().data().hostname = args;
      settingsManager().save();
      res.output = "Hostname updated to " + args + ". Reboot to apply globally.";
    }
  } else if (cmd == "date" || cmd == "time") {
    res.output = String("Uptime-seconds: ") + String(millis() / 1000);
  } else if (cmd == "uptime") {
    uint32_t up = millis() / 1000;
    res.output = String(up) + " seconds";
  } else if (cmd == "mem") {
    res.output = String("Free heap: ") + String(ESP.getFreeHeap()) + " bytes";
  } else if (cmd == "flash") {
    res.output = String("Flash chip size: ") + String(ESP.getFlashChipSize() / 1024 / 1024) + " MB";
  } else if (cmd == "wifi") {
    String lowerArgs = args;
    lowerArgs.toLowerCase();
    if (lowerArgs == "scan") {
      res.output = wifiManager().scanNetworksJson();
    } else if (lowerArgs == "status") {
      WifiSnapshot s = wifiManager().snapshot();
      res.output = "Mode=" + String(static_cast<int>(s.mode)) + " SSID=" + s.ssid + " IP=" + s.ip + " RSSI=" + String(s.rssi);
    } else {
      res.output = "Try: wifi scan | wifi status";
    }
  } else if (cmd == "reboot" || cmd == "restart") {
    res.output = "Rebooting TinyPiOS...";
    delay(250);
    ESP.restart();
  } else if (cmd == "shutdown") {
    res.output = "TinyPiOS cannot power off ESP32 hardware. Use reboot instead.";
  } else if (cmd == "oled") {
    String lowerArgs = args;
    lowerArgs.toLowerCase();
    if (lowerArgs == "clear") {
      oledManager().clear();
      res.output = "OLED cleared.";
    } else if (lowerArgs.startsWith("print")) {
      String msg = args.substring(5);
      msg.trim();
      oledManager().printLine(msg);
      res.output = "OLED updated.";
    } else {
      res.output = "Try: oled clear | oled print <text>";
    }
  } else if (cmd == "led") {
    String lowerArgs = args;
    lowerArgs.toLowerCase();
    pinMode(kBuiltinLedPin, OUTPUT);
    if (lowerArgs == "on") {
      digitalWrite(kBuiltinLedPin, HIGH);
      res.output = "LED ON";
    } else if (lowerArgs == "off") {
      digitalWrite(kBuiltinLedPin, LOW);
      res.output = "LED OFF";
    } else {
      res.output = "Try: led on | led off";
    }
  } else if (cmd == "temp") {
    res.output = "Temperature sensor is not exposed on ESP32 DevKit V1 in this build.";
  } else if (cmd == "about") {
    res.output = "TinyPiOS - Web desktop shell for ESP32 with OLED, LittleFS, OTA, and terminal.";
  } else if (cmd == "version") {
    res.output = "TinyPiOS v1.0.0";
  } else if (cmd == "whoami") {
    res.output = user;
  } else if (cmd == "passwd") {
    if (args.length() < 8) {
      res.output = "Usage: passwd <new-password> (min 8 chars)";
    } else {
      settingsManager().setPassword(args);
      settingsManager().save();
      res.output = "Password changed.";
    }
  } else if (cmd == "history") {
    String out;
    for (size_t i = 0; i < history_.size(); ++i) {
      out += String(i + 1) + " " + history_[i] + "\n";
    }
    res.output = out;
  } else {
    res.output = "Command not supported: " + cmd + " (TinyPiOS keeps commands friendly and safe.)";
  }

  return res;
}

String TerminalManager::historyJson() const {
  DynamicJsonDocument doc(4096);
  JsonArray arr = doc.createNestedArray("history");
  for (const auto &h : history_) {
    arr.add(h);
  }
  String out;
  serializeJson(doc, out);
  return out;
}

TerminalManager &terminalManager() {
  static TerminalManager terminal;
  return terminal;
}
