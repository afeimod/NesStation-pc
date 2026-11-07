// NesStation PC — 核心选项浏览（通用 libretro 核心选项 UI，运行中热改）
// + 文件浏览器 + 关于页
#include <algorithm>

#include "emu/EmuRunner.h"
#include "ui/OptionZh.h"
#include "ui/Scenes.h"
#include "ui/Widgets.h"
#include "core/Platform.h"
#include "util/Fs.h"
#include "util/NativeDialogs.h"

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
      // ★焦点跟随仅在键盘/手柄移动选择时执行一次（每帧执行会把拖拽滚动
      //   强行拉回选中行 —— 滑动条/列表拉不动的根因）
      if (in.pressed[(int)Btn::Up]) { sel_ = (sel_ + N - 1) % N; followSel_ = true; }
      if (in.pressed[(int)Btn::Down]) { sel_ = (sel_ + 1) % N; followSel_ = true; }
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
    if (w::header(app, std::string("核心设置 · ") + pc.coreTitle,
                  live_ ? "游戏运行中 · 热生效" : platformFullName(platform_), true)) {
      app.popScene();
      return;
    }

    LibretroCore* c = core(app);
    if (!c) return;
    const auto& opts = c->options();

    // 滚轮 + ★拖拽滚动长列表
    const Rect listRc(24, 110, app.draw.W - 48, app.draw.H - 150);
    scroll_ += w::dragScroll(app, listRc);
    if (listRc.contains(app.input.input().mouse)) {
      scroll_ = std::max(0.0f, scroll_ - (float)app.input.input().wheel * 56.0f);
    }
    // 焦点行跟随滚动可见（仅键盘移动选择时执行一次，不与拖拽滚动打架）
    if (followSel_) {
      followSel_ = false;
      const float rowY = 116 + (float)sel_ * 56 - scroll_;
      if (rowY < listRc.y) scroll_ -= (listRc.y - rowY);
      if (rowY > listRc.bottom() - 56) scroll_ += (rowY - (listRc.bottom() - 56));
    }
    d.pushClip(listRc);
    float y = 116 - scroll_;
    if (opts.empty()) {
      app.font.draw(app.sdlRenderer(), "该核心没有可配置选项", 40, 140, 16, sk.textDim);
    }
    std::string lastCat;
    for (size_t i = 0; i < opts.size(); i++) {
      const CoreOption& o = opts[i];
      if (!o.visible) continue;
      if (!o.categoryKey.empty() && o.categoryKey != lastCat) {
        lastCat = o.categoryKey;
        w::sectionLabel(app, 40, y, zhOptionCategory(o.categoryKey));
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
      // ★ 显示名与取值标签中文化（未命中词条时回退英文原文）
      std::vector<std::string> labels;
      labels.reserve(o.labels.size());
      for (const auto& lb : o.labels) labels.push_back(zhOptionValue(lb));
      int idx = cur;
      const Rect rc(24, y, app.draw.W - 60, 50);
      const bool focused = sel_ == (int)i;
      if (w::cycleRow(app, rc, zhOptionDesc(o.desc), labels, idx, focused,
                      o.info)) {
        applyOption(app, o, idx);
      }
      y += 56;
    }
    d.popClip();
    // 滚动边界钳制 + 滚动条
    {
      const float total = 116 + (float)opts.size() * 56;
      const float viewH = listRc.h;
      const float maxScroll = std::max(0.0f, total - 116 - viewH);
      if (scroll_ > maxScroll) scroll_ = maxScroll;
      if (scroll_ < 0) scroll_ = 0;
      if (total > viewH) {
        // ★滚动条可直接拖拽：返回新位置回写 scroll
        scroll_ = w::scrollIndicator(app, Rect(app.draw.W - 12, listRc.y, 6, listRc.h),
                                     viewH / (total - 116),
                                     scroll_ / (total - 116 - viewH)) *
                  (total - 116 - viewH);
      }
    }

    w::keyHintBar(app, {{"←→", "切换值"}, {"A", "下一项"}, {"B / Esc", "返回"}});
  }

 private:
  GamePlatform platform_;
  bool live_ = false;
  std::unique_ptr<LibretroCore> temp_;
  int sel_ = 0;
  float scroll_ = 0;
  bool followSel_ = false;  // 键盘移动选择时置位 → 绘制期跟随一次
};

