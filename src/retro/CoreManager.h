// NesStation PC — 核心管理：路径解析 / 可用性 / 选项持久化 / 快速存档
#pragma once
#include <map>
#include <string>
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

 private:
  // 按优先级查找核心文件（数据目录 → exe 旁内置目录）；找不到返回空串
  std::string findCoreFile(const std::string& coreFile) const;

  std::string coreDir_, bundledCoreDir_, systemDir_, savesDir_, statesDir_, optionsDir_;
};

}  // namespace ns
