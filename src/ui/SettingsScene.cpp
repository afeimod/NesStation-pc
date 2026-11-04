// NesStation PC — 设置：外观 / 核心 / 按键 / 路径 / 模拟器 / BIOS / 数据
#include <algorithm>

#include "ui/Scenes.h"
#include "ui/Widgets.h"
#include "util/Fs.h"

namespace ns {
namespace {

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
    // 左侧分类
    if (focusArea_ == 0) {
      if (in.pressed[(int)Btn::Down]) catSel_ = (catSel_ + 1) % kCats;
      if (in.pressed[(int)Btn::Up]) catSel_ = (catSel_ + kCats - 1) % kCats;
      if (in.pressed[(int)Btn::Right] || in.pressed[(int)Btn::Ok]) focusArea_ = 1;
    } else if (in.pressed[(int)Btn::Left]) {
      focusArea_ = 0;
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

    // 内容区
    d.pushClip(Rect(270, 110, app.draw.W - 270 - 20, app.draw.H - 150));
    float cy = 120;
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
    d.popClip();

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
    y += 34;

    for (GamePlatform p : allPlatforms()) {
      const PlatformCore pc = platformCore(p);
      Rect rc(270, y, app.draw.W - 300, 46);
      std::string status;
      if (pc.kind == CoreKind::Libretro) {
        if (app.cores.libretroCoreAvailable(p)) {
          const PlatformCore inUse = app.cores.resolveCore(p);
          status = std::string(inUse.coreFile) == std::string(pc.coreFile)
                       ? "✓ 已安装"
                       : std::string("✓ 用替代 ") + inUse.coreTitle;
        } else {
          status = app.cores.coreDownloadRunning() ? "… 下载中" : "✗ 缺失 · 点击下载";
        }
      } else {
        status = app.externals.available(p) ? "✓ 已探测" : "✗ 未找到";
      }
      if (w::listRow(app, rc, std::string(platformShortName(p)) + " · " + pc.coreTitle,
                     status, focusArea_ == 1, pc.biosNote)) {
        if (pc.kind == CoreKind::Libretro) {
          if (!app.cores.libretroCoreAvailable(p)) {
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
          app.toast("请在「独立模拟器」页配置 " + std::string(pc.coreTitle));
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

  // —— 独立模拟器 ——
  void drawEmulators(App& app, float& y) {
    const Skin& sk = Theme::skin();
    app.font.draw(app.sdlRenderer(),
                  "NGC/Wii（Dolphin）、Wii U（Cemu）、Switch（Eden）以独立模拟器形态接入。",
                  270, y, 13, sk.textDim);
    y += 24;
    app.font.draw(app.sdlRenderer(),
                  "推荐：把模拟器放至应用目录 emulators/<名称>/，或在此配置安装路径。",
                  270, y, 13, sk.textDim);
    y += 34;

    for (auto& e : app.externals.all()) {
      Rect rc(270, y, app.draw.W - 300, 50);
      const std::string status =
          e.foundPath.empty() ? "✗ 未找到" : "✓ " + e.foundPath;
      if (w::listRow(app, rc, e.title, status, focusArea_ == 1)) {
        BrowserRequest req;
        req.title = "选择 " + e.title + " 可执行文件";
        req.mode = 1;
        req.startDir = fsutil::exeDir();
        std::string key = e.key;
        req.onPick = [&app, key](const std::string& exe) {
          app.externals.setUserPath(key, exe);
          app.config.emulatorPaths[key] = exe;
          app.config.save(fsutil::join(app.dataDir(), "config.json"));
          app.toast("已配置 " + key);
        };
        app.pushScene(createFileBrowser(req));
      }
      y += 56;
    }
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
};

}  // namespace

std::unique_ptr<Scene> createSettingsScene(int initialTab) {
  return std::unique_ptr<Scene>(new SettingsScene(initialTab));
}

}  // namespace ns
