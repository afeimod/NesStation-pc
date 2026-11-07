#include "emu/ExternalEmulators.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX  // 防止 min/max 宏污染后续 STL 头
#endif
#include <windows.h>  // CreateProcessA / STARTUPINFOA / CloseHandle
#else
#include <fcntl.h>
#include <spawn.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>
extern char** environ;
#endif

#include <SDL.h>

#include "app/App.h"
#include "util/Fs.h"
#include "util/Log.h"

namespace ns {

namespace {

// ★v1.3.1 POSIX 启动验证回执通道（launch() 每次填充）
#ifndef _WIN32
App* g_appPtr = nullptr;         // postNotice 用（主循环里安全）
std::string g_stderrLog;        // 本次启动的 stderr 捕获文件
bool g_verify = false;          // 是否做后台秒退验证
#endif

#ifdef _WIN32
const char* kExeSuffix = ".exe";
#else
const char* kExeSuffix = "";
#endif

bool fileExists(const std::string& p) { return fsutil::isFile(p); }

// 拼接并检查（无后缀自动补 .exe on Windows）
std::string checkExe(const std::string& dir, const std::string& name) {
  if (name.empty() || dir.empty()) return "";
  std::string p = fsutil::join(dir, name);
  if (fileExists(p)) return p;
  p += kExeSuffix;
  if (fileExists(p)) return p;
  return "";
}

// %HOME% / ~ 展开（对齐 Android resolveExecutable 的占位符替换）
std::string expandHome(const std::string& dir) {
  const std::string home = fsutil::homeDir();
  std::string out = dir;
  if (!home.empty()) {
    size_t pos;
    while ((pos = out.find("%HOME%")) != std::string::npos)
      out.replace(pos, 6, home);
    while ((pos = out.find('~')) != std::string::npos) out.replace(pos, 1, home);
  }
  return out;
}

// 目录内找 exe 候选；★v1.3.0 改递归（深度 ≤ 4）：CI 超级构建的模拟器安装
// 布局带多层子目录（如 emulators/dolphin/x64/Release/Dolphin.exe），
// 旧版只下探一层 → 内置模拟器探测不到 → 点运行提示装独立模拟器。
static std::string findInDirRecursive(const std::string& dir,
                                      const std::vector<std::string>& names,
                                      int depth) {
  if (depth > 4 || !fsutil::isDir(dir)) return "";
  const auto entries = fsutil::listDir(dir);
  for (const auto& e : entries) {
    if (!e.isDir) continue;
    const std::string& sub = e.name;
    const std::string subDir = fsutil::join(dir, sub);
    // macOS .app 包：Foo.app/Contents/MacOS/Foo
    if (sub.size() > 4 && sub.compare(sub.size() - 4, 4, ".app") == 0) {
      const std::string stem = sub.substr(0, sub.size() - 4);
      const std::string p =
          checkExe(fsutil::join(subDir, "/Contents/MacOS"), stem);
      if (!p.empty()) return p;
      continue;
    }
    // 跳过明显无关目录，减小搜索面
    if (sub == "plugins" || sub == "langs" || sub == "licenses" ||
        sub == "themes" || sub == "documents" || sub.size() > 48)
      continue;
    for (const auto& n : names) {
      const std::string p = checkExe(subDir, n);
      if (!p.empty()) return p;
    }
    const std::string r = findInDirRecursive(subDir, names, depth + 1);
    if (!r.empty()) return r;
  }
  return "";
}

std::string findInDir(const std::string& dir, const std::vector<std::string>& names) {
  if (!fsutil::isDir(dir)) return "";
  // 根层直命中优先（最快路径：emulators/dolphin/Dolphin.exe）
  for (const auto& n : names) {
    const std::string p = checkExe(dir, n);
    if (!p.empty()) return p;
  }
  return findInDirRecursive(dir, names, 1);
}

// macOS .app 直接出现在目录里
std::string findAppBundles(const std::string& dir, const std::vector<std::string>& names) {
  if (!fsutil::isDir(dir)) return "";
  for (const auto& n : names) {
    if (n.size() < 4 || n.compare(n.size() - 4, 4, ".app") != 0) continue;
    const std::string stem = n.substr(0, n.size() - 4);
    std::string p = checkExe(fsutil::join(dir, n + "/Contents/MacOS"), stem);
    if (!p.empty()) return p;
  }
  return "";
}

// 分离启动：Windows CreateProcessW（★v1.3.0 宽字符：中文路径可用；
//   旧版 CreateProcessA 在中文系统把 UTF-8 路径按 GBK 解 → 找不到文件/
//   参数错乱）/ POSIX posix_spawn（不弹控制台）。
// verifyErr 非空时：Windows 端等 2.5s —— 进程若秒退（缺 DLL/运行库）
//   返回失败并写错误信息（旧版 CreateProcess 成功即返回 true，
//   模拟器秒退时界面无任何反应 —— “点了没动静”的根因）。
// ★v1.3.1 POSIX 端：stderr 重定向到日志文件 + 分离线程后台验证 3s
//   （不再阻塞 UI；秒退经 App::postNotice 播报原因，捕获动态库报错）
bool spawnDetached(const std::string& exe, const std::string& args,
                   std::string* verifyErr = nullptr) {
#ifdef _WIN32
  std::string cmd = "\"" + exe + "\"" + (args.empty() ? "" : " " + args);
  std::wstring wcmd;
  {
    const std::wstring w = fsutil::toWide(cmd);
    wcmd.resize(w.size() + 1);  // CreateProcessW 需要可写缓冲
    std::memcpy(&wcmd[0], w.c_str(), w.size() * sizeof(wchar_t));
    wcmd[w.size()] = L'\0';
  }
  STARTUPINFOW si{};
  si.cb = sizeof(si);
  PROCESS_INFORMATION pi{};
  const BOOL ok = CreateProcessW(nullptr, &wcmd[0], nullptr, nullptr, FALSE,
                                 CREATE_UNICODE_ENVIRONMENT, nullptr, nullptr, &si, &pi);
  if (!ok) {
    if (verifyErr) {
      *verifyErr = std::string("CreateProcess 失败（错误码 ") +
                   std::to_string((int)GetLastError()) + "）";
    }
    return false;
  }
  CloseHandle(pi.hThread);
  if (verifyErr) {
    // 等待 2.5s：仍在运行 = 启动成功；秒退 = 启动失败（缺运行库/路径问题）
    const DWORD wr = WaitForSingleObject(pi.hProcess, 2500);
    if (wr == WAIT_OBJECT_0) {
      DWORD code = 0;
      GetExitCodeProcess(pi.hProcess, &code);
      CloseHandle(pi.hProcess);
      if (code != 0) {
        *verifyErr = "模拟器启动后立即退出（错误码 " + std::to_string((int)code) +
                     "）—— 可能缺少 Visual C++ 运行库或文件不完整";
        return false;
      }
      // 秒退且退出码 0：部分模拟器用启动器转接（重拉自身再退出）——放行
      return true;
    }
  }
  CloseHandle(pi.hProcess);
  return true;
#else
  // 拆参数（引号外空格切分，路径带空格时由调用方加引号）
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

  // ★v1.3.1 stderr 捕获：子进程 stderr 追加写入日志（动态库缺失报错在这里）
  const std::string stderrLog = g_stderrLog;  // 由 launch() 设置（按模拟器分文件）
  posix_spawn_file_actions_t fa;
  posix_spawn_file_actions_init(&fa);
  int fdErr = -1;
  if (!stderrLog.empty()) {
    fdErr = open(stderrLog.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fdErr >= 0) posix_spawn_file_actions_adddup2(&fa, fdErr, 2);
  }

  pid_t pid = 0;
  const int rc = posix_spawn(&pid, exe.c_str(), &fa, nullptr, argv.data(), environ);
  if (fdErr >= 0) close(fdErr);
  posix_spawn_file_actions_destroy(&fa);
  if (rc != 0) return false;

  if (g_verify) {
    // ★后台验证线程（不阻塞 UI）：3s 内退出 = 秒退 → postNotice 播报 + 日志尾部
    std::thread([pid, exe, stderrLog]() {
      const Uint64 deadline = SDL_GetTicks() + 3000;
      int status = 0;
      while (SDL_GetTicks() < deadline) {
        const pid_t r = waitpid(pid, &status, WNOHANG);
        if (r == pid) {
          if (WIFEXITED(status) && WEXITSTATUS(status) != 0) {
            std::string tail;
            FILE* f = fsutil::openFile(stderrLog, "rb");
            if (f) {
              char buf[2048];
              size_t n = fread(buf, 1, sizeof(buf) - 1, f);
              fclose(f);
              buf[n] = 0;
              tail.assign(buf);
            }
            std::string msg = "【启动失败】" + fsutil::baseOf(exe) +
                " 启动后立即退出（退出码 " +
                std::to_string(WEXITSTATUS(status)) + "）";
            if (!tail.empty()) {
              // 取日志第一行有效信息（动态库缺失行最关键）
              size_t ls = tail.find_first_not_of(" \r\n");
              size_t le = tail.find('\n', ls == std::string::npos ? 0 : ls);
              if (ls != std::string::npos)
                msg += "\n" + tail.substr(ls, le == std::string::npos ? std::string::npos : le - ls);
            }
            if (!stderrLog.empty()) msg += "\n详细日志：" + stderrLog;
            if (g_appPtr) g_appPtr->postNotice(msg);
          }
          return;  // 正常退出码 0（启动器转接）或被信号终止 → 不报错
        }
        if (r < 0) return;  // waitpid 异常（已被回收等）
        SDL_Delay(120);
      }
      // 3s 仍在运行 → 启动成功；子进程由内核接管（SIGCHLD 忽略态下不产生僵尸）
    }).detach();
    return true;
  }
  int status = 0;
  waitpid(pid, &status, WNOHANG);  // 回收即刻状态，不阻塞
  return true;
#endif
}

std::string shellQuote(const std::string& s) {
#ifdef _WIN32
  return "\"" + s + "\"";
#else
  return "'" + s + "'";
#endif
}

// 通配匹配（对齐 Android matchesPattern：| 分隔多个模式，* 任意）
bool globMatch(const std::string& name, const std::string& pattern) {
  size_t p = 0;
  while (p <= pattern.size()) {
    const size_t bar = pattern.find('|', p);
    const std::string one = pattern.substr(
        p, bar == std::string::npos ? std::string::npos : bar - p);
    // 单模式：* 任意
    bool match = true;
    size_t ni = 0;
    for (size_t i = 0; i <= one.size() && match; i++) {
      if (i == one.size()) {
        match = (ni == name.size());
      } else if (one[i] == '*') {
        // 尝试所有续接（贪心回退）
        const std::string rest = one.substr(i + 1);
        if (rest.find('*') == std::string::npos) {
          match = name.size() >= rest.size() &&
                  name.compare(name.size() - rest.size(), rest.size(), rest) == 0;
          break;
        }
        // 多 * 场景少见，退化递归
        for (size_t k = ni; k <= name.size(); k++) {
          if (globMatch(name.substr(k), rest)) return true;
        }
        match = false;
      } else {
        if (ni >= name.size() || name[ni] != one[i]) match = false;
        else ni++;
      }
    }
    if (match) return true;
    if (bar == std::string::npos) break;
    p = bar + 1;
  }
  return false;
}

// 简易 JSON 资产解析（对齐 Android parseAssetUrls：browser_download_url 列表）
std::vector<std::string> parseAssetUrls(const std::string& body) {
  std::vector<std::string> out;
  const std::string key = "\"browser_download_url\"";
  size_t pos = 0;
  while ((pos = body.find(key, pos)) != std::string::npos) {
    pos = body.find(':', pos + key.size());
    if (pos == std::string::npos) break;
    const size_t q1 = body.find('"', pos);
    if (q1 == std::string::npos) break;
    const size_t q2 = body.find('"', q1 + 1);
    if (q2 == std::string::npos) break;
    out.push_back(body.substr(q1 + 1, q2 - q1 - 1));
    pos = q2 + 1;
  }
  return out;
}

#ifndef _WIN32
void makeExecutable(const std::string& path) { chmod(path.c_str(), 0755); }
#endif

}  // namespace

// —— ExternalCore 方法 ————————————————————————————————————————

std::vector<std::string> ExternalCore::exeNames() const {
#ifdef _WIN32
  return exesWin;
#elif defined(__APPLE__)
  return exesMac;
#else
  return exesLinux;
#endif
}

std::string ExternalCore::buildArgs(const std::string& romPath) const {
  const std::string quoted = "\"" + romPath + "\"";
  std::string out = argTemplate;
  size_t pos;
  while ((pos = out.find("{rom}")) != std::string::npos) out.replace(pos, 5, quoted);
  return out;
}

// —— 注册表（照搬 Android CoreRegistry.EXTERNAL，中文名/说明原样） ——————————

void ExternalEmulators::init(const std::string& dataDir) {
  dataDir_ = dataDir;
  list_.clear();

  auto add = [&](ExternalCore c) { list_.push_back(std::move(c)); };

  ExternalCore pcsx2;
  pcsx2.id = "pcsx2";
  pcsx2.name = "PCSX2";
  pcsx2.platform = GamePlatform::PS2;
  pcsx2.sourceRepo = "https://github.com/PCSX2/pcsx2";
  pcsx2.downloadPage = "https://pcsx2.net/downloads";
  pcsx2.githubApi = "https://api.github.com/repos/PCSX2/pcsx2/releases/latest";
  pcsx2.assetWin = "pcsx2-v*-windows-x64-qt.7z|pcsx2-v*-windows-x64.7z|pcsx2-*win*x64*.zip";
  pcsx2.assetLinux = "pcsx2-v*-linux-x64-qt.AppImage|pcsx2-*linux*x64*";
  pcsx2.assetMac = "pcsx2-v*-macos-qt.dmg|pcsx2-*macos*";
  pcsx2.exesWin = {"pcsx2-qtx64.exe", "pcsx2-qx64.exe", "pcsx2.exe"};
  pcsx2.exesLinux = {"pcsx2-qt", "pcsx2", "PCSX2"};
  pcsx2.exesMac = {"PCSX2.app"};
  pcsx2.argTemplate = "--fullscreen {rom}";
  pcsx2.commonDirs = {
      "%HOME%/AppData/Local/Programs/PCSX2", "%HOME%/scoop/apps/pcsx2",
      "C:/Program Files/PCSX2", "/usr/bin", "/usr/local/bin", "%HOME%/Applications",
      "/Applications"};
  pcsx2.notes = "PS2 推荐：原生 GPU 全速渲染。PS2 BIOS 放核心自身 bios 目录。";
  add(std::move(pcsx2));

  ExternalCore ppsspp;
  ppsspp.id = "ppsspp";
  ppsspp.name = "PPSSPP";
  ppsspp.platform = GamePlatform::PSP;
  ppsspp.sourceRepo = "https://github.com/hrydgard/ppsspp";
  ppsspp.downloadPage = "https://www.ppsspp.org/download";
  ppsspp.githubApi = "https://api.github.com/repos/hrydgard/ppsspp/releases/latest";
  ppsspp.assetWin = "PPSSPPWindows*.zip|PPSSPPWindowsSetup.exe";
  ppsspp.assetLinux = "ppsspp-*.AppImage|ppsspp*.deb";
  ppsspp.assetMac = "PPSSPPSDL*.dmg|PPSSPP*.dmg";
  ppsspp.exesWin = {"PPSSPPWindows64.exe", "PPSSPPWindows.exe"};
  ppsspp.exesLinux = {"PPSSPPSDL", "ppsspp", "ppsspp-qt"};
  ppsspp.exesMac = {"PPSSPP.app"};
  ppsspp.argTemplate = "--fullscreen {rom}";
  ppsspp.commonDirs = {
      "C:/Program Files/PPSSPP", "%HOME%/AppData/Local/Programs/PPSSPP",
      "/usr/bin", "/usr/local/bin", "/Applications", "%HOME%/Applications"};
  ppsspp.notes = "PSP 推荐：原生 GPU 全速渲染，自带加速即时存档。";
  add(std::move(ppsspp));

  ExternalCore flycast;
  flycast.id = "flycast";
  flycast.name = "Flycast";
  flycast.platform = GamePlatform::DC;
  flycast.sourceRepo = "https://github.com/flycast/flycast";
  flycast.downloadPage = "https://flycast.github.io";
  flycast.githubApi = "https://api.github.com/repos/flycast/flycast/releases/latest";
  flycast.assetWin = "flycast-win64*.zip|flycast-win32*.zip";
  flycast.assetLinux = "flycast-linux*.AppImage|flycast-*.AppImage";
  flycast.assetMac = "flycast-macos*.dmg|flycast*.dmg";
  flycast.exesWin = {"flycast.exe"};
  flycast.exesLinux = {"flycast", "flycast.elf"};
  flycast.exesMac = {"Flycast.app"};
  flycast.argTemplate = "{rom}";
  flycast.commonDirs = {
      "C:/Program Files/Flycast", "%HOME%/AppData/Local/Programs/flycast",
      "/usr/bin", "/usr/local/bin", "/Applications", "%HOME%/Applications"};
  flycast.notes = "DC/NAOMI/AtomisWave。光盘游戏需要 dc_boot.bin + dc_flash.bin（放核心数据目录）。";
  add(std::move(flycast));

  ExternalCore azahar;
  azahar.id = "azahar";
  azahar.name = "Azahar";
  azahar.platform = GamePlatform::N3DS;
  azahar.sourceRepo = "https://github.com/azahar-emu/azahar";
  azahar.downloadPage = "https://azahar-emu.org/pages/download.html";
  azahar.githubApi = "https://api.github.com/repos/azahar-emu/azahar/releases/latest";
  azahar.assetWin = "azahar-*-windows-x86_64.7z|azahar-*-windows-msvc.7z|azahar-*win*.zip";
  azahar.assetLinux = "azahar-*-linux-x86_64.tar.xz|azahar-*linux*.tar.*";
  azahar.assetMac = "azahar-*-macos-universal.dmg|azahar-*macos*.dmg";
  azahar.exesWin = {"azahar-qt.exe", "azahar.exe"};
  azahar.exesLinux = {"azahar", "azahar-qt"};
  azahar.exesMac = {"Azahar.app"};
  azahar.argTemplate = "-g {rom}";
  azahar.commonDirs = {
      "%HOME%/AppData/Local/azahar", "C:/Program Files/Azahar",
      "/usr/bin", "/usr/local/bin", "/Applications", "%HOME%/Applications"};
  azahar.notes = "3DS（Citra 系官方延续版）。加密游戏需 aes_keys.txt；部分游戏需系统档案。";
  add(std::move(azahar));

  ExternalCore ishiiruka;
  ishiiruka.id = "ishiiruka";
  ishiiruka.name = "Ishiiruka（Dolphin 系）";
  ishiiruka.platform = GamePlatform::NGCWII;
  ishiiruka.sourceRepo = "https://github.com/Tinob/Ishiiruka";
  ishiiruka.downloadPage = "https://github.com/Tinob/Ishiiruka/releases";
  ishiiruka.exesWin = {"Ishiiruka.exe", "Dolphin.exe", "Dolphin-x64.exe"};
  ishiiruka.exesLinux = {"dolphin-emu", "ishiiruka"};
  ishiiruka.exesMac = {"Dolphin.app", "Ishiiruka.app"};
  ishiiruka.argTemplate = "-e {rom}";
  ishiiruka.commonDirs = {
      "C:/Program Files/Dolphin-Ishiiruka", "C:/Program Files/Dolphin",
      "%HOME%/AppData/Local/Programs/Dolphin",
      "/usr/bin", "/usr/local/bin", "/Applications", "%HOME%/Applications"};
  ishiiruka.notes = "GC/Wii（Dolphin 优化分支，PC 版原生构建；标准 Dolphin 同参数兼容）。";
  add(std::move(ishiiruka));

  ExternalCore cemu;
  cemu.id = "cemu";
  cemu.name = "Cemu";
  cemu.platform = GamePlatform::WIIU;
  cemu.sourceRepo = "https://github.com/cemu-project/Cemu";
  cemu.downloadPage = "https://cemu.info";
  cemu.githubApi = "https://api.github.com/repos/cemu-project/Cemu/releases/latest";
  cemu.assetWin = "cemu-*-windows-x64.zip|cemu_*_win64.zip";
  cemu.assetLinux = "cemu-*-ubuntu-24.04-x86_64.AppImage|cemu-*.AppImage";
  cemu.assetMac = "cemu-*-macos-x86_64.dmg|cemu-*macos*";
  cemu.exesWin = {"Cemu.exe"};
  cemu.exesLinux = {"Cemu", "cemu"};
  cemu.exesMac = {"Cemu.app"};
  cemu.argTemplate = "-g {rom} -f";
  cemu.commonDirs = {
      "C:/Program Files/Cemu", "%HOME%/AppData/Local/Programs/Cemu",
      "/opt/cemu", "%HOME%/Applications", "/Applications"};
  cemu.notes = "Wii U（官方开源 PC 版，Vulkan 全速）。WUD 解密需 otp.bin。";
  add(std::move(cemu));

  ExternalCore eden;
  eden.id = "eden";
  eden.name = "Eden";
  eden.platform = GamePlatform::NSWITCH;
  eden.sourceRepo = "https://github.com/eden-emulator/mirror";
  eden.downloadPage = "https://eden-emu.dev";
  eden.exesWin = {"eden.exe", "Eden.exe"};
  eden.exesLinux = {"eden", "Eden"};
  eden.exesMac = {"Eden.app"};
  // ★v1.3.0：yuzu 系 CLI 用 -g/--game 指定游戏文件（新版已不接受裸位置参数）
  eden.argTemplate = "-g {rom}";
  eden.commonDirs = {
      "%HOME%/AppData/Local/eden", "C:/Program Files/Eden",
      "/usr/bin", "/usr/local/bin", "/opt/eden", "/Applications", "%HOME%/Applications"};
  eden.notes = "Switch（yuzu 系社区延续版）。加密游戏需 prod.keys（核心设置内导入）。";
  add(std::move(eden));

  ExternalCore freej2me;
  freej2me.id = "freej2me";
  freej2me.name = "FreeJ2ME";
  freej2me.platform = GamePlatform::JAVA;
  freej2me.sourceRepo = "https://github.com/hex007/FreeJ2ME";
  freej2me.downloadPage = "https://github.com/hex007/FreeJ2ME/releases";
  freej2me.exesWin = {"FreeJ2ME.jar"};
  freej2me.exesLinux = {"FreeJ2ME.jar"};
  freej2me.exesMac = {"FreeJ2ME.jar"};
  freej2me.argTemplate = "{rom}";
  freej2me.notes = "J2ME（Java ME）PC 端开源模拟器，需要系统已安装 Java 运行时（java）启动。";
  add(std::move(freej2me));

  for (auto& e : list_) detect(&e);
}

// —— 探测（对齐 Android resolveExecutable 顺序） ——————————————————

void ExternalEmulators::detect(ExternalCore* e) {
  e->foundPath.clear();

  // 1) 用户显式设置
  if (!e->userPath.empty() && fileExists(e->userPath)) {
    e->foundPath = e->userPath;
    return;
  }

  const std::vector<std::string> names = e->exeNames();
  if (names.empty()) return;

  // 2) NesStation 管理的安装目录（自动下载落点）
  std::string p = findInDir(installDir(e->id), names);
  if (p.empty()) p = findAppBundles(installDir(e->id), names);

  // 3) exe 同目录 emulators/<id>/（超级构建 / 便携包形态）
  //    ishiiruka 额外认 emulators/dolphin/（CMake 超级构建 NESSTATION_BUILD_DOLPHIN
  //    的产物目录名沿用上游 Dolphin；标准 Dolphin 同参数兼容，对齐 APK notes）
  if (p.empty()) {
    const std::string bundle = fsutil::join(fsutil::join(fsutil::exeDir(), "emulators"), e->id);
    p = findInDir(bundle, names);
    if (p.empty()) p = findAppBundles(bundle, names);
  }
  if (p.empty() && e->id == "ishiiruka") {
    const std::string bundle = fsutil::join(fsutil::join(fsutil::exeDir(), "emulators"), "dolphin");
    p = findInDir(bundle, names);
    if (p.empty()) p = findAppBundles(bundle, names);
  }

  // 4) 常见安装位置
  if (p.empty()) {
    for (const auto& dirRaw : e->commonDirs) {
      const std::string dir = expandHome(dirRaw);
      if (dir.empty() || !fsutil::isDir(dir)) continue;
      p = checkExe(dir, names[0]);
      for (size_t i = 0; p.empty() && i < names.size(); i++) p = checkExe(dir, names[i]);
      if (p.empty()) p = findInDir(dir, names);
      if (p.empty()) p = findAppBundles(dir, names);
      if (!p.empty()) break;
    }
  }

  e->foundPath = p;
}

std::string ExternalEmulators::installDir(const std::string& id) const {
  return fsutil::join(fsutil::join(dataDir_, "external"), id);
}

std::vector<const ExternalCore*> ExternalEmulators::forPlatform(GamePlatform p) const {
  std::vector<const ExternalCore*> out;
  for (const auto& e : list_)
    if (e.platform == p) out.push_back(&e);
  return out;
}

const ExternalCore* ExternalEmulators::find(const std::string& id) const {
  for (const auto& e : list_)
    if (e.id == id) return &e;
  return nullptr;
}

bool ExternalEmulators::available(GamePlatform p) const {
  return availableCount(p) > 0;
}

int ExternalEmulators::availableCount(GamePlatform p) const {
  int n = 0;
  for (const auto& e : list_)
    if (e.platform == p && !e.foundPath.empty()) n++;
  return n;
}

bool ExternalEmulators::available(const std::string& id) const {
  const ExternalCore* e = find(id);
  return e && !e->foundPath.empty();
}

std::string ExternalEmulators::statusText(GamePlatform p) const {
  for (const auto& e : list_) {
    if (e.platform != p) continue;
    if (!e.foundPath.empty()) return e.name + "：" + e.foundPath;
    return e.name + " 未找到（可在「核心管理」自动下载 / 手动选择路径）";
  }
  return "该平台无外置核心";
}

void ExternalEmulators::setUserPath(const std::string& id, const std::string& path) {
  for (auto& e : list_)
    if (e.id == id) {
      e.userPath = path;
      detect(&e);
    }
}

void ExternalEmulators::clearUserPath(const std::string& id) {
  for (auto& e : list_)
    if (e.id == id) {
      e.userPath.clear();
      e.foundPath.clear();
      // 删除 NesStation 管理的安装目录（对齐 Android installDir().deleteRecursively()）
      const std::string dir = installDir(id);
      std::string cmd;
#ifdef _WIN32
      cmd = "rd /s /q " + shellQuote(dir);
#else
      cmd = "rm -rf " + shellQuote(dir);
#endif
      (void)fsutil::runCommand(cmd);
      detect(&e);
    }
}

void ExternalEmulators::reDetect(const std::string& id) {
  for (auto& e : list_)
    if (e.id == id) detect(&e);
}

// —— 启动 ——————————————————————————————————————————————————————

bool ExternalEmulators::launch(const ExternalCore& c, const std::string& romPath,
                               std::string* err) {
  // 现场重探测（U 盘 / 刚安装等场景）
  for (auto& e : list_)
    if (e.id == c.id) detect(&e);
  const ExternalCore* live = find(c.id);
  if (!live || live->foundPath.empty()) {
    if (err) *err = c.name + " 未安装（可在设置 → 核心管理安装）";
    return false;
  }

  std::string exe = live->foundPath;
  std::string args = live->buildArgs(romPath);

  // FreeJ2ME：jar 需要 Java 运行时（对齐 APK「自带 Java 运行时」→ PC 用系统 java）
  if (exe.size() > 4 && exe.compare(exe.size() - 4, 4, ".jar") == 0) {
    exe = "java";  // CreateProcess / posix_spawn 均按 PATH 搜索
    args = "-jar \"" + live->foundPath + "\" " + args;
  }

  LOGI("启动 %s：%s %s", live->name.c_str(), exe.c_str(), args.c_str());
#ifdef _WIN32
  // ★v1.3.0：启动验证 —— 模拟器秒退（缺 DLL/路径问题）时明确报错，
  //   不再「提示已启动但啥也没发生」
  std::string vErr;
  if (!spawnDetached(exe, args, &vErr)) {
    if (err) *err = vErr + "（" + live->name + "）";
    LOGE("外置核心启动失败：%s", vErr.c_str());
    return false;
  }
  return true;
#else
  if (exe == "java") {
    // posix_spawn 对 PATH 搜索仅当路径不含 '/'；java 直接可用
    if (!spawnDetached(exe, args)) {
      if (err) *err = "启动 java 失败（请确认已安装 Java 运行时）";
      return false;
    }
    return true;
  }
  // ★v1.3.1 动态库预检：Linux 下模拟器秒退的头号原因是缺 Qt6/boost 等运行库
  //   （发行包只带引擎不带系统库）。先枚举缺失库，缺了就直接给出安装提示，
  //   不再「点了没反应」。
  {
    const std::vector<std::string> missing = fsutil::missingSharedLibs(exe);
    if (missing.size() == 1 && missing[0] == "?") {
      // 无法判定（非 ELF / 检测失败）→ 照常启动，靠后台验证兜底
    } else if (!missing.empty()) {
      std::string libs;
      for (const auto& m : missing) libs += libs.empty() ? m : ("、" + m);
      std::string hint;
      if (exe.find("dolphin") != std::string::npos ||
          exe.find("Dolphin") != std::string::npos ||
          exe.find("ishiiruka") != std::string::npos) {
        hint = "\nDebian/Ubuntu 安装：sudo apt install libqt6widgets6 libqt6gui6 "
               "libqt6core6 libqt6dbus6 libqt6svg6 libqt6charts6 libpulse0 libasound2";
      } else if (exe.find("eden") != std::string::npos ||
                 exe.find("Eden") != std::string::npos) {
        hint = "\nDebian/Ubuntu 安装：sudo apt install libboost-locale1.74+ libfmt9 "
               "liblz4-1 libopus0 libzstd1 zlib1g libvulkan1";
      } else if (exe.find("Cemu") != std::string::npos) {
        hint = "\nDebian/Ubuntu 安装：sudo apt install libgtk-3-0 libpulse0 "
               "libvulkan1 libboost-filesystem1.74+ libboost-program-options1.74+";
      }
      if (err) *err = live->name + " 缺少系统运行库：" + libs + hint;
      LOGE("外置核心启动失败（缺库）：%s", libs.c_str());
      return false;
    }
  }
  // ★v1.3.1 后台验证（stderr 捕获到 external/<id>-stderr.log，秒退时播报原因）
  g_appPtr = appPtr_;
  g_stderrLog = fsutil::join(dataDir_, "external", live->id + "-stderr.log");
  fsutil::makeDirs(fsutil::join(dataDir_, "external"));
  g_verify = true;
  if (!spawnDetached(exe, args)) {
    g_verify = false;
    if (err) *err = "posix_spawn 失败（" + live->name + "）";
    return false;
  }
  return true;
#endif
}

bool ExternalEmulators::launchPlatform(GamePlatform p, const std::string& romPath,
                                       std::string* err) {
  const auto defs = forPlatform(p);
  if (defs.empty()) {
    if (err) *err = "该平台不是独立模拟器形态";
    return false;
  }
  // 链上第一个已就绪的核心
  for (const auto* c : defs) {
    const ExternalCore* live = find(c->id);
    if (live && !live->foundPath.empty()) return launch(*live, romPath, err);
  }
  if (err) *err = defs[0]->name + " 未安装（可在设置 → 核心管理安装）";
  return false;
}

// —— GitHub 自动下载（对齐 Android CoreDownloader.installExternal） ——————————

bool ExternalEmulators::downloadOne(ExternalCore* c) {
  const std::string os =
#ifdef _WIN32
      "windows";
#elif defined(__APPLE__)
      "macos";
#else
      "linux";
#endif
  const std::string* pattern = nullptr;
#ifdef _WIN32
  pattern = &c->assetWin;
#elif defined(__APPLE__)
  pattern = &c->assetMac;
#else
  pattern = &c->assetLinux;
#endif
  if (pattern->empty()) {
    std::lock_guard<std::mutex> lk(mu_);
    prog_.label = c->name + " 暂无 " + os + " 自动下载（请手动安装）";
    return false;
  }

  const std::string tmp = fsutil::join(dataDir_, "external");
  fsutil::makeDirs(tmp);
  const std::string apiJson = fsutil::join(tmp, c->id + "-release.json");

  // 1) 查询 GitHub 最新版本
  {
    std::lock_guard<std::mutex> lk(mu_);
    prog_.label = "查询 GitHub 最新版本（" + c->name + "）…";
  }
  std::string cmd = std::string(
                        "curl -fSL --retry 2 --connect-timeout 15 --max-time 60 "
                        "-A NesStationPC/1.1 -H \"Accept: application/vnd.github+json\" -o ") +
                    shellQuote(apiJson) + " " + shellQuote(c->githubApi);
  if (fsutil::runCommand(cmd) != 0 || !fsutil::isFile(apiJson)) {
    std::lock_guard<std::mutex> lk(mu_);
    prog_.label = "查询 GitHub 失败（" + c->name + "：网络受限或速率限制）";
    return false;
  }
  std::string body;
  {
    FILE* f = fsutil::openFile(apiJson, "rb");
    if (!f) {
      std::lock_guard<std::mutex> lk(mu_);
      prog_.label = "读取版本信息失败（" + c->name + "）";
      return false;
    }
    char buf[65536];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), f)) > 0) body.append(buf, n);
    fclose(f);
    fsutil::removeFile(apiJson);
  }

  // 2) 匹配资产
  const auto urls = parseAssetUrls(body);
  std::string assetUrl, assetName;
  for (const auto& u : urls) {
    const std::string name = u.substr(u.find_last_of('/') + 1);
    if (globMatch(name, *pattern)) {
      assetUrl = u;
      assetName = name;
      break;
    }
  }
  if (assetUrl.empty()) {
    std::lock_guard<std::mutex> lk(mu_);
    prog_.label = "最新 Release 未匹配到 " + os + " 资产（" + c->name + " 可手动下载）";
    return false;
  }

  // 3) 下载
  {
    std::lock_guard<std::mutex> lk(mu_);
    prog_.label = "下载 " + assetName + " …";
  }
  const std::string archive = fsutil::join(tmp, c->id + "-download");
  cmd = std::string("curl -fSL --retry 3 --retry-delay 3 --connect-timeout 15 "
                    "--max-time 1800 -A NesStationPC/1.1 -o ") +
        shellQuote(archive) + " " + shellQuote(assetUrl);
  if (fsutil::runCommand(cmd) != 0 || !fsutil::isFile(archive)) {
    fsutil::removeFile(archive);
    std::lock_guard<std::mutex> lk(mu_);
    prog_.label = "下载失败（" + c->name + "）";
    return false;
  }

  // 4) 解压到安装目录
  {
    std::lock_guard<std::mutex> lk(mu_);
    prog_.label = "安装到核心目录（" + c->name + "）…";
  }
  const std::string dir = installDir(c->id);
  fsutil::makeDirs(dir);
  bool extracted = false;
  if (assetName.size() > 4 &&
      (assetName.compare(assetName.size() - 4, 4, ".zip") == 0 ||
       assetName.compare(assetName.size() - 4, 4, ".7z") == 0 ||
       assetName.find(".tar.") != std::string::npos ||
       assetName.size() > 4 && assetName.compare(assetName.size() - 4, 4, ".tgz") == 0)) {
    // 归档：zip → unzip / python3 / tar；tar.* → tar；7z → 7z
    if (assetName.compare(assetName.size() - 4, 4, ".zip") == 0) {
      const std::string ex[] = {
          "unzip -o -q " + shellQuote(archive) + " -d " + shellQuote(dir),
          "python3 -m zipfile -e " + shellQuote(archive) + " " + shellQuote(dir),
          "tar -xf " + shellQuote(archive) + " -C " + shellQuote(dir),
      };
      for (const auto& e : ex)
        if (fsutil::runCommand(e) == 0) {
          extracted = true;
          break;
        }
    } else if (assetName.find(".tar.") != std::string::npos ||
               assetName.compare(assetName.size() - 4, 4, ".tgz") == 0) {
      extracted = fsutil::runCommand("tar -xf " + shellQuote(archive) + " -C " + shellQuote(dir)) == 0;
    } else {  // .7z
      const std::string sz[] = {"7z", "7za", "/usr/bin/7z", "/usr/bin/7za"};
      for (const auto& z : sz) {
        if (fsutil::runCommand(std::string("command -v ") + z + " >/dev/null 2>&1") == 0) {
          extracted = fsutil::runCommand(z + " x -y -o" + shellQuote(dir) + " " + shellQuote(archive)) == 0;
          break;
        }
      }
      if (!extracted) {
        fsutil::removeFile(archive);
        std::lock_guard<std::mutex> lk(mu_);
        prog_.label = "需要 7-Zip 解包：请安装 7z 后重试，或手动解压到 " + dir;
        return false;
      }
    }
  } else {
    // 单文件资产（AppImage / exe 等）：直接落位
    const std::string dest = fsutil::join(dir, assetName);
    FILE* in = fsutil::openFile(archive, "rb");
    FILE* out = fsutil::openFile(dest, "wb");
    if (in && out) {
      char buf[65536];
      size_t n;
      while ((n = fread(buf, 1, sizeof(buf), in)) > 0) fwrite(buf, 1, n, out);
      extracted = true;
    }
    if (in) fclose(in);
    if (out) fclose(out);
  }
  fsutil::removeFile(archive);
  if (!extracted) {
    std::lock_guard<std::mutex> lk(mu_);
    prog_.label = "解压失败（" + c->name + "）";
    return false;
  }

  // 5) 可执行化（AppImage / 可执行名；对齐 Android setExecutable）
