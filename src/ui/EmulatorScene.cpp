// NesStation PC — 模拟器场景：游戏画面 + 游戏内菜单（主题化）
// ★v1.2.8：画面缩放/滤镜对齐 APK（EmulatorScreen SettingsPanel：
//   画面缩放 6 档 / 视频滤镜 11 档，游戏内改动写平台键）；自定义比例的
//   四角拖动编辑器 1:1 复刻 ui/swf/ScreenPositionEditor.kt。
#include <algorithm>
#include <chrono>
#include <cmath>
#include <string>
#include <vector>

#include "emu/EmuRunner.h"
#include "ui/OptionZh.h"
#include "util/Fs.h"
#include "retro/VideoBridge.h"
#include "ui/Scenes.h"
#include "ui/Widgets.h"

namespace ns {
namespace {

// —— APK PadLayoutStore.pk() 语义：游戏内改动写平台键，读取先平台后全局 ——
std::string platformVideoScale(App& app, GamePlatform platform) {
  auto it = app.config.videoScaleByPlatform.find(platformShortName(platform));
  if (it != app.config.videoScaleByPlatform.end()) return it->second;
  return app.config.videoScale;
}
std::string platformVideoFilter(App& app, GamePlatform platform) {
  auto it = app.config.videoFilterByPlatform.find(platformShortName(platform));
  if (it != app.config.videoFilterByPlatform.end()) return it->second;
  return app.config.videoFilter;
}
void setPlatformVideoScale(App& app, GamePlatform platform, const std::string& v) {
  app.config.videoScaleByPlatform[platformShortName(platform)] = v;
  app.config.save(fsutil::join(app.dataDir(), "config.json"));
}
void setPlatformVideoFilter(App& app, GamePlatform platform, const std::string& v) {
  app.config.videoFilterByPlatform[platformShortName(platform)] = v;
  app.config.save(fsutil::join(app.dataDir(), "config.json"));
}

// APK SettingsScreen / EmulatorScreen 的两套循环选项表（文案 1:1）
const std::vector<std::string>& videoScaleOptions() {
  static const std::vector<std::string> opts = {
      "全屏拉伸(默认)", "4:3", "3:2 (GBA 原生)", "8:7 (NES 像素比)", "16:9",
      "自定义(拖动四角)"};
  return opts;
}
const std::vector<std::string>& videoScaleKeys() {
  static const std::vector<std::string> keys = {"stretch", "4:3", "3:2", "8:7", "16:9",
                                                "custom"};
  return keys;
}
const std::vector<std::string>& videoFilterOptions() {
  static const std::vector<std::string> opts = {
      "关闭",   "扫描线",       "CRT",     "点阵",        "XBR",
      "HQ2X",   "HQ4X",         "XBR+点阵", "4XBR",       "4XBR+点阵",
      "HQ4X+点阵"};
  return opts;
}
const std::vector<std::string>& videoFilterKeys() {
  static const std::vector<std::string> keys = {
      "none",    "scanline", "crt",      "dot",     "xbr",
      "hq2x",    "hq4x",     "xbr_dot",  "4xbr",    "4xbr_dot",
      "hq4x_dot"};
  return keys;
}
int indexFor(const std::vector<std::string>& keys, const std::string& v) {
  for (size_t i = 0; i < keys.size(); i++)
    if (keys[i] == v) return (int)i;
  return 0;
}

class EmulatorScene : public Scene {
 public:
  void onEnter(App& app) override {
    SDL_ShowCursor(SDL_DISABLE);
    app_ = &app;
    // ★v1.2.6 启动游戏时自动全屏（对齐 Android fullscreenOnLaunch）：
    //   记住进入前状态，onExit 恢复；游戏中 F11 手动切换会同步回 config
    if (app.config.fullscreenOnLaunch && !app.config.fullscreen) {
      wasFullscreen_ = false;
      app.config.fullscreen = true;
      app.applyFullscreen();
      app.config.save(fsutil::join(app.dataDir(), "config.json"));
    } else {
      wasFullscreen_ = app.config.fullscreen;
    }
  }
  void onExit(App& app) override {
    // 自动全屏的会话：退出游戏恢复窗口模式（F11 手动开的不还原）
    if (app.config.fullscreenOnLaunch && !wasFullscreen_ && app.config.fullscreen) {
      app.config.fullscreen = false;
      app.applyFullscreen();
      app.config.save(fsutil::join(app.dataDir(), "config.json"));
    }
    SDL_ShowCursor(SDL_ENABLE);
  }

