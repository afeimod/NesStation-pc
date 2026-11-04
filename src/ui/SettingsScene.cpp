// NesStation PC — 设置：外观 / 核心 / 按键 / 路径 / 模拟器 / BIOS / 数据
#include <algorithm>
#include <cmath>

#if defined(__has_include)
#if __has_include(<SDL_misc.h>)
#include <SDL_misc.h>
#define NS_HAVE_SDL_MISC 1
#endif
#if __has_include(<SDL_version.h>)
#include <SDL_version.h>
#endif
#endif
#if defined(SDL_MAJOR_VERSION) && SDL_VERSION_ATLEAST(2, 0, 14)
#define NS_HAVE_SDL_OPENURL 1
#endif

#include "ui/Scenes.h"
#include "ui/Widgets.h"
#include "util/Fs.h"

namespace ns {
namespace {

// 打开浏览器（对齐 Android Desktop.browse；SDL 2.0.14+ 的 SDL_OpenURL）
void openUrl(const std::string& url) {
#if defined(NS_HAVE_SDL_OPENURL)
  SDL_OpenURL(url.c_str());
#elif defined(_WIN32)
  std::string cmd = "start \"\" \"" + url + "\"";
  std::system(cmd.c_str());
#else
  std::string cmd = "xdg-open '" + url + "'";
  std::system(cmd.c_str());
#endif
}

// 简易圆角矩形填充（行卡片用）
void d_roundRect(App& app, const Rect& rc, float r, Color8 c) {
  app.draw.roundRect(rc, r, c);
}

class SettingsScene : public Scene {
 public:
  explicit SettingsScene(int initialTab = -1) {
    if (initialTab >= 0 && initialTab < kCats) {
      catSel_ = initialTab;
      focusArea_ = 1;
    }
  }

  void onEnter(App& app) override { (void)app; }

  void update(App& app, float dt, const InputState& in) override {
    (void)dt;
    if (in.pressed[(int)Btn::Back]) {
      app.popScene();
      return;
    }
    // 内容区滚动：滚轮始终可用（鼠标直接交互）；键盘上下需内容区焦点
    {
      const float viewH = (float)app.draw.H - 150 - 110;
      const float maxScroll = contentH_ > viewH ? contentH_ - viewH : 0;
      if (maxScroll > 0) {
        if (in.wheel != 0) scroll_ -= (float)in.wheel * 56.0f;
        if (focusArea_ == 1) {
          if (in.pressed[(int)Btn::Up]) scroll_ -= 48;
          if (in.pressed[(int)Btn::Down]) scroll_ += 48;
        }
      }
      if (scroll_ < 0) scroll_ = 0;
      if (scroll_ > maxScroll) scroll_ = maxScroll;
    }
    // 左侧分类
    if (focusArea_ == 0) {
      if (in.pressed[(int)Btn::Down]) catSel_ = (catSel_ + 1) % kCats;
      if (in.pressed[(int)Btn::Up]) catSel_ = (catSel_ + kCats - 1) % kCats;
      if (in.pressed[(int)Btn::Right] || in.pressed[(int)Btn::Ok]) focusArea_ = 1;
    } else if (in.pressed[(int)Btn::Left]) {
      focusArea_ = 0;
    }
    // 切换分类时回到顶部
    if (lastCat_ != catSel_) {
      lastCat_ = catSel_;
      scroll_ = 0;
    }
  }

  void saveConfig(App& app) { app.config.save(fsutil::join(app.dataDir(), "config.json")); }

