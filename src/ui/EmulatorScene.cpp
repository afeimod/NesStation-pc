// NesStation PC — 模拟器场景：游戏画面 + 游戏内菜单（主题化）
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
    // 鼠标移动时显示光标（已显示）
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
    if (subMode_ == Sub::Video) return 6;
    if (subMode_ == Sub::Disk) {
      EmuRunner* emu = app.activeEmu();
      if (emu && emu->core()) return (int)emu->core()->diskControl().numImages();
    }
    return 0;
  }

  void draw(App& app) override {
    EmuRunner* emu = app.activeEmu();
    if (!emu) return;

    // —— 游戏画面（帧已在 update 里步进）——
    const Skin& sk = Theme::skin();
    Draw& d = app.draw;
    app.draw.rect(Rect(0, 0, app.draw.W, app.draw.H), sk.bgDeep);
    Rect gameArea(0, 0, (float)app.draw.W, (float)app.draw.H);
    app.video.draw(app.sdlRenderer(), gameArea, (ScalingMode)app.config.scalingMode,
                   (ScreenEffect)app.config.screenEffect, (DualLayout)app.config.dualLayout,
                   app.config.videoSmooth ? 1.0f : 0.0f);

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
    if (paused_ && !menuOpen_) {
      d.rect(Rect(0, 0, app.draw.W, app.draw.H), Color8(0, 0, 0, 90));
      const char* msg = "已暂停 —— 按 Esc 呼出菜单，F7 继续";
      app.font.draw(app.sdlRenderer(), msg,
                    app.draw.W / 2 - app.font.textWidth(msg, 20, true) / 2,
                    app.draw.H / 2, 20, sk.textHi, true);
    }

    if (menuOpen_) drawMenu(app);
  }

  void drawMenu(App& app) {
    const Skin& sk = Theme::skin();
    // 暗化背景
    app.draw.rect(Rect(0, 0, app.draw.W, app.draw.H), Color8(0, 0, 0, 140));

    EmuRunner* emu = app.activeEmu();
    const float panelW = 420;
    const float panelH = 520;
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
        {
          const Rect rc(panel.x + pad * 0.5f, y, panelW - pad, 44);
          std::vector<std::string> opts = {"保持比例", "拉伸填充", "整数缩放"};
          if (w::cycleRow(app, rc, "缩放模式", opts, app.config.scalingMode,
                          subIndex_ == 0))
            app.config.save(fsutil::join(app.dataDir(), "config.json"));
          y += 50;
        }
        {
          const Rect rc(panel.x + pad * 0.5f, y, panelW - pad, 44);
          std::vector<std::string> opts = {"无滤镜", "扫描线", "CRT 荫罩"};
          if (w::cycleRow(app, rc, "画面滤镜", opts, app.config.screenEffect,
                          subIndex_ == 1))
            app.config.save(fsutil::join(app.dataDir(), "config.json"));
          y += 50;
        }
        {
          const Rect rc(panel.x + pad * 0.5f, y, panelW - pad, 44);
          if (w::toggleRow(app, rc, "平滑缩放", app.config.videoSmooth, subIndex_ == 2))
            app.config.save(fsutil::join(app.dataDir(), "config.json"));
          y += 50;
        }
        {
          const Rect rc(panel.x + pad * 0.5f, y, panelW - pad, 44);
          if (w::toggleRow(app, rc, "显示 FPS", app.config.showFps, subIndex_ == 3))
            app.config.save(fsutil::join(app.dataDir(), "config.json"));
          y += 50;
        }
        if (emu && platformIsDualScreen(emu->game().platform)) {
          const Rect rc(panel.x + pad * 0.5f, y, panelW - pad, 44);
          std::vector<std::string> opts = {"双屏", "仅上屏", "仅下屏"};
          if (w::cycleRow(app, rc, "双屏布局", opts, app.config.dualLayout,
                          subIndex_ == 4))
            app.config.save(fsutil::join(app.dataDir(), "config.json"));
          y += 50;
        }
        {
          const Rect rc(panel.x + pad * 0.5f, y, panelW - pad, 44);
          if (w::listRow(app, rc, "全屏切换", "F11", subIndex_ == 5)) app.toggleFullscreen();
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
};

}  // namespace

std::unique_ptr<Scene> createEmulatorScene() {
  return std::unique_ptr<Scene>(new EmulatorScene());
}

}  // namespace ns
