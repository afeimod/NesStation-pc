// NesStation PC — 核心选项浏览（通用 libretro 核心选项 UI，运行中热改）
// + 文件浏览器 + 关于页
#include <algorithm>

#include "emu/EmuRunner.h"
#include "ui/Scenes.h"
#include "ui/Widgets.h"
#include "util/Fs.h"

namespace ns {
namespace {

// ==================== 核心选项 ====================
class CoreOptionsScene : public Scene {
 public:
  explicit CoreOptionsScene(GamePlatform p) : platform_(p) {}

  void onEnter(App& app) override {
    // 运行中同核心 → 直接热改；否则临时装载核心读取选项
    // （按候选链解析后的实际核心，可能是替代核心）
    const PlatformCore resolved = app.cores.resolveCore(platform_);
    EmuRunner* emu = app.activeEmu();
    if (emu && emu->running() && emu->core() && emu->isLibretro() &&
        platformCore(emu->game().platform).kind == CoreKind::Libretro &&
        app.cores.resolveCore(emu->game().platform).coreFile == resolved.coreFile) {
      live_ = true;
    } else {
      live_ = false;
      temp_.reset(new LibretroCore(app.audio, app.video, app.input));
      App* appPtr = &app;
      const std::string coreKey = resolved.coreFile;
      auto resolver = [appPtr, coreKey](const std::string& k) {
        return appPtr->cores.resolveOption(coreKey, k);
      };
      const std::string path = app.cores.corePathFor(platform_);
      if (path.empty() || !fsutil::isFile(path)) {
        app.toast("核心未安装：" + app.cores.availabilityHint(platform_));
        app.popScene();
        return;
      }
      if (!temp_->load(path, app.cores.systemDir(), app.cores.savesDir(), resolver)) {
        app.toast("核心装载失败：" + temp_->lastError());
        app.popScene();
        return;
      }
    }
  }

  void onExit(App& app) override {
    if (temp_) {
      temp_->unload();
      temp_.reset();
    }
    (void)app;
  }

  LibretroCore* core(App& app) {
    if (live_) return app.activeEmu()->core();
    return temp_.get();
  }

  void update(App& app, float dt, const InputState& in) override {
    (void)dt;
    if (in.pressed[(int)Btn::Back]) {
      app.popScene();
      return;
    }
    LibretroCore* c = core(app);
    if (!c) {
      app.popScene();
      return;
    }
    const int N = (int)c->options().size();
    if (N > 0) {
      if (in.pressed[(int)Btn::Up]) sel_ = (sel_ + N - 1) % N;
      if (in.pressed[(int)Btn::Down]) sel_ = (sel_ + 1) % N;
    }
  }

  void applyOption(App& app, const CoreOption& o, int valueIdx) {
    LibretroCore* c = core(app);
    if (!c || o.values.empty()) return;
    const std::string val = o.values[(size_t)valueIdx];
    c->setOptionValue(o.key, val);
    app.cores.storeOption(app.cores.resolveCore(platform_).coreFile, o.key, val);
    if (live_) app.toast("已应用（部分核心需重开游戏生效）");
    else app.toast("已保存（下次启动游戏生效）");
  }

