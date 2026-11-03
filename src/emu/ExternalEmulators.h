// NesStation PC — 独立模拟器（Dolphin / Cemu / Eden）探测与启动
//
// NGC/Wii、Wii U、Switch 三个平台以「源码集成」方式接入（CMake 超级构建
// 产出 <build>/bundle/emulators/），前端按以下顺序探测：
//   1. 用户在设置页配置的路径
//   2. exe 同目录 emulators/<name>/（超级构建 / 便携包）
//   3. 常见安装位置（Program Files / /usr/local 等）
#pragma once
#include <string>
#include <vector>

#include "core/Platform.h"

namespace ns {

struct ExternalEmulator {
  std::string key;         // "dolphin" | "cemu" | "eden"
  std::string title;       // "Dolphin"
  std::string foundPath;   // 探测到的可执行文件（空 = 未找到）
  std::string userPath;    // 用户覆盖路径
};

class ExternalEmulators {
 public:
  void init();

  // key: dolphin / cemu / eden
  const ExternalEmulator& get(const std::string& key) const;
  void setUserPath(const std::string& key, const std::string& path);

  bool available(GamePlatform p) const;
  std::string statusText(GamePlatform p) const;

  // 启动游戏（detached；返回是否成功拉起）
  bool launch(GamePlatform p, const std::string& romPath, std::string* errMsg);

  // 供设置页展示
  std::vector<ExternalEmulator> all() const { return list_; }

 private:
  void detect(ExternalEmulator* e);
  std::string buildArgs(const ExternalEmulator& e, const std::string& romPath) const;

  std::vector<ExternalEmulator> list_;
};

}  // namespace ns
