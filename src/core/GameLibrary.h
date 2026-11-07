// NesStation PC — 游戏库（扫描 / 持久化 / 收藏 / 游玩统计）
#pragma once
#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "core/Platform.h"

namespace ns {

struct GameEntry {
  std::string id;          // romPath 的稳定哈希（hex）
  std::string title;       // 文件名美化标题
  std::string customTitle; // 用户自定义显示名（空 = 用 title）
  std::string romPath;
  std::string coverPath;   // 本地封面（空 = 占位封面）
  GamePlatform platform = GamePlatform::NES;
  int64_t lastPlayedAt = 0;   // epoch 秒
  int64_t playTimeSec = 0;
  int playCount = 0;          // 游玩次数（APK playCount）
  bool favorite = false;
  // 核心选择的记忆（核心选择器「记住此游戏的选择」）：
  // "" 未定 / 候选链上的核心文件名（如 "snes9x_libretro"）
  std::string preferredCore;

  const std::string& displayTitle() const { return customTitle.empty() ? title : customTitle; }
};

// —— APK GamePlatform.kt 格式化（文案 1:1）——
// "—" / "N分钟" / "N.N小时"
std::string formatPlayTime(int64_t sec);
// "未游玩" / "刚刚" / "N分钟前" / "N小时前" / "N天前" / "很久以前"
std::string formatLastPlayed(int64_t epoch);

// 文件名 → 展示标题：去扩展名、下划线换空格、去常见标签 [xxx] (Rev A) 尾部留白
std::string prettifyTitle(const std::string& filename);

// ★v1.2.11 导入文件夹注册项（对齐 APK RomStore.imported_folders：
//   [{"uri": "...", "platform": "MD"}, ...] —— 每个平台页导入的文件夹
//   独立记忆，刷新时只重扫当前平台页的文件夹 = APK 核心隔离）
struct ImportedFolder {
  std::string dir;
  GamePlatform platform = GamePlatform::NES;
};

// 扫描/导入会话类型（完成 toast 文案区分）
enum class ScanKind {
  Startup,       // 启动/设置页重扫全局 ROM 目录（严格识别）
  ImportFolder,  // 导入文件夹（当前平台页 hint，宽松识别）
  ImportFile,    // 导入ROM 文件（同步，不走后台）
};

class GameLibrary {
 public:
  // ★v1.2.11 防御性析构：后台扫描线程若未 join（异常退出路径），在此收尾，
  //   避免 ~thread() 对 joinable 线程 std::terminate（正常路径 App 已显式 join）
  ~GameLibrary() { joinScanThread(); }

  // 数据目录（dataDir()）；库文件 <dataDir>/library.json
  void load(const std::string& dataDir);
  void save();

  const std::vector<GameEntry>& games() const { return games_; }
  std::vector<GameEntry>& gamesMut() { return games_; }

  GameEntry* find(const std::string& id);
  const GameEntry* find(const std::string& id) const;

  // 导入单个 ROM（platformHint 用于 .zip/.bin/.iso 等歧义扩展名消歧；
  // pass Unknown 则按扩展名自动判定）
  bool addRom(const std::string& romPath, GamePlatform platformHint = GamePlatform::NES,
              bool useHint = false);

  // 扫描目录（递归）并导入其中所有可识别 ROM；返回新增数量
  int scanDirectory(const std::string& dir, GamePlatform platformHint = GamePlatform::NES,
                    bool useHint = false, bool recursive = true);

  // ★ 重扫全部 ROM 目录（对齐 Android 版「启动/手动刷新时自动入库」）
  // 后台线程枚举 → pendingScan_ → 主线程 drainPendingScan 合入（线程安全）
  void startBackgroundRescan(bool recursive = true);

  // ★v1.2.11 按平台导入文件夹（APK runLocalFolderImport）：
  //   后台线程枚举 + APK 过滤链（ROM_EXTENSIONS / CD 附属去重 / Switch 限定 /
  //   隐藏文件跳过；DOS 页找启动器），主线程按平台页 hint 宽松识别入库；
  //   文夹注册到 importedFolders（刷新时核心隔离重扫）。不碰全局 ROM 目录。
  void startBackgroundImport(const std::string& dir, GamePlatform platformHint);

