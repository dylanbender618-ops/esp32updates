#pragma once

#include <Arduino.h>
#include <vector>

struct TerminalResponse {
  String output;
  bool clearScreen = false;
};

class TerminalManager {
 public:
  void begin();
  TerminalResponse execute(const String &commandLine, const String &user);
  String prompt() const;
  String cwd() const;
  String historyJson() const;

 private:
  String currentDir_ = "/";
  std::vector<String> history_;

  String resolvePath(const String &input) const;
  String listDir(const String &path) const;
  String treeDir(const String &path, int depth, const String &prefix) const;
  void pushHistory(const String &line);
};

TerminalManager &terminalManager();
