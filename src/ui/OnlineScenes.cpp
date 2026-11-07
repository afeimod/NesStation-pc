// NesStation PC — 在线游戏 / 对战平台 / SWF 三大场景（★v1.3.1）
//
// 1:1 复刻 APK 三大在线入口的信息架构（OnlineGamesScreen / BattleScreen /
// SwfListScreen），PC 环境适配：
//   · 在线游戏：完整站点列表（APK 内置 12 站照搬 + 自定义增删改），
//     「开始游戏」= 系统默认浏览器打开（PC 无内嵌 WebView，浏览器即等价物）
//   · 对战平台：同一套 HTTP API（健康检查 / 注册登录 / 房间列表 / 对战游戏
//     库 ROM 下载入库）；联机对局需 Android 版网络同步引擎，PC 端提供
//     浏览 + 下载 + 明确说明，绝不假装能开桌
//   · SWF：扫描用户目录的 .swf 列表；优先本地 ruffle 播放器，未安装时
//     打开 Ruffle 官方演示页（把 .swf 拖入网页即玩，与 APK Ruffle 注入同源）
#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <nlohmann/json.hpp>
#include <SDL.h>

#include "app/App.h"
#include "ui/Scenes.h"
#include "ui/Widgets.h"
#include "util/Fs.h"
#include "util/Log.h"

// popen/pclose 的 MSVC 名（CI Windows 构建）
#ifdef _WIN32
#define popen _popen
#define pclose _pclose
#endif

namespace ns {
namespace {

using nlohmann::json;

// —— 通用背景（与 CoreOptionsScene 同款纵向渐变，视觉一致）——
void sceneBackground(App& app) {
  const Skin& sk = Theme::skin();
  Draw& d = app.draw;
  for (int i = 0; i < 40; i++) {
    const float t = (float)i / 40;
    d.rect(Rect(0, app.draw.H * t, app.draw.W, app.draw.H / 40 + 1),
           sk.bg.lerp(sk.bgDeep, t));
  }
}

// ==================== 模态文本输入（标题/网址/账号等单字段） ====================
class TextInputScene : public Scene {
 public:
  TextInputScene(std::string title, std::string label, std::string initial,
                 std::function<void(const std::string&)> onOk)
      : title_(std::move(title)), label_(std::move(label)), buf_(std::move(initial)),
        onOk_(std::move(onOk)) {}

  void onEnter(App& app) override {
    app.input.textClear();
    app.input.textBufferMut() = buf_;
    app.input.textInputStart();
  }
  void onExit(App& app) override { app.input.textInputStop(); }

  void update(App& app, float dt, const InputState& in) override {
    (void)dt;
    buf_ = app.input.textBuffer();
    if (in.pressed[(int)Btn::Back]) {
      app.popScene();
      return;
    }
  }

  void draw(App& app) override {
    const Skin& sk = Theme::skin();
    Draw& d = app.draw;
    d.rect(Rect(0, 0, d.W, d.H), Color8(0, 0, 0, 175));  // 模态遮罩

    const float pw = std::min(560.0f, d.W - 80.0f);
    const float ph = 208;
    const Rect panel(d.W / 2 - pw / 2, d.H / 2 - ph / 2, pw, ph);
    d.chamferRect(panel, 14, sk.bgPanel);
    d.chamferRectOutline(panel, 14, sk.line, 1.2f);

    app.font.draw(app.sdlRenderer(), title_, panel.x + 22, panel.y + 18, 17,
                  sk.textHi, true);
    app.font.draw(app.sdlRenderer(), label_, panel.x + 22, panel.y + 46, 12, sk.textDim);

    // 输入框（占位/光标假象同 LibraryScene 搜索框）
    const Rect box(panel.x + 22, panel.y + 68, pw - 44, 44);
    d.chamferRect(box, 10, sk.bgPanelHi);
    d.chamferRectOutline(box, 10, sk.accent, 1.2f);
    const std::string shown = buf_.empty() ? std::string("|") : buf_;
    app.font.drawVCentered(app.sdlRenderer(),
                           app.font.ellipsize(shown, 15, (int)box.w - 24), box.x + 12,
                           box.cy(), 15, buf_.empty() ? sk.textDim : sk.textHi);

    // 按钮：取消（左） / 确定（右，焦点常驻 → Enter 直达）
    const float bw = 132, bh = 40;
    const Rect cancelRc(panel.x + 22, panel.bottom() - bh - 18, bw, bh);
    const Rect okRc(panel.right() - bw - 22, panel.bottom() - bh - 18, bw, bh);
    if (w::button(app, cancelRc, "取消", false)) {
      app.popScene();
      return;
    }
    if (w::button(app, okRc, "确定", true, 16, true)) {
      const std::string value = buf_;
      auto cb = onOk_;
      app.popScene();
      if (cb) cb(value);
    }
    app.font.drawVCentered(app.sdlRenderer(), "Enter 确认 · Esc 取消",
                           panel.x + 22, cancelRc.cy(), 11, sk.textDim);
  }