  void draw(App& app) override {
    const Skin& sk = Theme::skin();
    Draw& d = app.draw;
    for (int i = 0; i < 40; i++) {
      const float t = (float)i / 40;
      d.rect(Rect(0, app.draw.H * t, app.draw.W, app.draw.H / 40 + 1),
             sk.bg.lerp(sk.bgDeep, t));
    }
    if (w::header(app, "设置", "Settings", true)) {
      app.popScene();
      return;
    }

    const char* cats[kCats] = {"外观", "核心", "按键映射", "路径与扫描",
                               "独立模拟器", "BIOS / 系统", "性能", "数据"};
    float y = 120;
    for (int i = 0; i < kCats; i++) {
      const Rect rc(24, y, 220, 48);
      if (w::listRow(app, rc, cats[i], "", focusArea_ == 0 && catSel_ == i)) {
        catSel_ = i;
        focusArea_ = 1;
      }
      y += 54;
    }

    // 内容区（可滚动：滚轮 / 方向键；长页（核心 / 独立模拟器）不截断）
    const float viewTop = 110, viewH = (float)app.draw.H - 150 - 110;
    d.pushClip(Rect(270, viewTop, app.draw.W - 270 - 20, viewH));
    float cy = 120 - scroll_;
    switch (catSel_) {
      case 0: drawAppearance(app, cy); break;
      case 1: drawCores(app, cy); break;
      case 2: drawKeys(app, cy); break;
      case 3: drawPaths(app, cy); break;
      case 4: drawEmulators(app, cy); break;
      case 5: drawBios(app, cy); break;
      case 6: drawPerformance(app, cy); break;
      case 7: drawData(app, cy); break;
    }
    contentH_ = cy + scroll_ - 120;
    d.popClip();
    // 滚动条指示（内容超出时）
    if (contentH_ > viewH) {
      const Rect track(app.draw.W - 18, viewTop, 6, viewH);
      w::scrollIndicator(app, track, viewH / contentH_, scroll_ / (contentH_ - viewH));
    }

    w::keyHintBar(app, {{"A", "修改"}, {"←→", "切换选项"}, {"B / Esc", "返回"}});
  }

  // —— 外观 ——
  void drawAppearance(App& app, float& y) {
    {
      Rect rc(270, y, app.draw.W - 300, 52);
      std::vector<std::string> styles = {"Neon 3D 游戏站 (新UI)", "FSD 经典 (Xbox360 磁贴)"};
      int idx = Theme::isNeon() ? 0 : 1;
      if (w::cycleRow(app, rc, "主界面风格", styles, idx, focusArea_ == 1,
                      "选择后立即生效并持久化")) {
        Theme::setStyle(idx == 0 ? UiStyle::Neon : UiStyle::Fsd);
        app.config.uiStyle = idx;
        saveConfig(app);
        app.toast("已切换到：" + std::string(styles[(size_t)idx]));
      }
      y += 58;
    }
    {
      Rect rc(270, y, app.draw.W - 300, 52);
      if (w::toggleRow(app, rc, "全屏模式", app.config.fullscreen, focusArea_ == 1,
                       "快捷键 F11")) {
        app.toggleFullscreen();
        saveConfig(app);
      }
      y += 58;
    }
    {
      Rect rc(270, y, app.draw.W - 300, 52);
      std::vector<std::string> modes = {"保持比例", "拉伸填充", "整数缩放"};
      if (w::cycleRow(app, rc, "游戏画面缩放", modes, app.config.scalingMode,
                      focusArea_ == 1))
        saveConfig(app);
      y += 58;
    }
    {
      Rect rc(270, y, app.draw.W - 300, 52);
      std::vector<std::string> fx = {"无滤镜", "扫描线", "CRT 荫罩"};
      if (w::cycleRow(app, rc, "画面滤镜", fx, app.config.screenEffect, focusArea_ == 1))
        saveConfig(app);
      y += 58;
    }
    {
      Rect rc(270, y, app.draw.W - 300, 52);
      std::vector<std::string> dual = {"双屏", "仅上屏", "仅下屏"};
      if (w::cycleRow(app, rc, "双屏布局（NDS / 3DS）", dual, app.config.dualLayout,
                      focusArea_ == 1))
        saveConfig(app);
      y += 58;
    }
    {
      Rect rc(270, y, app.draw.W - 300, 52);
      if (w::toggleRow(app, rc, "平滑缩放", app.config.videoSmooth, focusArea_ == 1,
                       "像素风游戏建议关闭"))
        saveConfig(app);
      y += 58;
    }
    {
      Rect rc(270, y, app.draw.W - 300, 52);
      if (w::toggleRow(app, rc, "显示 FPS", app.config.showFps, focusArea_ == 1))
        saveConfig(app);
      y += 58;
    }
  }