// ==================== 文件浏览器 ====================
// 逻辑对齐 Android 版 FileBrowserDialog（顶部路径栏 + 主目录/上一级 + 列表）。
// ★v1.2.2 布局重构（用户反馈：顶部一大串存储位置书签把真正目录挤到很下面）：
//   默认进入【当前目录】纯净视图（仅「..（上一级）」+ 子目录/文件列表），
//   书签（主目录/桌面/下载/磁盘/存储卷）收进右上「快速位置」按钮，按需展开。
// 快速位置视图顶部有「← 返回当前目录」项；无权限与空目录明确提示。
class FileBrowserScene : public Scene {
 public:
  explicit FileBrowserScene(const BrowserRequest& req) : req_(req) {}

  void onEnter(App& app) override {
    // 起始目录：请求指定 → 主目录（对齐 APK user.home）→ exe 目录
    if (!req_.startDir.empty() && fsutil::isDir(req_.startDir)) {
      cur_ = req_.startDir;
    } else {
      const std::string home = fsutil::homeDir();
      cur_ = !home.empty() ? home : fsutil::exeDir();
    }
    rebuild(app);
    (void)app;
  }

  void update(App& app, float dt, const InputState& in) override {
    (void)dt;
    if (in.pressed[(int)Btn::Back]) {
      app.popScene();
      return;
    }
    // 目录变化时才重建（不再每帧扫描磁盘）
    if (dirty_) rebuild(app);
    const int N = (int)entries_.size();
    if (N == 0) return;
    // 上下移动跳过分区标签行（★置位 followSel_：仅键盘移动时跟随滚动）
    if (in.pressed[(int)Btn::Up]) { sel_ = step(sel_, N - 1); followSel_ = true; }
    if (in.pressed[(int)Btn::Down]) { sel_ = step(sel_, 1); followSel_ = true; }
    if (in.pressed[(int)Btn::Ok]) activate(app, entries_[(size_t)sel_]);
  }

  struct Entry {
    std::string name;
    std::string path;
    bool dir = false;
    bool label = false;   // 分区标题（不可选中）
    bool isBookmark = false;
  };

  int step(int from, int delta) const {
    if (entries_.empty()) return 0;
    int i = from;
    for (int n = 0; n < (int)entries_.size(); n++) {
      i = (i + delta + (int)entries_.size()) % (int)entries_.size();
      if (!entries_[(size_t)i].label) return i;
    }
    return from;
  }

  void markDirty() { dirty_ = true; }

