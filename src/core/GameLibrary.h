// NesStation PC — 游戏库（扫描 / 持久化 / 收藏 / 游玩统计）
#pragma once
#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>
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

class GameLibrary {
 public:
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
  bool scanRunning() const;
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

  // ROM 目录（设置页管理）
  const std::vector<std::string>& romDirs() const { return romDirs_; }
  bool addRomDir(const std::string& dir);  // true=新增；false=已存在/非法UTF-8名
  void removeRomDir(const std::string& dir);

  const std::string& coverDir() const { return coverDir_; }
  const std::string& dataDir() const { return dataDir_; }

 private:
  std::string dataDir_;
  std::string libraryFile_;
  std::string coverDir_;
  std::vector<GameEntry> games_;
  std::vector<std::string> romDirs_;

  // 后台扫描（启动时自动重扫 ROM 目录）
  mutable std::mutex scanMu_;
  std::thread scanThread_;
  std::vector<std::string> pendingScan_;  // 后台枚举结果（互斥锁保护）
  std::atomic<bool> scanRunning_{false};
  std::atomic<int> lastScanAdded_{0};
};

// romPath → 稳定 id（FNV-1a 64bit hex）
std::string gameIdForPath(const std::string& path);

}  // namespace ns