 private:
  std::string title_, label_, buf_;
  std::function<void(const std::string&)> onOk_;
};

// ==================== 在线游戏 ====================
// 数据模型对齐 APK WebGameStore：内置站点（不可删）+ 自定义（dataDir/online-games.json）
struct WebGame {
  std::string title;
  std::string url;
  bool builtin = true;
};

// APK WebGameStore.getBuiltinGames 原样照搬（顺序也不动）
std::vector<WebGame> builtinWebGames() {
  return {
      {"4399电脑版", "https://www.4399.com/", true},
      {"4399手机版", "https://h.4399.com/wap/", true},
      {"灵动游戏", "https://www.mhhf.com/", true},
      {"瑾龙游戏", "https://www.jlgames.cn/", true},
      {"小霸王其乐无穷", "https://www.yikm.net/", true},
      {"7k7k游戏", "https://www.7k7k.com/", true},
      {"37游戏", "https://www.37.com/", true},
      {"Poki游戏", "https://poki.com/zh", true},
      {"233乐园", "https://www.233leyuan.com/game-detail-h5", true},
      {"y8游戏", "https://zh.y8.com/", true},
      {"FC在线", "https://www.playfc.cn/", true},
      {"Flash游戏", "http://www.flashgame.com.cn/", true},
  };
}

std::string onlineGamesFile(App& app) {
  return fsutil::join(app.dataDir(), "online-games.json");
}

std::vector<WebGame> loadWebGames(App& app) {
  std::vector<WebGame> out = builtinWebGames();
  std::string text;
  if (fsutil::readFile(onlineGamesFile(app), &text)) {
    try {
      const json j = json::parse(text, nullptr, false);
      if (!j.is_discarded() && j.is_array()) {
        for (const auto& e : j) {
          WebGame g;
          g.title = e.value("title", "");
          g.url = e.value("url", "");
          g.builtin = false;
          if (!g.title.empty() && !g.url.empty()) out.push_back(std::move(g));
        }
      }
    } catch (...) {
    }
  }
  return out;
}

void saveCustomWebGames(App& app, const std::vector<WebGame>& games) {
  json arr = json::array();
  for (const auto& g : games) {
    if (g.builtin) continue;
    arr.push_back({{"title", g.title}, {"url", g.url}});
  }
  fsutil::writeFile(onlineGamesFile(app), arr.dump(2));
}

class OnlineGamesScene : public Scene {
 public:
  void onEnter(App& app) override { games_ = loadWebGames(app); }

  void update(App& app, float dt, const InputState& in) override {
    (void)dt;
    const int N = (int)games_.size();
    if (N > 0) {
      if (in.pressed[(int)Btn::Up]) { sel_ = (sel_ + N - 1) % N; followSel_ = true; }
      if (in.pressed[(int)Btn::Down]) { sel_ = (sel_ + 1) % N; followSel_ = true; }
    }
    // 激活（Enter/手柄A/鼠标点击）由 draw 里的 w::listRow 统一处理，防双触发
    // Y / 右键 → 长按卡片菜单（APK 同款：开始 / 重命名 / 删除）
    if ((in.pressed[(int)Btn::Option] || in.mouseRightPressed) && N > 0) {
      const WebGame g = games_[(size_t)sel_];
      App* appPtr = &app;
      std::vector<ContextMenuItem> items;
      items.push_back({1, "开始游戏", "浏览器打开", false, [appPtr, g]() {
                         appPtr->toast("正在浏览器打开：" + g.title);
                         if (!fsutil::openUrl(g.url))
                           appPtr->toast("打开失败：系统缺少浏览器入口（xdg-open / start）");
                       }});
      items.push_back({19, "重命名", "", false, [appPtr, g]() {
                         appPtr->pushScene(std::unique_ptr<Scene>(new TextInputScene(
                             "重命名在线游戏", "站点显示名称", g.title,
                             [appPtr, g](const std::string& name) {
                               if (name.empty()) return;
                               // 改内存列表 + 持久化（重进场景时重读）
                               std::vector<WebGame> games = loadWebGames(*appPtr);
                               for (auto& e : games)
                                 if (e.url == g.url) e.title = name;
                               saveCustomWebGames(*appPtr, games);
                               appPtr->toast("已重命名：" + name);
                             })));
                       }});
      if (!g.builtin) {
        items.push_back({0, "删除自定义游戏", "", true, [appPtr, g]() {
                           std::vector<WebGame> games = loadWebGames(*appPtr);
                           games.erase(std::remove_if(games.begin(), games.end(),
                                                      [&](const WebGame& e) {
                                                        return e.url == g.url;
                                                      }),
                                       games.end());
                           saveCustomWebGames(*appPtr, games);
                           appPtr->toast("已删除：" + g.title);
                         }});
      }
      pushContextMenu(app, Vec2(in.mouse.x, in.mouse.y), g.title,
                      g.builtin ? "内置站点" : "自定义 · " + g.url,
                      std::move(items));
    }
  }

  void open(App& app, const WebGame& g) {
    app.toast("正在浏览器打开：" + g.title);
    if (!fsutil::openUrl(g.url)) {
      app.toast("打开失败：系统缺少浏览器入口（xdg-open / start）");
    }
  }

  void pushAdd(App* app) {
    app->pushScene(std::unique_ptr<Scene>(new TextInputScene(
        "添加自定义游戏", "站点名称（如：怀旧小游戏）", "",
        [app](const std::string& name) {
          if (name.empty()) return;
          app->pushScene(std::unique_ptr<Scene>(new TextInputScene(
              "添加自定义游戏", "站点网址（https://…）", "https://",
              [app, name](const std::string& url) {
                if (url.size() < 8 ||
                    (url.compare(0, 7, "http://") != 0 &&
                     url.compare(0, 8, "https://") != 0)) {
                  app->toast("网址需以 http:// 或 https:// 开头");
                  return;
                }
                std::vector<WebGame> games = loadWebGames(*app);
                games.push_back(WebGame{name, url, false});
                saveCustomWebGames(*app, games);
                app->toast("已添加：" + name);
              })));
        })));
  }