  void update(App& app, float dt, const InputState& in) override {
    (void)dt;
    app_ = &app;
    EmuRunner* emu = app.activeEmu();
    if (!emu || !emu->running()) {
      app.popScene();
      return;
    }

    // —— 自定义比例编辑器（APK ScreenPositionEditor：自消费输入）——
    if (editLayout_) {
      updateLayoutEditor(app, in);
      return;
    }

    // —— 帧驱动（★v1.2.11 对齐 APK FramePacer：墙钟截止时间步进）——
    //   APK 模拟线程：t0=now → runFrame → pace(t0, 1e9/fps)（到点才放行下一帧）。
    //   PC 在渲染循环内等价实现：nextFrameAt_ 为下一帧的绝对时刻，每帧校验
    //   到点才步进；步进预算 = 1/targetFps，快进 ÷speed（APK pace(t0, fps*ff)）。
    //   → 无论 VSync 有无/刷新率多少，FC 恒 60.000fps（不再 80-120 乱跑）。
    // activeMult：Tab 按住用设置倍速，否则用菜单档位 ffLevel_（0 = 关）
    const int cfgMult = app.config.fastForwardSpeed > 1 ? app.config.fastForwardSpeed : 2;
    const Uint8* kbd = SDL_GetKeyboardState(nullptr);
    const bool tabHeld = kbd[SDL_SCANCODE_TAB] != 0;
    const int activeMult = (tabHeld ? cfgMult : ffLevel_);
    if (activeMult > 1) emu_ffMult_ = activeMult;  // HUD 显示实际生效倍数
    {
      const int speed = (paused_ || menuOpen_) ? 0 : (activeMult > 1 ? activeMult : 1);
      if (speed > 0) {
        const double now =
            (double)std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::steady_clock::now().time_since_epoch())
                .count() /
            1e9;
        const double budget = 1.0 / emu->targetFps() / (double)speed;
        if (!paceInit_) {
          nextFrameAt_ = now;
          paceInit_ = true;
        }
        int guard = 0;
        const int guardMax = speed > 1 ? 32 : 8;  // 卡顿补帧上限（APK 逐帧重锚，等价）
        while (now >= nextFrameAt_ && guard < guardMax) {
          emu->runFrameOnce();
          emuFrames_++;  // FPS HUD 计模拟帧（APK onFrame 计数器），非渲染帧率
          nextFrameAt_ += budget;
          guard++;
        }
        // 防积压发散：落后过多（卡顿/休眠唤醒）直接重锚，不追帧风暴
        if (nextFrameAt_ < now - 0.25) nextFrameAt_ = now;
      } else {
        paceInit_ = false;  // 暂停/菜单：恢复时重新锚定（不追帧）
      }
      emu->setFastForward(activeMult > 1 && !paused_);
    }

    // —— 快捷键 ——
    if (!menuOpen_) {
      if (in.pressed[(int)Btn::Menu] || in.pressed[(int)Btn::Back] ||
          in.mouseRightPressed) {
        // F1 / Esc / B / 右键 → 呼出游戏菜单（鼠标玩家也能呼出）
        menuOpen_ = true;
        menuIndex_ = 0;
        subMode_ = Sub::Main;
        justOpened_ = true;  // ★v1.2.10 修复：呼出帧的同一次 Back/Menu 按下
                             //   不得被 updateMenu 立即消费成「关闭」（
                             //   Esc/F1 开菜单同帧开又关 = 菜单永远打不开）
        SDL_ShowCursor(SDL_ENABLE);
      }
      // F6 循环菜单档位 0→2→4→6→8→16→0（Tab 按住快进用设置倍速）
      if (kbd[SDL_SCANCODE_F6]) {
        if (!f6Prev_) cycleFfLevel(app);
        f6Prev_ = true;
      } else {
        f6Prev_ = false;
      }
      // F7 暂停/继续（对齐 Android 暂停浮层；F5 存 / F8 读保持原键位）
      const bool f7 = kbd[SDL_SCANCODE_F7] != 0;
      if (f7 && !f7Prev_) {
        paused_ = !paused_;
        app.toast(paused_ ? "已暂停" : "继续游戏");
      }
      f7Prev_ = f7;
      const bool f5 = kbd[SDL_SCANCODE_F5] != 0;
      if (f5 && !f5Prev_) {
        if (emu->saveStateSlot(app, 1)) app.toast("已存档：槽位 1");
        else app.toast("存档失败（核心不支持）");
      }
      f5Prev_ = f5;
      const bool f8 = kbd[SDL_SCANCODE_F8] != 0;
      if (f8 && !f8Prev_) {
        if (emu->loadStateSlot(app, 1)) app.toast("已读档：槽位 1");
        else app.toast("槽位 1 没有存档");
      }
      f8Prev_ = f8;
      const bool f11 = kbd[SDL_SCANCODE_F11] != 0;
      if (f11 && !f11Prev_) app.toggleFullscreen();
      f11Prev_ = f11;
      const bool f12 = kbd[SDL_SCANCODE_F12] != 0;
      if (f12 && !f12Prev_) takeScreenshot(app);
      f12Prev_ = f12;
    }

    if (menuOpen_) {
      updateMenu(app, in);
      justOpened_ = false;
    }
  }

  // 快进档位循环（对齐 Android 菜单：0→2→4→6→8→16→0）
  void cycleFfLevel(App& app) {
    static const int levels[] = {0, 2, 4, 6, 8, 16};
    int i = 0;
    for (int k = 0; k < 6; k++)
      if (levels[k] == ffLevel_) { i = k; break; }
    ffLevel_ = levels[(i + 1) % 6];
    app.config.fastForwardSpeed = ffLevel_ > 1 ? ffLevel_ : app.config.fastForwardSpeed;
    app.config.save(fsutil::join(app.dataDir(), "config.json"));
    app.toast(ffLevel_ > 1 ? std::string("快进 ×") + std::to_string(ffLevel_)
                           : std::string("快进已关闭"));
  }

  void takeScreenshot(App& app) {
    const std::string dir = fsutil::join(app.dataDir(), "shots");
    fsutil::makeDirs(dir);
    char name[64];
    time_t now = time(nullptr);
    struct tm* t = localtime(&now);
    if (t)
      strftime(name, sizeof(name), "%Y%m%d_%H%M%S.png", t);
    else
      snprintf(name, sizeof(name), "%u.png", (unsigned)SDL_GetTicks());
    const std::string path = fsutil::join(dir, name);
    if (app.saveScreenshot(path))
      app.toast(std::string("截图已保存：") + name);
    else
      app.toast("截图失败");
  }

