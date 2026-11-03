// NesStation PC — 游戏库（扫描 / 持久化 / 收藏 / 游玩统计）
#pragma once
#include <cstdint>
#include <string>
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
  bool favorite = false;

  const std::string& displayTitle() const { return customTitle.empty() ? title : customTitle; }
};

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

  void remove(const std::string& id);            // 仅移出库（不删文件）
  void toggleFavorite(const std::string& id);
  void rename(const std::string& id, const std::string& customTitle);
  void recordPlay(const std::string& id, int64_t seconds);

  // 查询
  std::vector<GameEntry> byPlatform(GamePlatform p) const;
  std::vector<GameEntry> favorites() const;
  std::vector<GameEntry> recent(int limit = 50) const;
  int countFor(GamePlatform p) const;

  // ROM 目录（设置页管理）
  const std::vector<std::string>& romDirs() const { return romDirs_; }
  void addRomDir(const std::string& dir);
  void removeRomDir(const std::string& dir);

  const std::string& coverDir() const { return coverDir_; }
  const std::string& dataDir() const { return dataDir_; }

 private:
  std::string dataDir_;
  std::string libraryFile_;
  std::string coverDir_;
  std::vector<GameEntry> games_;
  std::vector<std::string> romDirs_;
};

// romPath → 稳定 id（FNV-1a 64bit hex）
std::string gameIdForPath(const std::string& path);

}  // namespace ns