  void rebuild(App& app) {
    dirty_ = false;
    entries_.clear();
    // 转义修复："\x01drives" 在 MSVC/GCC 下 \x01d 会被当作十六进制贪婪解析
    // (0x01D + "rives")，拆分字面量确保语义为 0x01 + "drives"
    builtFor_ = drivesMode_ ? std::string("\x01" "drives")
                             : (quickMode_ ? cur_ + "\x01" "quick" : cur_);
#ifdef _WIN32
    if (drivesMode_) {
      entries_.push_back({"← 返回快速位置", "", false, false, false});
      entries_.push_back({"存储位置", "", false, true});
      const std::string home = fsutil::homeDir();
      if (!home.empty()) addBookmark("主目录", home);
      for (const auto& n : {"Desktop", "桌面"}) addBookmark("桌面", fsutil::join(home, n));
      for (const auto& n : {"Downloads", "下载"}) addBookmark("下载", fsutil::join(home, n));
      for (const auto& n : {"Documents", "文档"}) addBookmark("文档", fsutil::join(home, n));
      entries_.push_back({"磁盘", "", false, true});
      for (char c = 'A'; c <= 'Z'; c++) {
        const std::string root = std::string(1, c) + ":/";
        if (fsutil::isDir(root)) entries_.push_back({std::string("磁盘 ") + c + ":（" + root + "）", root, true});
      }
      return;
    }
#endif
    // —— 快速位置（书签收藏夹，按需展开；默认视图不显示）——
    if (quickMode_) {
      entries_.push_back({"← 返回当前目录", "", false, false, false});
      entries_.push_back({"存储位置", "", false, true});
      const std::string home = fsutil::homeDir();
      if (!home.empty()) addBookmark("主目录", home);
#ifdef _WIN32
      for (const auto& n : {"Desktop", "桌面"}) addBookmark("桌面", fsutil::join(home, n));
      for (const auto& n : {"Downloads", "下载"}) addBookmark("下载", fsutil::join(home, n));
      for (const auto& n : {"Documents", "文档"}) addBookmark("文档", fsutil::join(home, n));
      addBookmark("应用数据", app.dataDir());
      // arm64 Wine（Box86/Box64 等）环境：Z: 盘映射宿主根 —— 书签直达
      // Android 内部存储 / U 盘，解决「arm64 只能读 Z 盘（exeDir）」的入口问题
      if (fsutil::isDir("Z:/")) {
        addBookmark("内部存储（Android）", "Z:/storage/emulated/0");
        addBookmark("SD 卡 / U 盘（/storage）", "Z:/storage");
        addBookmark("可移动介质（/media）", "Z:/media");
        addBookmark("挂载点（/mnt）", "Z:/mnt");
        addBookmark("宿主文件系统（Z:/）", "Z:/");
      }
      entries_.push_back({"磁盘", "", false, true});
      entries_.push_back({"▣ 此电脑（所有磁盘）", "", true, false, true});
#else
      for (const auto& n : {"Desktop", "桌面"}) addBookmark("桌面", fsutil::join(home, n));
      for (const auto& n : {"Downloads", "下载"}) addBookmark("下载", fsutil::join(home, n));
      for (const auto& n : {"Documents", "文档"}) addBookmark("文档", fsutil::join(home, n));
      addBookmark("应用数据", app.dataDir());
      addBookmark("文件系统根 /", "/");
      for (const std::string& m : fsutil::storageMounts()) {
        std::string label = "存储卷 " + fsutil::baseOf(m);
        if (m == "/storage/emulated/0" || m == "/sdcard") label = "内部存储";
        else if (m == "/data") label = "应用数据（/data）";
        else if (m == "/storage") label = "全部存储（/storage）";
        addBookmark(label, m);
      }
#endif
      return;
    }

    // —— 当前目录（默认视图：直接展示，不再被书签挤到下方）——
    if (!fsutil::isDir(cur_)) {
      entries_.push_back({"（目录不存在：" + cur_ + "）", "", false, true});
      return;
    }
    if (!fsutil::canReadDir(cur_)) {
      entries_.push_back({"⛔ 无权限读取此目录 — 请点右上「快速位置」选择其他位置",
                          "", false, true});
      return;
    }
    entries_.push_back({"当前目录", "", false, true});
    const std::string parent = fsutil::dirOf(cur_);
    if (parent != cur_ && !parent.empty()) {
      entries_.push_back({".. （上一级）", parent, true});
    }
    const auto listing = fsutil::listDir(cur_);
    for (const auto& e : listing) {
      if (!e.isDir) {
        // 选文件模式过滤
        if (req_.mode == 0) continue;
        if (req_.mode == 2) {
          const std::string ext = fsutil::extOf(e.name);
          if (ext != "dll" && ext != "so" && ext != "dylib") continue;
        }
        // ★v1.2.6 模式 3：图片选择（自定义封面 / 主页背景）
        if (req_.mode == 3) {
          const std::string ext = fsutil::extOf(e.name);
          if (ext != "png" && ext != "jpg" && ext != "jpeg" && ext != "bmp" &&
              ext != "webp")
            continue;
        }
      }
      entries_.push_back({e.name + (e.isDir ? "/" : ""), e.fullPath, e.isDir});
    }
    if (listing.empty()) entries_.push_back({"（空目录）", "", false, true});
    if (sel_ >= (int)entries_.size()) sel_ = 0;
  }

  void addBookmark(const std::string& label, const std::string& path) {
    if (path.empty() || !fsutil::isDir(path)) return;
    for (const auto& e : entries_)
      if (!e.label && e.path == path) return;  // 去重（书签间重复）
    entries_.push_back({label, path, true, false, true});
  }