  void updateMenu(App& app, const InputState& in) {
    EmuRunner* emu = app.activeEmu();
    (void)emu;
    switch (subMode_) {
      case Sub::Main: {
        const int N = menuItemCount(app);
        if (in.pressed[(int)Btn::Up]) {
          menuIndex_ = (menuIndex_ + N - 1) % N;
          followMenu_ = true;
        }
        if (in.pressed[(int)Btn::Down]) {
          menuIndex_ = (menuIndex_ + 1) % N;
          followMenu_ = true;
        }
        // ★v1.2.10：呼出帧的同一次按键不作为关闭指令（Esc/F1 开菜单修复）
        if (!justOpened_ &&
            (in.pressed[(int)Btn::Back] || in.pressed[(int)Btn::Menu]))
          closeMenu();
        break;
      }
      case Sub::States:
      case Sub::Video:
      case Sub::Disk: {
        const int N = subItemCount(app);
        if (N > 0) {
          if (in.pressed[(int)Btn::Up]) {
            subIndex_ = (subIndex_ + N - 1) % N;
            followMenu_ = true;
          }
          if (in.pressed[(int)Btn::Down]) {
            subIndex_ = (subIndex_ + 1) % N;
            followMenu_ = true;
          }
        }
        if (in.pressed[(int)Btn::Back] || in.pressed[(int)Btn::Menu]) {
          subMode_ = Sub::Main;
          menuScroll_ = 0;
        }
        break;
      }
    }
  }

  void closeMenu() {
    menuOpen_ = false;
    subMode_ = Sub::Main;
    SDL_ShowCursor(SDL_DISABLE);
    // 快进档位保留（对齐 Android：菜单设置的快进状态在菜单关闭后持续生效）
  }

  int menuItemCount(App& app) const {
    // 继续/暂停/重置/存档/读档/快进/截图/画面设置/退出（对齐 APK 游戏内菜单顺序；
    // 核心设置/金手指/磁盘切换按条件插入，与 drawMenu 的 idx 顺序一致）
    int n = 9;
    EmuRunner* emu = app.activeEmu();
    if (emu && emu->core() && emu->core()->diskControl().available() &&
        emu->core()->diskControl().numImages() > 1)
      n++;
    if (emu && emu->core() && !emu->core()->options().empty()) n++;
    if (emu && emu->running() && emu->isLibretro()) n++;  // ★v1.3.1 金手指
    return n;
  }

  // ★v1.2.10 行号体系：子页第 0 行统一为「‹ 返回主菜单」，其余顺延
  int subItemCount(App& app) const {
    if (subMode_ == Sub::States) return 12;  // 返回 + 槽位 0-10
    if (subMode_ == Sub::Video) return videoRowCount(app) + 1;  // + 返回行
    if (subMode_ == Sub::Disk) {
      EmuRunner* emu = app.activeEmu();
      if (emu && emu->core())
        return (int)emu->core()->diskControl().numImages() + 1;  // + 返回行
      return 1;
    }
    return 0;
  }

  // 画面设置子页行数（不含返回行；★v1.2.10 新增「高清缩放」行）
  int videoRowCount(App& app) const {
    EmuRunner* emu = app.activeEmu();
    int n = 6;  // 画面缩放 / 视频滤镜 / 高清缩放 / 整数缩放 / 平滑缩放 / 显示 FPS
    if (emu && platformIsDualScreen(emu->game().platform)) n++;  // 双屏布局
    n++;  // 全屏切换
    return n;
  }

  // —— ★v1.2.10 高清缩放（内部分辨率倍数）——
  // 在当前核心选项中定位「内部分辨率/分辨率倍数」类选项（已知键名优先，
  // 描述特征兜底）。返回 nullptr = 该核心不支持（2D 核心等）。
  const CoreOption* hdOption(App& app) const {
    EmuRunner* emu = app.activeEmu();
    if (!emu || !emu->core() || !emu->isLibretro()) return nullptr;
    // 已知键名表（libretro 常见 3D 核心的内部分辨率选项）
    static const char* kHdKeys[] = {
        "pcsx_rearmed_internal_resolution",  // PSX 分辨率倍数 1x-4x
        "pcsx2_upscale_multiplier",          // PS2 分辨率倍数 1x-6x
        "melonds_gles_resolution",           // NDS 渲染分辨率 1x-8x
        "reicast_internal_resolution",       // Flycast(DC) 内部分辨率
        "flycast_internal_resolution",
        "citra_resolution",                  // 3DS (Citra) 分辨率
        "azahar_resolution",                 // 3DS (Azahar) 分辨率
    };
    for (const auto& o : emu->core()->options())
      for (const char* k : kHdKeys)
        if (o.key == k) return &o;
    // 描述特征兜底（新核心/改名键）
    for (const auto& o : emu->core()->options()) {
      if (!o.visible || o.values.empty()) continue;
      if (o.desc.find("内部分辨率") != std::string::npos ||
          o.desc.find("Internal Resolution") != std::string::npos ||
          o.desc.find("Resolution Multiplier") != std::string::npos ||
          o.desc.find("Render Resolution") != std::string::npos ||
          o.desc.find("分辨率倍数") != std::string::npos)
        return &o;
    }
    return nullptr;
  }

  // 高清缩放当前档位索引（持久层值 → 选项索引；未存储用核心默认）
  int hdValueIndex(App& app, const CoreOption* o) const {
    EmuRunner* emu = app.activeEmu();
    if (!emu || !emu->core() || !o || o->values.empty()) return 0;
    const std::string cur = app.cores.resolveOption(
        app.cores.resolveCore(emu->game().platform).coreFile, o->key);
    if (!cur.empty())
      for (size_t v = 0; v < o->values.size(); v++)
        if (cur == o->values[v]) return (int)v;
    return o->defaultValue;
  }