  // —— 核心 ——
  void drawCores(App& app, float& y) {
    const Skin& sk = Theme::skin();
    Draw& d = app.draw;
    // 下载进度（后台线程）
    const auto dl = app.cores.coreDownloadProgress();
    if (dl.running) {
      const Rect barRc(270, y, app.draw.W - 300, 8);
      d.rect(barRc, sk.bgPanel);
      const float t = dl.total > 0 ? (float)dl.done / dl.total : 0;
      d.rect(Rect(barRc.x, barRc.y, std::max(8.0f, barRc.w * t), barRc.h), sk.accent);
      char buf[96];
      snprintf(buf, sizeof(buf), "正在下载核心 %d/%d：%s（libretro buildbot）",
               dl.done + 1, dl.total, dl.label.c_str());
      app.font.draw(app.sdlRenderer(), buf, 270, y + 14, 13, sk.accent);
      y += 44;
    } else if (dl.finished) {
      app.font.draw(
          app.sdlRenderer(),
          dl.hadFailure ? "核心下载完成（部分平台本架构无预编译包，已自动回退/可稍后重试）"
                        : "核心下载完成 ✓",
          270, y, 13, dl.hadFailure ? sk.textDim : sk.accent);
      y += 24;
    }

    // 一键下载全部缺失核心（对齐 APK CoreManagerScreen 的下载入口）
    {
      const Rect rc(270, y, app.draw.W - 300, 46);
      bool downloading = app.cores.coreDownloadRunning();
      if (w::primaryButton(app, rc,
                           downloading ? "下载中…" : "⇩ 一键下载全部缺失核心（官方 buildbot）",
                           20)) {
        if (downloading) {
          app.toast("正在下载，请稍候");
        } else {
          app.cores.clearDownloadFinishFlag();
          app.cores.startCoreDownload({});
          app.toast("开始下载全部缺失核心");
        }
      }
      y += 56;
    }

    // 说明
    app.font.draw(app.sdlRenderer(),
                  "libretro 核心位于：" + app.cores.coreDir(), 270, y, 13, sk.textDim);
    y += 26;
    if (app.cores.bundledCoreDir() != app.cores.coreDir()) {
      app.font.draw(app.sdlRenderer(),
                    "安装包内置核心（自动识别，可被上方目录同名核心覆盖）：" +
                        app.cores.bundledCoreDir(),
                    270, y, 13, sk.textDim);
      y += 26;
    }
    app.font.draw(app.sdlRenderer(),
                  "点击「✗ 缺失」的平台行可直接下载；也可运行 scripts/fetch_cores.sh(.ps1)",
                  270, y, 13, sk.textDim);
    y += 20;
    app.font.draw(app.sdlRenderer(),
                  "页面可滚动（滚轮 / ↑↓）：下方「外置核心」区块含 Ishiiruka（Dolphin 系）/ Cemu / "
                  "Eden 等 8 个独立模拟器，支持自动下载。",
                  270, y, 13, sk.textDim);
    y += 34;

    for (GamePlatform p : allPlatforms()) {
      const PlatformCore pc = platformCore(p);
      Rect rc(270, y, app.draw.W - 300, 46);
      std::string status;
      std::string title = std::string(platformShortName(p)) + " · " + pc.coreTitle;
      if (pc.experimental) title += "（实验性）";
      if (pc.kind == CoreKind::Libretro) {
        if (app.cores.libretroCoreAvailable(p)) {
          const PlatformCore inUse = app.cores.resolveCore(p);
          status = std::string(inUse.coreFile) == std::string(pc.coreFile)
                       ? "✓ 已安装"
                       : std::string("✓ 用替代 ") + inUse.coreTitle;
          // 同平台外置独立核心已就绪 → 补充提示（对齐 Android 双核心平台）
          if (app.externals.availableCount(p) > 0)
            status += " · 独立核心已就绪";
        } else if (app.externals.availableCount(p) > 0) {
          status = "✓ 独立核心已就绪";
        } else {
          status = app.cores.coreDownloadRunning() ? "… 下载中" : "✗ 缺失 · 点击下载";
        }
      } else {
        // Standalone 平台（Ishiiruka / Cemu / Eden）：外置独立核心
        const auto exts = app.externals.forPlatform(p);
        bool ready = app.externals.availableCount(p) > 0;
        if (ready) {
          status = "✓ 已就绪（独立核心）";
        } else if (!exts.empty() && !exts[0]->githubApi.empty()) {
          status = app.externals.downloadRunning() ? "… 下载中" : "✗ 未安装 · 点击下载";
        } else {
          status = "✗ 未安装 · 点击打开官网";
        }
      }
      if (w::listRow(app, rc, title, status, focusArea_ == 1, pc.biosNote)) {
        if (pc.kind == CoreKind::Libretro) {
          if (!app.cores.libretroCoreAvailable(p) &&
              app.externals.availableCount(p) == 0) {
            if (app.cores.coreDownloadRunning()) {
              app.toast("正在下载，请稍候");
            } else {
              app.cores.clearDownloadFinishFlag();
              app.cores.startCoreDownload({p});
              app.toast(std::string("开始下载 ") + platformShortName(p) + " 核心");
            }
          } else {
            app.pushScene(createCoreOptionsScene(p));
          }
        } else {
          // 独立核心：未安装 → 自动下载（GitHub）/ 官网；已就绪 → 跳转外置核心页
          const auto exts = app.externals.forPlatform(p);
          if (!app.externals.available(p) && !exts.empty()) {
            const ExternalCore* e = exts[0];
            if (!e->githubApi.empty()) {
              if (app.externals.downloadRunning()) {
                app.toast("正在下载，请稍候");
              } else {
                app.externals.startDownload(e->id);
                app.toast("开始自动下载 " + e->name);
              }
            } else {
              openUrl(e->downloadPage);
              app.toast("已打开 " + e->name + " 官网下载页");
            }
          } else {
            catSel_ = 4;  // 跳转「独立模拟器」页
            app.toast("已切换到「独立模拟器」页");
          }
        }
      }
      y += 52;
    }

    // ===== 外置核心（独立模拟器 · GitHub 官方发布）=====
    // 对齐 Android CoreManagerScreen 的双区块结构：进程内核心之后列外置核心，
    // 行内直达「自动下载 / 打开官网」；手动选择与移除在「独立模拟器」页
    y += 14;
    w::sectionLabel(app, 270, y, "外置核心（独立模拟器 · GitHub 官方发布）");
    y += 30;
    {
      const auto pr = app.externals.progress();
      if (pr.running || pr.finished) {
        app.font.draw(app.sdlRenderer(), pr.label, 270, y, 13, pr.ok ? sk.green : sk.accent);
        y += 22;
        if (pr.finished) {
          Rect rc(270, y, 220, 30);
          if (w::button(app, rc, "知道了", false, 12, false))
            app.externals.clearFinishFlag();
          y += 36;
        }
      }
    }
    for (const auto& e : app.externals.all()) {
      const bool ready = !e.foundPath.empty();
      std::string status;
      if (ready) {
        status = "✓ 已就绪";
      } else if (!e.githubApi.empty()) {
        status = app.externals.downloadRunning() ? "… 下载中" : "✗ 未安装 · 点击下载";
      } else {
        status = "✗ 未安装 · 点击打开官网";
      }
      const Rect rc(270, y, app.draw.W - 300, 46);
      if (w::listRow(app, rc, e.name + " · " + platformShortName(e.platform), status,
                     false, e.notes)) {
        if (!ready) {
          if (!e.githubApi.empty()) {
            if (app.externals.downloadRunning()) {
              app.toast("正在下载，请稍候");
            } else {
              app.externals.startDownload(e.id);
              app.toast("开始自动下载 " + e.name);
            }
          } else {
            openUrl(e.downloadPage);
            app.toast("已打开 " + e.name + " 下载页");
          }
        } else {
          catSel_ = 4;  // 已就绪 → 跳「独立模拟器」页查看路径 / 移除
          app.toast("已切换到「独立模拟器」页");
        }
      }
      y += 52;
    }
  }