  void draw(App& app) override {
    const Skin& sk = Theme::skin();
    Draw& d = app.draw;
    sceneBackground(app);
    const char* subtitle = "内置 12 站点 + 自定义 · 浏览器打开";
    if (w::header(app, "在线游戏", subtitle, true)) {
      app.popScene();
      return;
    }
    // 右上：添加按钮（APK FloatingActionBar 的 Add）
    {
      const Rect addRc(d.W - 170, 16, 150, 36);
      if (w::button(app, addRc, "＋ 添加游戏", false, 14, true)) pushAdd(&app);
    }

    const Rect listRc(24, 74, d.W - 48, d.H - 74 - 64);
    scroll_ += w::dragScroll(app, listRc);
    if (listRc.contains(app.input.input().mouse))
      scroll_ = std::max(0.0f, scroll_ - (float)app.input.input().wheel * 64.0f);
    if (followSel_) {
      followSel_ = false;
      const float rowY = 78 + (float)sel_ * 62 - scroll_;
      if (rowY < listRc.y) scroll_ -= listRc.y - rowY;
      if (rowY > listRc.bottom() - 62) scroll_ += rowY - (listRc.bottom() - 62);
    }
    d.pushClip(listRc);
    float y = 78 - scroll_;
    const float rowW = listRc.w;
    for (size_t i = 0; i < games_.size(); i++) {
      const WebGame& g = games_[i];
      const Rect rc(24, y, rowW, 56);
      const bool focused = sel_ == (int)i;
      const bool hit = w::listRow(app, rc, g.title,
                                  g.builtin ? "内置" : "自定义", focused, g.url);
      if (hit) {
        sel_ = (int)i;
        open(app, g);
      }
      y += 62;
    }
    if (games_.empty()) {
      app.font.draw(app.sdlRenderer(), "暂无站点（右上「＋ 添加游戏」）", 40, 120, 15,
                    sk.textDim);
    }
    d.popClip();
    // 滚动条
    {
      const float total = 78 + (float)games_.size() * 62;
      const float maxScroll = std::max(0.0f, total - 78 - listRc.h);
      if (scroll_ > maxScroll) scroll_ = maxScroll;
      if (scroll_ < 0) scroll_ = 0;
      if (total > listRc.h + 78) {
        scroll_ = w::scrollIndicator(app, Rect(d.W - 12, listRc.y, 6, listRc.h),
                                     listRc.h / (total - 78),
                                     scroll_ / (total - 78 - listRc.h)) *
                  (total - 78 - listRc.h);
      }
    }
    w::keyHintBar(app, {{"A / Enter", "浏览器打开"},
                        {"Y / 右键", "选项"},
                        {"B / Esc", "返回"}});
  }

 private:
  std::vector<WebGame> games_;
  int sel_ = 0;
  float scroll_ = 0;
  bool followSel_ = false;
};

// ==================== SWF / Flash 小游戏 ====================
// 对齐 APK SwfListScreen（目录扫描 + 搜索 + 播放）的 PC 形态：
//   · 目录来源：config.swfDirs（默认数据目录 swf/），「添加目录」走文件浏览器
//   · 播放：① 本地 ruffle（emulators/ruffle/ 或 PATH）直接拉起
//           ② 未安装 → 打开 Ruffle 官方演示页，把 .swf 拖进网页即玩
//             （APK 内嵌 Ruffle 注入 WebView，PC 的浏览器 + 官方演示页为等价物）
class SwfScene : public Scene {
 public:
  struct Entry {
    std::string name;  // 文件名（含扩展名）
    std::string path;  // 完整路径
    uint64_t size = 0;
  };

  void onEnter(App& app) override {
    if (app.config.swfDirs.empty()) {
      // 默认目录：<dataDir>/swf（不存在也保留，添加 .swf 后刷新即见）
      app.config.swfDirs.push_back(fsutil::join(app.dataDir(), "swf"));
      app.saveConfig();
    }
    rescan(app);
  }

  void rescan(App& app) {
    entries_.clear();
    for (const auto& dir : app.config.swfDirs) {
      if (!fsutil::isDir(dir)) continue;
      std::vector<std::string> files;
      fsutil::walkFiles(dir, &files, 6);
      for (const auto& f : files) {
        if (fsutil::extOf(f) != "swf") continue;
        if (!fsutil::isValidUtf8(f)) continue;
        Entry e;
        e.path = f;
        e.name = fsutil::baseOf(f);
        e.size = fsutil::fileSize(f);
        entries_.push_back(std::move(e));
      }
    }
    std::sort(entries_.begin(), entries_.end(),
              [](const Entry& a, const Entry& b) { return a.name < b.name; });
    if (sel_ >= (int)entries_.size()) sel_ = 0;
  }

  // 本地 ruffle 播放器探测：emulators/ruffle/ruffle → PATH 的 ruffle/ruffle_desktop
  std::string findRuffle(App& app) {
    const std::string bundled =
        fsutil::join(fsutil::join(fsutil::exeDir(), "emulators"), "ruffle");
    const char* names[] = {"ruffle", "ruffle_desktop", "ruffle-desktop"};
    for (const char* n : names) {
      std::string p = fsutil::join(bundled, n);
      if (fsutil::isFile(p)) return p;
    }
#ifdef _WIN32
    for (const char* n : names) {
      std::string p = fsutil::join(bundled, std::string(n) + ".exe");
      if (fsutil::isFile(p)) return p;
    }
#endif
    for (const char* n : names) {
#ifdef _WIN32
      if (fsutil::runCommand(std::string("where ") + n + " >nul 2>&1") == 0)
        return n;
#else
      const std::string cmd =
          std::string("command -v ") + n + " >/dev/null 2>&1";
      if (fsutil::runCommand(cmd) == 0) return n;
#endif
    }
    return "";
  }

