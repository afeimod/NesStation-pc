#include "emu/ExternalEmulators.h"

#include <cstdlib>
#include <thread>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX  // 防止 min/max 宏污染后续 STL 头
#endif
#include <windows.h>  // CreateProcessA / STARTUPINFOA / CloseHandle
#else
#include <spawn.h>
#include <sys/wait.h>
extern char** environ;
#endif

#include "util/Fs.h"
#include "util/Log.h"

namespace ns {

namespace {

#ifdef _WIN32
const char* kExeSuffix = ".exe";
#else
const char* kExeSuffix = "";
#endif

bool fileExists(const std::string& p) { return fsutil::isFile(p); }

// 拼接并检查（无后缀自动补 .exe on Windows）
std::string checkExe(const std::string& dir, const std::string& name) {
  std::string p = fsutil::join(dir, name);
  if (fileExists(p)) return p;
  p += kExeSuffix;
  if (fileExists(p)) return p;
  return "";
}

// 分离启动：Windows CreateProcess / POSIX posix_spawn（不弹控制台、不等待）
bool spawnDetached(const std::string& exe, const std::string& args) {
#ifdef _WIN32
  std::string cmd = "\"" + exe + "\"" + (args.empty() ? "" : " " + args);
  STARTUPINFOA si{};
  si.cb = sizeof(si);
  PROCESS_INFORMATION pi{};
  const BOOL ok = CreateProcessA(nullptr, &cmd[0], nullptr, nullptr, FALSE, 0, nullptr,
                                 nullptr, &si, &pi);
  if (!ok) return false;
  CloseHandle(pi.hProcess);
  CloseHandle(pi.hThread);
  return true;
#else
  (void)exe;
  (void)args;
  return false;  // 由 posixSpawn 实现
#endif
}

#ifndef _WIN32
bool posixSpawn(const std::string& exe, const std::string& args) {
  // 拆参数（简单空格切分，路径已带引号外的空格时由调用方保证无空格）
  std::vector<std::string> parts;
  std::string cur;
  bool inQuote = false;
  for (char c : args) {
    if (c == '"') inQuote = !inQuote;
    else if (c == ' ' && !inQuote) {
      if (!cur.empty()) { parts.push_back(cur); cur.clear(); }
    } else {
      cur.push_back(c);
    }
  }
  if (!cur.empty()) parts.push_back(cur);

  std::vector<char*> argv;
  argv.push_back(const_cast<char*>(exe.c_str()));
  for (auto& s : parts) argv.push_back(const_cast<char*>(s.c_str()));
  argv.push_back(nullptr);

  pid_t pid = 0;
  const int rc = posix_spawn(&pid, exe.c_str(), nullptr, nullptr, argv.data(), environ);
  if (rc != 0) return false;
  return true;  // 父进程不等待
}
#endif

}  // namespace

void ExternalEmulators::init() {
  list_.clear();
  ExternalEmulator dolphin;
  dolphin.key = "dolphin";
  dolphin.title = "Dolphin";
  ExternalEmulator cemu;
  cemu.key = "cemu";
  cemu.title = "Cemu";
  ExternalEmulator eden;
  eden.key = "eden";
  eden.title = "Eden";
  list_.push_back(dolphin);
  list_.push_back(cemu);
  list_.push_back(eden);
  for (auto& e : list_) detect(&e);
}

void ExternalEmulators::detect(ExternalEmulator* e) {
  e->foundPath.clear();

  // 1. 用户配置路径
  if (!e->userPath.empty() && fileExists(e->userPath)) {
    e->foundPath = e->userPath;
    return;
  }

  const std::string exeDir = fsutil::exeDir();
  const std::string bundle = fsutil::join(exeDir, "emulators");

  if (e->key == "dolphin") {
    // 超级构建 / 便携包
    std::string p = checkExe(fsutil::join(bundle, "dolphin"), "Dolphin");
    if (p.empty()) p = checkExe(fsutil::join(bundle, "dolphin", "bin"), "dolphin-emu");
    if (p.empty()) p = checkExe(fsutil::join(bundle, "dolphin", "bin"), "dolphin-nogui");
#ifdef _WIN32
    if (p.empty()) {
      for (const char* pf : {"C:/Program Files/Dolphin", "C:/Program Files (x86)/Dolphin"}) {
        p = checkExe(pf, "Dolphin");
        if (!p.empty()) break;
      }
    }
#else
    if (p.empty()) p = checkExe("/usr/local/bin", "dolphin-emu");
    if (p.empty()) p = checkExe("/usr/bin", "dolphin-emu");
    if (p.empty()) p = checkExe("/usr/local/bin", "dolphin-nogui");
#endif
    e->foundPath = p;
    return;
  }

  if (e->key == "cemu") {
    std::string p = checkExe(fsutil::join(bundle, "cemu"), "Cemu");
    if (p.empty()) p = checkExe(fsutil::join(bundle, "cemu", "bin"), "Cemu");
#ifdef _WIN32
    if (p.empty()) {
      for (const char* pf : {"C:/Program Files/Cemu", "C:/Program Files (x86)/Cemu"}) {
        p = checkExe(pf, "Cemu");
        if (!p.empty()) break;
      }
    }
#else
    if (p.empty()) p = checkExe("/usr/local/bin", "Cemu");
    if (p.empty()) p = checkExe("/usr/bin", "Cemu");
#endif
    e->foundPath = p;
    return;
  }

  if (e->key == "eden") {
    std::string p = checkExe(fsutil::join(bundle, "eden"), "eden");
    if (p.empty()) p = checkExe(fsutil::join(bundle, "eden", "bin"), "eden");
#ifdef _WIN32
    if (p.empty()) {
      for (const char* pf : {"C:/Program Files/Eden", "C:/Program Files (x86)/Eden"}) {
        p = checkExe(pf, "eden");
        if (!p.empty()) break;
      }
    }
#else
    if (p.empty()) p = checkExe("/usr/local/bin", "eden");
    if (p.empty()) p = checkExe("/usr/bin", "eden");
    if (p.empty()) p = checkExe("/opt/eden", "eden");
#endif
    e->foundPath = p;
    return;
  }
}

const ExternalEmulator& ExternalEmulators::get(const std::string& key) const {
  for (const auto& e : list_)
    if (e.key == key) return e;
  static ExternalEmulator none;
  return none;
}

void ExternalEmulators::setUserPath(const std::string& key, const std::string& path) {
  for (auto& e : list_)
    if (e.key == key) {
      e.userPath = path;
      detect(&e);
    }
}

bool ExternalEmulators::available(GamePlatform p) const {
  const PlatformCore c = platformCore(p);
  if (c.kind != CoreKind::Standalone) return false;
  return !get(c.emulatorKey).foundPath.empty();
}

std::string ExternalEmulators::statusText(GamePlatform p) const {
  const PlatformCore c = platformCore(p);
  const ExternalEmulator& e = get(c.emulatorKey);
  if (!e.foundPath.empty()) return e.title + "：" + e.foundPath;
  return e.title + " 未找到（放在 emulators/" + e.key + "/ 或到设置页配置路径）";
}

std::string ExternalEmulators::buildArgs(const ExternalEmulator& e,
                                         const std::string& romPath) const {
  const std::string quoted = "\"" + romPath + "\"";
  if (e.key == "dolphin") {
    // dolphin-emu / Dolphin.exe：-e 执行 + 全屏
    return "-e " + quoted + " --fullscreen";
  }
  if (e.key == "cemu") {
    // Cemu 2.x：-g 游戏 + -f 全屏
    return "-g " + quoted + " -f";
  }
  if (e.key == "eden") {
    // Eden：-g 游戏 + -f 全屏
    return "-g " + quoted + " -f";
  }
  return quoted;
}

bool ExternalEmulators::launch(GamePlatform p, const std::string& romPath,
                               std::string* errMsg) {
  const PlatformCore c = platformCore(p);
  if (c.kind != CoreKind::Standalone) {
    if (errMsg) *errMsg = "该平台不是独立模拟器形态";
    return false;
  }
  detect(const_cast<ExternalEmulator*>(&get(c.emulatorKey)));  // 现场重探测（U盘等）
  const ExternalEmulator& e = get(c.emulatorKey);
  if (e.foundPath.empty()) {
    if (errMsg) *errMsg = e.title + " 未安装";
    return false;
  }
  const std::string args = buildArgs(e, romPath);
  LOGI("启动 %s：%s %s", e.title.c_str(), e.foundPath.c_str(), args.c_str());
#ifdef _WIN32
  if (!spawnDetached(e.foundPath, args)) {
    if (errMsg) *errMsg = "CreateProcess 失败";
    return false;
  }
  return true;
#else
  if (!posixSpawn(e.foundPath, args)) {
    if (errMsg) *errMsg = "posix_spawn 失败";
    return false;
  }
  return true;
#endif
}

}  // namespace ns