  void draw(App& app) override {
    const Skin& sk = Theme::skin();
    Draw& d = app.draw;
    for (int i = 0; i < 40; i++) {
      const float t = (float)i / 40;
      d.rect(Rect(0, app.draw.H * t, app.draw.W, app.draw.H / 40 + 1),
             sk.bg.lerp(sk.bgDeep, t));
    }
    const PlatformCore pc = app.cores.resolveCore(platform_);
    w::header(app, std::string("核心设置 · ") + pc.coreTitle,
              live_ ? "游戏运行中 · 热生效" : platformFullName(platform_));

    LibretroCore* c = core(app);
    if (!c) return;
    const auto& opts = c->options();

    d.pushClip(Rect(24, 110, app.draw.W - 48, app.draw.H - 150));
    float y = 116;
    if (opts.empty()) {
      app.font.draw(app.sdlRenderer(), "该核心没有可配置选项", 40, 140, 16, sk.textDim);
    }
    std::string lastCat;
    for (size_t i = 0; i < opts.size(); i++) {
      const CoreOption& o = opts[i];
      if (!o.visible) continue;
      if (!o.categoryKey.empty() && o.categoryKey != lastCat) {
        lastCat = o.categoryKey;
        w::sectionLabel(app, 40, y, lastCat);
        y += 28;
      }
      int cur = 0;
      for (size_t v = 0; v < o.values.size(); v++) {
        // 当前值查询（经 GET_VARIABLE 路径走 currentValues —— 这里读 CoreManager
        // 持久层 + 默认）
        const std::string stored =
            app.cores.resolveOption(app.cores.resolveCore(platform_).coreFile, o.key);
        if (!stored.empty() && stored == o.values[v]) {
          cur = (int)v;
          break;
        }
      }
      // 用 labels 显示
      std::vector<std::string> labels = o.labels;
      int idx = cur;
      const Rect rc(24, y, app.draw.W - 60, 50);
      const bool focused = sel_ == (int)i;
      if (w::cycleRow(app, rc, o.desc, labels, idx, focused, o.info)) {
        applyOption(app, o, idx);
      }
      y += 56;
      if (y > app.draw.H - 200) break;
    }
    d.popClip();

    w::hintBar(app, {{"←→", "切换值"}, {"A", "下一项"}, {"B", "返回"}});
  }

 private:
  GamePlatform platform_;
  bool live_ = false;
  std::unique_ptr<LibretroCore> temp_;
  int sel_ = 0;
};

// ==================== 文件浏览器 ====================
class FileBrowserScene : public Scene {
 public:
  explicit FileBrowserScene(const BrowserRequest& req) : req_(req) {}

  void onEnter(App& app) override {
    cur_ = req_.startDir.empty() || !fsutil::isDir(req_.startDir) ? fsutil::exeDir()
                                                                  : req_.startDir;
    (void)app;
  }

  void update(App& app, float dt, const InputState& in) override {
    (void)dt;
    if (in.pressed[(int)Btn::Back]) {
      if (inHoldingBack_) {
      }
      app.popScene();
      return;
    }
    entries_ = buildEntries(app);
    const int N = (int)entries_.size();
    if (N == 0) return;
    if (in.pressed[(int)Btn::Up]) sel_ = (sel_ + N - 1) % N;
    if (in.pressed[(int)Btn::Down]) sel_ = (sel_ + 1) % N;
    if (in.pressed[(int)Btn::Ok]) activate(app, entries_[(size_t)sel_]);
  }

  struct Entry {
    std::string name;
    std::string path;
    bool dir = false;
  };

  std::vector<Entry> buildEntries(App& app) {
    std::vector<Entry> out;
#ifdef _WIN32
    if (drivesMode_) {
      for (char c = 'C'; c <= 'Z'; c++) {
        const std::string root = std::string(1, c) + ":/";
        if (fsutil::isDir(root)) out.push_back({root, root, true});
      }
      return out;
    }
#endif
    if (fsutil::isDir(cur_)) {
      const std::string parent = fsutil::dirOf(cur_);
      if (parent != cur_ && !parent.empty()) {
        out.push_back({".. （上一级）", parent, true});
      }
#ifdef _WIN32
      out.push_back({"▣ 本电脑（所有磁盘）", "", true, });
#endif
      for (const auto& e : fsutil::listDir(cur_)) {
        if (!e.isDir) {
          // 选文件模式过滤
          if (req_.mode == 0) continue;
          if (req_.mode == 2) {
            const std::string ext = fsutil::extOf(e.name);
            if (ext != "dll" && ext != "so" && ext != "dylib") continue;
          }
        }
        out.push_back({e.name + (e.isDir ? "/" : ""), e.fullPath, e.isDir});
      }
    }
    return out;
  }

  void activate(App& app, const Entry& e) {
#ifdef _WIN32
    if (e.name == "▣ 本电脑（所有磁盘）") {
      drivesMode_ = true;
      sel_ = 0;
      return;
    }
    if (drivesMode_ && e.dir) {
      drivesMode_ = false;
      cur_ = e.path;
      sel_ = 0;
      return;
    }
#endif
    if (e.dir) {
      cur_ = e.path;
      drivesMode_ = false;
      sel_ = 0;
      return;
    }
    if (req_.mode == 0) return;  // 目录选择模式点文件无动作
    app.popScene();
    if (req_.onPick) req_.onPick(e.path);
  }

