#include "filesystem.h"

#include <ArduinoJson.h>

namespace {
void addEntries(JsonArray &arr, const String &dirPath, int depth) {
  File dir = LittleFS.open(dirPath, "r");
  if (!dir || !dir.isDirectory()) {
    return;
  }

  File item = dir.openNextFile();
  while (item) {
    JsonObject node = arr.createNestedObject();
    node["name"] = String(item.name()).substring(String(item.name()).lastIndexOf('/') + 1);
    node["path"] = String(item.name());
    node["isDir"] = item.isDirectory();
    node["size"] = static_cast<uint32_t>(item.size());

    if (item.isDirectory() && depth > 0) {
      JsonArray children = node.createNestedArray("children");
      addEntries(children, String(item.name()), depth - 1);
    }
    item = dir.openNextFile();
  }
}
}  // namespace

bool FilesystemManager::begin() {
  if (!LittleFS.begin(true)) {
    return false;
  }
  if (!LittleFS.exists("/config")) {
    LittleFS.mkdir("/config");
  }
  if (!LittleFS.exists("/home")) {
    LittleFS.mkdir("/home");
  }
  return true;
}

bool FilesystemManager::isValidPath(const String &path) const {
  if (path.isEmpty()) return false;
  if (!path.startsWith("/")) return false;
  if (path.indexOf("..") >= 0) return false;
  if (path.indexOf('\0') >= 0) return false;
  return path.length() < 128;
}

String FilesystemManager::normalizePath(const String &path) const {
  if (path.isEmpty()) return "/";
  String out = path;
  if (!out.startsWith("/")) out = "/" + out;
  while (out.indexOf("//") >= 0) out.replace("//", "/");
  if (out.length() > 1 && out.endsWith("/")) out.remove(out.length() - 1);
  return out;
}

bool FilesystemManager::exists(const String &path) const {
  String p = normalizePath(path);
  return isValidPath(p) && LittleFS.exists(p);
}

bool FilesystemManager::isDirectory(const String &path) const {
  String p = normalizePath(path);
  if (!isValidPath(p)) return false;
  File f = LittleFS.open(p, "r");
  bool dir = f && f.isDirectory();
  f.close();
  return dir;
}

String FilesystemManager::listJson(const String &path) const {
  String p = normalizePath(path);
  if (!isValidPath(p)) {
    return "{\"error\":\"invalid path\"}";
  }

  DynamicJsonDocument doc(8192);
  doc["path"] = p;
  JsonArray entries = doc.createNestedArray("entries");
  addEntries(entries, p, 2);
  FsUsage u = usage();
  doc["used"] = static_cast<uint32_t>(u.usedBytes);
  doc["total"] = static_cast<uint32_t>(u.totalBytes);

  String out;
  serializeJson(doc, out);
  return out;
}

bool FilesystemManager::mkdir(const String &path) {
  String p = normalizePath(path);
  return isValidPath(p) && LittleFS.mkdir(p);
}

bool FilesystemManager::writeFile(const String &path, const String &content) {
  String p = normalizePath(path);
  if (!isValidPath(p)) return false;
  File f = LittleFS.open(p, "w");
  if (!f) return false;
  bool ok = f.print(content);
  f.close();
  return ok;
}

String FilesystemManager::readFile(const String &path) const {
  String p = normalizePath(path);
  if (!isValidPath(p) || !LittleFS.exists(p)) return "";

  File f = LittleFS.open(p, "r");
  if (!f || f.isDirectory()) {
    return "";
  }

  String out;
  out.reserve(f.size());
  char buf[1024];
  while (f.available()) {
    size_t n = f.readBytes(buf, sizeof(buf));
    if (n == 0) break;
    out.concat(buf, n);
  }
  f.close();
  return out;
}

bool FilesystemManager::removePath(const String &path) {
  String p = normalizePath(path);
  if (!isValidPath(p) || p == "/") return false;

  File f = LittleFS.open(p, "r");
  if (!f) return false;
  bool isDir = f.isDirectory();
  f.close();

  if (!isDir) {
    return LittleFS.remove(p);
  }

  File dir = LittleFS.open(p, "r");
  File item = dir.openNextFile();
  while (item) {
    String child = String(item.name());
    item.close();
    if (!removePath(child)) {
      return false;
    }
    item = dir.openNextFile();
  }
  dir.close();
  return LittleFS.rmdir(p);
}

bool FilesystemManager::renamePath(const String &from, const String &to) {
  String src = normalizePath(from);
  String dst = normalizePath(to);
  if (!isValidPath(src) || !isValidPath(dst)) return false;
  return LittleFS.rename(src, dst);
}

bool FilesystemManager::copyPath(const String &from, const String &to) {
  String src = normalizePath(from);
  String dst = normalizePath(to);
  if (!isValidPath(src) || !isValidPath(dst)) return false;

  File in = LittleFS.open(src, "r");
  if (!in || in.isDirectory()) {
    in.close();
    return false;
  }

  File out = LittleFS.open(dst, "w");
  if (!out) {
    in.close();
    return false;
  }

  uint8_t buf[256];
  while (in.available()) {
    size_t n = in.read(buf, sizeof(buf));
    if (n == 0) break;
    if (out.write(buf, n) != n) {
      in.close();
      out.close();
      return false;
    }
  }

  in.close();
  out.close();
  return true;
}

FsUsage FilesystemManager::usage() const {
  return FsUsage{LittleFS.totalBytes(), LittleFS.usedBytes()};
}

FilesystemManager &filesystemManager() {
  static FilesystemManager manager;
  return manager;
}