  // 应用高清缩放档位（写运行中核心 + 持久层）
  void applyHdOption(App& app, const CoreOption* o, int idx) {
    EmuRunner* emu = app.activeEmu();
    if (!emu || !emu->core() || !o || idx < 0 || idx >= (int)o->values.size()) return;
    const std::string val = o->values[(size_t)idx];
    emu->core()->setOptionValue(o->key, val);
    app.cores.storeOption(app.cores.resolveCore(emu->game().platform).coreFile,
                          o->key, val);
    app.toast("高清缩放已应用（部分核心需重开游戏生效）");
  }

  void draw(App& app) override {
    EmuRunner* emu = app.activeEmu();
    if (!emu) return;

    // —— 游戏画面（帧已在 update 里步进）——
    const Skin& sk = Theme::skin();
    Draw& d = app.draw;
    app.draw.rect(Rect(0, 0, app.draw.W, app.draw.H), sk.bgDeep);
    Rect gameArea(0, 0, (float)app.draw.W, (float)app.draw.H);

    // ★v1.2.8：APK videoScale/videoFilter（游戏内 = 平台键优先）
    const GamePlatform plat = emu->game().platform;
    VideoDrawParams vp;
    const std::string vs = platformVideoScale(app, plat);
    const std::string vf = platformVideoFilter(app, plat);
    vp.videoScale = vs.c_str();
    vp.customRect = app.config.customLayout;
    vp.videoFilter = vf.c_str();
    vp.integerScale = app.config.integerScale;
    vp.dual = (DualLayout)app.config.dualLayout;
    vp.smooth = app.config.videoSmooth ? 1.0f : 0.0f;
    app.video.draw(app.sdlRenderer(), gameArea, vp);

    // FPS HUD（对齐 Android：右上角胶囊，快进金色 / 常态青色，追加暂停态）
    // ★v1.2.11：计「模拟帧」而非渲染帧 —— APK 的 fpsFrameCounter 在
    //   onFrame（每次 runFrame 后）自增；旧版计 draw() 次数，高刷屏/
    //   无 VSync 时显示 80-120（用户误以为没有帧数限制的根源）。
    if (app.config.showFps) {
      const Uint32 now = SDL_GetTicks();
      if (fpsLast_ == 0) fpsLast_ = now;
      if (now - fpsLast_ > 500) {
        fps_ = (float)(emuFrames_ - fpsFramesBase_) * 1000.0f / (now - fpsLast_);
        fpsFramesBase_ = emuFrames_;
        fpsLast_ = now;
      }
      std::string hud = "FPS " + std::to_string((int)(fps_ + 0.5f));
      if (emu->fastForward()) hud += " · 快进 ×" + std::to_string(emu_ffMult_);
      if (paused_) hud += " · 已暂停";
      const float hudW = app.font.textWidth(hud, 14, true) + 24;
      const Rect pill(app.draw.W - hudW - 14, 12, hudW, 30);
      d.roundRect(pill, 15, Color8(0, 0, 0, 150));
      app.font.draw(app.sdlRenderer(), hud, pill.x + 12, pill.y + 7, 14,
                    emu->fastForward() ? sk.gold : sk.accent, true);
    }

    // 快进指示（无 FPS HUD 时也要能看到）
    if (emu->fastForward() && !app.config.showFps) {
      app.font.draw(app.sdlRenderer(),
                    std::string("快进 ×") + std::to_string(emu_ffMult_),
                    app.draw.W - 90, 18, 14, sk.gold, true);
    }

    // 暂停浮层（对齐 Android「已暂停 —— 按 Esc 呼出菜单」）
    if (paused_ && !menuOpen_ && !editLayout_) {
      d.rect(Rect(0, 0, app.draw.W, app.draw.H), Color8(0, 0, 0, 90));
      const char* msg = "已暂停 —— 按 Esc 呼出菜单，F7 继续";
      app.font.draw(app.sdlRenderer(), msg,
                    app.draw.W / 2 - app.font.textWidth(msg, 20, true) / 2,
                    app.draw.H / 2, 20, sk.textHi, true);
    }

    if (editLayout_) drawLayoutEditor(app);
    else if (menuOpen_) drawMenu(app);
  }