  void activate(App& app, const Entry& e) {
    if (e.label || e.path.empty()) return;
#ifdef _WIN32
    if (e.name == "▣ 此电脑（所有磁盘）") {
      drivesMode_ = true;
      sel_ = 0;
      scroll_ = 0;
      markDirty();
      return;
    }
    if (drivesMode_) {
      if (e.name.rfind("← 返回", 0) == 0) {
        drivesMode_ = false;
        sel_ = 0;
        scroll_ = 0;
        markDirty();
        return;
      }
      drivesMode_ = false;
      quickMode_ = false;
      cur_ = e.path;
      sel_ = 0;
      scroll_ = 0;
      markDirty();
      return;
    }
#endif
    // 快速位置视图的「← 返回当前目录」
    if (quickMode_ && e.name.rfind("← 返回", 0) == 0) {
      quickMode_ = false;
      sel_ = 0;
      scroll_ = 0;
      markDirty();
      return;
    }
    if (e.dir) {
      cur_ = e.path;
      quickMode_ = false;  // 从书签进入目录 → 回到纯净视图
      sel_ = 0;
      scroll_ = 0;
      markDirty();
      return;
    }
    if (req_.mode == 0) return;  // 目录选择模式点文件无动作
    // ★闪退修复：popScene() 会立即析构本场景（unique_ptr 弹栈），随后再访问
    //   成员 req_.onPick 即 use-after-free —— x64 偶尔侥幸、arm64 必崩。
    //   先把回调和参数拷到栈上再弹栈调用。
    auto onPick = req_.onPick;
    const std::string picked = e.path;
    app.popScene();
    if (onPick) onPick(picked);
  }

  void draw(App& app) override {
    const Skin& sk = Theme::skin();
    Draw& d = app.draw;
    d.rect(Rect(0, 0, app.draw.W, app.draw.H), sk.bgDeep);
    if (w::header(app, req_.title,
                  fsutil::pathDisplay(drivesMode_ ? "此电脑"
                                       : (quickMode_ ? "快速位置" : cur_)),
                  true)) {
      // 返回 = 取消（★先拷贝回调再弹栈 —— 避免 use-after-free）
      auto onCancel = req_.onCancel;
      app.popScene();
      if (onCancel) onCancel();
      return;
    }

    // ★v1.2.2：快速位置开关（书签收纳，默认纯净视图）
    if (!drivesMode_) {
      const Rect quickRc(app.draw.W - 240 - 216, 30, 200, 46);
      if (w::primaryButton(app, quickRc,
                           quickMode_ ? "← 当前目录" : "▣ 快速位置", 4)) {
        quickMode_ = !quickMode_;
        sel_ = 0;
        scroll_ = 0;
        markDirty();
        return;
      }
    }

    // 目录模式下：选择当前目录按钮（仅纯净视图）
    if (req_.mode == 0 && !drivesMode_ && !quickMode_ &&
        !cur_.empty() && fsutil::isDir(cur_)) {
      const Rect pickRc(app.draw.W - 240, 30, 200, 46);
      if (w::primaryButton(app, pickRc, "选择此目录", 4)) {
        // ★先拷贝回调与路径再弹栈 —— 「添加目录就闪退」的 use-after-free 修复
        auto onPick = req_.onPick;
        const std::string picked = cur_;
        app.popScene();
        if (onPick) onPick(picked);
        return;
      }
    }

    // 滚轮 + ★拖拽滚动
    const Rect listRc(24, 110, app.draw.W - 48, app.draw.H - 160);
    scroll_ += w::dragScroll(app, listRc);
    if (listRc.contains(app.input.input().mouse)) {
      scroll_ = std::max(0.0f, scroll_ - (float)app.input.input().wheel * 48.0f);
    }
    // ★焦点行跟随仅在键盘/手柄移动选择时执行一次 —— 旧实现每帧强制把
    //   scroll 拉回选中行，任何拖拽/滚轮立刻被弹回（滑动条拉不动的根因）
    if (followSel_) {
      followSel_ = false;
      const float rowY = 116 + (float)sel_ * 48 - scroll_;
      if (rowY < listRc.y) scroll_ -= (listRc.y - rowY);
      if (rowY > listRc.bottom() - 48) scroll_ += (rowY - (listRc.bottom() - 48));
    }
    d.pushClip(listRc);
    float y = 116 - scroll_;
    for (size_t i = 0; i < entries_.size(); i++) {
      const Entry& e = entries_[i];
      if (e.label) {
        w::sectionLabel(app, 40, y + 14, e.name);
        y += 48;
        continue;
      }
      const Rect rc(24, y, app.draw.W - 60, 42);
      const bool focused = sel_ == (int)i;
      std::string value;
      if (e.isBookmark || (e.dir && e.name.rfind("..", 0) == 0))
        value = fsutil::pathDisplay(e.path);
      else if (e.dir)
        value = "目录";
      if (w::listRow(app, rc, e.name, value, focused)) {
        activate(app, e);
      }
      y += 48;
    }
    d.popClip();
    // 滚动边界钳制 + 滚动条（拖拽/滚轮不越界）
    {
      const float total = 116 + (float)entries_.size() * 48;
      const float maxScroll = std::max(0.0f, total - 116 - listRc.h);
      if (scroll_ > maxScroll) scroll_ = maxScroll;
      if (scroll_ < 0) scroll_ = 0;
      if (total > listRc.h) {
        // ★滚动条可直接拖拽：返回新位置回写 scroll
        scroll_ = w::scrollIndicator(app, Rect(app.draw.W - 12, listRc.y, 6, listRc.h),
                                     listRc.h / (total - 116),
                                     scroll_ / (total - 116 - listRc.h)) *
                  (total - 116 - listRc.h);
      }
    }

    w::keyHintBar(app, {{"A / 双击", "进入 / 选择"}, {"B / Esc", "返回"}});
  }

