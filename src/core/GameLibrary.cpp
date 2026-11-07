#include "core/GameLibrary.h"

#include <algorithm>
#include <cstdio>
#include <unordered_map>

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

// —— APK GamePlatform.kt 格式化（文案 1:1；HomeScenes / Library 共用）——
std::string formatPlayTime(int64_t sec) {
  if (sec < 60) return sec > 0 ? std::to_string(sec) + " 秒" : "—";
  if (sec < 3600) return std::to_string(sec / 60) + " 分钟";
  return std::to_string(sec / 3600) + " 小时 " + std::to_string((sec % 3600) / 60) + " 分";
}

std::string formatLastPlayed(int64_t epoch) {
  if (epoch <= 0) return "从未游玩";
  const int64_t diff = (int64_t)time(nullptr) - epoch;
  if (diff < 60) return "刚刚游玩";
  if (diff < 3600) return std::to_string(diff / 60) + " 分钟前";
  if (diff < 86400) return std::to_string(diff / 3600) + " 小时前";
  return std::to_string(diff / 86400) + " 天前";
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

// —— ★v1.2.11 APK 导入过滤链（LibraryScreen.kt scanLocalFolderForRoms）——
namespace {

// 任一路径分量以 '.' 开头 → 隐藏文件/目录（APK 跳过规则）
bool hiddenSegment(const std::string& path) {
  size_t i = 0;
  while (i < path.size()) {
    size_t j = path.find('/', i);
    if (j == std::string::npos) j = path.find('\\', i);
    if (j == std::string::npos) j = path.size();
    if (j > i && path[i] == '.') return true;
    i = j + 1;
  }
  return false;
}

bool extInVec(const std::string& e, const std::vector<std::string>& v) {
  for (const auto& x : v)
    if (x == e) return true;
  return false;
}

// APK ROM_EXTENSIONS 白名单 + CD 附属去重（skipIfCue/skipIfCcd/skipIfGdi）+
// Switch 限定（SWITCH_DIRECT_EXTENSIONS）。maxDepth=5 由调用方保证。
std::vector<std::string> filterImportFiles(const std::vector<std::string>& files,
                                           GamePlatform hint) {
  const std::vector<std::string>& master = romExtensionsMaster();
  // 每目录 cue/ccd/gdi 存在性（CD 附属文件两遍去重，APK 同）
  std::unordered_map<std::string, bool> hasCue, hasCcd, hasGdi;
  for (const auto& f : files) {
    const std::string ext = fsutil::extOf(f);
    const std::string dir = fsutil::dirOf(f);
    if (ext == "cue") hasCue[dir] = true;
    if (ext == "ccd") hasCcd[dir] = true;
    if (ext == "gdi") hasGdi[dir] = true;
  }
  auto dirHas = [](const std::unordered_map<std::string, bool>& m,
                   const std::string& dir) {
    const auto it = m.find(dir);
    return it != m.end() && it->second;
  };
  std::vector<std::string> out;
  for (const auto& f : files) {
    if (hiddenSegment(f)) continue;
    const std::string ext = fsutil::extOf(f);
    if (ext.empty() || !extInVec(ext, master)) continue;
    // Switch 页：只收 xci/nsp/nca/nro（Eden 不能跑压缩包）
    if (hint == GamePlatform::NSWITCH &&
        !(ext == "xci" || ext == "nsp" || ext == "nca" || ext == "nro"))
      continue;
    if (ext == "sub") continue;  // .sub 永远是 CD 附属文件
    const std::string dir = fsutil::dirOf(f);
    // 目录内有 .cue → 只导入 .cue（跳过 img/bin/ccd/iso）
    if (dirHas(hasCue, dir) &&
        (ext == "img" || ext == "bin" || ext == "ccd" || ext == "iso"))
      continue;
    // 目录内有 .ccd → 跳过 img（sub 已跳过）
    if (dirHas(hasCcd, dir) && ext == "img") continue;
    // 目录内有 .gdi（DC）→ 跳过轨道文件
    if (dirHas(hasGdi, dir) &&
        (ext == "bin" || ext == "raw" || ext == "img" || ext == "iso"))
      continue;
    out.push_back(f);
  }
  return out;
}

// APK findDosLauncherInLocalFolder：DOS 页导入 = 找启动器（优先级表 →
// 任意 .bat → 任意 .exe → 任意 .com），单条入库，标题 "folder(exec)"
std::string findDosLauncher(const std::string& dir) {
  static const char* kPriority[] = {
      "play.bat",    "run.bat",     "start.bat", "autoexec.bat", "go.bat",
      "launch.bat",  "main.bat",    "play.exe",  "run.exe",      "start.exe",
      "setup.exe",   "game.exe",    "main.exe",  "launch.exe"};
  std::vector<std::string> files;
  try {
    fsutil::walkFiles(dir, &files, 5);
  } catch (...) {
    return "";
  }
  std::string best;
  int bestRank = 999;
  std::string firstBat, firstExe, firstCom;
  for (const auto& f : files) {
    if (hiddenSegment(f)) continue;
    const std::string ext = fsutil::extOf(f);
    if (ext != "bat" && ext != "exe" && ext != "com") continue;
    std::string name = fsutil::baseOf(f);
    for (auto& c : name)
      if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
    for (size_t r = 0; r < sizeof(kPriority) / sizeof(kPriority[0]); r++) {
      if (name == kPriority[r] && (int)r < bestRank) {
        bestRank = (int)r;
        best = f;
      }
    }
    if (ext == "bat" && firstBat.empty()) firstBat = f;
    if (ext == "exe" && firstExe.empty()) firstExe = f;
    if (ext == "com" && firstCom.empty()) firstCom = f;
  }
  if (bestRank < 999) return best;
  if (!firstBat.empty()) return firstBat;
  if (!firstExe.empty()) return firstExe;
  return firstCom;
}

// path 是否位于 dir 下（分隔符/大小写归一后前缀比较；Windows 盘符大小写兼容）
bool pathUnder(const std::string& path, const std::string& dir) {
  auto norm = [](std::string s) {
    for (auto& c : s) {
      if (c == '\\') c = '/';
      c = (char)tolower((unsigned char)c);
    }
    return s;
  };
  std::string p = norm(path), d = norm(dir);
  while (d.size() > 1 && d.back() == '/') d.pop_back();
  if (d.empty()) return false;
  return p.rfind(d + "/", 0) == 0;
}

}  // namespace

void GameLibrary::load(const std::string& dataDir) {
  dataDir_ = dataDir;
  libraryFile_ = fsutil::join(dataDir, "library.json");
  coverDir_ = fsutil::join(dataDir, "covers");
  games_.clear();
  romDirs_.clear();
  importedFolders_.clear();

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
            if (!e.romPath.empty() && fsutil::isFile(e.romPath)) {
              // id 恒为 romPath 的稳定哈希：加载时重算自愈（手改/旧格式/
              // 缺失 id 的库文件不会破坏扫描去重）
              e.id = gameIdForPath(e.romPath);
              games_.push_back(std::move(e));
            }
          }
        }
        if (j.contains("romDirs") && j["romDirs"].is_array()) {
          for (const auto& d : j["romDirs"])
            if (d.is_string()) romDirs_.push_back(d.get<std::string>());
        }
        // ★v1.2.11 导入文件夹注册表（APK imported_folders 同构）
        if (j.contains("importedFolders") && j["importedFolders"].is_array()) {
          for (const auto& f : j["importedFolders"]) {
            ImportedFolder im;
            im.dir = f.value("dir", "");
            im.platform = platformFromString(f.value("platform", "NES"));
            if (!im.dir.empty()) importedFolders_.push_back(std::move(im));
          }
        }
      } else {
        LOGW("library.json 解析失败，重建库");
      }
    } catch (const std::exception& e) {
      LOGW("library.json 读取异常: %s", e.what());
    }
  }
  LOGI("游戏库载入：%zu 个游戏，%zu 个 ROM 目录，%zu 个导入文件夹",
       games_.size(), romDirs_.size(), importedFolders_.size());

  // ★v1.2.10 存量数据修复：旧版把识别不了的 ROM 全部入库为 NES（FC 核心）。
  //   载入时对「平台=NES 但扩展名非 NES 系」的条目重新识别，能确定真实
  //   平台则就地修正（对齐 APK RomStore.add 重复导入时更新 platform 的语义）。
  revalidatePlatforms();
}