  // —— 按键映射 ——
  void drawKeys(App& app, float& y) {
    const Skin& sk = Theme::skin();
    app.font.draw(app.sdlRenderer(),
                  "点击键位后按新键 / 手柄键绑定（Esc 取消）。默认与原项目 README 控制表一致。",
                  270, y, 13, sk.textDim);
    y += 30;

    for (int port = 0; port < 2; port++) {
      char portTitle[32];
      snprintf(portTitle, sizeof(portTitle), "端口 %d（%s）", port + 1,
               app.input.gamepadName(port).c_str());
      app.font.draw(app.sdlRenderer(), portTitle, 270, y, 16, sk.accent, true);
      y += 26;
      int col = 0;
      for (int b = 0; b < RP_COUNT; b++) {
        const float bx = 270 + (col % 4) * ((app.draw.W - 300) / 4);
        const float by = y + (col / 4) * 50;
        const Rect rc(bx, by, (app.draw.W - 310) / 4, 44);
        const KeyBind& kb = app.input.mapping().binds[port][b];
        std::string key;
        if (app.input.isCapturing() && app.input.capturePort() == port &&
            app.input.captureBtn() == b) {
          key = "按新键…";
        } else if (kb.key != SDLK_UNKNOWN) {
          key = sdlKeyName(kb.key);
        } else if (kb.padBtn != SDL_CONTROLLER_BUTTON_INVALID) {
          key = sdlPadButtonName(kb.padBtn);
        } else {
          key = "未绑定";
        }
        if (w::listRow(app, rc, retroPadName(b), key, focusArea_ == 1)) {
          app.input.captureBind(port, b);
        }
        col++;
      }
      y += 3 * 50 + 12;
    }
    {
      Rect rc(270, y, app.draw.W - 300, 46);
      if (w::listRow(app, rc, "恢复默认映射", "", focusArea_ == 1)) {
        app.input.resetMapping();
        app.config.padMapping = app.input.mapping();
        saveConfig(app);
        app.toast("已恢复默认按键映射");
      }
      y += 52;
    }
  }