 private:
  BrowserRequest req_;
  std::string cur_;
  std::string builtFor_;
  std::vector<Entry> entries_;
  int sel_ = 0;
  float scroll_ = 0;
  bool drivesMode_ = false;
  bool quickMode_ = false;   // ★v1.2.2：快速位置（书签）视图开关，默认纯净
  bool dirty_ = false;
  bool followSel_ = false;  // 键盘移动选择时置位 → 绘制期跟随一次
};

// ==================== 核心选择器（对齐 Android 版 CorePickerDialog）====================
// 触发条件（App::launchGame）：该平台已安装核心 > 1 且游戏未记忆选择。
// 布局对齐 Kotlin 版：标题「选择 {平台} 核心」+ 副文 + 选项列表（徽章：链首「推荐」
// 绿 / 其余「备选」金）+ 底部勾选 chip「✓ 记住此游戏的选择」（默认勾选）。
class CorePickerScene : public Scene {
 public:
  CorePickerScene(const GameEntry& game,
                  std::function<void(const std::string&)> onPick)
      : game_(game), onPick_(std::move(onPick)) {}

  // 选择项：libretro 进程内核心 / 外置独立核心（对齐 Android
  // EngineFactory.CoreChoice：libretro 在前、外置在后，id 规则 "<id>-ext"）
  enum Badge { BadgeNone = 0, BadgeRecommended, BadgeAlt, BadgeExperimental };
  struct Choice {
    std::string title;       // "PCSX2（进程内核心）" / "PCSX2（独立核心）"
    std::string note;        // 副文
    std::string pickValue;   // libretro: 核心文件名；外置: "<id>-ext"
    std::string fileName;    // 右下角小字：核心文件 / 官网域名
    bool isExternal = false;
    Badge badge = BadgeNone;  // 徽章（对齐 Android CorePickerDialog：
                             // 外置=推荐绿；双形态平台进程内=实验性金；
                             // 纯 libretro 平台链首=推荐，其余=备选）
  };

  void onEnter(App& app) override {
    choices_.clear();
    const auto libs = app.cores.installedCoresFor(game_.platform);
    const auto exts = app.externals.forPlatform(game_.platform);
    // 已就绪的外置核心（未安装的不列出，避免选后启动失败）
    std::vector<const ExternalCore*> extReady;
    for (const auto* c : exts)
      if (app.externals.available(c->id)) extReady.push_back(c);
    const bool hasExternal = !extReady.empty();
    for (size_t i = 0; i < libs.size(); i++) {
      const PlatformCore& c = libs[i];
      Choice ch;
      ch.title = std::string(c.coreTitle) + "（进程内核心）";
      ch.note = hasExternal
                    ? "进程内 libretro · 软件渲染 · NesStation 统一菜单/快存"
                    : c.biosNote;
      ch.pickValue = c.coreFile;
      ch.fileName = c.coreFile;
      ch.isExternal = false;
      ch.badge = hasExternal ? BadgeExperimental
                             : (i == 0 ? BadgeRecommended : BadgeAlt);
      choices_.push_back(std::move(ch));
    }
    // 仅列出已就绪的外置核心
    for (const auto* c : extReady) {
      Choice ch;
      ch.title = c->name + "（独立核心）";
      ch.note = "原生 PC 版 · GPU 全速 · 原生存档系统";
      ch.pickValue = c->id + "-ext";
      const size_t slash = c->downloadPage.find("//");
      ch.fileName = slash == std::string::npos
                        ? c->downloadPage
                        : c->downloadPage.substr(slash + 2);
      ch.isExternal = true;
      ch.badge = BadgeRecommended;  // 外置核心为推荐配置（对齐 Android）
      choices_.push_back(std::move(ch));
    }
  }