  // ======================= 自定义比例编辑器（APK ScreenPositionEditor 1:1）
  // 交互（ScreenPositionEditor.kt:110-215）：
  //   · 4 角手柄（归一化容差 6% 屏幕尺寸 → PC 用 26 逻辑像素命中半径）
  //     拖动独立更新 x1/y1 或 x2/y2，最小尺寸 MIN_SIZE=0.05（5% 屏幕）；
  //   · 矩形内部任意点 = 整体拖动（保持宽高，四边 clamp [0,1]）；
  //   · 实时预览（拖动中游戏画面立即跟随）；抬手持久化（APK confirm）；
  //   · 视觉（48-99 行）：半透明粉红填充 #33E91E63 + 边框 + 4 圆手柄；
  //   · 底部「完成」「重置」按钮（APK 5955-5996）。
  void updateLayoutEditor(App& app, const InputState& in) {
    const float W = (float)app.draw.W, H = (float)app.draw.H;
    const float tol = 0.06f * std::min(W, H);  // APK 归一化容差 6%
    auto clamp01 = [](float v) { return std::min(1.0f, std::max(0.0f, v)); };
    constexpr float MIN_SIZE = 0.05f;  // APK MIN_SIZE

    // 手柄中心（屏幕坐标）
    const float hx[4] = {rect_[0] * W, rect_[2] * W, rect_[2] * W, rect_[0] * W};
    const float hy[4] = {rect_[1] * H, rect_[1] * H, rect_[3] * H, rect_[3] * H};

    if (in.mousePressed) {
      editDrag_ = -1;
      float best = tol * tol;
      for (int i = 0; i < 4; i++) {
        const float dx = in.mouse.x - hx[i], dy = in.mouse.y - hy[i];
        const float d2 = dx * dx + dy * dy;
        if (d2 <= best) { best = d2; editDrag_ = i; }
      }
      if (editDrag_ < 0) {
        // 矩形内部 = 整体拖动（APK body 拖动）
        if (in.mouse.x >= hx[0] && in.mouse.x <= hx[2] && in.mouse.y >= hy[0] &&
            in.mouse.y <= hy[3]) {
          editDrag_ = 4;
          editAnchorX_ = in.mouse.x;
          editAnchorY_ = in.mouse.y;
          editOrig_[0] = rect_[0]; editOrig_[1] = rect_[1];
          editOrig_[2] = rect_[2]; editOrig_[3] = rect_[3];
        }
      }
    }
    if (in.mouseHeld && editDrag_ >= 0) {
      const float nx = clamp01(in.mouse.x / W);
      const float ny = clamp01(in.mouse.y / H);
      switch (editDrag_) {
        case 0: rect_[0] = std::min(nx, rect_[2] - MIN_SIZE);
                rect_[1] = std::min(ny, rect_[3] - MIN_SIZE); break;
        case 1: rect_[2] = std::max(nx, rect_[0] + MIN_SIZE);
                rect_[1] = std::min(ny, rect_[3] - MIN_SIZE); break;
        case 2: rect_[2] = std::max(nx, rect_[0] + MIN_SIZE);
                rect_[3] = std::max(ny, rect_[1] + MIN_SIZE); break;
        case 3: rect_[0] = std::min(nx, rect_[2] - MIN_SIZE);
                rect_[3] = std::max(ny, rect_[1] + MIN_SIZE); break;
        case 4: {
          // body：保持宽高跟随触点，四边 clamp（APK 191-204）
          const float dx = (in.mouse.x - editAnchorX_) / W;
          const float dy = (in.mouse.y - editAnchorY_) / H;
          const float rw = editOrig_[2] - editOrig_[0];
          const float rh = editOrig_[3] - editOrig_[1];
          rect_[0] = clamp01(std::min(editOrig_[0] + dx, 1.0f - rw));
          rect_[1] = clamp01(std::min(editOrig_[1] + dy, 1.0f - rh));
          rect_[2] = rect_[0] + rw;
          rect_[3] = rect_[1] + rh;
          break;
        }
      }
    }
    if (in.mouseReleased && editDrag_ >= 0) {
      editDrag_ = -1;
      persistLayout(app);  // 抬手持久化（APK confirm=true）
    }

    if (in.pressed[(int)Btn::Back] || in.pressed[(int)Btn::Menu]) {
      // 取消：还原进入编辑器时的矩形
      for (int i = 0; i < 4; i++) rect_[i] = editBackup_[i];
      editLayout_ = false;
      SDL_ShowCursor(SDL_DISABLE);
      app.toast("已取消自定义比例修改");
    }
  }

  void persistLayout(App& app) {
    for (int i = 0; i < 4; i++) app.config.customLayout[i] = rect_[i];
    app.config.save(fsutil::join(app.dataDir(), "config.json"));
  }

  void enterLayoutEditor(App& app) {
    for (int i = 0; i < 4; i++) {
      rect_[i] = app.config.customLayout[i];
      editBackup_[i] = rect_[i];
    }
    editLayout_ = true;
    editDrag_ = -1;
    menuOpen_ = false;
    SDL_ShowCursor(SDL_ENABLE);
  }

  void drawLayoutEditor(App& app) {
    const Skin& sk = Theme::skin();
    Draw& d = app.draw;
    const float W = (float)app.draw.W, H = (float)app.draw.H;
    const Rect rc(rect_[0] * W, rect_[1] * H, (rect_[2] - rect_[0]) * W,
                  (rect_[3] - rect_[1]) * H);
    // 半透明粉红填充 + 边框（APK #33E91E63）
    d.rect(rc, Color8(0xE9, 0x1E, 0x63, 0x33));
    d.rectOutline(rc, Color8(0xE9, 0x1E, 0x63, 220), 2);
    // 4 圆手柄
    for (int i = 0; i < 4; i++) {
      const float cx = (i == 0 || i == 3) ? rc.x : rc.right();
      const float cy = (i == 0 || i == 1) ? rc.y : rc.bottom();
      d.circle(cx, cy, 14, Color8(0xE9, 0x1E, 0x63, 200));
      d.circleOutline(cx, cy, 14, Color8(255, 255, 255, 230), 2);
    }
    // 标题 + 提示
    const char* title = "自定义画面比例";
    app.font.draw(app.sdlRenderer(), title,
                  app.draw.W / 2 - app.font.textWidth(title, 20, true) / 2, 18, 20,
                  sk.textHi, true);
    const char* hint = "拖动四角调整大小 · 拖动矩形内部移动 · Esc 取消";
    app.font.draw(app.sdlRenderer(), hint,
                  app.draw.W / 2 - app.font.textWidth(hint, 13) / 2, 48, 13, sk.textDim);

    // 底部按钮（APK：完成 / 重置）
    const InputState& in = app.input.input();
    const float bw = 150, bh = 42;
    {
      const Rect rcDone(W / 2 - bw - 10, H - bh - 18, bw, bh);
      if (w::primaryButton(app, rcDone, "完成")) {
        persistLayout(app);
        editLayout_ = false;
        SDL_ShowCursor(SDL_DISABLE);
        app.toast("自定义比例已保存");
      }
    }
    {
      const Rect rcReset(W / 2 + 10, H - bh - 18, bw, bh);
      if (w::button(app, rcReset, "重置", false)) {
        rect_[0] = 0; rect_[1] = 0; rect_[2] = 1; rect_[3] = 1;
        persistLayout(app);
        app.toast("已重置为全屏");
      }
    }
  }