void GameLibrary::revalidatePlatforms() {
  static const char* kNesExts[] = {"nes", "unf", "unif", "fds", "nez", "unh"};
  int fixed = 0;
  for (auto& e : games_) {
    if (e.platform != GamePlatform::NES) continue;
    const std::string ext = fsutil::extOf(e.romPath);
    std::string extL = ext;
    for (auto& c : extL)
      if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
    bool isNesExt = false;
    for (const char* x : kNesExts)
      if (extL == x) isNesExt = true;
    if (isNesExt) continue;  // 合法 NES 条目

    GamePlatform p;
    bool known = false;
    if (extL == "zip") {
      // zip 探头（宽松）：能识别内部平台则修正
      known = detectRomPlatformStrict(e.romPath, &p);
    } else if (extL == "7z" || extL == "gz") {
      p = GamePlatform::ARCADE;
      known = true;
    } else {
      // 严格判定（白名单/光盘嗅探）→ 未命中再用扩展名宽松表；
      // .cue 嗅探不出时默认 PSX（原逻辑「.cue 默认 PS1」，.cue 进 DOS 表是
      // fromExtension 的归置而非识别结论）
      if (detectRomPlatformStrict(e.romPath, &p)) {
        known = true;
      } else if (extL == "cue") {
        p = GamePlatform::PSX;
        known = true;
      } else if (platformFromExtensionKnown(extL, &p)) {
        known = true;
      }
    }
    if (known && p != GamePlatform::NES) {
      LOGI("平台修正：%s → %s", fsutil::baseOf(e.romPath).c_str(),
           platformShortName(p));
      e.platform = p;
      fixed++;
    }
  }
  if (fixed > 0) {
    LOGI("存量平台修复：%d 个条目", fixed);
    save();
  }
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
  // ★v1.2.11 导入文件夹注册表（APK imported_folders：[{uri,platform}] 同构）
  j["importedFolders"] = json::array();
  for (const auto& f : importedFolders_) {
    json im;
    im["dir"] = f.dir;
    im["platform"] = platformShortName(f.platform);
    j["importedFolders"].push_back(std::move(im));
  }
  // ★error_handler=replace：非法 UTF-8 字节替换为 U+FFFD 而非抛异常
  //   （防御第二层：即使上层漏拦，保存也绝不闪退）
  const std::string text = j.dump(2, ' ', false, json::error_handler_t::replace);
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
  // ★非 UTF-8 文件名直接跳过（nlohmann::json dump() 遇非法 UTF-8 会抛异常
  //   ——「添加目录后闪退」的真正根因；且此类文件名在界面上也只能显示乱码。
  //   安卓存储里从 Windows 拷来的 GBK 编码文件名很常见）
  if (!fsutil::isValidUtf8(romPath)) {
    LOGW("跳过非 UTF-8 文件名（无法入库）：%s", romPath.c_str());
    return false;
  }
  const std::string id = gameIdForPath(romPath);
  if (find(id)) return false;  // 已在库中
  const std::string ext = fsutil::extOf(romPath);
  if (ext.empty()) return false;

  // ★v1.2.10 平台识别（对齐 APK PlatformDetector.kt 三档语义；
  //   「扫描的游戏全给 FC 核心」的根因修复：旧版 platformFromExtension/
  //   sniffRomPlatform 识别失败一律兑底 NES 并永久写入 library.json）：
  //   · useHint=true  → 手动导入（宽松档 detectFromUri）：CD 镜像按平台页
  //     hint 消歧（内容嗅探优先）；zip 探头；7z/gz→街机；兑底 hint；
  //   · useHint=false → 自动扫描（严格档 detectFromFileStrict）：白名单 +
  //     zip 探头 + 光盘魔数嗅探；识别不了 → 跳过入库（绝不 NES 兑底）
  GamePlatform p;
  if (useHint) {
    p = detectRomPlatformManual(romPath, platformHint);
  } else {
    if (!detectRomPlatformStrict(romPath, &p)) {
      LOGW("跳过无法识别平台的文件：%s", romPath.c_str());
      return false;
    }
  }
  return addEntry(romPath, p);
}