  void update(App& app, float dt, const InputState& in) override {
    (void)dt;
    if (in.pressed[(int)Btn::Back] || in.pressed[(int)Btn::Menu]) {
      app.popScene();
      return;
    }
    const int N = (int)choices_.size();
    if (N == 0) {
      app.popScene();
      return;
    }
    if (in.pressed[(int)Btn::Up]) sel_ = (sel_ + N - 1) % N;
    if (in.pressed[(int)Btn::Down]) sel_ = (sel_ + 1) % N;
    // F / Y 切换「记住选择」
    if (in.pressed[(int)Btn::Fav]) remember_ = !remember_;
    if (in.pressed[(int)Btn::Ok] && !hoverRow_) pick(app, sel_);
  }

  void pick(App& app, int idx) {
    if (idx < 0 || idx >= (int)choices_.size()) return;
    const std::string value = choices_[(size_t)idx].pickValue;
    if (remember_) {
      if (GameEntry* e = app.library.find(game_.id)) {
        e->preferredCore = value;
        app.library.save();
      }
    }
    auto cb = onPick_;
    app.popScene();
    if (cb) cb(value);
  }

  void draw(App& app) override {
    const Skin& sk = Theme::skin();
    Draw& d = app.draw;
    d.rect(Rect(0, 0, app.draw.W, app.draw.H), Color8(0, 0, 0, 160));
    // 点击面板外 = 取消
    {
      const InputState& in = app.input.input();
      if (in.mousePressed && !panel_.contains(in.mouse)) {
        app.popScene();
        return;
      }
    }

    const float pw = 620, ph = 380;
    panel_ = Rect(app.draw.W / 2 - pw / 2, app.draw.H / 2 - ph / 2, pw, ph);
    skinPanel(d, panel_, true, 16);

    float y = panel_.y + 26;
    app.font.draw(app.sdlRenderer(),
                  std::string("选择 ") + platformShortName(game_.platform) + " 核心",
                  panel_.x + 28, y, 22, sk.textHi, true);
    y += 34;
    // 副文：对齐 Android CorePickerDialog 双核心平台文案；纯 libretro 平台用候选链文案
    {
      const bool hasExternal =
          !choices_.empty() && choices_.back().isExternal;
      app.font.draw(app.sdlRenderer(),
                    hasExternal
                        ? "该平台有两个 PC 核心可用 —— 外置核心为推荐配置（原生 GPU 全速），libretro 核心进程内运行（软件渲染，兼容实验性）。"
                        : "该平台有多个已安装核心可用 —— 首选为核心链推荐，备选为候选核心。",
                    panel_.x + 28, y, 13, sk.textDim);
    }
    y += 14;
    d.line(panel_.x + 24, y, panel_.right() - 24, y, sk.line, 1);
    y += 16;

    const InputState& in = app.input.input();
    hoverRow_ = false;
    const size_t maxRows = choices_.size() < 4 ? choices_.size() : 4;
    for (size_t i = 0; i < maxRows; i++) {
      const Choice& c = choices_[i];
      const Rect rc(panel_.x + 20, y, pw - 40, 64);
      const bool focused = sel_ == (int)i;
      // 点击（按下沿）选中；hover 仅做视觉（无按下）
      if (rc.contains(in.mouse) && in.mousePressed) {
        pick(app, (int)i);
        return;
      }
      const bool hov = rc.contains(in.mouse);
      if (focused || hov)
        d.roundRect(rc, 8, sk.accent.withA(hov ? 52 : 30));
      if (hov) {
        sel_ = (int)i;
        hoverRow_ = true;
      }
      // 标题 + 徽章（推荐绿 / 备选金 / 实验性金）
      app.font.draw(app.sdlRenderer(), c.title, rc.x + 16, rc.y + 10, 18,
                    sk.textHi, true);
      if (c.badge != BadgeNone) {
        const std::string badge =
            c.badge == BadgeRecommended
                ? "推荐"
                : (c.badge == BadgeExperimental ? "实验性" : "备选");
        const Color8 bc = c.badge == BadgeRecommended ? sk.green : sk.gold;
        const float bw = app.font.textWidth(badge, 13) + 20;
        const Rect brc(rc.right() - bw - 12, rc.y + 10, bw, 24);
        d.roundRect(brc, 5, bc.withA(48));
        app.font.draw(app.sdlRenderer(), badge,
                      brc.x + (brc.w - app.font.textWidth(badge, 13)) / 2,
                      brc.y + 5, 13, bc, true);
      }
      // 副文 + 右下角小字（核心文件 / 官网域名）
      app.font.draw(app.sdlRenderer(), c.note, rc.x + 16, rc.y + 38, 12, sk.textDim);
      app.font.draw(app.sdlRenderer(), c.fileName,
                    rc.right() - 16 - app.font.textWidth(c.fileName, 12),
                    rc.y + 38, 12, sk.textDim);
      y += 72;
    }

    // 记住选择 chip（对齐 Kotlin「✓ 记住此游戏的选择」）
    {
      const Rect rc(panel_.x + 28, panel_.bottom() - 74, 260, 44);
      const bool hov = rc.contains(in.mouse);
      if (hov && in.mousePressed) {
        remember_ = !remember_;
        app.toast(remember_ ? "已选择：记住此游戏的核心选择" : "本次不记忆核心选择");
      } else if (in.pressed[(int)Btn::Fav]) {
        remember_ = !remember_;
      }
      d.roundRect(rc, sk.panelRadius, hov ? sk.bgPanelHi : sk.bgPanel);
      d.roundRectOutline(rc, sk.panelRadius, remember_ ? sk.accent : sk.line, 1.4f);
      w::icon(app, 2, Rect(rc.x + 12, rc.y + 12, 20, 20),
              remember_ ? sk.gold : sk.textDim);
      app.font.draw(app.sdlRenderer(), remember_ ? "记住此游戏的选择" : "不记住选择",
                    rc.x + 42, rc.y + 14, 14, remember_ ? sk.textHi : sk.text);
    }
    // 取消按钮
    {
      const Rect rc(panel_.right() - 120, panel_.bottom() - 74, 92, 44);
      if (w::button(app, rc, "取消", false, 15)) app.popScene();
    }

    w::keyHintBar(app, {{"A / 点击", "用此核心启动"}, {"F", "记住选择"}, {"B / Esc", "取消"}});
  }