  // —— 路径与扫描 ——
  void drawPaths(App& app, float& y) {
    const Skin& sk = Theme::skin();
    w::sectionLabel(app, 270, y, "ROM 目录（点击扫描，✗ 移除）");
    y += 28;
    if (app.library.romDirs().empty()) {
      app.font.draw(app.sdlRenderer(), "（还没有 ROM 目录 —— 点击下方添加）", 270, y, 14,
                    sk.textDim);
      y += 30;
    }
    for (const auto& dir : app.library.romDirs()) {
      Rect rc(270, y, app.draw.W - 340, 46);
      if (w::listRow(app, rc, fsutil::pathDisplay(dir), "重新扫描", focusArea_ == 1)) {
        const int n = app.library.scanDirectory(dir, GamePlatform::NES, false,
                                                app.config.scanRecursive);
        app.library.save();
        app.toast("扫描完成：新增 " + std::to_string(n) + " 个游戏");
      }
      Rect rm(app.draw.W - 60, y, 36, 46);
      if (w::button(app, rm, "✗", false, 15, false)) {
        app.library.removeRomDir(dir);
        app.library.save();
      }
      y += 52;
    }
    {
      Rect rc(270, y, app.draw.W - 300, 46);
      if (w::button(app, rc, "＋ 添加 ROM 目录", focusArea_ == 1, 16, true)) {
        BrowserRequest req;
        req.title = "选择 ROM 目录";
        req.mode = 0;
        req.startDir = fsutil::exeDir();
        req.onPick = [&app](const std::string& dir) {
          app.library.addRomDir(dir);
          const int n =
              app.library.scanDirectory(dir, GamePlatform::NES, false, app.config.scanRecursive);
          app.library.save();
          app.toast("已添加，扫描到 " + std::to_string(n) + " 个游戏");
        };
        app.pushScene(createFileBrowser(req));
      }
      y += 52;
    }
    {
      Rect rc(270, y, app.draw.W - 300, 46);
      if (w::toggleRow(app, rc, "扫描子目录（递归）", app.config.scanRecursive,
                       focusArea_ == 1))
        saveConfig(app);
      y += 52;
    }
  }

