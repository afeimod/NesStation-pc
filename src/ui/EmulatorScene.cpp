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
    fastForwardPrev_ = false;
    app_ = &app;
  }
  void onExit(App& app) override {
    (void)app;
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

    // —— 帧驱动（按核心标称频率步进；快进 ×2）——
    if (!menuOpen_) {
      const double step = 1.0 / (emu->coreFps() > 1.0 ? emu->coreFps() : 60.0);
      accum_ += (double)dt;
      if (fastForward_) accum_ += (double)dt;  // 2 倍速
      int guard = 0;
      while (accum_ >= step && guard < 4) {
        emu->runFrameOnce();
        accum_ -= step;
        guard++;
      }
      if (accum_ > step * 8) accum_ = 0;  // 防积累发散
    }

    // —— 快捷键 ——
    if (!menuOpen_) {
      if (in.pressed[(int)Btn::Menu] || in.pressed[(int)Btn::Back]) {
        menuOpen_ = true;
        menuIndex_ = 0;
        subMode_ = Sub::Main;
        SDL_ShowCursor(SDL_ENABLE);
      }
      // Tab 快进按住 / F6 切换
      const Uint8* kbd = SDL_GetKeyboardState(nullptr);
      const bool tabHeld = kbd[SDL_SCANCODE_TAB] != 0;
      if (tabHeld != fastForwardPrev_) {
        fastForward_ = tabHeld;
        emu->setFastForward(fastForward_ || ffToggle_);
        fastForwardPrev_ = tabHeld;
      }
      const bool f6 = kbd[SDL_SCANCODE_F6] != 0;
      if (f6 && !f6Prev_) {
        ffToggle_ = !ffToggle_;
        fastForward_ = ffToggle_;
        emu->setFastForward(ffToggle_);
      }
      f6Prev_ = f6;
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
    }

    if (menuOpen_) updateMenu(app, in);
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
    if (app_) {
      if (EmuRunner* emu = app_->activeEmu()) {
        ffToggle_ = false;
        fastForward_ = false;
        fastForwardPrev_ = false;
        emu->setFastForward(false);
      }
    }
  }

  int menuItemCount(App& app) const {
    int n = 6;  // 继续/存档/读档/重置/画面设置/退出
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
    app.draw.rect(Rect(0, 0, app.draw.W, app.draw.H), sk.bgDeep);
    Rect gameArea(0, 0, (float)app.draw.W, (float)app.draw.H);
    app.video.draw(app.sdlRenderer(), gameArea, (ScalingMode)app.config.scalingMode,
                   (ScreenEffect)app.config.screenEffect, (DualLayout)app.config.dualLayout,
                   app.config.videoSmooth ? 1.0f : 0.0f);

    // FPS
    if (app.config.showFps) {
      fpsAccum_++;
      const Uint32 now = SDL_GetTicks();
      if (now - fpsLast_ > 500) {
        fps_ = fpsAccum_ * 1000.0f / (now - fpsLast_);
        fpsAccum_ = 0;
        fpsLast_ = now;
      }
      char buf[32];
      snprintf(buf, sizeof(buf), "FPS %.1f", fps_);
      app.font.draw(app.sdlRenderer(), buf, 12, 10, 14, sk.accent);
    }

    // 快进指示
    if (emu->fastForward()) {
      app.font.draw(app.sdlRenderer(), "快进 ×2", app.draw.W - 80, 10, 14, sk.gold, true);
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
        if (row("", "即时存档", idx++)) {
          subMode_ = Sub::States;
          subIndex_ = 0;
          saveMode_ = true;
        }
        if (row("", "即时读档", idx++)) {
          subMode_ = Sub::States;
          subIndex_ = 0;
          saveMode_ = false;
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
        app.font.draw(app.sdlRenderer(), saveMode_ ? "存档到槽位" : "读取槽位",
                      panel.x + pad, panel.y + 90, 18, sk.accent, true);
        y = panel.y + 130;
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
        app.font.draw(app.sdlRenderer(), "画面设置", panel.x + pad, panel.y + 90, 18,
                      sk.accent, true);
        y = panel.y + 130;
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
        app.font.draw(app.sdlRenderer(), "切换磁盘", panel.x + pad, panel.y + 90, 18,
                      sk.accent, true);
        y = panel.y + 130;
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

    w::hintBar(app, {
                        {"A", "确认"},
                        {"B", "返回"},
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
  Sub subMode_ = Sub::Main;
  int menuIndex_ = 0;
  int subIndex_ = 0;
  bool saveMode_ = true;
  bool fastForwardPrev_ = false;
  bool fastForward_ = false;
  bool ffToggle_ = false;
  bool f5Prev_ = false, f6Prev_ = false, f8Prev_ = false, f11Prev_ = false;
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