 private:
  GameEntry game_;
  std::function<void(const std::string&)> onPick_;
  std::vector<Choice> choices_;
  Rect panel_;
  int sel_ = 0;
  bool remember_ = true;   // 对齐 Android 版默认勾选
  bool hoverRow_ = false;
};

// ==================== 模态提示框 ====================
// 用于核心缺失 / 启动失败等重要信息的醒目提示（区别于轻量 toast）
class MessageScene : public Scene {
 public:
  MessageScene(const std::string& title, const std::string& body, bool warn,
               std::string okLabel, std::function<void()> onOk)
      : title_(title), body_(body), warn_(warn), okLabel_(std::move(okLabel)),
        onOk_(std::move(onOk)) {}

  void update(App& app, float dt, const InputState& in) override {
    (void)dt;
    if (in.pressed[(int)Btn::Ok] || in.pressed[(int)Btn::Back] ||
        in.pressed[(int)Btn::Menu]) {
      confirm(app);
    }
  }

  void confirm(App& app) {
    app.popScene();
    if (onOk_) onOk_();
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
    const Rect btn(panel.cx() - 100, panel.bottom() - 66, 200, 46);
    if (w::primaryButton(app, btn, okLabel_)) confirm(app);

    w::keyHintBar(app, {{"A / Esc", "关闭"}});
  }

 private:
  std::string title_, body_, okLabel_;
  bool warn_ = true;
  std::function<void()> onOk_;
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
    if (w::header(app, "关于 NesStation PC", "", true)) {
      app.popScene();
      return;
    }