  // —— 独立模拟器（外置核心管理，对齐 Android CoreManagerScreen 外置核心区）——
  void drawEmulators(App& app, float& y) {
    const Skin& sk = Theme::skin();
    app.font.draw(app.sdlRenderer(),
                  "外置 PC 核心独立模拟器（GitHub 官方发布）",
                  270, y, 16, sk.accent, true);
    y += 26;
    app.font.draw(app.sdlRenderer(),
                  "自动检测安装位置；支持 GitHub Release 自动下载，也可手动选择可执行文件。",
                  270, y, 13, sk.textDim);
    y += 22;
    app.font.draw(app.sdlRenderer(),
                  "把模拟器放至应用目录 emulators/<名称>/ 也会被自动识别。",
                  270, y, 13, sk.textDim);
    y += 32;

    // 下载进度条（对齐 Android CoreManagerScreen 顶部进度）
    {
      const auto pr = app.externals.progress();
      if (pr.running || pr.finished) {
        app.font.draw(app.sdlRenderer(), pr.label, 270, y, 13,
                      pr.finished && !pr.ok ? (pr.ok ? sk.green : sk.gold) : sk.accent);
        y += 20;
        const Rect bar(270, y, app.draw.W - 300, 6);
        d_roundRect(app, bar, 3, sk.bgDeep);
        if (pr.running) {
          // 不确定进度动画（20% 宽滑块往返）
          static float t = 0;
          t += 0.02f;
          const float off = 0.5f + 0.5f * sinf(t);
          const Rect fill(bar.x + (bar.w - bar.w * 0.25f) * off, bar.y, bar.w * 0.25f,
                          bar.h);
          d_roundRect(app, fill, 3, sk.accent);
        } else {
          d_roundRect(app, bar, 3, pr.ok ? sk.green : sk.gold);
        }
        y += 16;
        if (pr.finished) {
          // 结束提示后自动清旗（下一帧不再显示）
          Rect rc(270, y, app.draw.W - 300, 34);
          if (w::button(app, rc, pr.ok ? "知道了" : "知道了（可手动选择路径）",
                        false, 13, false)) {
            app.externals.clearFinishFlag();
          }
          y += 40;
        }
      }
    }

    for (const auto& e : app.externals.all()) {
      const bool ready = !e.foundPath.empty();
      // 行卡片：名称 + 平台徽章 / 说明 / 已解析路径
      const Rect rc(270, y, app.draw.W - 300, 78);
      const bool hov = w::hovered(app, rc);
      d_roundRect(app, rc, 8, hov ? sk.bgPanelHi : sk.bgPanel);
      app.font.draw(app.sdlRenderer(), e.name, rc.x + 16, rc.y + 10, 16, sk.textHi, true);
      {
        const std::string plat = platformShortName(e.platform);
        const float pw = app.font.textWidth(plat, 12) + 16;
        const Rect brc(rc.x + 16 + app.font.textWidth(e.name, 16) + 10, rc.y + 12, pw, 20);
        d_roundRect(app, brc, 4, Color8(sk.accent.r, sk.accent.g, sk.accent.b, 40));
        app.font.draw(app.sdlRenderer(), plat,
                      brc.x + (brc.w - app.font.textWidth(plat, 12)) / 2, brc.y + 3, 12,
                      sk.accent);
      }
      app.font.draw(app.sdlRenderer(), e.notes, rc.x + 16, rc.y + 34, 12, sk.textDim);
      if (ready) {
        app.font.draw(app.sdlRenderer(), "→ " + e.foundPath, rc.x + 16, rc.y + 56, 11,
                      sk.green);
      } else if (!e.userPath.empty()) {
        app.font.draw(app.sdlRenderer(), "⚠ 配置的路径无效：" + e.userPath, rc.x + 16,
                      rc.y + 56, 11, sk.red);
      }

      // 右侧操作按钮（对齐 Android ExternalCoreRow：自动下载 / 官网 / 手动选择 / 移除）
      const float bw = 96, bh = 30;
      float bx = rc.right() - bw - 14;
      const Rect pickRc(bx, rc.y + 24, bw, bh);
      if (w::button(app, pickRc, "手动选择", false, 12, false)) {
        BrowserRequest req;
        req.title = "选择 " + e.name + " 可执行文件（exe / AppImage / jar）";
        req.mode = 1;
        req.startDir = fsutil::exeDir();
        std::string id = e.id;
        req.onPick = [&app, id](const std::string& exe) {
          app.externals.setUserPath(id, exe);
          app.config.emulatorPaths[id] = exe;
          app.config.save(fsutil::join(app.dataDir(), "config.json"));
          app.toast("已配置 " + id);
        };
        app.pushScene(createFileBrowser(req));
      }
      bx -= bw + 8;
      if (!e.githubApi.empty()) {
        // GitHub 自动下载（对齐 Android「自动下载」/ 已安装「已就绪」）
        if (ready) {
          const Rect st(bx, rc.y + 24, bw, bh);
          app.font.draw(app.sdlRenderer(), "已就绪",
                        st.x + (st.w - app.font.textWidth("已就绪", 13)) / 2, st.y + 7,
                        13, sk.green, true);
        } else {
          const Rect dl(bx, rc.y + 24, bw, bh);
          if (w::button(app, dl, app.externals.downloadRunning() ? "下载中…" : "自动下载",
                        false, 12, true)) {
            if (app.externals.downloadRunning()) {
              app.toast("正在下载，请稍候");
            } else {
              app.externals.startDownload(e.id);
              app.toast("开始自动下载 " + e.name);
            }
          }
        }
      } else {
        // 无自动下载 → 打开官网（对齐 Android OpenInNew）
        const Rect wp(bx, rc.y + 24, bw, bh);
        if (w::button(app, wp, "打开官网", false, 12, false)) {
          openUrl(e.downloadPage);
          app.toast("已打开 " + e.name + " 下载页");
        }
      }
      bx -= bw + 8;
      if (ready) {
        // 移除（对齐 Android Delete：清安装目录 + 用户路径）
        const Rect rm(bx, rc.y + 24, 64, bh);
        if (w::button(app, rm, "移除", false, 12, false)) {
          std::string id = e.id;
          app.externals.clearUserPath(id);
          app.config.emulatorPaths.erase(id);
          app.config.save(fsutil::join(app.dataDir(), "config.json"));
          app.toast("已移除 " + e.name);
        }
      }
      y += 86;
    }
    y += 6;
    app.font.draw(app.sdlRenderer(),
                  "说明：独立模拟器使用其原生 GPU 渲染与存档系统；启动参数已按各官方推荐配置。",
                  270, y, 12, sk.textDim);
  }