  void play(App& app, const Entry& e) {
    const std::string ruffle = findRuffle(app);
    if (!ruffle.empty()) {
      const std::string cmd = "\"" + ruffle + "\" \"" + e.path + "\"";
      LOGI("ruffle 启动：%s", cmd.c_str());
#ifdef _WIN32
      if (fsutil::runCommand("start \"\" " + cmd) == 0) {
        app.toast("已用 ruffle 打开：" + e.name);
        return;
      }
#else
      if (fsutil::runCommand(cmd + " >/dev/null 2>&1 &") == 0) {
        app.toast("已用 ruffle 打开：" + e.name);
        return;
      }
#endif
    }
    // 无本地 ruffle → Ruffle 官方演示页（浏览器拖入 .swf 即玩，与 APK 同引擎）
    if (fsutil::openUrl("https://ruffle.rs/demo/")) {
      app.toast("已打开 Ruffle 演示页——把 " + e.name + " 拖入网页即可游玩");
      app.toast("文件位置：" + e.path);
    } else {
      app.toast("打开浏览器失败，请手动访问 ruffle.rs/demo 并拖入 SWF 文件");
    }
  }

  void pushAddDir(App* app) {
    BrowserRequest req;
    req.title = "选择 SWF 目录";
    req.mode = 0;
    req.onPick = [app](const std::string& dir) {
      for (const auto& d : app->config.swfDirs)
        if (d == dir) {
          app->toast("该目录已在列表中");
          return;
        }
      app->config.swfDirs.push_back(dir);
      app->saveConfig();
      app->toast("已添加 SWF 目录：" + fsutil::pathDisplay(dir));
      // 目录添加后立即重扫（onEnter 不会再触发）
      if (SwfScene* s = app->findScene<SwfScene>()) s->rescan(*app);
    };
    browsePath(*app, req);
  }

  void update(App& app, float dt, const InputState& in) override {
    (void)dt;
    const int N = (int)entries_.size();
    if (N > 0) {
      if (in.pressed[(int)Btn::Up]) { sel_ = (sel_ + N - 1) % N; followSel_ = true; }
      if (in.pressed[(int)Btn::Down]) { sel_ = (sel_ + 1) % N; followSel_ = true; }
    }
    if ((in.pressed[(int)Btn::Option] || in.mouseRightPressed) && N > 0) {
      const Entry e = entries_[(size_t)sel_];
      App* appPtr = &app;
      std::vector<ContextMenuItem> items;
      items.push_back({23, "播放", "ruffle / 浏览器", false, [appPtr, e]() {
                         if (SwfScene* s = appPtr->findScene<SwfScene>())
                           s->play(*appPtr, e);
                       }});
      items.push_back({4, "打开所在目录", "", false, [appPtr, e]() {
                         fsutil::openUrl(fsutil::dirOf(e.path));
                       }});
      pushContextMenu(app, Vec2(in.mouse.x, in.mouse.y), e.name,
                      fsutil::pathDisplay(e.path), std::move(items));
    }
  }

  void draw(App& app) override {
    const Skin& sk = Theme::skin();
    Draw& d = app.draw;
    sceneBackground(app);
    char sub[128];
    snprintf(sub, sizeof(sub), "Flash 模拟（Ruffle）· %d 个游戏", (int)entries_.size());
    if (w::header(app, "SWF 小游戏", sub, true)) {
      app.popScene();
      return;
    }
    // 右上工具组：添加目录 / 刷新
    {
      const Rect addRc(d.W - 320, 16, 150, 36);
      const Rect refRc(d.W - 162, 16, 142, 36);
      if (w::button(app, addRc, "＋ 添加目录", false, 14, true)) pushAddDir(&app);
      if (w::button(app, refRc, "刷新", false, 14)) {
        rescan(app);
        app.toast("已刷新");
      }
    }

    const Rect listRc(24, 74, d.W - 48, d.H - 74 - 64);
    scroll_ += w::dragScroll(app, listRc);
    if (listRc.contains(app.input.input().mouse))
      scroll_ = std::max(0.0f, scroll_ - (float)app.input.input().wheel * 64.0f);
    if (followSel_) {
      followSel_ = false;
      const float rowY = 78 + (float)sel_ * 62 - scroll_;
      if (rowY < listRc.y) scroll_ -= listRc.y - rowY;
      if (rowY > listRc.bottom() - 62) scroll_ += rowY - (listRc.bottom() - 62);
    }
    d.pushClip(listRc);
    float y = 78 - scroll_;
    for (size_t i = 0; i < entries_.size(); i++) {
      const Entry& e = entries_[i];
      const Rect rc(24, y, listRc.w, 56);
      const bool focused = sel_ == (int)i;
      char sizeTxt[32];
      if (e.size >= 1024 * 1024) snprintf(sizeTxt, sizeof(sizeTxt), "%.1f MB", e.size / 1048576.0);
      else snprintf(sizeTxt, sizeof(sizeTxt), "%u KB", (unsigned)(e.size / 1024));
      if (w::listRow(app, rc, e.name, sizeTxt, focused,
                     fsutil::pathDisplay(e.path))) {
        sel_ = (int)i;
        play(app, e);
      }
      y += 62;
    }
    if (entries_.empty()) {
      app.font.draw(app.sdlRenderer(),
                    "没有找到 .swf 文件——「＋ 添加目录」选择存放 Flash 游戏的文件夹",
                    40, 120, 15, sk.textDim);
    }
    d.popClip();
    {
      const float total = 78 + (float)entries_.size() * 62;
      const float maxScroll = std::max(0.0f, total - 78 - listRc.h);
      if (scroll_ > maxScroll) scroll_ = maxScroll;
      if (scroll_ < 0) scroll_ = 0;
      if (total > listRc.h + 78) {
        scroll_ = w::scrollIndicator(app, Rect(d.W - 12, listRc.y, 6, listRc.h),
                                     listRc.h / (total - 78),
                                     scroll_ / (total - 78 - listRc.h)) *
                  (total - 78 - listRc.h);
      }
    }
    w::keyHintBar(app, {{"A / Enter", "播放"}, {"Y / 右键", "选项"}, {"B / Esc", "返回"}});
  }