  void draw(App& app) override {
    const Skin& sk = Theme::skin();
    Draw& d = app.draw;
    d.rect(Rect(0, 0, app.draw.W, app.draw.H), sk.bgDeep);
    w::header(app, req_.title, fsutil::pathDisplay(drivesMode_ ? "本电脑" : cur_));

    // 目录模式下：选择当前目录按钮
    if (req_.mode == 0 && !drivesMode_) {
      const Rect pickRc(app.draw.W - 240, 30, 200, 46);
      if (w::button(app, pickRc, "选择此目录", true, 16, true)) {
        app.popScene();
        if (req_.onPick) req_.onPick(cur_);
        return;
      }
    }

    d.pushClip(Rect(24, 110, app.draw.W - 48, app.draw.H - 160));
    float y = 116;
    for (size_t i = 0; i < entries_.size(); i++) {
      const Rect rc(24, y, app.draw.W - 60, 42);
      if (w::listRow(app, rc, entries_[i].name, entries_[i].dir ? "目录" : "", sel_ == (int)i)) {
        activate(app, entries_[(size_t)i]);
      }
      y += 48;
      if (y > app.draw.H - 170) break;
    }
    d.popClip();

    w::hintBar(app, {{"A", "进入 / 选择"}, {"B", "返回"}});
  }

 private:
  BrowserRequest req_;
  std::string cur_;
  std::vector<Entry> entries_;
  int sel_ = 0;
  bool drivesMode_ = false;
  bool inHoldingBack_ = false;
};

// ==================== 模态提示框 ====================
// 用于核心缺失 / 启动失败等重要信息的醒目提示（区别于轻量 toast）
class MessageScene : public Scene {
 public:
  MessageScene(const std::string& title, const std::string& body,
               bool warn = true)
      : title_(title), body_(body), warn_(warn) {}

  void update(App& app, float dt, const InputState& in) override {
    (void)dt;
    if (in.pressed[(int)Btn::Ok] || in.pressed[(int)Btn::Back] ||
        in.pressed[(int)Btn::Menu]) {
      app.popScene();
    }
  }

  void draw(App& app) override {
    const Skin& sk = Theme::skin();
    Draw& d = app.draw;
    // 半透明遮罩（覆盖下层场景）
    d.rect(Rect(0, 0, app.draw.W, app.draw.H), Color8(0, 0, 0, 175));

    // 文本折行（标题手动，正文自动）
    std::vector<std::string> lines;
    {
      // 按 \n 拆段后逐段折行
      size_t pos = 0;
      while (pos <= body_.size()) {
        const size_t nl = body_.find('\n', pos);
        const std::string seg =
            body_.substr(pos, nl == std::string::npos ? std::string::npos
                                                      : nl - pos);
        if (seg.empty())
          lines.push_back("");
        else
          for (const auto& l : app.font.wrap(seg, 16, app.draw.W - 360))
            lines.push_back(l);
        if (nl == std::string::npos) break;
        pos = nl + 1;
      }
    }

    const float panelW = std::min(app.draw.W - 120.0f, 720.0f);
    const float bodyH = (float)lines.size() * 26 + 30;
    const float panelH = 150 + bodyH;
    const Rect panel(app.draw.W / 2 - panelW / 2, app.draw.H / 2 - panelH / 2,
                     panelW, panelH);
    d.roundRect(panel, 14, Color8(sk.bgPanel.r, sk.bgPanel.g, sk.bgPanel.b, 246));
    d.roundRectOutline(panel, 14, warn_ ? Color8(0xE7, 0x4C, 0x3C, 230) : sk.accent,
                       2);

    // 标题
    float y = panel.y + 34;
    app.font.draw(app.sdlRenderer(), title_,
                  panel.cx() - app.font.textWidth(title_, 24, true) / 2, y, 24,
                  warn_ ? Color8(0xFF, 0x88, 0x78) : sk.textHi, true);
    y += 46;
    d.rect(Rect(panel.cx() - 36, y, 72, 3), warn_ ? Color8(0xE7, 0x4C, 0x3C) : sk.accent);
    y += 26;

    // 正文
    for (const auto& ln : lines) {
      if (!ln.empty())
        app.font.draw(app.sdlRenderer(), ln,
                      panel.cx() - app.font.textWidth(ln, 16) / 2, y, 16, sk.text);
      y += 26;
    }

    // 确定按钮
    const Rect btn(panel.cx() - 90, panel.bottom() - 66, 180, 46);
    if (w::button(app, btn, "确定", true, 17, true)) app.popScene();

    w::hintBar(app, {{"A / B", "关闭"}});
  }