  // —— BIOS / 系统 ——
  void drawBios(App& app, float& y) {
    const Skin& sk = Theme::skin();
    app.font.draw(app.sdlRenderer(), "系统目录：" + app.cores.systemDir(), 270, y, 13,
                  sk.textDim);
    y += 30;
    static const struct {
      const char* name;
      const char* note;
    } biosList[] = {
        {"disksys.rom", "NES FDS 磁碟 BIOS（放 system/）"},
        {"neogeo.zip / pgm.zip", "FBNeo 街机 BIOS（放 system/fbneo/）"},
        {"bios_CD_E/J/U.zip", "Mega-CD BIOS（放 system/）"},
        {"syscard1/2/3.pce", "PCE-CD System Card（放 system/）"},
        {"dc_boot.bin / dc_flash.bin", "Dreamcast BIOS（放 system/dc/）"},
        {"psx BIOS（scph*.bin）", "PS1 可选 BIOS（放 system/pcsx_rearmed/）"},
        {"aes_keys.txt", "3DS 加密密钥（放 system/azahar/）"},
        {"Dolphin Sys 目录", "GC 字体/DSP ROM（可选，见 Dolphin 设置）"},
        {"prod.keys / 固件", "Switch（在 Eden 模拟器内配置）"},
    };
    for (const auto& b : biosList) {
      Rect rc(270, y, app.draw.W - 300, 44);
      w::listRow(app, rc, b.name, "", false, b.note);
      y += 50;
    }
    y += 10;
    {
      Rect rc(270, y, app.draw.W - 300, 46);
      if (w::button(app, rc, "打开系统目录（system/）", focusArea_ == 1, 16, false)) {
        BrowserRequest req;
        req.title = "系统目录";
        req.mode = 0;
        req.startDir = app.cores.systemDir();
        req.onPick = [](const std::string&) {};
        app.pushScene(createFileBrowser(req));
      }
      y += 52;
    }
    app.font.draw(app.sdlRenderer(),
                  "⚠ BIOS 文件含受版权保护的代码，请仅使用合法获取的文件，不得分发。",
                  270, y, 12, sk.red);
  }