// 入库（平台已判定）：去重/UTF-8/封面关联与标题美化
bool GameLibrary::addEntry(const std::string& romPath, GamePlatform p) {
  const std::string id = gameIdForPath(romPath);
  if (find(id)) return false;
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
  // ★目录枚举全程异常防护（PRoot/挂载点/权限异常的 filesystem_error 在此兜底，
  //   不再向上传播到 UI 层导致闪退）
  try {
    if (recursive) {
      files = fsutil::listFilesRecursive(dir);
    } else {
      for (const auto& e : fsutil::listDir(dir))
        if (!e.isDir) files.push_back(e.fullPath);
    }
  } catch (...) {
    LOGE("扫描目录异常（跳过）：%s", dir.c_str());
    return 0;
  }
  int added = 0;
  try {
    for (const auto& f : files)
      if (addRom(f, platformHint, useHint)) added++;
  } catch (...) {
    // 单个条目异常不影响扫描结果（已计入的保留）
  }
  return added;
}

// ★ 重扫全部 ROM 目录（对齐 Android 版「启动/手动刷新时自动入库」）
// 线程安全设计：后台线程只做磁盘枚举（重 I/O），收集到的候选文件路径
// 经互斥锁交给 pendingScan_；由主线程 drainPendingScan() 合入 games_
//（games_ 仅主线程读写，杜绝并发崩溃 —— arm64 闪退的常见根源）
void GameLibrary::startBackgroundRescan(bool recursive) {
  joinScanThread();
  if (romDirs_.empty()) return;
  scanKind_ = ScanKind::Startup;
  scanRunning_.store(true);
  scanDone_.store(false);
  lastScanAdded_.store(0);
  scanTotalAdded_.store(0);
  {
    std::lock_guard<std::mutex> lk(scanMu_);
    pendingUseHint_ = false;  // 严格识别（自动扫描）
    pendingHint_ = GamePlatform::NES;
  }
  scanThread_ = std::thread([this, recursive] {
    std::vector<std::string> found;
    const std::vector<std::string> dirs = romDirs_;  // 启动早期快照（主循环前）
    for (const auto& dir : dirs) {
      try {
        if (recursive) {
          const auto fs = fsutil::listFilesRecursive(dir);
          found.insert(found.end(), fs.begin(), fs.end());
        } else {
          for (const auto& e : fsutil::listDir(dir))
            if (!e.isDir) found.push_back(e.fullPath);
        }
      } catch (...) {
        // 单目录异常跳过，继续其余目录
      }
    }
    {
      std::lock_guard<std::mutex> lk(scanMu_);
      pendingScan_ = std::move(found);
    }
    scanRunning_.store(false);
    scanDone_.store(true);
  });
}