 private:
  std::vector<Entry> entries_;
  int sel_ = 0;
  float scroll_ = 0;
  bool followSel_ = false;
};

// ==================== 对战平台 ====================
// 对齐 APK BattleScreen 的信息架构（服务器 / 登录 / 房间列表），PC 端：
//   · 服务器地址可改（默认 afei.ddns.net:1808，对齐 APK BuildConfig）
//   · 同一套 HTTP API：/api/health、/api/auth/login|register、/api/games、
//     /api/rooms；curl 后台线程请求，结果经共享状态回主线程
//   · 对战 ROM 可下载入库（下载到 <dataDir>/battle-roms 并扫描）
//   · 开桌/入座需要 Android 版联机同步引擎，PC 端明确说明而非假装
class BattleScene : public Scene {
 public:
  struct Game {
    std::string id, title, platform, fileName;
    long long size = 0;
    int maxPlayers = 4;
  };
  struct Room {
    std::string id, gameTitle, host, status;
    int players = 0, maxPlayers = 4, spectators = 0, table = 0;
  };
  // 工作线程 ↔ 场景 的共享状态（场景销毁后线程仍安全写完即退）
  struct State {
    std::mutex mu;
    std::atomic<bool> busy{false};
    bool ok = false;
    std::string error;     // 失败原因（人话）
    std::vector<Game> games;
    std::vector<Room> rooms;
    std::string health;    // 健康检查摘要
    std::atomic<bool> needToast{false};
    std::string toastMsg;
    // 登录回执（工作线程写，主线程读）
    std::string token;     // 空 = 未登录成功
    // 主线程专用
    bool tried = false;
    std::atomic<bool> needScan{false};  // ROM 下载完成 → 主线程扫描入库
  };
  ~BattleScene() {
    if (thread_ && thread_->joinable()) thread_->detach();
  }

  void onEnter(App& app) override {
    if (!st_) st_ = std::make_shared<State>();
    if (!st_->busy && st_->games.empty() && st_->rooms.empty() && !st_->tried) {
      st_->tried = true;
      refresh(app);
    }
  }

  std::string httpBase(App& app) const {
    return "http://" + app.config.battleHost + ":" + app.config.battleHttpPort;
  }

  // 临时文件路径（curl 落盘 / payload 中转；各平台安全可写）
  static std::string tmpJsonPath() {
#ifdef _WIN32
    const char* tmpDir = getenv("TEMP");
    return tmpDir ? fsutil::join(tmpDir, "nesstation-battle.json")
                  : std::string("nesstation-battle.json");
#else
    return "/tmp/nesstation-battle.json";
#endif
  }

  // curl 取 JSON 到临时文件（工作线程内）
  static bool curlJson(const std::string& url, const std::string& bearer,
                       std::string* out, long* code) {
    const std::string tmpPath = tmpJsonPath();
    std::string cmd = "curl -sS --connect-timeout 8 --max-time 15 -w \"%{http_code}\"";
    cmd += " -H \"Accept: application/json\"";
    if (!bearer.empty()) cmd += " -H \"Authorization: Bearer " + bearer + "\"";
    cmd += " -o " + (tmpPath.find(' ') == std::string::npos ? tmpPath : "\"" + tmpPath + "\"");
    cmd += " " + url;
    FILE* p = popen(cmd.c_str(), "r");
    if (!p) return false;
    char buf[64] = {0};
    fgets(buf, sizeof(buf), p);
    pclose(p);
    if (code) *code = atol(buf);
    fsutil::readFile(tmpPath, out);
    fsutil::removeFile(tmpPath);
    return true;
  }

  static std::string jsonEscape(const std::string& s) {
    std::string out;
    for (char c : s) {
      switch (c) {
        case '"': out += "\\\""; break;
        case '\\': out += "\\\\"; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        default: out += c;
      }
    }
    return out;
  }

  void refresh(App& app) {
    if (st_->busy) return;
    auto st = st_;
    const std::string base = httpBase(app);
    if (thread_ && thread_->joinable()) thread_->detach();
    st_->busy = true;
    thread_ = std::make_shared<std::thread>([st, base]() {
      // 1) 房间列表
      std::string body;
      long code = 0;
      if (!curlJson(base + "/api/rooms", "", &body, &code) || code != 200) {
        std::lock_guard<std::mutex> lk(st->mu);
        st->ok = false;
        st->error = code == 0 ? "无法连接服务器（curl 失败 / 无网络）"
                              : "服务器错误（HTTP " + std::to_string(code) + "）";
        st->games.clear();
        st->rooms.clear();
        st->busy = false;
        return;
      }
      std::vector<Room> rooms;
      std::vector<Game> games;
      try {
        const json j = json::parse(body, nullptr, false);
        if (!j.is_discarded() && j.contains("rooms") && j["rooms"].is_array()) {
          for (const auto& r : j["rooms"]) {
            Room rm;
            rm.id = r.value("id", "");
            rm.gameTitle = r.value("gameTitle", "");
            rm.host = r.value("host", "");
            rm.status = r.value("status", "");
            rm.players = r.value("players", 0);
            rm.maxPlayers = r.value("maxPlayers", 4);
            rm.spectators = r.value("spectators", 0);
            rm.table = r.value("table", 0);
            rooms.push_back(std::move(rm));
          }
        }
      } catch (...) {
      }
      // 2) 对战游戏库
      std::string gbody;
      if (curlJson(base + "/api/games", "", &gbody, &code) && code == 200) {
        try {
          const json j = json::parse(gbody, nullptr, false);
          if (!j.is_discarded() && j.contains("games") && j["games"].is_array()) {
            for (const auto& g : j["games"]) {
              Game gm;
              gm.id = g.value("id", "");
              gm.title = g.value("title", "");
              gm.platform = g.value("platform", "arcade");
              gm.fileName = g.value("fileName", gm.id + ".zip");
              gm.size = g.value("size", 0);
              gm.maxPlayers = g.value("maxPlayers", 4);
              games.push_back(std::move(gm));
            }
          }
        } catch (...) {
        }
      }
      std::lock_guard<std::mutex> lk(st->mu);
      st->ok = true;
      st->error.clear();
      st->rooms = std::move(rooms);
      st->games = std::move(games);
      st->health = std::to_string(st->games.size()) + " 个对战游戏 · " +
                   std::to_string(st->rooms.size()) + " 个房间";
      st->busy = false;
    });
  }