  // ★v1.2.11 刷新（APK refreshList 核心隔离）：只重扫「当前平台页导入的
  //   文件夹」（allFolders=true 时全部已注册文件夹），严格识别（识别不了
  //   跳过，绝不兑底），同步移除已不存在的条目；返回 {新增, 移除}。
  std::pair<int, int> refreshImportedFolders(GamePlatform platform,
                                             bool allFolders);

  bool scanRunning() const;
  bool scanFinished() const { return scanDone_.load(); }  // 后台枚举完成（含 0 结果）
  void clearScanFinished() { scanDone_.store(false); }
  ScanKind scanKind() const { return scanKind_; }
  int scanTotalAdded() const { return scanTotalAdded_.load(); }
  void resetScanTotal() { scanTotalAdded_.store(0); }
  size_t pendingScanCount() const;
  int drainPendingScan();  // 主线程：合入候选并返回新增数
  int lastScanAdded() const { return lastScanAdded_.load(); }
  void clearScanAddedFlag() { lastScanAdded_.store(0); }
  void joinScanThread();

  void remove(const std::string& id);            // 仅移出库（不删文件）
  void toggleFavorite(const std::string& id);
  void rename(const std::string& id, const std::string& customTitle);
  void recordPlay(const std::string& id, int64_t seconds);

  // ★v1.2.10 存量数据修复：对「平台=NES 但扩展名非 NES 系」的条目重新识别
  //（旧版识别失败全兑底 NES 写入库；载入时自动修正）
  void revalidatePlatforms();

  // 查询
  std::vector<GameEntry> byPlatform(GamePlatform p) const;
  std::vector<GameEntry> favorites() const;
  std::vector<GameEntry> recent(int limit = 50) const;
  int countFor(GamePlatform p) const;

  // ROM 目录（设置页管理；全局自动扫描用，与导入文件夹注册表无关）
  const std::vector<std::string>& romDirs() const { return romDirs_; }
  bool addRomDir(const std::string& dir);  // true=新增；false=已存在/非法UTF-8名
  void removeRomDir(const std::string& dir);

  // ★v1.2.11 导入文件夹注册表（APK imported_folders）：按平台隔离记忆
  const std::vector<ImportedFolder>& importedFolders() const { return importedFolders_; }
  std::vector<ImportedFolder> importedFoldersFor(GamePlatform p) const;
  void addImportedFolder(const std::string& dir, GamePlatform platform);  // 去重后追加
  void removeImportedFolder(const std::string& dir);

  const std::string& coverDir() const { return coverDir_; }
  const std::string& dataDir() const { return dataDir_; }

 private:
  // 入库（平台已判定）—— addRom 识别后调用；刷新重扫直接调（自带刷新档识别）
  bool addEntry(const std::string& romPath, GamePlatform p);

  std::string dataDir_;
  std::string libraryFile_;
  std::string coverDir_;
  std::vector<GameEntry> games_;
  std::vector<std::string> romDirs_;
  std::vector<ImportedFolder> importedFolders_;

  // 后台扫描（启动时自动重扫 ROM 目录 / 按平台导入文件夹）
  mutable std::mutex scanMu_;
  std::thread scanThread_;
  std::vector<std::string> pendingScan_;  // 后台枚举结果（互斥锁保护）
  GamePlatform pendingHint_ = GamePlatform::NES;  // 入库时用的平台页 hint
  bool pendingUseHint_ = false;                   // true=宽松（导入）；false=严格（扫描）
  std::atomic<bool> scanRunning_{false};
  std::atomic<bool> scanDone_{false};   // ★v1.2.11 后台枚举完成标志（含 0 结果）
  std::atomic<int> lastScanAdded_{0};
  std::atomic<int> scanTotalAdded_{0};  // ★v1.2.11 本次会话累计新增（完成 toast）
  ScanKind scanKind_ = ScanKind::Startup;
};

// romPath → 稳定 id（FNV-1a 64bit hex）
std::string gameIdForPath(const std::string& path);

}  // namespace ns