#ifndef _WIN32
  for (const auto& e : fsutil::listDir(dir)) {
    const std::string p = fsutil::join(dir, e.name);
    if (e.name.find(".AppImage") != std::string::npos) makeExecutable(p);
    for (const auto& n : c->exeNames())
      if (e.name == n) makeExecutable(p);
  }
#endif

  // 6) 重探测
  detect(c);
  if (c->foundPath.empty()) {
    std::lock_guard<std::mutex> lk(mu_);
    prog_.label = "已安装但未找到可执行文件（" + c->name + "：可手动选择路径）";
    return false;
  }
  {
    std::lock_guard<std::mutex> lk(mu_);
    prog_.label = "已安装 " + c->name + " ✓：" + c->foundPath;
  }
  return true;
}

void ExternalEmulators::startDownload(const std::string& id) {
  if (dlRunning_.load()) return;
  ExternalCore* target = nullptr;
  for (auto& e : list_)
    if (e.id == id) target = &e;
  if (!target || target->githubApi.empty()) return;
  // 拷贝一份给线程（list_ 可能被 UI 侧 reDetect 改写 foundPath；线程内只写自己的拷贝，
  // 结束时把 foundPath 同步回主列表）
  const ExternalCore snapshot = *target;
  if (dlThread_.joinable()) dlThread_.join();
  {
    std::lock_guard<std::mutex> lk(mu_);
    prog_ = ExtProgress{};
    prog_.running = true;
    prog_.label = "准备下载 " + snapshot.name + "…";
  }
  dlRunning_ = true;
  dlThread_ = std::thread([this, snapshot]() {
    ExternalCore c = snapshot;
    const bool ok = downloadOne(&c);
    // 同步探测结果回主列表
    for (auto& e : list_)
      if (e.id == c.id) e.foundPath = c.foundPath;
    {
      std::lock_guard<std::mutex> lk(mu_);
      prog_.running = false;
      prog_.finished = true;
      prog_.ok = ok;
    }
    dlRunning_ = false;
  });
}

ExternalEmulators::ExtProgress ExternalEmulators::progress() const {
  std::lock_guard<std::mutex> lk(mu_);
  return prog_;
}

void ExternalEmulators::clearFinishFlag() {
  std::lock_guard<std::mutex> lk(mu_);
  prog_.finished = false;
}

ExternalEmulators::~ExternalEmulators() {
  if (dlThread_.joinable()) dlThread_.join();
}

}  // namespace ns
