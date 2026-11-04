// NesStation PC — 核心管理：路径解析 / 可用性 / 选项持久化 / 快速存档
// + 应用内核心下载（对齐 Android 版 CoreManagerScreen / CoreDownloader：
//   libretro buildbot 官方预编译核心，候选链回退，后台线程 + 进度快照）
#pragma once
#include <atomic>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "core/Platform.h"
#include "retro/LibretroCore.h"

namespace ns {

class CoreManager {
 public:
  void init(const std::string& dataDir);

  // 目录
  // coreDir：用户数据目录下的 cores/（用户下载/更新的核心，优先）
  // bundledCoreDir：exe 同目录 cores/（安装包内置核心，随包分发，只读）
  const std::string& coreDir() const { return coreDir_; }
  const std::string& bundledCoreDir() const { return bundledCoreDir_; }
  const std::string& systemDir() const { return systemDir_; }
  const std::string& savesDir() const { return savesDir_; }
  const std::string& statesDir() const { return statesDir_; }
  const std::string& optionsDir() const { return optionsDir_; }

  // 平台核心是否就绪（Libretro：候选链上任一核心文件存在；
  // Standalone：探测到模拟器或已配置路径）
  // 解析实际使用的核心：候选链上第一个已安装者；
  // 全部缺失时返回链首（首选），供提示与选项持久化使用
  PlatformCore resolveCore(GamePlatform p) const;
  std::string corePathFor(GamePlatform p) const;
  bool libretroCoreAvailable(GamePlatform p) const;

  // preferredCore 支持（对齐 Android 版 CorePickerDialog「记住此游戏的选择」）：
  // preferred 版本在候选核心已安装时优先使用，否则回退链首
  PlatformCore resolveCorePreferred(GamePlatform p,
                                    const std::string& preferredCoreFile) const;
  std::string corePathForPreferred(GamePlatform p,
                                   const std::string& preferredCoreFile) const;
  // 该平台当前已安装的全部 libretro 候选（核心选择器用；空 = 单核心/未安装）
  std::vector<PlatformCore> installedCoresFor(GamePlatform p) const;
  // 未就绪时的提示（含获取方式与全部候选核心）
  std::string availabilityHint(GamePlatform p) const;

  // —— 核心选项持久层 ——
  // LibretroCore::load 的 resolver 回到这里
  std::string resolveOption(const std::string& coreKey, const std::string& optKey) const;
  void storeOption(const std::string& coreKey, const std::string& optKey,
                   const std::string& value);
  std::map<std::string, std::string> loadOptions(const std::string& coreKey) const;
  void saveOptions(const std::string& coreKey,
                   const std::map<std::string, std::string>& kv);

  // —— 快速存档 ——
  std::string statePath(const std::string& gameId, int slot) const;  // slot 0=自动
  std::vector<int> existingSlots(const std::string& gameId) const;
  bool saveState(LibretroCore& core, const std::string& gameId, int slot);
  bool loadState(LibretroCore& core, const std::string& gameId, int slot);

  // 已安装核心清单（文件名）
  std::vector<std::string> installedCores() const;

  // —— 应用内核心下载（后台线程，UI 轮询进度）——
  struct DlProgress {
    bool running = false;
    bool finished = false;    // 本次任务结束（UI 提示后 clearDownloadFinishFlag）
    bool hadFailure = false;
    std::string label;        // 当前：“FCEUmm（NES）”
    int done = 0, total = 0;  // 平台计数
  };
  // 空/缺平台 = 全部缺失平台；后台依次下载候选链上首个可用核心到 coreDir()
  void startCoreDownload(const std::vector<GamePlatform>& platforms);
  bool coreDownloadRunning() const { return dlRunning_.load(); }
  DlProgress coreDownloadProgress() const;
  void clearDownloadFinishFlag();
  ~CoreManager();

 private:
  // 按优先级查找核心文件（数据目录 → exe 旁内置目录）；找不到返回空串
  std::string findCoreFile(const std::string& coreFile) const;
  void downloadWorker(std::vector<GamePlatform> platforms);
  bool downloadOne(const std::string& coreFile, std::string* err);

  std::string coreDir_, bundledCoreDir_, systemDir_, savesDir_, statesDir_, optionsDir_;

  // 下载线程状态
  std::thread dlThread_;
  std::atomic<bool> dlRunning_{false};
  mutable std::mutex dlMu_;
  DlProgress dl_;
};

}  // namespace ns