  // ======================= 游戏内菜单（★v1.2.10：上方一列竖排） =======================
  // 用户要求 + APK MenuOverlay 视觉语言：菜单贴屏幕顶部、窄列竖排；
  // 面板 #DD1E2A3A 圆角 16dp、分隔线 20% 白、游戏名副行；
  // 菜单项/快捷键标注按 PC 键位（F7 暂停 / F5 存 / F8 读 / Tab 快进 / F12 截图）。
  // 菜单项顺序对齐 APK：继续 → 暂停(F7) → 重置 → 存档(F5) → 读档(F8) →
  // 快进(Tab) → 截图(F12) → 画面设置 → 核心设置 → [切换磁盘] → 退出游戏
  void drawMenu(App& app) {
    const Skin& sk = Theme::skin();
    Draw& d = app.draw;
    EmuRunner* emu = app.activeEmu();
    const InputState& in = app.input.input();

    // 全屏压暗（APK MenuOverlay 遮罩 0x88000000）
    d.rect(Rect(0, 0, (float)d.W, (float)d.H), Color8(0, 0, 0, 136));

    // —— 面板几何：顶部居中一列 ——
    const float pad = 14, rowH = 42, rowGap = 4, headH = 58;
    const float pw = std::min(348.0f, (float)d.W - 20.0f);
    // ★行数：Main 用 menuItemCount（subItemCount 对 Main 返回 0 —— 曾致
    //   面板只剩标题区、行区零高度裁剪，菜单项全部不显示）
    const float rows = (float)(subMode_ == Sub::Main ? menuItemCount(app)
                                                     : subItemCount(app));
    const float ph = std::min(headH + 8 + rows * (rowH + rowGap), (float)d.H - 18.0f);
    const Rect panel((float)d.W / 2 - pw / 2, 10, pw, ph);
    // 点击面板外 = 关闭菜单
    if (in.mousePressed && !panel.contains(in.mouse)) {
      closeMenu();
      return;
    }
    // 面板底色（APK #DD1E2A3A · 圆角 16dp）+ 微弱描边
    d.roundRect(panel, 16, Color8::hex(0x1E2A3A, 0xDD));
    d.roundRectOutline(panel, 16, Color8(255, 255, 255, 34), 1.2f);

    // —— 标题行（标题 + 游戏名副行）+ 分隔线（APK 20% 白）——
    const char* title = subMode_ == Sub::States
                            ? (saveMode_ ? "保存即时存档" : "读取即时存档")
                            : subMode_ == Sub::Video ? "画面设置"
                            : subMode_ == Sub::Disk ? "切换磁盘"
                                                    : "游戏菜单";
    app.font.draw(app.sdlRenderer(), title, panel.x + pad, panel.y + 11, 16, sk.textHi,
                  true);
    if (emu) {
      app.font.draw(app.sdlRenderer(),
                    app.font.ellipsize(emu->game().displayTitle(), 11, pw - pad * 2),
                    panel.x + pad, panel.y + 36, 11, Color8(0xAA, 0xB8, 0xC8));
    }
    d.line(panel.x + pad, panel.y + headH - 9, panel.right() - pad, panel.y + headH - 9,
           Color8(255, 255, 255, 51), 1);

    // —— 行区域（滚轮/拖拽滚动 + 裁剪；小窗口下列表也能滚到底） ——
    const Rect rowsRc(panel.x + 8, panel.y + headH, pw - 16, ph - headH - 8);
    menuScroll_ += w::dragScroll(app, rowsRc);
    if (rowsRc.contains(in.mouse)) menuScroll_ -= (float)in.wheel * (rowH + rowGap);
    const float contentH = rows * (rowH + rowGap);
    const float maxScroll = contentH > rowsRc.h ? contentH - rowsRc.h : 0.0f;
    // 键盘上下移动时焦点行跟随滚动一次（不与拖拽滚动打架）
    if (followMenu_) {
      followMenu_ = false;
      const int cur = subMode_ == Sub::Main ? menuIndex_ : subIndex_;
      const float rowY = (float)cur * (rowH + rowGap) - menuScroll_;
      if (rowY < 0) menuScroll_ += rowY;
      if (rowY > rowsRc.h - rowH) menuScroll_ += rowY - (rowsRc.h - rowH);
    }
    menuScroll_ = std::max(0.0f, std::min(menuScroll_, maxScroll));
    d.pushClip(rowsRc);
    float y = rowsRc.y - menuScroll_;

    // 行绘制：标题左 + 快捷键/值右（listRow 内含 hover/点击/焦点高亮）
    auto row = [&](const std::string& t, int idx, const std::string& value = "",
                   bool enabled = true) {
      const Rect rc(panel.x + 8, y, pw - 16, rowH);
      const bool focused =
          subMode_ == Sub::Main ? menuIndex_ == idx : subIndex_ == idx;
      y += rowH + rowGap;
      if (!enabled) {  // 不可用行（如核心不支持高清缩放）：灰显不可点
        app.font.drawVCentered(app.sdlRenderer(), t, rc.x + 14, rc.cy(), 15, sk.textDim);
        if (!value.empty())
          app.font.drawVCentered(app.sdlRenderer(), value,
                                 rc.right() - 14 - app.font.textWidth(value, 13), rc.cy(),
                                 13, sk.textDim);
        return false;
      }
      return w::listRow(app, rc, t, value, focused);
    };

    switch (subMode_) {
      case Sub::Main: {
        int idx = 0;
        if (row("继续游戏", idx++)) closeMenu();
        if (row(paused_ ? "继续游戏（取消暂停）" : "暂停游戏", idx++, "F7")) {
          paused_ = !paused_;
          closeMenu();
        }
        if (row("重置", idx++)) {
          if (emu) emu->resetCore();
          app.toast("已重置");
          closeMenu();
        }
        if (row("即时存档", idx++, "F5")) {
          subMode_ = Sub::States;
          subIndex_ = 0;
          menuScroll_ = 0;
          saveMode_ = true;
        }
        if (row("即时读档", idx++, "F8")) {
          subMode_ = Sub::States;
          subIndex_ = 0;
          menuScroll_ = 0;
          saveMode_ = false;
        }
        if (row(ffLevel_ > 1 ? "快进（点击换挡）" : "快进（关）", idx++,
                ffLevel_ > 1 ? "×" + std::to_string(ffLevel_) : "Tab")) {
          cycleFfLevel(app);
        }
        if (row("截图", idx++, "F12")) {
          closeMenu();
          takeScreenshot(app);
        }
        if (row("画面设置", idx++)) {
          subMode_ = Sub::Video;
          subIndex_ = 0;
          menuScroll_ = 0;
        }
        if (emu && emu->core() && !emu->core()->options().empty()) {
          if (row("核心设置", idx++)) {
            app.pushScene(createCoreOptionsScene(emu->game().platform));
            menuOpen_ = false;
          }
        }
        // ★v1.3.1 金手指（APK 底部工具栏同款入口；RetroArch .cht 逐帧应用）
        if (emu && emu->running() && emu->isLibretro()) {
          if (row("金手指", idx++)) {
            app.pushScene(createCheatsScene());
            menuOpen_ = false;
          }
        }
        if (emu && emu->core() && emu->core()->diskControl().available() &&
            emu->core()->diskControl().numImages() > 1) {
          if (row("切换磁盘", idx++)) {
            subMode_ = Sub::Disk;
            subIndex_ = 0;
            menuScroll_ = 0;
          }
        }
        if (row("退出游戏", idx++, "自动存档")) {
          d.popClip();
          exitGame(app);
          return;
        }
        break;
      }
      case Sub::States: {
        if (row("‹ 返回主菜单", 0)) subMode_ = Sub::Main;
        const auto existing = emu ? emu->slots(app) : std::vector<int>{};
        for (int slot = 0; slot <= 10; slot++) {
          const std::string name =
              slot == 0 ? "槽位 A（自动）" : ("槽位 " + std::to_string(slot));
          const bool has =
              std::find(existing.begin(), existing.end(), slot) != existing.end();
          if (row(name, slot + 1, has ? "● 有档" : "— 空")) {
            if (emu) {
              if (saveMode_) {
                if (emu->saveStateSlot(app, slot))
                  app.toast("已存档：" + name);
                else
                  app.toast("存档失败（核心不支持）");
              } else {
                if (emu->loadStateSlot(app, slot))
                  app.toast("已读档：" + name);
                else
                  app.toast(name + " 没有存档");
              }
            }
            subMode_ = Sub::Main;
          }
        }
        break;
      }
      case Sub::Video: {
        if (row("‹ 返回主菜单", 0)) subMode_ = Sub::Main;
        const GamePlatform plat = emu ? emu->game().platform : GamePlatform::NES;
        int idx = 1;
        // —— 画面缩放（整体窗口比例；APK SettingsPanel「画面缩放」） ——
        {
          const Rect rc(panel.x + 8, y, pw - 16, rowH);
          int vi = indexFor(videoScaleKeys(), platformVideoScale(app, plat));
          if (w::cycleRow(app, rc, "画面缩放", videoScaleOptions(), vi,
                          subIndex_ == idx)) {
            const std::string v = videoScaleKeys()[vi];
            setPlatformVideoScale(app, plat, v);
            if (v == "custom") {  // APK：选 custom 直进四角编辑器
              d.popClip();
              enterLayoutEditor(app);
              return;
            }
          }
          y += rowH + rowGap;
          idx++;
        }
        // —— 视频滤镜（叠加滤镜 11 档） ——
        {
          const Rect rc(panel.x + 8, y, pw - 16, rowH);
          int fi = indexFor(videoFilterKeys(), platformVideoFilter(app, plat));
          if (w::cycleRow(app, rc, "视频滤镜", videoFilterOptions(), fi,
                          subIndex_ == idx)) {
            setPlatformVideoFilter(app, plat, videoFilterKeys()[fi]);
          }
          y += rowH + rowGap;
          idx++;
        }
        // —— ★v1.2.10 高清缩放（内部分辨率倍数 —— 3D 核心内部渲染分辨率，
        //    非整体画面缩放；对齐 APK PSX「分辨率倍数 (立即生效)」/
        //    PS2「分辨率倍数」/ NDS「渲染分辨率」/ DC「内部分辨率」） ——
        {
          const CoreOption* hd = hdOption(app);
          if (hd && !hd->values.empty()) {
            const Rect rc(panel.x + 8, y, pw - 16, rowH);
            int cur = hdValueIndex(app, hd);
            std::vector<std::string> labels;
            labels.reserve(hd->labels.size());
            for (const auto& lb : hd->labels) labels.push_back(zhOptionValue(lb));
            if (w::cycleRow(app, rc, "高清缩放（内部分辨率）", labels, cur,
                            subIndex_ == idx)) {
              applyHdOption(app, hd, cur);
            }
          } else {
            // 2D 核心等无内部分辨率概念：明确提示而非缺行（用户点名要该项）
            row("高清缩放（内部分辨率）", idx, "不支持", false);
          }
          y += rowH + rowGap;
          idx++;
        }
        // —— 整数缩放（像素完美） ——
        {
          const Rect rc(panel.x + 8, y, pw - 16, rowH);
          if (w::toggleRow(app, rc, "整数缩放（像素完美）", app.config.integerScale,
                           subIndex_ == idx))
            app.config.save(fsutil::join(app.dataDir(), "config.json"));
          y += rowH + rowGap;
          idx++;
        }
        // —— 平滑缩放 ——
        {
          const Rect rc(panel.x + 8, y, pw - 16, rowH);
          if (w::toggleRow(app, rc, "平滑缩放", app.config.videoSmooth,
                           subIndex_ == idx))
            app.config.save(fsutil::join(app.dataDir(), "config.json"));
          y += rowH + rowGap;
          idx++;
        }
        // —— 显示 FPS ——
        {
          const Rect rc(panel.x + 8, y, pw - 16, rowH);
          if (w::toggleRow(app, rc, "显示 FPS", app.config.showFps,
                           subIndex_ == idx))
            app.config.save(fsutil::join(app.dataDir(), "config.json"));
          y += rowH + rowGap;
          idx++;
        }
        if (emu && platformIsDualScreen(emu->game().platform)) {
          const Rect rc(panel.x + 8, y, pw - 16, rowH);
          std::vector<std::string> opts = {"双屏", "仅上屏", "仅下屏"};
          if (w::cycleRow(app, rc, "双屏布局", opts, app.config.dualLayout,
                          subIndex_ == idx))
            app.config.save(fsutil::join(app.dataDir(), "config.json"));
          y += rowH + rowGap;
          idx++;
        }
        {
          const Rect rc(panel.x + 8, y, pw - 16, rowH);
          if (w::listRow(app, rc, "全屏切换", "F11", subIndex_ == idx))
            app.toggleFullscreen();
          y += rowH + rowGap;
          idx++;
        }
        break;
      }
      case Sub::Disk: {
        if (row("‹ 返回主菜单", 0)) subMode_ = Sub::Main;
        if (emu && emu->core()) {
          DiskControl& dc = emu->core()->diskControl();
          const unsigned n = dc.numImages();
          for (unsigned i = 0; i < n; i++) {
            if (row(dc.imageLabel(i), (int)i + 1)) {
              if (dc.select(i)) app.toast("已切换：" + dc.imageLabel(i));
              subMode_ = Sub::Main;
            }
          }
        }
        break;
      }
    }
    d.popClip();

    w::keyHintBar(app, {
                          {"↑↓", "选择"},
                          {"A / 点击", "确认"},
                          {"B / Esc", "返回"},
                          {"点击面板外", "关闭"},
                      });
  }

