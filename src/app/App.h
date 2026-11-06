// NesStation PC — 应用中枢：窗口 / 主循环 / 场景栈 / 配置 / 子系统集合
#pragma once
#include <deque>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include <SDL.h>

#include "core/CoverArt.h"
#include "core/GameLibrary.h"
#include "emu/ExternalEmulators.h"
#include "retro/AudioBridge.h"
#include "retro/CoreManager.h"
#include "retro/InputManager.h"
#include "retro/VideoBridge.h"
#include "ui/Draw.h"
#include "ui/Font.h"
#include "ui/Theme.h"
#include "ui/Ui.h"

namespace ns {

class App;
class EmuRunner;

// —— 版本号（CMake 注入；本地直编 g++ 时回退）——
#ifndef NESSTATION_VERSION
#define NESSTATION_VERSION "1.2.8"
#endif
inline const char* appVersionText() { return "v" NESSTATION_VERSION; }

// —— 配置（<dataDir>/config.json）——
struct Config {
  int uiStyle = 0;             // 0=Neon 1=FSD
  // ★v1.2.8：画面缩放/滤镜完全对齐 APK PadLayout（字符串键，APK 同枚举值）。
  //   旧 scalingMode/screenEffect 读取时迁移到新键（见 load）。
  std::string videoScale = "stretch";   // APK video_scale：stretch/4:3/3:2/8:7/16:9/custom
  std::string videoFilter = "none";    // APK video_filter：none/scanline/crt/dot/xbr/
                                       // hq2x/hq4x/xbr_dot/4xbr/4xbr_dot/hq4x_dot
  float customLayout[4] = {0, 0, 1, 1}; // APK custom_layout_{l,t,r,b}（归一化 0..1）
  bool integerScale = false;    // PC 增强：整数缩放（像素完美，APK 无此项）
  std::string saveMode = "nesstation"; // APK global_save_mode：统一目录 / core_builtin
  // 平台级覆盖（APK pk() 逻辑：游戏内设置写 <key>_<PLATFORM>，设置页写全局键）
  std::map<std::string, std::string> videoScaleByPlatform;
  std::map<std::string, std::string> videoFilterByPlatform;
  int dualLayout = 0;          // DualLayout（NDS/3DS）
  bool videoSmooth = true;
  bool audioMuted = false;
  bool fullscreen = false;
  bool showFps = false;
  int uiScalePercent = 0;      // ★v1.2.8 整体 UI 缩放：0=自动 / 100~300 固定百分比
                               //（字体/标题/菜单/卡片统一放大，高 DPI 不再发糊）
  bool autoSaveState = true;   // 退出游戏自动存 slot 0
  bool scanRecursive = true;
  int fastForwardSpeed = 4;    // 快进倍速：0 关 / 2 / 4 / 6 / 8 / 16（对齐 Android SettingsStore）
  // ★v1.2.6（对齐 Android AppSettings 剩余字段）：
  std::string homeBackground;  // 自定义主页背景图路径（空 = 默认背景）
  bool fullscreenOnLaunch = false;   // 启动游戏时自动全屏（退出游戏恢复）
  int audioBufferMs = 64;      // 音频缓冲毫秒（越大越稳、延迟越高）
  std::map<std::string, std::string> emulatorPaths;  // dolphin/cemu/eden → exe
  // 双核心平台的默认核心（platformShortName → coreId，对齐 Android defaultCores；
  // 值格式与 preferredCore 一致：核心文件名 / "<id>-ext"；空 = 每次询问）
  std::map<std::string, std::string> defaultCores;
  PadMapping padMapping = PadMapping::defaults();

  void load(const std::string& file);
  void save(const std::string& file) const;
};

// —— 场景基类 ——
class Scene {
 public:
  virtual ~Scene() = default;
  virtual void onEnter(App&) {}
  virtual void onExit(App&) {}
  virtual void update(App& app, float dt, const InputState& in) = 0;
  virtual void draw(App& app) = 0;
};

// —— App ——
class App {
 public:
  ~App();

  int run(int argc, char** argv);

  // 子系统
  Draw draw;
  FontCache font;
  InputManager input;
  AudioBridge audio;
  VideoBridge video;
  CoreManager cores;
  GameLibrary library;
  ExternalEmulators externals;
  CoverArt covers;
  Config config;

  // —— 场景管理 ——
  void pushScene(std::unique_ptr<Scene> s);
  void popScene();
  void replaceScene(std::unique_ptr<Scene> s);
  Scene* topScene() { return scenes_.empty() ? nullptr : scenes_.back().get(); }
  template <typename T>
  T* findScene() {
    for (auto& s : scenes_)
      if (auto* p = dynamic_cast<T*>(s.get())) return p;
    return nullptr;
  }

  // —— 资源 ——
  std::string assetPath(const std::string& rel) const;
  SDL_Texture* texture(const std::string& key);  // 封面缓存（LRU）
  void invalidateTexture(const std::string& key);

  // —— 杂项 ——
  void toast(const std::string& msg) { toasts_.push_back({msg, 3.5f}); }
  void requestExit() { running_ = false; }
  bool isFullscreen() const { return config.fullscreen; }
  void toggleFullscreen();
  void applyFullscreen();   // 按当前 config.fullscreen 应用窗口状态（不翻转）
  void applyUiScale();      // ★v1.2.7 按当前 config.uiScalePercent 重算界面缩放
  float currentUiScale() const { return uiscale::get(); }
  int width() const { return winW_; }
  int height() const { return winH_; }
  const std::string& dataDir() const { return dataDir_; }
  void drawButtonHints();          // 底部手柄按键提示栏（场景共用）
  SDL_Renderer* sdlRenderer() { return renderer_; }

  // 截图当前帧存 PNG（F12 快捷键 / 无头自检共用）
  bool saveScreenshot(const std::string& pngPath);

  // 每帧回调（仅调试/自检挂载；正常运行为空）
  void setTestHook(std::function<void(App&, Uint32 frame)> h) { testHook_ = std::move(h); }

  // 游戏启动入口（EmulatorScene 接管）
  void launchGame(const GameEntry& game);

  EmuRunner* activeEmu() { return emu_.get(); }

 private:
  bool init();
  bool initFontChain();   // 系统字体选链（v1.2.4：不随包附带字体）
  void shutdown();
  void pumpEvents();
  void frame();
  void drawToasts(float dt);
  void updateSurface();   // ★v1.2.7 窗口/像素尺寸变化 → 逻辑尺寸+UI缩放+鼠标映射全量重算

  SDL_Window* window_ = nullptr;
  SDL_Renderer* renderer_ = nullptr;
  int winW_ = 1280, winH_ = 720;
  bool running_ = false;

  std::string dataDir_;
  std::string configFile_;
  std::deque<std::unique_ptr<Scene>> scenes_;
  std::unique_ptr<EmuRunner> emu_;

  struct Toast {
    std::string msg;
    float life;
  };
  std::vector<Toast> toasts_;

  // 纹理缓存
  struct TexEntry {
    SDL_Texture* tex = nullptr;
    uint64_t lastUsed = 0;
  };
  std::map<std::string, TexEntry> texCache_;
  uint64_t frameNo_ = 0;

 public:
  uint64_t frameNo() const { return frameNo_; }
  void evictTextures();

  void setWindowIcon();
  std::function<void(App&, Uint32)> testHook_;
  Uint32 testFrame_ = 0;
};

}  // namespace ns