// ★v1.2.11 按平台导入文件夹（APK runLocalFolderImport + scanLocalFolderForRoms）
//   后台线程：枚举（maxDepth=5）+ APK 过滤链；主线程：按平台页 hint 宽松
//   识别入库（drainPendingScan）。文件夹注册到 importedFolders_（不碰全局
//   ROM 目录 —— 刷新时按平台隔离重扫）。
void GameLibrary::startBackgroundImport(const std::string& dir,
                                        GamePlatform platformHint) {
  joinScanThread();
  if (!fsutil::isDir(dir) || !fsutil::isValidUtf8(dir)) return;
  scanKind_ = ScanKind::ImportFolder;
  scanRunning_.store(true);
  scanDone_.store(false);
  lastScanAdded_.store(0);
  scanTotalAdded_.store(0);
  {
    std::lock_guard<std::mutex> lk(scanMu_);
    pendingUseHint_ = true;  // 宽松识别（手动导入 = 用户意图优先）
    pendingHint_ = platformHint;
  }
  // 先注册文件夹（APK setLastImportFolder：去重后追加，平台页隔离记忆）
  addImportedFolder(dir, platformHint);
  scanThread_ = std::thread([this, dir, platformHint] {
    std::vector<std::string> found;
    try {
      if (platformHint == GamePlatform::DOS) {
        // DOS 页：不扫 ROM，找启动器（APK findDosLauncherInLocalFolder）
        const std::string launcher = findDosLauncher(dir);
        if (!launcher.empty()) found.push_back(launcher);
      } else {
        std::vector<std::string> files;
        fsutil::walkFiles(dir, &files, 5);  // APK maxDepth = 5
        found = filterImportFiles(files, platformHint);
      }
    } catch (...) {
      // 枚举异常 → 空结果（主循环会提示未找到可导入文件）
    }
    {
      std::lock_guard<std::mutex> lk(scanMu_);
      pendingScan_ = std::move(found);
    }
    scanRunning_.store(false);
    scanDone_.store(true);
  });
}

// ★v1.2.11 刷新（APK refreshList 核心隔离）：只重扫当前平台页导入的文件夹
//（allFolders=true 时全部），严格识别（detectForRefresh 档：识别不了跳过），
//   同步移除「该文件夹下、平台匹配、文件已不存在」的条目（APK 本地分支语义）
std::pair<int, int> GameLibrary::refreshImportedFolders(GamePlatform platform,
                                                        bool allFolders) {
  const std::vector<ImportedFolder> folders =
      allFolders ? importedFolders_ : importedFoldersFor(platform);
  int added = 0, removed = 0;
  for (const auto& f : folders) {
    if (!fsutil::isDir(f.dir)) continue;
    std::vector<std::string> files;
    try {
      if (f.platform == GamePlatform::DOS) {
        const std::string launcher = findDosLauncher(f.dir);
        if (!launcher.empty()) files.push_back(launcher);
      } else {
        std::vector<std::string> raw;
        fsutil::walkFiles(f.dir, &raw, 5);
        files = filterImportFiles(raw, f.platform);
      }
    } catch (...) {
      continue;
    }
    for (const auto& file : files) {
      if (find(gameIdForPath(file))) continue;  // 已在库
      GamePlatform p;
      if (!detectRomPlatformRefresh(file, f.platform, &p)) continue;  // 严格
      if (addEntry(file, p)) added++;
    }
    // 移除：该文件夹下、平台匹配、文件已不存在（APK File.exists() 守卫）
    std::vector<std::string> gone;
    for (const auto& e : games_) {
      if (e.platform != f.platform) continue;
      if (!pathUnder(e.romPath, f.dir)) continue;
      if (!fsutil::exists(e.romPath)) gone.push_back(e.id);
    }
    for (const auto& id : gone) {
      remove(id);
      removed++;
    }
  }
  if (added > 0 || removed > 0) save();
  return {added, removed};
}

