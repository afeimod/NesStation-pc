#include "retro/CoreManager.h"

#include <algorithm>

#include "util/Fs.h"
#include "util/Log.h"
#include <nlohmann/json.hpp>

namespace ns {

using nlohmann::json;

#ifdef _WIN32
static const char* kLibExt = ".dll";
#else
static const char* kLibExt = ".so";
#endif

void CoreManager::init(const std::string& dataDir) {
  coreDir_ = fsutil::join(dataDir, "cores");
  // 安装包内置核心：exe 同目录 cores/（Windows zip / Linux tar.gz / deb 的
  // /opt/nesstation/cores；exeDir 经 /proc/self/exe 解析，/usr/bin 软链同样生效）
  bundledCoreDir_ = fsutil::join(fsutil::exeDir(), "cores");
  systemDir_ = fsutil::join(dataDir, "system");
  savesDir_ = fsutil::join(dataDir, "saves");
  statesDir_ = fsutil::join(dataDir, "states");
  optionsDir_ = fsutil::join(dataDir, "core-options");
  for (const char* d : {coreDir_.c_str(), systemDir_.c_str(), savesDir_.c_str(),
                        statesDir_.c_str(), optionsDir_.c_str()}) {
    fsutil::makeDirs(d);
  }
  // 核心子目录（FBNeo BIOS 等）
  fsutil::makeDirs(fsutil::join(systemDir_, "fbneo"));
  fsutil::makeDirs(fsutil::join(systemDir_, "pcsx2"));
  fsutil::makeDirs(fsutil::join(systemDir_, "azahar"));
  fsutil::makeDirs(fsutil::join(systemDir_, "dc"));
  LOGI("CoreManager 初始化：%s", dataDir.c_str());
}

std::string CoreManager::findCoreFile(const std::string& coreFile) const {
  const std::string name = coreFile + kLibExt;
  const std::string user = fsutil::join(coreDir_, name);
  if (fsutil::isFile(user)) return user;
  const std::string bundled = fsutil::join(bundledCoreDir_, name);
  if (fsutil::isFile(bundled)) return bundled;
  return "";
}

PlatformCore CoreManager::resolveCore(GamePlatform p) const {
  const std::vector<PlatformCore> chain = platformCoreChain(p);
  if (chain.empty()) return platformCore(p);
  for (const PlatformCore& c : chain) {
    if (c.kind != CoreKind::Libretro) return c;
    if (!findCoreFile(c.coreFile).empty()) return c;
  }
  return chain.front();  // 全部缺失 → 返回首选（用于提示/选项）
}

std::string CoreManager::corePathFor(GamePlatform p) const {
  const PlatformCore c = resolveCore(p);
  if (c.kind != CoreKind::Libretro) return "";
  return findCoreFile(c.coreFile);
}

bool CoreManager::libretroCoreAvailable(GamePlatform p) const {
  for (const PlatformCore& c : platformCoreChain(p)) {
    if (c.kind != CoreKind::Libretro) return false;  // Standalone 平台
    if (!findCoreFile(c.coreFile).empty()) return true;
  }
  return !findCoreFile(platformCore(p).coreFile).empty();
}

std::string CoreManager::availabilityHint(GamePlatform p) const {
  const PlatformCore c = platformCore(p);
  if (c.kind == CoreKind::Standalone) {
    return std::string("需要 PC 版 ") + c.coreTitle +
           "：放在应用目录 emulators/ 下或到设置页配置安装路径";
  }
  if (libretroCoreAvailable(p)) return "";
  const std::vector<PlatformCore> chain = platformCoreChain(p);
  std::string names;
  for (const PlatformCore& cc : chain) {
    if (!names.empty()) names += " / ";
    names += cc.coreTitle;
  }
  std::string hint = std::string("缺少核心（") + names + "）\n";
  hint += "libretro 预编译核心并非所有架构都提供，可任选其一：\n";
  hint += "① 运行 scripts/fetch_cores 脚本自动下载本架构可用核心\n";
  hint += "② 从 libretro buildbot 手动下载后放入：\n    " + coreDir_;
  if (!bundledCoreDir_.empty() && bundledCoreDir_ != coreDir_)
    hint += "\n（安装包若内置核心，位于 " + bundledCoreDir_ + "，自动识别）";
  return hint;
}

std::string CoreManager::resolveOption(const std::string& coreKey,
                                       const std::string& optKey) const {
  const auto kv = loadOptions(coreKey);
  const auto it = kv.find(optKey);
  return it != kv.end() ? it->second : "";
}

void CoreManager::storeOption(const std::string& coreKey, const std::string& optKey,
                              const std::string& value) {
  auto kv = loadOptions(coreKey);
  kv[optKey] = value;
  saveOptions(coreKey, kv);
}

std::map<std::string, std::string> CoreManager::loadOptions(
    const std::string& coreKey) const {
  std::map<std::string, std::string> out;
  const std::string file = fsutil::join(optionsDir_, coreKey + ".json");
  std::string text;
  if (fsutil::readFile(file, &text)) {
    try {
      const json j = json::parse(text, nullptr, false);
      if (!j.is_discarded() && j.is_object()) {
        for (auto it = j.begin(); it != j.end(); ++it)
          out[it.key()] = it.value().get<std::string>();
      }
    } catch (...) {
    }
  }
  return out;
}

void CoreManager::saveOptions(const std::string& coreKey,
                              const std::map<std::string, std::string>& kv) {
  json j = json::object();
  for (const auto& [k, v] : kv) j[k] = v;
  const std::string file = fsutil::join(optionsDir_, coreKey + ".json");
  fsutil::writeFile(file, j.dump(2));
}

std::string CoreManager::statePath(const std::string& gameId, int slot) const {
  const std::string dir = fsutil::join(statesDir_, gameId);
  fsutil::makeDirs(dir);
  return fsutil::join(dir, slot <= 0 ? "auto.state" : ("slot" + std::to_string(slot) + ".state"));
}

std::vector<int> CoreManager::existingSlots(const std::string& gameId) const {
  std::vector<int> out;
  const std::string dir = fsutil::join(statesDir_, gameId);
  for (const auto& e : fsutil::listDir(dir)) {
    if (e.isDir) continue;
    const std::string n = fsutil::stemOf(e.name);
    if (n == "auto") out.push_back(0);
    else if (n.rfind("slot", 0) == 0) {
      try {
        out.push_back(std::stoi(n.substr(4)));
      } catch (...) {
      }
    }
  }
  std::sort(out.begin(), out.end());
  return out;
}

bool CoreManager::saveState(LibretroCore& core, const std::string& gameId, int slot) {
  std::vector<uint8_t> data;
  if (!core.serialize(&data)) return false;
  return fsutil::writeFile(statePath(gameId, slot),
                           std::string((const char*)data.data(), data.size()));
}

bool CoreManager::loadState(LibretroCore& core, const std::string& gameId, int slot) {
  std::string data;
  if (!fsutil::readFile(statePath(gameId, slot), &data)) return false;
  return core.unserialize((const uint8_t*)data.data(), data.size());
}

std::vector<std::string> CoreManager::installedCores() const {
  std::vector<std::string> out;
  for (const auto& e : fsutil::listDir(coreDir_)) {
    if (!e.isDir) out.push_back(e.name);
  }
  // 内置目录去重后一并列出（设置页展示）
  for (const auto& e : fsutil::listDir(bundledCoreDir_)) {
    if (!e.isDir && std::find(out.begin(), out.end(), e.name) == out.end())
      out.push_back(e.name);
  }
  return out;
}

}  // namespace ns