    const float cx = app.draw.W / 2;
    app.font.draw(app.sdlRenderer(), "NESSTATION", cx - app.font.textWidth("NESSTATION", 52, true) / 2,
                  app.draw.H * 0.16f, 52, sk.textHi, true);
    d.rect(Rect(cx - 40, app.draw.H * 0.16f + 66, 80, 4), sk.accent);
    const std::string v = std::string("PC EDITION · ") + appVersionText() +
                          " · 多平台复古游戏站（Windows / Linux）";
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
        "快捷键：F1/Esc/右键 菜单 · F5 存档 · F8 读档 · F7 暂停",
        "　　　　Tab/F6 快进（0→2→4→6→8→16）· F11 全屏 · F12 截图",
        "　　　　右键点击游戏卡 → 快捷菜单（启动 / 收藏 / 移除 / 核心选择）",
        "致谢：libretro / RetroArch 社区与全部上游模拟器项目",
    };
    float y = app.draw.H * 0.16f + 140;
    for (const char* ln : lines) {
      const std::string s = ln;
      app.font.draw(app.sdlRenderer(), s, cx - app.font.textWidth(s, 15) / 2, y, 15,
                    s.find("核心矩阵") != std::string::npos ? sk.accent : sk.text);
      y += 26;
    }
    w::keyHintBar(app, {{"A / B / Esc", "返回"}});
  }
};

}  // namespace

std::unique_ptr<Scene> createCoreOptionsScene(GamePlatform platform) {
  return std::unique_ptr<Scene>(new CoreOptionsScene(platform));
}

std::unique_ptr<Scene> createCorePickerScene(const GameEntry& game,
                                             std::function<void(const std::string&)> onPick) {
  return std::unique_ptr<Scene>(new CorePickerScene(game, std::move(onPick)));
}

std::unique_ptr<Scene> createFileBrowser(const BrowserRequest& req) {
  return std::unique_ptr<Scene>(new FileBrowserScene(req));
}

// ★v1.3.0 统一浏览入口：系统原生对话框优先（Windows IFileDialog /
//   Linux zenity、kdialog），不可用时回退内置浏览器。
//   用户反馈两点根治：① Windows 只有 C 盘入口（原生对话框自带全盘符 +
//   快速访问）；② 中文文件名口口乱码（原生对话框系统渲染，返回 UTF-8）。
void browsePath(App& app, const BrowserRequest& req) {
  std::string filter;
  switch (req.mode) {
    case 1: {  // ROM 文件（主白名单拼成通配符）
      std::string pats;
      for (const auto& e : romExtensionsMaster()) {
        if (!pats.empty()) pats += ";";
        pats += "*." + e;
      }
      filter = "ROM 文件|" + pats + "|所有文件|*.*";
      break;
    }
    case 2:
      filter = "核心库 (dll/so/dylib)|*.dll;*.so;*.dylib";
      break;
    case 3:
      filter = "图片 (png/jpg/jpeg/bmp/webp)|*.png;*.jpg;*.jpeg;*.bmp;*.webp";
      break;
    default:
      break;
  }
  std::string path;
  const nativedlg::PickResult r =
      req.mode == 0
          ? nativedlg::pickFolder(app.window(), req.title.c_str(), &path)
          : nativedlg::pickFile(app.window(), req.title.c_str(),
                                filter.empty() ? nullptr : filter.c_str(), &path);
  if (r == nativedlg::PickResult::Ok) {
    if (req.onPick) req.onPick(path);
    return;
  }
  if (r == nativedlg::PickResult::Cancel) {
    if (req.onCancel) req.onCancel();
    return;
  }
  // Unavailable（无桌面环境/无 zenity、kdialog）→ 内置浏览器兑底
  app.pushScene(createFileBrowser(req));
}

std::unique_ptr<Scene> createAboutScene() {
  return std::unique_ptr<Scene>(new AboutScene());
}

std::unique_ptr<Scene> createMessageScene(const std::string& title,
                                          const std::string& body, bool warn,
                                          const std::string& okLabel,
                                          std::function<void()> onOk) {
  return std::unique_ptr<Scene>(
      new MessageScene(title, body, warn, okLabel, std::move(onOk)));
}

}  // namespace ns