  void login(App* app, const std::string& user, const std::string& pass) {
    auto st = st_;
    const std::string base = httpBase(*app);
    if (thread_ && thread_->joinable()) thread_->detach();
    st_->busy = true;
    thread_ = std::make_shared<std::thread>([st, base, user, pass]() {
      // payload 写临时文件后 -d @file（避开 shell 引号注入/转义陷阱）
      const std::string payload =
          "{\"username\":\"" + jsonEscape(user) + "\",\"password\":\"" +
          jsonEscape(pass) + "\"}";
      const std::string tmpPath = tmpJsonPath();
      fsutil::writeFile(tmpPath, payload);
      const std::string tmpPayload = tmpPath + ".payload";
      fsutil::writeFile(tmpPayload, payload);
      long code = 0;
      std::string token, msg;
      auto post = [&](const char* path) -> long {
        const std::string cmd =
            "curl -sS --connect-timeout 8 --max-time 15 -o " +
            (tmpPath.find(' ') == std::string::npos ? tmpPath : "\"" + tmpPath + "\"") +
            " -w \"%{http_code}\" -X POST -H \"Content-Type: application/json\" -d @" +
            (tmpPayload.find(' ') == std::string::npos ? tmpPayload : "\"" + tmpPayload + "\"") +
            " " + base + path;
        FILE* p = popen(cmd.c_str(), "r");
        char buf[64] = {0};
        if (p) {
          fgets(buf, sizeof(buf), p);
          pclose(p);
        }
        return atol(buf);
      };
      code = post("/api/auth/login");
      std::string resp;
      fsutil::readFile(tmpPath, &resp);
      fsutil::removeFile(tmpPath);
      if (code == 200) {
        try {
          const json j = json::parse(resp, nullptr, false);
          token = j.value("token", "");
        } catch (...) {
        }
        msg = token.empty() ? "登录响应异常" : "登录成功：" + user;
      } else if (code == 401 || code == 400 || code == 404 || code == 422) {
        // 登录失败 → 尝试注册（APK 同样自动注册语义）
        const long rcode = post("/api/auth/register");
        std::string rresp;
        fsutil::readFile(tmpPath, &rresp);
        fsutil::removeFile(tmpPath);
        if (rcode == 200 || rcode == 201) {
          try {
            const json j = json::parse(rresp, nullptr, false);
            token = j.value("token", "");
          } catch (...) {
          }
          msg = token.empty() ? "注册响应异常" : "注册并登录成功：" + user;
        } else {
          msg = "登录/注册失败（账号或密码错误）";
        }
      } else if (code == 0) {
        msg = "无法连接服务器（无网络或地址错误）";
      } else {
        msg = "服务器错误（HTTP " + std::to_string(code) + "）";
      }
      fsutil::removeFile(tmpPayload);
      std::lock_guard<std::mutex> lk(st->mu);
      st->busy = false;
      st->token = token;      // 空 = 未登录
      st->needToast = true;
      st->toastMsg = msg;
    });
  }

  void downloadRom(App* app, const Game& g) {
    auto st = st_;
    const std::string base = httpBase(*app);
    const std::string destDir = fsutil::join(app->dataDir(), "battle-roms");
    fsutil::makeDirs(destDir);
    const std::string dest = fsutil::join(destDir, g.fileName);
    if (thread_ && thread_->joinable()) thread_->detach();
    st_->busy = true;
    const std::string title = g.title;
    const std::string romUrl = base + "/api/games/" + g.id + "/rom";
    thread_ = std::make_shared<std::thread>([st, romUrl, dest, title, destDir]() {
      // 下载（--create-dirs 不需要，目录已建；-f 让 4xx 直接失败）
      auto q = [](const std::string& s) {
        return s.find(' ') == std::string::npos ? s : "\"" + s + "\"";
      };
      std::string cmd = "curl -fSL --connect-timeout 10 --max-time 1800 --retry 2 ";
      cmd += "-A NesStationPC/1.3 -o " + q(dest) + " " + q(romUrl);
      const int rc = fsutil::runCommand(cmd);
      std::lock_guard<std::mutex> lk(st->mu);
      st->busy = false;
      if (rc != 0 || !fsutil::isFile(dest)) {
        fsutil::removeFile(dest);
        st->needToast = true;
        st->toastMsg = "下载失败：" + title + "（服务器未提供或网络中断）";
        return;
      }
      st->needToast = true;
      st->toastMsg = "已下载：" + title + " → " + destDir;
      st->needScan = true;  // 主线程扫描入库
    });
  }