  // —— 性能（对齐 Android 设置「性能」+「显示」分类）——
  void drawPerformance(App& app, float& y) {
    const Skin& sk = Theme::skin();
    // 快进倍速（0/2/4/6/8/16，对齐 Android SpinnerRow）
    {
      static const int vals[] = {0, 2, 4, 6, 8, 16};
      std::vector<std::string> opts = {"关", "2×", "4×", "6×", "8×", "16×"};
      int idx = 2;  // 默认 4×
      for (int i = 0; i < 6; i++)
        if (vals[i] == app.config.fastForwardSpeed) {
          idx = i;
          break;
        }
      Rect rc(270, y, app.draw.W - 300, 52);
      if (w::cycleRow(app, rc, "默认快进倍速", opts, idx, focusArea_ == 1,
                      "游戏内按住 Tab 快进；F6 循环档位")) {
        app.config.fastForwardSpeed = vals[idx];
        saveConfig(app);
        app.toast(std::string("快进倍速：") + opts[(size_t)idx]);
      }
      y += 58;
    }
    // 只读快捷键展示（对齐 Android「显示」分类：菜单呼出键 / 快进键）
    {
      Rect rc(270, y, app.draw.W - 300, 46);
      w::listRow(app, rc, "菜单呼出键", "Esc / F1 / 右键", false, "游戏内呼出菜单；菜单中返回");
      y += 52;
    }
    {
      Rect rc(270, y, app.draw.W - 300, 46);
      w::listRow(app, rc, "快进", "Tab 按住 / F6 换挡", false, "菜单中可点击换挡 0→2→4→6→8→16");
      y += 52;
    }
    {
      Rect rc(270, y, app.draw.W - 300, 46);
      w::listRow(app, rc, "快存 / 快读", "F5 / F8", false, "槽位 1；菜单内共 12 槽");
      y += 52;
    }
    {
      Rect rc(270, y, app.draw.W - 300, 46);
      w::listRow(app, rc, "暂停", "F7", false, "对齐 Android 暂停浮层");
      y += 52;
    }
    {
      Rect rc(270, y, app.draw.W - 300, 46);
      w::listRow(app, rc, "截图", "F12", false, "存至数据目录 shots/");
      y += 52;
    }
    app.font.draw(app.sdlRenderer(),
                  "提示：右键点击游戏卡可打开快捷菜单（启动 / 收藏 / 移除 / 核心选择）",
                  270, y + 6, 13, sk.textDim);
  }

  // —— 数据 ——
  void drawData(App& app, float& y) {
    {
      Rect rc(270, y, app.draw.W - 300, 50);
      if (w::toggleRow(app, rc, "退出游戏时自动存档（槽位 A）", app.config.autoSaveState,
                       focusArea_ == 1))
        saveConfig(app);
      y += 56;
    }
    {
      Rect rc(270, y, app.draw.W - 300, 50);
      if (w::toggleRow(app, rc, "静音", app.config.audioMuted, focusArea_ == 1)) {
        app.audio.setMuted(app.config.audioMuted);
        saveConfig(app);
      }
      y += 56;
    }
    {
      Rect rc(270, y, app.draw.W - 300, 50);
      if (w::listRow(app, rc, "为全部游戏重新获取封面", "联网", focusArea_ == 1,
                     "thumbnails.libretro.com 官方缩略图库")) {
        app.covers.requestFetchAllMissing(app.library.games());
        app.toast("已排队获取封面（后台进行）");
      }
      y += 56;
    }
    const Skin& sk = Theme::skin();
    app.font.draw(app.sdlRenderer(), "数据目录：" + app.dataDir(), 270, y, 13, sk.textDim);
    y += 22;
    app.font.draw(app.sdlRenderer(),
                  "存档 / 快速存档 / 核心选项 / 封面缓存均在数据目录下。", 270, y, 13,
                  sk.textDim);
    y += 22;
    app.font.draw(app.sdlRenderer(), "日志：" + fsutil::join(app.dataDir(), "nesstation.log"),
                  270, y, 13, sk.textDim);
  }

 private:
  static constexpr int kCats = 8;
  int catSel_ = 0;
  int focusArea_ = 0;
  int lastCat_ = -1;        // 检测分类切换 → 回顶
  float scroll_ = 0;        // 内容区滚动偏移
  float contentH_ = 0;      // 上一帧内容高度（滚动范围）
};

}  // namespace

std::unique_ptr<Scene> createSettingsScene(int initialTab) {
  return std::unique_ptr<Scene>(new SettingsScene(initialTab));
}

}  // namespace ns
