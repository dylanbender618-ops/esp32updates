#pragma once

#include <Arduino.h>
#include <LittleFS.h>

struct FsUsage {
  size_t totalBytes;
  size_t usedBytes;
};

class FilesystemManager {
 public:
  bool begin();

  bool isValidPath(const String &path) const;
  String normalizePath(const String &path) const;
  bool exists(const String &path) const;
  bool isDirectory(const String &path) const;

  String listJson(const String &path) const;
  bool mkdir(const String &path);
  bool writeFile(const String &path, const String &content);
  String readFile(const String &path) const;
  bool removePath(const String &path);
  bool renamePath(const String &from, const String &to);
  bool copyPath(const String &from, const String &to);

  FsUsage usage() const;
};

FilesystemManager &filesystemManager();