  void update(App& app, float dt, const InputState& in) override {
    (void)dt;
    // 工作线程回执 → toast + 登录持久化 + ROM 入库扫描（全部主线程执行）
    if (st_) {
      if (st_->needToast.exchange(false)) {
        std::string msg, token;
        {
          std::lock_guard<std::mutex> lk(st_->mu);
          msg = st_->toastMsg;
          token = st_->token;
        }
        app.toast(msg);
        if (!token.empty()) {
          app.config.battleToken = token;
          app.saveConfig();
        }
      }
      if (st_->needScan.exchange(false)) {
        const std::string dir = fsutil::join(app.dataDir(), "battle-roms");
        const int n = app.library.scanDirectory(dir, GamePlatform::ARCADE, false, true);
        app.library.save();
        app.toast(n > 0 ? "已加入游戏库（" + std::to_string(n) + " 个新游戏）"
                        : "已下载（未能识别入库，可在游戏库手动导入该目录）");
      }
    }
    if (in.pressed[(int)Btn::Back]) {
      app.popScene();
      return;
    }
    const int N = tab_ == 0 ? (int)roomsView().size() : (int)gamesView().size();
    if (N > 0) {
      if (in.pressed[(int)Btn::Up]) { sel_ = (sel_ + N - 1) % N; followSel_ = true; }
      if (in.pressed[(int)Btn::Down]) { sel_ = (sel_ + 1) % N; followSel_ = true; }
    } else {
      sel_ = 0;
    }
    if (in.pressed[(int)Btn::Left] || in.pressed[(int)Btn::Right]) {
      tab_ = 1 - tab_;
      sel_ = 0;
      followSel_ = true;
    }
  }

  std::vector<Room> roomsView() const {
    if (!st_) return {};
    std::lock_guard<std::mutex> lk(st_->mu);
    return st_->rooms;
  }
  std::vector<Game> gamesView() const {
    if (!st_) return {};
    std::lock_guard<std::mutex> lk(st_->mu);
    return st_->games;
  }

  void pushServerEdit(App* app) {
    app->pushScene(std::unique_ptr<Scene>(new TextInputScene(
        "对战服务器 · 地址", "服务器主机名或 IP（默认 afei.ddns.net）",
        app->config.battleHost,
        [app](const std::string& host) {
          if (host.empty()) return;
          app->pushScene(std::unique_ptr<Scene>(new TextInputScene(
              "对战服务器 · HTTP 端口", "端口号（默认 1808）",
              app->config.battleHttpPort,
              [app, host](const std::string& port) {
                if (atoi(port.c_str()) <= 0) {
                  app->toast("端口无效");
                  return;
                }
                app->config.battleHost = host;
                app->config.battleHttpPort = port;
                app->saveConfig();
                app->toast("服务器已保存：" + host + ":" + port);
                if (BattleScene* s = app->findScene<BattleScene>()) s->refresh(*app);
              })));
        })));
  }

