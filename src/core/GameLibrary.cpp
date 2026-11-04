#include "core/GameLibrary.h"

#include <algorithm>
#include <cstdio>

#include "util/Fs.h"
#include "util/Log.h"
#include <nlohmann/json.hpp>

namespace ns {

using nlohmann::json;

std::string gameIdForPath(const std::string& path) {
  uint64_t h = 1469598103934665603ULL;
  for (unsigned char c : path) {
    h ^= c;
    h *= 1099511628211ULL;
  }
  char buf[32];
  snprintf(buf, sizeof(buf), "%016llx", (unsigned long long)h);
  return buf;
}

std::string prettifyTitle(const std::string& filename) {
  std::string name = fsutil::stemOf(filename);
  // 下划线 / 点分隔 → 空格（点仅在无中文时替换，避免误伤中文句点）
  std::string out;
  out.reserve(name.size());
  bool hasCJK = false;
  for (unsigned char c : name)
    if (c >= 0x80) { hasCJK = true; break; }
  for (size_t i = 0; i < name.size(); i++) {
    char c = name[i];
    if (c == '_') out.push_back(' ');
    else if (c == '.' && !hasCJK && i + 1 < name.size() &&
             name.find('.', i + 1) == std::string::npos)
      out.push_back(' ');  // 最后一个点视为分隔
    else
      out.push_back(c);
  }
  // 去掉常见尾部标签：[!] [b1] (Rev A) (U) (J) [!] 等
  while (!out.empty()) {
    // 去首尾空白
    while (!out.empty() && (out.front() == ' ')) out.erase(out.begin());
    while (!out.empty() && (out.back() == ' ')) out.pop_back();
    if (out.size() >= 2 && out.back() == ']') {
      const size_t open = out.rfind('[');
      if (open != std::string::npos && open > 0) {
        bool simple = true;
        for (size_t i = open + 1; i + 1 < out.size(); i++)
          if (out[i] == '[' || out[i] == ']') { simple = false; break; }
        if (simple) {
          out.erase(open);
          continue;
        }
      }
    }
    if (out.size() >= 2 && out.back() == ')') {
      const size_t open = out.rfind('(');
      if (open != std::string::npos && open > 0) {
        bool simple = true;
        for (size_t i = open + 1; i + 1 < out.size(); i++)
          if (out[i] == '(' || out[i] == ')') { simple = false; break; }
        if (simple) {
          out.erase(open);
          continue;
        }
      }
    }
    break;
  }
  if (out.empty()) out = name;
  return out;
}

void GameLibrary::load(const std::string& dataDir) {
  dataDir_ = dataDir;
  libraryFile_ = fsutil::join(dataDir, "library.json");
  coverDir_ = fsutil::join(dataDir, "covers");
  games_.clear();
  romDirs_.clear();

  std::string text;
  if (fsutil::readFile(libraryFile_, &text)) {
    try {
      const json j = json::parse(text, nullptr, false);
      if (!j.is_discarded()) {
        if (j.contains("games") && j["games"].is_array()) {
          for (const auto& g : j["games"]) {
            GameEntry e;
            e.id = g.value("id", "");
            e.title = g.value("title", "");
            e.customTitle = g.value("customTitle", "");
            e.preferredCore = g.value("preferredCore", "");  // 核心选择器「记住选择」
            e.romPath = g.value("romPath", "");
            e.coverPath = g.value("coverPath", "");
            e.platform = platformFromString(g.value("platform", "NES"));
            e.lastPlayedAt = g.value("lastPlayedAt", (int64_t)0);
            e.playTimeSec = g.value("playTimeSec", (int64_t)0);
            e.favorite = g.value("favorite", false);
            if (!e.romPath.empty() && fsutil::isFile(e.romPath)) games_.push_back(std::move(e));
          }
        }
        if (j.contains("romDirs") && j["romDirs"].is_array()) {
          for (const auto& d : j["romDirs"])
            if (d.is_string()) romDirs_.push_back(d.get<std::string>());
        }
      } else {
        LOGW("library.json 解析失败，重建库");
      }
    } catch (const std::exception& e) {
      LOGW("library.json 读取异常: %s", e.what());
    }
  }
  LOGI("游戏库载入：%zu 个游戏，%zu 个 ROM 目录", games_.size(), romDirs_.size());
}

void GameLibrary::save() {
  json j;
  j["version"] = 1;
  j["games"] = json::array();
  for (const auto& e : games_) {
    json g;
    g["id"] = e.id;
    g["title"] = e.title;
    g["customTitle"] = e.customTitle;
    g["preferredCore"] = e.preferredCore;
    g["romPath"] = e.romPath;
    g["coverPath"] = e.coverPath;
    g["platform"] = platformShortName(e.platform);
    g["lastPlayedAt"] = e.lastPlayedAt;
    g["playTimeSec"] = e.playTimeSec;
    g["favorite"] = e.favorite;
    j["games"].push_back(std::move(g));
  }
  j["romDirs"] = romDirs_;
  const std::string text = j.dump(2);
  if (!fsutil::writeFile(libraryFile_, text)) LOGW("library.json 写入失败");
}

GameEntry* GameLibrary::find(const std::string& id) {
  for (auto& e : games_)
    if (e.id == id) return &e;
  return nullptr;
}
const GameEntry* GameLibrary::find(const std::string& id) const {
  for (const auto& e : games_)
    if (e.id == id) return &e;
  return nullptr;
}

bool GameLibrary::addRom(const std::string& romPath, GamePlatform platformHint,
                         bool useHint) {
  if (!fsutil::isFile(romPath)) return false;
  const std::string id = gameIdForPath(romPath);
  if (find(id)) return false;  // 已在库中
  const std::string ext = fsutil::extOf(romPath);
  if (ext.empty()) return false;

  // 与原项目一致的歧义处理：zip / bin / iso / elf 按导入页平台消歧
  GamePlatform p;
  if (useHint) {
    p = platformHint;
  } else if (ext == "zip" || ext == "bin" || ext == "iso" || ext == "elf" || ext == "cue" ||
             ext == "chd" || ext == "img" || ext == "exe") {
    p = platformHint;
  } else {
    p = platformFromExtension(ext);
  }

  GameEntry e;
  e.id = id;
  e.romPath = romPath;
  e.title = prettifyTitle(fsutil::baseOf(romPath));
  e.platform = p;
  // 同目录同名图片 = 本地封面
  const std::string stem = fsutil::stemOf(romPath);
  for (const char* imgExt : {".png", ".jpg", ".jpeg", ".webp"}) {
    const std::string cand = fsutil::join(fsutil::dirOf(romPath), stem + imgExt);
    if (fsutil::isFile(cand)) {
      e.coverPath = cand;
      break;
    }
  }
  // covers/<id>.png
  const std::string coverId = fsutil::join(coverDir_, id + ".png");
  if (e.coverPath.empty() && fsutil::isFile(coverId)) e.coverPath = coverId;

  games_.push_back(std::move(e));
  return true;
}

int GameLibrary::scanDirectory(const std::string& dir, GamePlatform platformHint,
                               bool useHint, bool recursive) {
  if (!fsutil::isDir(dir)) return 0;
  std::vector<std::string> files;
  if (recursive) {
    files = fsutil::listFilesRecursive(dir);
  } else {
    for (const auto& e : fsutil::listDir(dir))
      if (!e.isDir) files.push_back(e.fullPath);
  }
  int added = 0;
  for (const auto& f : files)
    if (addRom(f, platformHint, useHint)) added++;
  return added;
}

void GameLibrary::remove(const std::string& id) {
  games_.erase(std::remove_if(games_.begin(), games_.end(),
                              [&](const GameEntry& e) { return e.id == id; }),
               games_.end());
}

void GameLibrary::toggleFavorite(const std::string& id) {
  if (auto* e = find(id)) e->favorite = !e->favorite;
}

void GameLibrary::rename(const std::string& id, const std::string& customTitle) {
  if (auto* e = find(id)) e->customTitle = customTitle;
}

void GameLibrary::recordPlay(const std::string& id, int64_t seconds) {
  if (auto* e = find(id)) {
    e->lastPlayedAt = time(nullptr);
    e->playTimeSec += seconds;
  }
}

std::vector<GameEntry> GameLibrary::byPlatform(GamePlatform p) const {
  std::vector<GameEntry> out;
  for (const auto& e : games_)
    if (e.platform == p) out.push_back(e);
  return out;
}

std::vector<GameEntry> GameLibrary::favorites() const {
  std::vector<GameEntry> out;
  for (const auto& e : games_)
    if (e.favorite) out.push_back(e);
  return out;
}

std::vector<GameEntry> GameLibrary::recent(int limit) const {
  std::vector<GameEntry> out;
  for (const auto& e : games_)
    if (e.lastPlayedAt > 0) out.push_back(e);
  std::sort(out.begin(), out.end(), [](const GameEntry& a, const GameEntry& b) {
    return a.lastPlayedAt > b.lastPlayedAt;
  });
  if ((int)out.size() > limit) out.resize((size_t)limit);
  return out;
}

int GameLibrary::countFor(GamePlatform p) const {
  int n = 0;
  for (const auto& e : games_)
    if (e.platform == p) n++;
  return n;
}

void GameLibrary::addRomDir(const std::string& dir) {
  if (std::find(romDirs_.begin(), romDirs_.end(), dir) == romDirs_.end())
    romDirs_.push_back(dir);
}

void GameLibrary::removeRomDir(const std::string& dir) {
  romDirs_.erase(std::remove(romDirs_.begin(), romDirs_.end(), dir), romDirs_.end());
}

}  // namespace ns