 private:
  std::string title_, body_;
  bool warn_ = true;
};

// ==================== 关于 ====================
class AboutScene : public Scene {
 public:
  void update(App& app, float dt, const InputState& in) override {
    (void)dt;
    if (in.pressed[(int)Btn::Back] || in.pressed[(int)Btn::Ok]) app.popScene();
  }

  void draw(App& app) override {
    const Skin& sk = Theme::skin();
    Draw& d = app.draw;
    for (int i = 0; i < 40; i++) {
      const float t = (float)i / 40;
      d.rect(Rect(0, app.draw.H * t, app.draw.W, app.draw.H / 40 + 1),
             sk.bg.lerp(sk.bgDeep, t));
    }
    w::header(app, "关于 NesStation PC");

    const float cx = app.draw.W / 2;
    app.font.draw(app.sdlRenderer(), "NESSTATION", cx - app.font.textWidth("NESSTATION", 52, true) / 2,
                  app.draw.H * 0.16f, 52, sk.textHi, true);
    d.rect(Rect(cx - 40, app.draw.H * 0.16f + 66, 80, 4), sk.accent);
    const std::string v = "PC EDITION · v1.0 · 多平台复古游戏站（Windows / Linux）";
    app.font.draw(app.sdlRenderer(), v, cx - app.font.textWidth(v, 16) / 2,
                  app.draw.H * 0.16f + 86, 16, sk.text);

    const char* lines[] = {
        "基于 Android 版 NesStation（GameBox）重写，主题界面双风格：Neon 3D / FSD 经典",
        "核心矩阵（全部为 PC 版）：",
        "  FCEUmm · snes9x · mGBA · Geargrafx · DOSBox-Pure · FBNeo · Genesis-Plus-GX",
        "  melonDS · PCSX-ReARMed · PCSX2 · SquirrelJME · Flycast · PPSSPP · Azahar",
        "  源码集成独立模拟器：Dolphin（NGC/Wii）· Cemu（Wii U）· Eden（Switch）",
        "已移除无 PC 版核心：DraStic（激烈）、Citra MMJ（Android 专属）",
        "应用代码 MIT · 各核心遵循其上游许可证 · 详见 assets/legal",
        "",
        "快捷键：F1/Esc 菜单 · F5 存档 · F8 读档 · F6/Tab 快进 · F11 全屏",
        "致谢：libretro / RetroArch 社区与全部上游模拟器项目",
    };
    float y = app.draw.H * 0.16f + 140;
    for (const char* ln : lines) {
      const std::string s = ln;
      app.font.draw(app.sdlRenderer(), s, cx - app.font.textWidth(s, 15) / 2, y, 15,
                    s.find("核心矩阵") != std::string::npos ? sk.accent : sk.text);
      y += 26;
    }
    w::hintBar(app, {{"A/B", "返回"}});
  }
};

}  // namespace

std::unique_ptr<Scene> createCoreOptionsScene(GamePlatform platform) {
  return std::unique_ptr<Scene>(new CoreOptionsScene(platform));
}

std::unique_ptr<Scene> createFileBrowser(const BrowserRequest& req) {
  return std::unique_ptr<Scene>(new FileBrowserScene(req));
}

std::unique_ptr<Scene> createAboutScene() {
  return std::unique_ptr<Scene>(new AboutScene());
}

std::unique_ptr<Scene> createMessageScene(const std::string& title,
                                          const std::string& body, bool warn) {
  return std::unique_ptr<Scene>(new MessageScene(title, body, warn));
}

}  // namespace ns
