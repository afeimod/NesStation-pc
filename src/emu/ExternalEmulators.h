// NesStation PC — 外置核心（独立模拟器）注册表 / 探测 / 启动 / GitHub 自动下载
//
// 1:1 移植 Android 版 CoreRegistry.EXTERNAL（ExternalCoreDef）与
// CoreDownloader.installExternal：
//   - 全部 8 个外置核心（PCSX2 / PPSSPP / Flycast / Azahar /
//     Ishiiruka（Dolphin 系）/ Cemu / Eden / FreeJ2ME），中文名与说明照搬 APK；
//   - 可执行文件解析顺序（对齐 resolveExecutable）：
//       1) 用户在设置页配置的路径
//       2) NesStation 管理的安装目录 <dataDir>/external/<id>（自动下载落点）
//       3) exe 同目录 emulators/<id>/（PC 超级构建 / 便携包形态）
//       4) 常见安装位置（%HOME% 占位，含一层子目录 / macOS .app 包内）
//   - GitHub Releases 自动下载（api → 资产名通配匹配 → 下载 → 解压 → 可执行化）；
//   - 启动参数照搬 APK argTemplate（{rom} 为游戏路径占位）。
#pragma once
#include <atomic>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "core/Platform.h"

namespace ns {

// 外置核心定义（对齐 Android ExternalCoreDef）
struct ExternalCore {
  std::string id;            // "pcsx2" ... "freej2me"
  std::string name;          // 显示名（中文，照搬 APK）："Ishiiruka（Dolphin 系）"...
  GamePlatform platform;
  std::string sourceRepo;    // 源码仓库（展示用）
  std::string downloadPage;  // 官方下载页
  std::string githubApi;     // GitHub Releases latest API；空 = 无自动下载（手动安装）
  std::string assetWin, assetLinux, assetMac;  // 资产名通配（| 分隔，* 任意）
  std::vector<std::string> exesWin, exesLinux, exesMac;
  std::string argTemplate;   // 启动参数模板，{rom} 为游戏路径占位
  std::vector<std::string> commonDirs;  // 常见安装目录（%HOME% 占位）
  std::string notes;         // 中文说明（照搬 APK notes）

  // 运行时状态
  std::string foundPath;     // 探测到的可执行文件（空 = 未找到）
  std::string userPath;      // 用户覆盖路径（设置页 / config.json）

  // 当前 OS 的 exe 候选名
  std::vector<std::string> exeNames() const;
  // 启动参数（{rom} 替换为带引号路径）
  std::string buildArgs(const std::string& romPath) const;
};

class App;

class ExternalEmulators {
 public:
  void init(const std::string& dataDir);
  // ★v1.3.1 后台启动验证线程经此回发通知（App::init 里调用）
  void setApp(App* app) { appPtr_ = app; }

  const std::vector<ExternalCore>& all() const { return list_; }
  // 平台的全部外置核心（注册顺序）
  std::vector<const ExternalCore*> forPlatform(GamePlatform p) const;
  const ExternalCore* find(const std::string& id) const;

  // 平台任一外置核心已就绪 / 已就绪数量
  bool available(GamePlatform p) const;
  int availableCount(GamePlatform p) const;
  bool available(const std::string& id) const;
  std::string statusText(GamePlatform p) const;

  void setUserPath(const std::string& id, const std::string& path);
  void clearUserPath(const std::string& id);  // 移除用户路径 + 安装目录（对齐 APK 删除）
  void reDetect(const std::string& id);       // 重新探测（安装 / 删除后调用）

  // 启动（对齐 Android ExternalEngine）：模板 {rom} 替换后分离拉起
  bool launch(const ExternalCore& c, const std::string& romPath, std::string* err);
  // 平台级启动（platformCore 的 Standalone 分支）：链上第一个已就绪核心
  bool launchPlatform(GamePlatform p, const std::string& romPath, std::string* err);

  // —— GitHub 自动下载（后台线程，UI 轮询进度）——
  struct ExtProgress {
    bool running = false;
    bool finished = false;  // 本次任务结束（UI 提示后 clearFinishFlag）
    bool ok = false;
    std::string label;      // 中文状态文本
  };
  void startDownload(const std::string& id);
  bool downloadRunning() const { return dlRunning_.load(); }
  ExtProgress progress() const;
  void clearFinishFlag();
  ~ExternalEmulators();

  // 安装目录（自动下载落点）：<dataDir>/external/<id>
  std::string installDir(const std::string& id) const;

 private:
  void detect(ExternalCore* e);
  bool downloadOne(ExternalCore* c);  // 线程内执行，更新 prog_

  std::string dataDir_;
  App* appPtr_ = nullptr;   // ★v1.3.1 后台验证线程 postNotice 用
  std::vector<ExternalCore> list_;
  std::thread dlThread_;
  std::atomic<bool> dlRunning_{false};
  mutable std::mutex mu_;
  ExtProgress prog_;
};

}  // namespace ns