  void draw(App& app) override {
    const Skin& sk = Theme::skin();
    Draw& d = app.draw;
    sceneBackground(app);
    char sub[160];
    snprintf(sub, sizeof(sub), "服务器 %s:%s · 联机对战平台",
             app.config.battleHost.c_str(), app.config.battleHttpPort.c_str());
    if (w::header(app, "对战平台", sub, true)) {
      app.popScene();
      return;
    }

    // 顶栏工具：登录（未登录）/ 刷新 / 服务器
    {
      const std::string loginLabel =
          app.config.battleUser.empty() ? "登录 / 注册" : "已登录：" + app.config.battleUser;
      const Rect loginRc(d.W - 476, 16, 168, 36);
      const Rect refRc(d.W - 300, 16, 90, 36);
      const Rect srvRc(d.W - 202, 16, 182, 36);
      if (w::button(app, loginRc, loginLabel, false, 12, false)) {
        if (app.config.battleUser.empty()) {
          app.pushScene(std::unique_ptr<Scene>(new TextInputScene(
              "登录对战平台", "用户名（不存在则自动注册）", "",
              [&app](const std::string& user) {
                app.pushScene(std::unique_ptr<Scene>(new TextInputScene(
                    "登录对战平台", "密码", "",
                    [&app, user](const std::string& pass) {
                      if (user.empty() || pass.empty()) {
                        app.toast("用户名和密码不能为空");
                        return;
                      }
                      app.config.battleUser = user;
                      app.saveConfig();
                      if (BattleScene* s = app.findScene<BattleScene>())
                        s->login(&app, user, pass);
                    })));
              })));
        } else {
          // 已登录 → 退出登录
          app.config.battleUser.clear();
          app.config.battleToken.clear();
          app.saveConfig();
          app.toast("已退出登录");
        }
      }
      if (w::button(app, refRc, "刷新", false, 14)) refresh(app);
      if (w::button(app, srvRc, "服务器设置", false, 12)) pushServerEdit(&app);
    }

    // Tab 行：房间（N） / 对战游戏库（N）
    const auto rooms = roomsView();
    const auto games = gamesView();
    {
      const Rect t0(24, 68, 210, 36);
      const Rect t1(244, 68, 260, 36);
      char l0[64], l1[64];
      snprintf(l0, sizeof(l0), "房间（%d）", (int)rooms.size());
      snprintf(l1, sizeof(l1), "对战游戏库（%d）", (int)games.size());
      if (w::button(app, t0, l0, tab_ == 0, 14, tab_ == 0)) { tab_ = 0; sel_ = 0; }
      if (w::button(app, t1, l1, tab_ == 1, 14, tab_ == 1)) { tab_ = 1; sel_ = 0; }
    }

    const Rect listRc(24, 116, d.W - 48, d.H - 116 - 100);
    scroll_ += w::dragScroll(app, listRc);
    if (listRc.contains(app.input.input().mouse))
      scroll_ = std::max(0.0f, scroll_ - (float)app.input.input().wheel * 64.0f);
    if (followSel_) {
      followSel_ = false;
      const float rowY = 120 + (float)sel_ * 62 - scroll_;
      if (rowY < listRc.y) scroll_ -= listRc.y - rowY;
      if (rowY > listRc.bottom() - 62) scroll_ += rowY - (listRc.bottom() - 62);
    }
    d.pushClip(listRc);
    float y = 120 - scroll_;
    if (tab_ == 0) {
      if (st_ && st_->busy) {
        app.font.draw(app.sdlRenderer(), "正在连接服务器…", 40, 130, 15, sk.accent);
      } else if (!rooms.empty()) {
        for (size_t i = 0; i < rooms.size(); i++) {
          const Room& r = rooms[i];
          const Rect rc(24, y, listRc.w, 56);
          const std::string status =
              r.status == "waiting" ? "等待中" : (r.status == "playing" ? "对战中" : r.status);
          const std::string value = std::to_string(r.players) + "/" +
                                    std::to_string(r.maxPlayers) + " 人 · " + status;
          const std::string sub2 =
              "房主 " + r.host + (r.spectators > 0
                                ? " · 观战 " + std::to_string(r.spectators)
                                : "");
          const bool hit = w::listRow(app, rc, r.gameTitle, value, sel_ == (int)i, sub2);
          if (hit) {
            app.toast("联机对局需 Android 版对战引擎（PC 端暂不支持入座）");
            app.toast("可通过「对战游戏库」下载该游戏的 ROM 单机游玩");
          }
          y += 62;
        }
      } else {
        std::string err;
        bool ok = false;
        if (st_) {
          std::lock_guard<std::mutex> lk(st_->mu);
          err = st_->error;
          ok = st_->ok;
        }
        app.font.draw(app.sdlRenderer(),
                      ok ? "当前没有房间——开一桌需要 Android 端（或等待其他玩家）"
                         : ("连接失败：" + err),
                      40, 130, 15, ok ? sk.textDim : sk.btnB);
      }
    } else {
      if (!games.empty()) {
        for (size_t i = 0; i < games.size(); i++) {
          const Game& g = games[i];
          const Rect rc(24, y, listRc.w, 56);
          char sizeTxt[32];
          if (g.size >= 1024 * 1024)
            snprintf(sizeTxt, sizeof(sizeTxt), "%.1f MB", g.size / 1048576.0);
          else
            snprintf(sizeTxt, sizeof(sizeTxt), "%lld KB", g.size / 1024);
          const bool hit = w::listRow(app, rc, g.title, sizeTxt, sel_ == (int)i,
                                      "平台 " + g.platform + " · 最多 " +
                                          std::to_string(g.maxPlayers) + " 人");
          if (hit) {
            sel_ = (int)i;
            downloadRom(&app, g);
          }
          y += 62;
        }
      } else if (st_ && st_->busy) {
        app.font.draw(app.sdlRenderer(), "正在获取对战游戏库…", 40, 130, 15, sk.accent);
      } else {
        app.font.draw(app.sdlRenderer(), "游戏库为空（服务器未配置 ROM）", 40, 130, 15,
                      sk.textDim);
      }
    }
    d.popClip();
    {
      const int N = tab_ == 0 ? (int)rooms.size() : (int)games.size();
      const float total = 120 + (float)N * 62;
      const float maxScroll = std::max(0.0f, total - 120 - listRc.h);
      if (scroll_ > maxScroll) scroll_ = maxScroll;
      if (scroll_ < 0) scroll_ = 0;
      if (total > listRc.h + 120) {
        scroll_ = w::scrollIndicator(app, Rect(d.W - 12, listRc.y, 6, listRc.h),
                                     listRc.h / (total - 120),
                                     scroll_ / (total - 120 - listRc.h)) *
                  (total - 120 - listRc.h);
      }
    }

    // 底部说明条（对齐 APK BattleScreen 的联机提示位）
    {
      const Rect bar(24, d.H - 92, d.W - 48, 44);
      d.chamferRect(bar, 10, sk.bgPanel);
      app.font.drawVCentered(app.sdlRenderer(),
                             "联机对局需 Android 版对战引擎；PC 端支持浏览房间 / "
                             "登录 / 下载对战 ROM（下载后加入游戏库单机游玩）",
                             bar.x + 16, bar.cy(), 12, sk.textDim);
    }
    w::keyHintBar(app, {{"←→", "切页"}, {"A / Enter", "选择"}, {"B / Esc", "返回"}});
  }

 private:
  std::shared_ptr<State> st_;
  std::shared_ptr<std::thread> thread_;
  int tab_ = 0;   // 0 房间 1 游戏库
  int sel_ = 0;
  float scroll_ = 0;
  bool followSel_ = false;
};

// ==================== 场景工厂 ====================
}  // namespace（匿名）
}  // namespace ns

std::unique_ptr<ns::Scene> ns::createOnlineGamesScene() {
  return std::unique_ptr<Scene>(new OnlineGamesScene());
}
std::unique_ptr<ns::Scene> ns::createSwfScene() {
  return std::unique_ptr<Scene>(new SwfScene());
}
std::unique_ptr<ns::Scene> ns::createBattleScene() {
  return std::unique_ptr<Scene>(new BattleScene());
}
