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

  // 快进档位循环（F6：0→2→4→6→8→16→0，APK 档位环同款）
  void cycleFfLevel(App& app) {
    switch (ffLevel_) {
      case 0: ffLevel_ = 2; break;
      case 2: ffLevel_ = 4; break;
      case 4: ffLevel_ = 6; break;
      case 6: ffLevel_ = 8; break;
      case 8: ffLevel_ = 16; break;
      default: ffLevel_ = 0; break;
    }
    if (ffLevel_ > 1) lastFfLevel_ = ffLevel_;
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
        // ★v1.3.5 横排图标栏（APK MenuOverlay）：←→ 在图标间移动，
        //   ↑↓ 空闲（APK D-pad 同款只左右导航）
        const int N = menuItemCount(app);
        if (N > 0) {
          if (in.pressed[(int)Btn::Left]) {
            menuIndex_ = (menuIndex_ + N - 1) % N;
            followMenu_ = true;
          }
          if (in.pressed[(int)Btn::Right]) {
            menuIndex_ = (menuIndex_ + 1) % N;
            followMenu_ = true;
          }
        }
        // ★v1.2.10：呼出帧的同一次按键不作为关闭指令（Esc/F1 开菜单修复）
        if (!justOpened_ &&
            (in.pressed[(int)Btn::Back] || in.pressed[(int)Btn::Menu]))
          closeMenu();
        break;
      }
      case Sub::States:
      case Sub::Speed:
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
          subIndex_ = 0;
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
    // ★v1.3.4 对齐 APK MenuOverlay（EmulatorScreen.kt:12491-12518）按钮顺序：
    //   暂停/继续 → 快进 → 倍速 → 截图 → 存档 → 读档 → 重置 → 金手指 →
    //   手柄布局 → 设置 → 核心日志 → 隐藏菜单 → 退出（固定 12 项）
    //   + PC 专属「切换磁盘」（核心上报多盘镜像时；APK 无此项，PC 多盘
    //     CD 游戏必需，放退出前）。
    int n = 12;
    EmuRunner* emu = app.activeEmu();
    if (emu && emu->running() && emu->isLibretro()) n++;  // 金手指
    if (emu && emu->core() && emu->core()->diskControl().available() &&
        emu->core()->diskControl().numImages() > 1)
      n++;  // 切换磁盘
    return n;
  }

  // ★v1.2.10 行号体系：子页第 0 行统一为「‹ 返回主菜单」，其余顺延
  int subItemCount(App& app) const {
    if (subMode_ == Sub::States) return 12;  // 返回 + 槽位 0-10
    if (subMode_ == Sub::Speed) return 6;   // 返回 + 2/4/6/8/16（APK 档位）
    if (subMode_ == Sub::Disk) {
      EmuRunner* emu = app.activeEmu();
      if (emu && emu->core())
        return (int)emu->core()->diskControl().numImages() + 1;  // + 返回行
      return 1;
    }
    return 0;
  }

  // ★v1.3.4：画面设置子页与通用「高清缩放」行已并入「核心设置」面板
  //   （APK SettingsPanel 按平台分区：分辨率倍数/渲染分辨率/内部分辨率
  //    都是各平台面板的专属行，见 ui/PanelSpec.cpp）；全局 画面缩放/视频滤镜
  //    留在主设置（APK SettingsScreen 同款）。

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

  // ======================= 游戏内菜单（★v1.3.5：底部横排图标栏） =======================
  // ★1:1 对齐 APK MenuOverlay（EmulatorScreen.kt:12428-12519）：
  //   · 横屏 = 屏幕底部整宽图标栏；背景 #DD1E2A3A 圆角 16dp、外边距 16/12dp、
  //     内边距 12/6dp、可横向滚动；
  //   · 栏内左侧游戏名（白 14sp 半粗，占满剩余宽），右侧图标按钮一排：
  //     每钮 46x50dp = 图标(≈26dp) + 下方 9sp 小字标签（#AAB8C8），
  //     焦点 = 图标后衬 32dp 白 25% 圆角 8 高亮（D-pad 电视同款）；
  //   · 按钮顺序与色调（APK 12513 行序）：暂停(白)/继续 → 快进(激活金
  //     #FFD66B) → 倍速(文字 1x/Nx，激活金) → 截图(白) → 存档(白) →
  //     读档(白) → 重置(金) → 金手指(绿 #7ED957，支持时) → 手柄布局(白)
  //     → 设置(白) → 核心日志(白) → 隐藏菜单(蓝 #4A90D9) → [切换磁盘
  //     (PC 多盘专属)] → 退出(红 #FF6B6B)；
  //   · 存档/读档/倍速/磁盘 = 居中弹窗（APK showSlotPicker/FFSpeedPicker
  //     对话框语义），B/Esc 返回图标栏；
  //   · 点击遮罩空白处关闭（PC 鼠标惯例，APK 触屏无此路径不冲突）。
  void drawMenu(App& app) {
    const Skin& sk = Theme::skin();
    Draw& d = app.draw;
    EmuRunner* emu = app.activeEmu();
    const InputState& in = app.input.input();
    const float W = (float)d.W, H = (float)d.H;

    // —— 全屏压暗（APK MenuOverlay 遮罩 0x88000000）——
    d.rect(Rect(0, 0, W, H), Color8(0, 0, 0, 136));

    // —— 底部图标栏几何（APK：外 16/12dp + 圆角 16 + 内 12/6dp）——
    const float barH = 66;                       // 图标钮 50 + 内边距 6*2 + 余量
    const Rect bar(16, H - 12 - barH, W - 32, barH);
    // 点击面板外 = 关闭菜单（子弹窗打开时点外 = 回主栏）
    if (in.mousePressed && !bar.contains(in.mouse) &&
        (subMode_ == Sub::Main || !subDialogRc_.contains(in.mouse))) {
      if (subMode_ == Sub::Main) {
        closeMenu();
        return;
      }
      subMode_ = Sub::Main;
      subIndex_ = 0;
    }
    d.roundRect(bar, 16, Color8::hex(0x1E2A3A, 0xDD));
    d.roundRectOutline(bar, 16, Color8(255, 255, 255, 30), 1.2f);

    // —— 按钮清单（顺序 = APK；图标 id 见 Widgets icon() 表）——
    struct MItem {
      int icon;             // -1 = 倍速文字
      std::string label;    // 下方小字（动态）
      Color8 tint;
      int action;           // 动作编号
    };
    const Color8 kWhite(255, 255, 255, 255);
    const Color8 kGold = Color8::hex(0xFFD66B);
    const Color8 kGreen = Color8::hex(0x7ED957);
    const Color8 kBlue = Color8::hex(0x4A90D9);
    const Color8 kRed = Color8::hex(0xFF6B6B);
    const bool ffOn = ffLevel_ > 1;
    std::vector<MItem> items;
    items.push_back({41, paused_ ? "继续" : "暂停", kWhite, 0});
    items.push_back({42, ffOn ? ("快进 " + std::to_string(ffLevel_) + "x") : "快进",
                     ffOn ? kGold : kWhite, 1});
    items.push_back({-1, "倍速", ffOn ? kGold : kWhite, 2});
    items.push_back({43, "截图", kWhite, 3});
    items.push_back({6, "存档", kWhite, 4});
    items.push_back({7, "读档", kWhite, 5});
    items.push_back({20, "重置", kGold, 6});
    const bool hasCheats = emu && emu->running() && emu->isLibretro();
    if (hasCheats) items.push_back({44, "金手指", kGreen, 7});
    items.push_back({45, "手柄布局", kWhite, 8});
    items.push_back({3, "设置", kWhite, 9});
    items.push_back({15, "核心日志", kWhite, 10});
    items.push_back({46, "隐藏菜单", kBlue, 11});
    const bool hasDisk = emu && emu->core() &&
                         emu->core()->diskControl().available() &&
                         emu->core()->diskControl().numImages() > 1;
    if (hasDisk) items.push_back({29, "切换磁盘", kWhite, 12});
    items.push_back({47, "退出", kRed, 13});
    const int N = (int)items.size();
    if (menuIndex_ >= N) menuIndex_ = N - 1;
    if (menuIndex_ < 0) menuIndex_ = 0;

    // —— 布局：左侧游戏名 + 按钮行（放不下可滚）——
    const float padX = 12, btnW = 48, gap = 2;
    std::string title = emu ? emu->game().displayTitle() : std::string("游戏菜单");
    const float titleMax = 190;
    title = app.font.ellipsize(title, 14, (int)titleMax);
    const float titleW = std::max(60.0f, app.font.textWidth(title, 14, true) + 10.0f);
    const float btnsW = (float)N * (btnW + gap) - gap;
    const float availW = bar.w - padX * 2 - titleW - 6;
    const bool needScroll = btnsW > availW;
    // 滚动：滚轮 + 拖拽（栏空白处）；键盘焦点跟随
    const Rect btnArea(bar.x + padX + titleW + 6, bar.y, availW, barH);
    if (needScroll) {
      menuScroll_ += w::dragScroll(app, btnArea);
      if (btnArea.contains(in.mouse)) menuScroll_ -= (float)in.wheel * 40;
    }
    float maxScroll = needScroll ? btnsW - availW : 0;
    if (followMenu_ && needScroll) {
      followMenu_ = false;
      const float cx = (float)menuIndex_ * (btnW + gap) + btnW / 2 - menuScroll_;
      if (cx < 40) menuScroll_ += cx - 40;
      if (cx > availW - 40) menuScroll_ += cx - (availW - 40);
    }
    menuScroll_ = std::max(0.0f, std::min(menuScroll_, maxScroll));

    // —— 游戏名（APK：白 14sp 半粗，占满剩余宽）——
    app.font.drawVCentered(app.sdlRenderer(), title, bar.x + padX,
                           bar.y + barH * 0.5f - 6, 14, kWhite, true);

    // —— 图标按钮（图标 + 下方 9sp 标签 #AAB8C8）——
    if (needScroll) d.pushClip(bar);
    for (int i = 0; i < N; i++) {
      const float bx = bar.x + padX + titleW + 6 + (float)i * (btnW + gap) - menuScroll_;
      if (bx + btnW < bar.x - 8 || bx > bar.right() + 8) continue;
      const MItem& it = items[(size_t)i];
      const Rect rc(bx, bar.y + 4, btnW, barH - 8);
      const bool focused = subMode_ == Sub::Main && menuIndex_ == i;
      const bool hov = rc.contains(in.mouse);
      // 图标区（40x40 居中 + 焦点白 25% 圆角 8 衬底 —— APK 32dp 同款）
      const Rect iconRc(rc.x + (btnW - 28) / 2, rc.y + 5, 28, 28);
      if (focused || hov) {
        d.roundRect(Rect(iconRc.x - 4, iconRc.y - 3, iconRc.w + 8, iconRc.h + 6),
                    8, Color8(255, 255, 255, focused ? 64 : 40));
      }
      if (it.icon < 0) {
        // 倍速：文字（APK：13sp Bold，激活金色）
        const std::string txt = ffOn ? std::to_string(ffLevel_) + "x" : "1x";
        app.font.drawVCentered(app.sdlRenderer(), txt, iconRc.x - 3,
                               iconRc.cy(), 13, it.tint, true);
      } else {
        w::icon(app, it.icon, iconRc, it.tint);
      }
      // 下方标签（9sp #AAB8C8）
      app.font.draw(app.sdlRenderer(), it.label,
                    rc.cx() - app.font.textWidth(it.label, 9) / 2,
                    rc.y + 40, 9, Color8::hex(0xAAB8C8));
      // 激活（点击 / 焦点 + Ok）
      if (w::activated(app, rc, focused)) activateMenuAction(app, it.action);
    }
    if (needScroll) d.popClip();

    // —— 子弹窗（存档/读档/倍速/磁盘 —— APK 对话框语义）——
    if (subMode_ != Sub::Main) drawMenuSubDialog(app);
  }

  // 横排图标栏动作分发（APK MenuOverlay 各 onXxx 回调 1:1）
  void activateMenuAction(App& app, int action) {
    EmuRunner* emu = app.activeEmu();
    switch (action) {
      case 0:  // onTogglePause
        paused_ = !paused_;
        app.toast(paused_ ? "已暂停" : "继续游戏");
        break;
      case 1: {  // onToggleFastForward：>0→0；0→上次非零档位
        ffLevel_ = ffLevel_ > 1 ? 0 : (lastFfLevel_ > 1 ? lastFfLevel_ : 6);
        if (ffLevel_ > 1) lastFfLevel_ = ffLevel_;
        app.toast(ffLevel_ > 1 ? std::string("快进 ×") + std::to_string(ffLevel_)
                               : std::string("快进已关闭"));
        break;
      }
      case 2:  // onCycleFFSpeed → 倍速弹窗
        subMode_ = Sub::Speed;
        subIndex_ = 0;
        break;
      case 3:  // onScreenshot
        takeScreenshot(app);
        break;
      case 4:  // onSaveState → 槽位弹窗
        subMode_ = Sub::States;
        subIndex_ = 0;
        saveMode_ = true;
        break;
      case 5:  // onLoadState → 槽位弹窗
        subMode_ = Sub::States;
        subIndex_ = 0;
        saveMode_ = false;
        break;
      case 6:  // onReset（engine.reset(hard=false)）
        if (emu) emu->resetCore();
        app.toast("已重置");
        break;
      case 7:  // onCheats（v69 全核心金手指）
        app.pushScene(createCheatsScene());
        menuOpen_ = false;
        break;
      case 8:  // onLayoutEditor → PC 键位/手柄输入设置
        app.pushScene(createSettingsScene(4));
        menuOpen_ = false;
        break;
      case 9:  // onSettings → CoreSettingsPanel（按平台分区）
        app.pushScene(createCoreOptionsScene(
            emu ? emu->game().platform : GamePlatform::NES));
        menuOpen_ = false;
        break;
      case 10: {  // onCoreLog
        std::string log;
        fsutil::readFile(fsutil::join(app.dataDir(), "nesstation.log"), &log);
        std::vector<std::string> lines;
        std::string cur;
        for (char c : log) {
          if (c == '\n') {
            lines.push_back(cur);
            cur.clear();
          } else {
            cur += c;
          }
        }
        if (!cur.empty()) lines.push_back(cur);
        std::string body;
        const size_t from = lines.size() > 40 ? lines.size() - 40 : 0;
        for (size_t i = from; i < lines.size(); i++) body += lines[i] + "\n";
        app.pushScene(createMessageScene(
            "核心日志（最近 40 行 · 完整日志见数据目录 nesstation.log）",
            body.empty() ? "暂无日志" : body, false));
        menuOpen_ = false;
        break;
      }
      case 11:  // onClose 隐藏菜单
        closeMenu();
        break;
      case 12:  // PC 专属：切换磁盘弹窗
        subMode_ = Sub::Disk;
        subIndex_ = 0;
        break;
      case 13:  // onExit 退出游戏
        exitGame(app);
        break;
    }
  }

  // 子弹窗（存档/读档槽位 / 快进倍速 / 磁盘镜像）——APK AlertDialog 语义：
  // 居中圆角面板 #DD1E2A3A、标题 + 竖排行、B/Esc 返回图标栏
  void drawMenuSubDialog(App& app) {
    const Skin& sk = Theme::skin();
    Draw& d = app.draw;
    const InputState& in = app.input.input();
    EmuRunner* emu = app.activeEmu();

    const char* title = subMode_ == Sub::States
                            ? (saveMode_ ? "保存即时存档" : "读取即时存档")
                            : subMode_ == Sub::Speed ? "快进速度"
                                                     : "切换磁盘";
    // 行集
    struct Row {
      std::string label;
      std::string value;
      int slot;  // -1 = 返回
    };
    std::vector<Row> rows;
    rows.push_back({"‹ 返回菜单", "", -1});
    if (subMode_ == Sub::States) {
      const auto existing = emu ? emu->slots(app) : std::vector<int>{};
      for (int slot = 0; slot <= 10; slot++) {
        const std::string name =
            slot == 0 ? "槽位 A（自动）" : ("槽位 " + std::to_string(slot));
        const bool has =
            std::find(existing.begin(), existing.end(), slot) != existing.end();
        rows.push_back({name, has ? "● 有档" : "— 空", slot});
      }
    } else if (subMode_ == Sub::Speed) {
      static const int kSpeeds[] = {2, 4, 6, 8, 16};
      for (int sp : kSpeeds) {
        std::string label = std::to_string(sp) + " 倍速";
        if (sp == 6) label += "（默认）";
        if (sp == 16) label += "（极速）";
        rows.push_back({label, ffLevel_ == sp ? "当前" : "", sp});
      }
    } else if (subMode_ == Sub::Disk) {
      if (emu && emu->core()) {
        DiskControl& dc = emu->core()->diskControl();
        const unsigned n = dc.numImages();
        for (unsigned i = 0; i < n; i++)
          rows.push_back({dc.imageLabel(i), "", (int)i});
      }
    }
    const int R = (int)rows.size();
    if (subIndex_ >= R) subIndex_ = R - 1;
    if (subIndex_ < 0) subIndex_ = 0;

    // 面板几何（居中；行多时滚动）
    const float pad = 14, rowH = 42, rowGap = 4, headH = 46;
    const float pw = std::min(340.0f, (float)d.W - 20.0f);
    const float ph = std::min(headH + 8 + (float)R * (rowH + rowGap),
                              (float)d.H - 120.0f);
    const Rect panel((float)d.W / 2 - pw / 2, (float)d.H / 2 - ph / 2 - 20, pw, ph);
    subDialogRc_ = panel;
    d.roundRect(panel, 16, Color8::hex(0x1E2A3A, 0xDD));
    d.roundRectOutline(panel, 16, Color8(255, 255, 255, 34), 1.2f);

    app.font.draw(app.sdlRenderer(), title, panel.x + pad, panel.y + 13, 16,
                  sk.textHi, true);
    d.line(panel.x + pad, panel.y + headH - 8, panel.right() - pad,
           panel.y + headH - 8, Color8(255, 255, 255, 51), 1);

    const Rect rowsRc(panel.x + 8, panel.y + headH, pw - 16, ph - headH - 8);
    menuScroll_ += w::dragScroll(app, rowsRc);
    if (rowsRc.contains(in.mouse)) menuScroll_ -= (float)in.wheel * (rowH + rowGap);
    const float contentH = (float)R * (rowH + rowGap);
    const float maxScroll = contentH > rowsRc.h ? contentH - rowsRc.h : 0.0f;
    if (followMenu_) {
      followMenu_ = false;
      const float rowY = (float)subIndex_ * (rowH + rowGap) - menuScroll_;
      if (rowY < 0) menuScroll_ += rowY;
      if (rowY > rowsRc.h - rowH) menuScroll_ += rowY - (rowsRc.h - rowH);
    }
    menuScroll_ = std::max(0.0f, std::min(menuScroll_, maxScroll));
    d.pushClip(rowsRc);
    float y = rowsRc.y - menuScroll_;
    for (int i = 0; i < R; i++) {
      const Rect rc(panel.x + 8, y, pw - 16, rowH);
      y += rowH + rowGap;
      const bool focused = subIndex_ == i;
      if (w::listRow(app, rc, rows[(size_t)i].label, rows[(size_t)i].value, focused)) {
        const int slot = rows[(size_t)i].slot;
        if (slot < 0) {
          subMode_ = Sub::Main;
          subIndex_ = 0;
          menuScroll_ = 0;
        } else if (subMode_ == Sub::States) {
          const std::string name =
              slot == 0 ? "槽位 A（自动）" : ("槽位 " + std::to_string(slot));
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
          subIndex_ = 0;
          menuScroll_ = 0;
        } else if (subMode_ == Sub::Speed) {
          ffLevel_ = slot;
          lastFfLevel_ = slot;
          app.toast(std::string("快进 ×") + std::to_string(slot));
          subMode_ = Sub::Main;
          subIndex_ = 0;
          menuScroll_ = 0;
        } else if (subMode_ == Sub::Disk) {
          if (emu && emu->core()) {
            DiskControl& dc = emu->core()->diskControl();
            if (dc.select((unsigned)slot))
              app.toast("已切换：" + dc.imageLabel((unsigned)slot));
          }
          subMode_ = Sub::Main;
          subIndex_ = 0;
          menuScroll_ = 0;
        }
      }
    }
    d.popClip();
  }


  void exitGame(App& app) {
    EmuRunner* emu = app.activeEmu();
    if (emu) {
      emu->stop(app, app.config.autoSaveState);
    }
    app.popScene();
  }

 private:
  enum class Sub { Main, States, Speed, Disk };
  bool menuOpen_ = false;
  bool justOpened_ = false;  // ★v1.2.10 呼出帧的同一次按键不触发关闭
  bool wasFullscreen_ = true;   // ★v1.2.6 进入游戏前的全屏状态（自动全屏恢复用）
  Sub subMode_ = Sub::Main;
  int menuIndex_ = 0;
  int subIndex_ = 0;
  bool saveMode_ = true;
  bool paused_ = false;       // F7 / 菜单暂停（对齐 Android 暂停浮层）
  int ffLevel_ = 0;           // 快进档位 0/2/4/6/8/16（0 = 关；APK onToggleFastForward）
  int lastFfLevel_ = 6;       // 上次非零档位（APK lastNonZeroFFSpeed，默认 6）
  int emu_ffMult_ = 2;        // 最近一次生效的快进倍数（HUD 显示用）
  bool f5Prev_ = false, f6Prev_ = false, f7Prev_ = false, f8Prev_ = false;
  bool f11Prev_ = false;
  bool f12Prev_ = false;
  // ★v1.2.11 FramePacer 状态（墙钟截止时间；替代旧 dt 累加器 accum_）
  double nextFrameAt_ = 0;   // 下一模拟帧的绝对时刻（steady_clock 秒）
  bool paceInit_ = false;
  uint64_t emuFrames_ = 0;   // 模拟帧计数（FPS HUD 用，对齐 APK onFrame）
  float menuScroll_ = 0;   // ★v1.2.10 菜单行滚动（小窗口下长列表）
  Rect subDialogRc_{0, 0, 0, 0};  // ★v1.3.5 子弹窗矩形（点外关闭判定用）
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