  void exitGame(App& app) {
    EmuRunner* emu = app.activeEmu();
    if (emu) {
      emu->stop(app, app.config.autoSaveState);
    }
    app.popScene();
  }

 private:
  enum class Sub { Main, States, Video, Disk };
  bool menuOpen_ = false;
  bool justOpened_ = false;  // ★v1.2.10 呼出帧的同一次按键不触发关闭
  bool wasFullscreen_ = true;   // ★v1.2.6 进入游戏前的全屏状态（自动全屏恢复用）
  Sub subMode_ = Sub::Main;
  int menuIndex_ = 0;
  int subIndex_ = 0;
  bool saveMode_ = true;
  bool paused_ = false;       // F7 / 菜单暂停（对齐 Android 暂停浮层）
  int ffLevel_ = 0;           // 快进档位 0/2/4/6/8/16（0 = 关；对齐 Android 档位环）
  int emu_ffMult_ = 2;        // 最近一次生效的快进倍数（HUD 显示用）
  bool f5Prev_ = false, f6Prev_ = false, f7Prev_ = false, f8Prev_ = false;
  bool f11Prev_ = false;
  bool f12Prev_ = false;
  // ★v1.2.11 FramePacer 状态（墙钟截止时间；替代旧 dt 累加器 accum_）
  double nextFrameAt_ = 0;   // 下一模拟帧的绝对时刻（steady_clock 秒）
  bool paceInit_ = false;
  uint64_t emuFrames_ = 0;   // 模拟帧计数（FPS HUD 用，对齐 APK onFrame）
  float menuScroll_ = 0;   // ★v1.2.10 菜单行滚动（小窗口下长列表）
  bool followMenu_ = false;  // 键盘移动后焦点行跟随滚动一次
  float fps_ = 0;
  uint64_t fpsFramesBase_ = 0;  // 上次 FPS 采样时的模拟帧计数
  Uint32 fpsLast_ = 0;
  App* app_ = nullptr;

  // —— 自定义比例编辑器状态（APK customRect 会话态）——
  bool editLayout_ = false;
  float rect_[4] = {0, 0, 1, 1};       // 当前编辑矩形（归一化）
  float editBackup_[4] = {0, 0, 1, 1}; // 进入编辑器时的备份（Esc 取消）
  float editOrig_[4] = {0, 0, 1, 1};   // body 拖动起始快照
  float editAnchorX_ = 0, editAnchorY_ = 0;
  int editDrag_ = -1;                  // -1 无 / 0-3 角 / 4 body
};

}  // namespace

std::unique_ptr<Scene> createEmulatorScene() {
  return std::unique_ptr<Scene>(new EmulatorScene());
}

}  // namespace ns
