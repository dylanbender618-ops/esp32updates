#include "editor.h"

#include "filesystem.h"

bool EditorManager::save(const String &path, const String &content) {
  return filesystemManager().writeFile(path, content);
}

String EditorManager::load(const String &path) const {
  return filesystemManager().readFile(path);
}

EditorManager &editorManager() {
  static EditorManager manager;
  return manager;
}
