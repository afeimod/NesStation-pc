// NesStation PC — 模拟器场景：游戏画面 + 游戏内菜单（主题化）
// ★v1.2.8：画面缩放/滤镜对齐 APK（EmulatorScreen SettingsPanel：
//   画面缩放 6 档 / 视频滤镜 11 档，游戏内改动写平台键）；自定义比例的
//   四角拖动编辑器 1:1 复刻 ui/swf/ScreenPositionEditor.kt。
#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

#include "emu/EmuRunner.h"
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

    // —— 帧驱动（按核心标称频率步进；快进 ×N = 时间累加倍速）——
    // activeMult：Tab 按住用设置倍速，否则用菜单档位 ffLevel_（0 = 关）
    const int cfgMult = app.config.fastForwardSpeed > 1 ? app.config.fastForwardSpeed : 2;
    const Uint8* kbd = SDL_GetKeyboardState(nullptr);
    const bool tabHeld = kbd[SDL_SCANCODE_TAB] != 0;
    const int activeMult = (tabHeld ? cfgMult : ffLevel_);
    if (activeMult > 1) emu_ffMult_ = activeMult;  // HUD 显示实际生效倍数
    {
      const int speed = (paused_ || menuOpen_) ? 0 : (activeMult > 1 ? activeMult : 1);
      if (speed > 0) {
        const double step = 1.0 / (emu->coreFps() > 1.0 ? emu->coreFps() : 60.0);
        accum_ += (double)dt * speed;
        int guard = 0;
        while (accum_ >= step && guard < 8) {
          emu->runFrameOnce();
          accum_ -= step;
          guard++;
        }
        if (accum_ > step * 8) accum_ = 0;  // 防积累发散
      } else {
        accum_ = 0;
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

    if (menuOpen_) updateMenu(app, in);
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
        if (in.pressed[(int)Btn::Up]) menuIndex_ = (menuIndex_ + N - 1) % N;
        if (in.pressed[(int)Btn::Down]) menuIndex_ = (menuIndex_ + 1) % N;
        if (in.pressed[(int)Btn::Back] || in.pressed[(int)Btn::Menu]) closeMenu();
        break;
      }
      case Sub::States:
      case Sub::Video:
      case Sub::Disk: {
        const int N = subItemCount(app);
        if (N > 0) {
          if (in.pressed[(int)Btn::Up]) subIndex_ = (subIndex_ + N - 1) % N;
          if (in.pressed[(int)Btn::Down]) subIndex_ = (subIndex_ + 1) % N;
        }
        if (in.pressed[(int)Btn::Back] || in.pressed[(int)Btn::Menu]) {
          subMode_ = Sub::Main;
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
    // 继续/暂停/重置/存档/读档/快进/截图/画面设置/退出（对齐 Android 游戏内菜单）
    int n = 9;
    EmuRunner* emu = app.activeEmu();
    if (emu && emu->core() && emu->core()->diskControl().available() &&
        emu->core()->diskControl().numImages() > 1)
      n++;
    if (emu && emu->core() && !emu->core()->options().empty()) n++;
    return n;
  }

  int subItemCount(App& app) const {
    if (subMode_ == Sub::States) return 12;  // 槽位 0-10
    if (subMode_ == Sub::Video) return videoRowCount(app);
    if (subMode_ == Sub::Disk) {
      EmuRunner* emu = app.activeEmu();
      if (emu && emu->core()) return (int)emu->core()->diskControl().numImages();
    }
    return 0;
  }

  // 画面设置子页行数（APK SettingsPanel：画面缩放/视频滤镜 + PC 增强行）
  int videoRowCount(App& app) const {
    EmuRunner* emu = app.activeEmu();
    int n = 5;  // 画面缩放 / 视频滤镜 / 整数缩放 / 平滑缩放 / 显示 FPS
    if (emu && platformIsDualScreen(emu->game().platform)) n++;  // 双屏布局
    n++;  // 全屏切换
    return n;
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
    if (app.config.showFps) {
      fpsAccum_++;
      const Uint32 now = SDL_GetTicks();
      if (now - fpsLast_ > 500) {
        fps_ = fpsAccum_ * 1000.0f / (now - fpsLast_);
        fpsAccum_ = 0;
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

  void drawMenu(App& app) {
    const Skin& sk = Theme::skin();
    // 暗化背景
    app.draw.rect(Rect(0, 0, app.draw.W, app.draw.H), Color8(0, 0, 0, 140));

    EmuRunner* emu = app.activeEmu();
    const float panelW = 460;
    const float panelH = 560;
    Rect panel(app.draw.W / 2 - panelW / 2, app.draw.H / 2 - panelH / 2, panelW, panelH);
    // 点击面板外 = 关闭菜单
    {
      const InputState& in = app.input.input();
      if (in.mousePressed && !panel.contains(in.mouse)) {
        closeMenu();
        return;
      }
    }
    skinPanel(app.draw, panel, true, 14);

    const float pad = 24;
    app.font.draw(app.sdlRenderer(), "游戏菜单", panel.x + pad, panel.y + 20, 22, sk.textHi,
                  true);
    app.font.draw(app.sdlRenderer(), emu ? emu->game().displayTitle() : "", panel.x + pad,
                  panel.y + 52, 13, sk.textDim);
    app.draw.line(panel.x + pad, panel.y + 74, panel.right() - pad, panel.y + 74, sk.line, 1);

    const InputState& in = app.input.input();
    float y = panel.y + 90;
    const float rowH = 46;

    auto row = [&](const std::string& icon, const std::string& title, int idx,
                   const std::string& value = "") {
      if (subMode_ != Sub::Main) return false;
      const Rect rc(panel.x + pad * 0.5f, y, panelW - pad, rowH);
      const bool focused = menuIndex_ == idx;
      const bool act = w::listRow(app, rc, title, value, focused);
      if (focused && (in.pressed[(int)Btn::Up] || in.pressed[(int)Btn::Down])) {
        // 焦点行不响应鼠标 hover 抢占
      }
      y += rowH + 6;
      return act;
    };

    switch (subMode_) {
      case Sub::Main: {
        int idx = 0;
        if (row("", "继续游戏", idx++)) closeMenu();
        if (row("", paused_ ? "继续（取消暂停）" : "暂停游戏", idx++, "F7")) {
          paused_ = !paused_;
          closeMenu();
        }
        if (row("", "重置", idx++)) {
          if (emu) emu->resetCore();
          app.toast("已重置");
          closeMenu();
        }
        if (row("", "即时存档", idx++, "F5")) {
          subMode_ = Sub::States;
          subIndex_ = 0;
          saveMode_ = true;
        }
        if (row("", "即时读档", idx++, "F8")) {
          subMode_ = Sub::States;
          subIndex_ = 0;
          saveMode_ = false;
        }
        if (row("", ffLevel_ > 1 ? "快进（点击换挡）" : "快进（关）", idx++,
                ffLevel_ > 1 ? "×" + std::to_string(ffLevel_) : "Tab")) {
          cycleFfLevel(app);
        }
        if (row("", "截图", idx++, "F12")) {
          closeMenu();
          takeScreenshot(app);
        }
        if (emu && emu->core() && emu->core()->diskControl().available() &&
            emu->core()->diskControl().numImages() > 1) {
          if (row("", "切换磁盘", idx++, "")) {
            subMode_ = Sub::Disk;
            subIndex_ = 0;
          }
        }
        if (row("", "画面设置", idx++)) {
          subMode_ = Sub::Video;
          subIndex_ = 0;
        }
        if (emu && emu->core() && !emu->core()->options().empty()) {
          if (row("", "核心设置", idx++)) {
            app.pushScene(createCoreOptionsScene(emu->game().platform));
            menuOpen_ = false;
          }
        }
        if (row("", "退出游戏", idx++, "自动存档")) {
          exitGame(app);
          return;
        }
        break;
      }
      case Sub::States: {
        // 子页顶部返回行（鼠标玩家可见的返回入口）
        {
          const Rect rc(panel.x + pad * 0.5f, panel.y + 88, panelW - pad, 36);
          if (w::listRow(app, rc, "‹ 返回主菜单", "", false)) subMode_ = Sub::Main;
        }
        app.font.draw(app.sdlRenderer(), saveMode_ ? "存档到槽位" : "读取槽位",
                      panel.x + pad, panel.y + 132, 18, sk.accent, true);
        y = panel.y + 168;
        const auto existing = emu ? emu->slots(app) : std::vector<int>{};
        for (int slot = 0; slot <= 10; slot++) {
          if (y > panel.bottom() - 60) break;
          const std::string name = slot == 0 ? "槽位 A（自动）" : ("槽位 " + std::to_string(slot));
          const bool has = std::find(existing.begin(), existing.end(), slot) != existing.end();
          const Rect rc(panel.x + pad * 0.5f, y, panelW - pad, 40);
          const bool focused = subIndex_ == slot;
          const std::string val = has ? "● 有档" : "—";
          if (w::listRow(app, rc, name, val, focused)) {
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
          y += 46;
        }
        break;
      }
      case Sub::Video: {
        {
          const Rect rc(panel.x + pad * 0.5f, panel.y + 88, panelW - pad, 36);
          if (w::listRow(app, rc, "‹ 返回主菜单", "", false)) subMode_ = Sub::Main;
        }
        app.font.draw(app.sdlRenderer(), "画面设置", panel.x + pad, panel.y + 132, 18,
                      sk.accent, true);
        y = panel.y + 168;
        const GamePlatform plat = emu ? emu->game().platform : GamePlatform::NES;
        // —— 画面缩放（APK SettingsPanel DropdownSetting「画面缩放」）——
        {
          const Rect rc(panel.x + pad * 0.5f, y, panelW - pad, 44);
          int idx = indexFor(videoScaleKeys(), platformVideoScale(app, plat));
          if (w::cycleRow(app, rc, "画面缩放", videoScaleOptions(), idx,
                          subIndex_ == 0)) {
            const std::string v = videoScaleKeys()[idx];
            setPlatformVideoScale(app, plat, v);
            if (v == "custom") enterLayoutEditor(app);  // APK：选 custom 直进编辑器
          }
          y += 50;
        }
        // —— 视频滤镜（APK SettingsPanel DropdownSetting「视频滤镜」11 档）——
        {
          const Rect rc(panel.x + pad * 0.5f, y, panelW - pad, 44);
          int idx = indexFor(videoFilterKeys(), platformVideoFilter(app, plat));
          if (w::cycleRow(app, rc, "视频滤镜", videoFilterOptions(), idx,
                          subIndex_ == 1)) {
            setPlatformVideoFilter(app, plat, videoFilterKeys()[idx]);
          }
          y += 50;
        }
        // —— 整数缩放（PC 增强：像素完美）——
        {
          const Rect rc(panel.x + pad * 0.5f, y, panelW - pad, 44);
          if (w::toggleRow(app, rc, "整数缩放（像素完美）", app.config.integerScale,
                           subIndex_ == 2))
            app.config.save(fsutil::join(app.dataDir(), "config.json"));
          y += 50;
        }
        {
          const Rect rc(panel.x + pad * 0.5f, y, panelW - pad, 44);
          if (w::toggleRow(app, rc, "平滑缩放", app.config.videoSmooth, subIndex_ == 3))
            app.config.save(fsutil::join(app.dataDir(), "config.json"));
          y += 50;
        }
        {
          const Rect rc(panel.x + pad * 0.5f, y, panelW - pad, 44);
          if (w::toggleRow(app, rc, "显示 FPS", app.config.showFps, subIndex_ == 4))
            app.config.save(fsutil::join(app.dataDir(), "config.json"));
          y += 50;
        }
        if (emu && platformIsDualScreen(emu->game().platform)) {
          const Rect rc(panel.x + pad * 0.5f, y, panelW - pad, 44);
          std::vector<std::string> opts = {"双屏", "仅上屏", "仅下屏"};
          if (w::cycleRow(app, rc, "双屏布局", opts, app.config.dualLayout,
                          subIndex_ == 5))
            app.config.save(fsutil::join(app.dataDir(), "config.json"));
          y += 50;
        }
        {
          const Rect rc(panel.x + pad * 0.5f, y, panelW - pad, 44);
          if (w::listRow(app, rc, "全屏切换", "F11", subIndex_ == 6)) app.toggleFullscreen();
          y += 50;
        }
        break;
      }
      case Sub::Disk: {
        {
          const Rect rc(panel.x + pad * 0.5f, panel.y + 88, panelW - pad, 36);
          if (w::listRow(app, rc, "‹ 返回主菜单", "", false)) subMode_ = Sub::Main;
        }
        app.font.draw(app.sdlRenderer(), "切换磁盘", panel.x + pad, panel.y + 132, 18,
                      sk.accent, true);
        y = panel.y + 168;
        if (emu && emu->core()) {
          DiskControl& dc = emu->core()->diskControl();
          const unsigned n = dc.numImages();
          for (unsigned i = 0; i < n && y < panel.bottom() - 60; i++) {
            const Rect rc(panel.x + pad * 0.5f, y, panelW - pad, 40);
            if (w::listRow(app, rc, dc.imageLabel(i), "", subIndex_ == (int)i)) {
              if (dc.select(i)) app.toast("已切换：" + dc.imageLabel(i));
              subMode_ = Sub::Main;
            }
            y += 46;
          }
        }
        break;
      }
    }

    w::keyHintBar(app, {
                          {"A / 点击", "确认"},
                          {"B / Esc / 右键", "返回"},
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
  double accum_ = 0;
  float fps_ = 0;
  int fpsAccum_ = 0;
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
