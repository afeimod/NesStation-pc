// NesStation PC — 场景工厂（皮肤化主页 / 游戏库 / 设置 / 关于 / 文件浏览 / 模拟器）
#pragma once
#include <memory>
#include <string>

#include "app/App.h"

namespace ns {

// 主页（按当前皮肤返回 Neon 3D 或 FSD 磁贴）
std::unique_ptr<Scene> createHomeScene();

// 游戏库（平台过滤：GamePlatform 枚举值；-1 = 全部；-2 = 收藏；-3 = 最近）
std::unique_ptr<Scene> createLibraryScene(int platformFilter);

// 设置（含外观风格切换 / 核心管理 / 按键映射 / 路径 / 模拟器 / BIOS）
// initialTab：初始分类（0 外观…6 数据；-1 = 记住上次）。主页「核心管理」入口传 1。
std::unique_ptr<Scene> createSettingsScene(int initialTab = -1);

// 核心选项浏览（platform：目标平台；运行中则热改）
std::unique_ptr<Scene> createCoreOptionsScene(GamePlatform platform);

// 关于
std::unique_ptr<Scene> createAboutScene();

// 模态提示框（核心缺失 / 启动失败等重要信息；warn=true 红色警示边框）
// okLabel：确认按钮文案；onOk：确认回调（空 = 仅关闭）
std::unique_ptr<Scene> createMessageScene(const std::string& title,
                                          const std::string& body, bool warn = true,
                                          const std::string& okLabel = "确定",
                                          std::function<void()> onOk = nullptr);

// 文件浏览器（选择目录 / 文件；title 标题；mode: 0=选目录 1=选文件 2=选核心）
// 返回值经 std::function 回调
struct BrowserRequest {
  std::string title;
  int mode = 0;
  std::string startDir;
  std::function<void(const std::string&)> onPick;
  std::function<void()> onCancel;
};
std::unique_ptr<Scene> createFileBrowser(const BrowserRequest& req);

// 模拟器场景（EmuRunner 专用，由 EmuRunner 推入）
std::unique_ptr<Scene> createEmulatorScene();

}  // namespace ns
