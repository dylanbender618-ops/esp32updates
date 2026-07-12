#pragma once

#include <Arduino.h>

class EditorManager {
 public:
  bool save(const String &path, const String &content);
  String load(const String &path) const;
};

EditorManager &editorManager();