bool GameLibrary::scanRunning() const { return scanRunning_.load(); }

size_t GameLibrary::pendingScanCount() const {
  std::lock_guard<std::mutex> lk(scanMu_);
  return pendingScan_.size();
}

// 主线程调用：把后台枚举到的候选文件合入库（去重 + 平台嗅探在此进行）
// ★分帧合入：单次最多处理 kDrainBatch 个 —— 后台扫描一次带回上万文件时，
//   一帧内全部 addRom 会卡住界面数秒（手机上像死机）；分批后每帧几百个，
//   界面全程流畅，剩余部分下一帧继续合入
int GameLibrary::drainPendingScan() {
  std::vector<std::string> batch;
  GamePlatform hint = GamePlatform::NES;
  bool useHint = false;
  {
    std::lock_guard<std::mutex> lk(scanMu_);
    batch.swap(pendingScan_);
    hint = pendingHint_;
    useHint = pendingUseHint_;
  }
  if (batch.empty()) return 0;
  const ScanKind kind = scanKind_;
  constexpr size_t kDrainBatch = 400;
  const size_t n = std::min(batch.size(), kDrainBatch);
  int added = 0;
  for (size_t i = 0; i < n; i++) {
    try {
      if (addRom(batch[i], hint, useHint)) {
        added++;
        // DOS 导入：标题 = "folder(exec)"（APK findDosLauncherInLocalFolder）
        if (kind == ScanKind::ImportFolder && hint == GamePlatform::DOS) {
          if (GameEntry* e = find(gameIdForPath(batch[i]))) {
            const std::string folder = fsutil::baseOf(fsutil::dirOf(batch[i]));
            const std::string exec = fsutil::stemOf(batch[i]);
            e->title = folder + "(" + exec + ")";
          }
        }
      }
    } catch (...) {
    }
  }
  if (batch.size() > n) {  // 剩余回队，下一帧继续（保持原顺序）
    std::lock_guard<std::mutex> lk(scanMu_);
    pendingScan_.insert(pendingScan_.begin(), batch.begin() + (long)n, batch.end());
  }
  lastScanAdded_.store(added);
  scanTotalAdded_.store(scanTotalAdded_.load() + added);  // 会话累计
  return added;
}

void GameLibrary::joinScanThread() {
  if (scanThread_.joinable()) scanThread_.join();
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

// ★返回 true=新增目录；false=已存在或非法 UTF-8 目录名（调用方提示）
bool GameLibrary::addRomDir(const std::string& dir) {
  if (!fsutil::isValidUtf8(dir)) return false;
  if (std::find(romDirs_.begin(), romDirs_.end(), dir) == romDirs_.end()) {
    romDirs_.push_back(dir);
    return true;
  }
  return false;
}

void GameLibrary::removeRomDir(const std::string& dir) {
  romDirs_.erase(std::remove(romDirs_.begin(), romDirs_.end(), dir), romDirs_.end());
}

// —— ★v1.2.11 导入文件夹注册表（APK RomStore.imported_folders 语义）——
std::vector<ImportedFolder> GameLibrary::importedFoldersFor(GamePlatform p) const {
  std::vector<ImportedFolder> out;
  for (const auto& f : importedFolders_)
    if (f.platform == p) out.push_back(f);
  return out;
}

void GameLibrary::addImportedFolder(const std::string& dir, GamePlatform platform) {
  if (!fsutil::isValidUtf8(dir)) return;
  // APK setLastImportFolder：uri 已存在则不重复追加（目录复用，平台不变）
  for (const auto& f : importedFolders_) {
    if (f.dir == dir) return;
  }
  importedFolders_.push_back({dir, platform});
}

void GameLibrary::removeImportedFolder(const std::string& dir) {
  importedFolders_.erase(
      std::remove_if(importedFolders_.begin(), importedFolders_.end(),
                     [&](const ImportedFolder& f) { return f.dir == dir; }),
      importedFolders_.end());
}

}  // namespace ns
