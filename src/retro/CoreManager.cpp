#include "retro/CoreManager.h"

#include <algorithm>
#include <cstdlib>

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

PlatformCore CoreManager::resolveCorePreferred(
    GamePlatform p, const std::string& preferredCoreFile) const {
  if (!preferredCoreFile.empty()) {
    for (const PlatformCore& c : platformCoreChain(p)) {
      if (c.kind == CoreKind::Libretro && preferredCoreFile == c.coreFile &&
          !findCoreFile(c.coreFile).empty()) {
        return c;  // 记忆的核心已安装 → 直接采用
      }
    }
  }
  return resolveCore(p);  // 未安装 / 未记忆 → 链上首个已安装
}

std::string CoreManager::corePathForPreferred(
    GamePlatform p, const std::string& preferredCoreFile) const {
  const PlatformCore c = resolveCorePreferred(p, preferredCoreFile);
  if (c.kind != CoreKind::Libretro) return "";
  return findCoreFile(c.coreFile);
}

std::vector<PlatformCore> CoreManager::installedCoresFor(GamePlatform p) const {
  std::vector<PlatformCore> out;
  if (platformCore(p).kind != CoreKind::Libretro) return out;  // Standalone 平台
  for (const PlatformCore& c : platformCoreChain(p)) {
    if (c.kind != CoreKind::Libretro) continue;
    if (!findCoreFile(c.coreFile).empty()) out.push_back(c);
  }
  return out;
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
  hint += "① 设置 → 核心 → 点击对应平台行 / 「一键下载」按钮，应用内直接下载\n";
  hint += "② 运行 scripts/fetch_cores 脚本下载（候选链自动回退）\n";
  hint += "③ 从 libretro buildbot 手动下载后放入：\n    " + coreDir_;
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

// ==================== 应用内核心下载 ====================
// 与 Android 版 CoreDownloader 同源：libretro buildbot 官方预编译核心，
// 按当前 OS/架构拼 URL（与 scripts/fetch_cores、CI 打包同一条候选链）。

static std::string buildbotBase() {
#ifdef _WIN32
  return "https://buildbot.libretro.com/nightly/windows/x86_64/latest";
#else
#if defined(__aarch64__)
  return "https://buildbot.libretro.com/nightly/linux/aarch64/latest";
#elif defined(__arm__)
  return "https://buildbot.libretro.com/nightly/linux/armhf/latest";
#else
  return "https://buildbot.libretro.com/nightly/linux/x86_64/latest";
#endif
#endif
}

// 拼接 shell 引号（路径可能含空格）
static std::string shellQuote(const std::string& s) {
#ifdef _WIN32
  return "\"" + s + "\"";
#else
  return "'" + s + "'";
#endif
}

bool CoreManager::downloadOne(const std::string& coreFile, std::string* err) {
  const std::string url =
      buildbotBase() + "/" + coreFile + kLibExt + ".zip";
  const std::string zipPath =
      fsutil::join(coreDir_, coreFile + kLibExt + ".zip");
  // 1) curl 下载（Windows 10+ / macOS / 各主流发行版均预装）
  const std::string cmd =
      std::string("curl -fSL --retry 3 --retry-delay 3 --connect-timeout 15 "
                  "--max-time 300 -A NesStationPC/1.1 -o ") +
      shellQuote(zipPath) + " " + shellQuote(url);
  const int rc = std::system(cmd.c_str());
  if (rc != 0 || !fsutil::isFile(zipPath)) {
    fsutil::removeFile(zipPath);
    if (err) *err = "下载失败（" + coreFile + "：本架构可能无预编译包）";
    return false;
  }
  // 2) 解压：unzip → python3 → tar（bsdtar 可解 zip，Win10+ 自带）
  const std::string dir = coreDir_;  // 静态：只在下载线程调用，coreDir_ 不变
  const std::string extractors[] = {
      "unzip -o -q " + shellQuote(zipPath) + " -d " + shellQuote(dir),
      "python3 -m zipfile -e " + shellQuote(zipPath) + " " + shellQuote(dir),
      "tar -xf " + shellQuote(zipPath) + " -C " + shellQuote(dir),
  };
  bool extracted = false;
  for (const auto& ex : extractors) {
    if (std::system(ex.c_str()) == 0) {
      extracted = true;
      break;
    }
  }
  fsutil::removeFile(zipPath);
  if (!extracted ||
      !fsutil::isFile(fsutil::join(dir, coreFile + kLibExt))) {
    if (err) *err = "解压失败（" + coreFile + "）";
    return false;
  }
  return true;
}

void CoreManager::startCoreDownload(const std::vector<GamePlatform>& platforms) {
  if (dlRunning_.load()) return;
  std::vector<GamePlatform> targets = platforms;
  if (targets.empty()) {
    // 全部缺失的 libretro 平台
    for (GamePlatform p : allPlatforms()) {
      if (platformCore(p).kind == CoreKind::Libretro && !libretroCoreAvailable(p))
        targets.push_back(p);
    }
  }
  if (targets.empty()) return;
  if (dlThread_.joinable()) dlThread_.join();
  {
    std::lock_guard<std::mutex> lk(dlMu_);
    dl_ = DlProgress{};
    dl_.running = true;
    dl_.total = (int)targets.size();
  }
  dlRunning_ = true;
  dlThread_ = std::thread(&CoreManager::downloadWorker, this,
                          std::move(targets));
}

void CoreManager::downloadWorker(std::vector<GamePlatform> platforms) {
  bool anyFail = false;
  int done = 0;
  for (GamePlatform p : platforms) {
    const std::vector<PlatformCore> chain = platformCoreChain(p);
    std::string label = std::string(platformShortName(p));
    {
      std::lock_guard<std::mutex> lk(dlMu_);
      dl_.label = label;
      dl_.done = done;
    }
    bool ok = false;
    for (const PlatformCore& c : chain) {
      if (c.kind != CoreKind::Libretro) continue;
      // 已装则跳过
      if (!findCoreFile(c.coreFile).empty()) {
        ok = true;
        break;
      }
      std::string err;
      if (downloadOne(c.coreFile, &err)) {
        ok = true;
        break;
      }
      LOGI("核心下载：%s", err.c_str());
    }
    if (!ok) anyFail = true;
    done++;
    {
      std::lock_guard<std::mutex> lk(dlMu_);
      dl_.done = done;
    }
  }
  {
    std::lock_guard<std::mutex> lk(dlMu_);
    dl_.running = false;
    dl_.finished = true;
    dl_.hadFailure = anyFail;
    dl_.label.clear();
  }
  dlRunning_ = false;
}

CoreManager::DlProgress CoreManager::coreDownloadProgress() const {
  std::lock_guard<std::mutex> lk(dlMu_);
  return dl_;
}

void CoreManager::clearDownloadFinishFlag() {
  std::lock_guard<std::mutex> lk(dlMu_);
  dl_.finished = false;
}

CoreManager::~CoreManager() {
  if (dlThread_.joinable()) dlThread_.join();
}

}  // namespace ns
