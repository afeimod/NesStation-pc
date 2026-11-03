// NesStation PC — 主页：Neon 3D 游戏站 / FSD 经典磁贴（按皮肤切换）
#include <algorithm>
#include <cmath>
#include <ctime>

#include "ui/Scenes.h"
#include "ui/Widgets.h"

namespace ns {
namespace {

std::string clockText() {
  const time_t now = time(nullptr);
  const struct tm* t = localtime(&now);
  char buf[16];
  if (t) strftime(buf, sizeof(buf), "%H:%M", t);
  return buf;
}

std::string lastPlayedText(int64_t epoch) {
  if (epoch <= 0) return "从未游玩";
  const int64_t diff = (int64_t)time(nullptr) - epoch;
  if (diff < 60) return "刚刚游玩";
  if (diff < 3600) return std::to_string(diff / 60) + " 分钟前";
  if (diff < 86400) return std::to_string(diff / 3600) + " 小时前";
  return std::to_string(diff / 86400) + " 天前";
}

std::string playTimeText(int64_t sec) {
  if (sec < 60) return sec > 0 ? std::to_string(sec) + " 秒" : "—";
  if (sec < 3600) return std::to_string(sec / 60) + " 分钟";
  return std::to_string(sec / 3600) + " 小时 " + std::to_string((sec % 3600) / 60) + " 分";
}

// 排序：最近游玩优先（与原项目 NeonHomeScreen 一致）
std::vector<GameEntry> recentFirst(const std::vector<GameEntry>& games) {
  std::vector<GameEntry> out = games;
  std::sort(out.begin(), out.end(), [](const GameEntry& a, const GameEntry& b) {
    const bool ap = a.lastPlayedAt > 0, bp = b.lastPlayedAt > 0;
    if (ap != bp) return ap;
    return a.lastPlayedAt > b.lastPlayedAt;
  });
  return out;
}

void drawFlowCard(App& app, float cx, float cy, float cardW, float cardH, int dist,
                    const GameEntry& g) {
    const Skin& sk = Theme::skin();
    Draw& d = app.draw;
    const float aDist = std::abs((float)dist);
    const float scale = dist == 0 ? 1.0f : std::pow(0.76f, aDist);
    const float w = cardW * scale, h = cardH * scale;
    const float xoff = (float)dist * cardW * (0.36f + 0.10f * aDist);
    const float yoff = aDist * 14.0f;
    const Rect rc(cx + xoff - w / 2, cy + yoff - h / 2, w, h);

    // 倒影
    const Rect refl(rc.x, rc.bottom() + 8, w, h * 0.5f);
    SDL_Texture* tex = w::coverTexture(app, g);
    if (tex) {
      app.draw.textureEx(tex, Rect(0, 0, 0, 0), refl, 0, Color8(255, 255, 255, 46), true);
    }
    // 卡片
    w::coverCard(app, rc, g, dist == 0);
    if (dist == 0) {
      // 选中卡：电光青描边 + 底部光带
      d.glowLine(rc.x, rc.bottom() + 2, rc.right(), rc.bottom() + 2, sk.accent, 2, 10);
      // 平台色顶条
      const Color8 pc = Color8::hex(platformAccent(g.platform));
      d.rect(Rect(rc.x + 8, rc.y + 8, 26, 4), pc);
    }
  }

// ==================== Neon 3D 主页 ====================
class NeonHome : public Scene {
 public:
  void onEnter(App& app) override { refresh(app); }

  void refresh(App& app) {
    games_ = recentFirst(app.library.games());
    if (sel_ >= (int)games_.size()) sel_ = 0;
    if (games_.empty()) {
      app.covers.requestFetchAllMissing(app.library.games());
    } else {
      // 只为可见附近的缺封面游戏排队
      for (int i = -4; i <= 4; i++) {
        const int idx = sel_ + i;
        if (idx >= 0 && idx < (int)games_.size()) app.covers.requestFetch(games_[(size_t)idx]);
      }
    }
  }

  void update(App& app, float dt, const InputState& in) override {
    (void)dt;
    // 库被外部改动（导入/删除）后刷新
    if ((int)app.library.games().size() != libCount_) {
      libCount_ = (int)app.library.games().size();
      refresh(app);
    }

    if (in.pressed[(int)Btn::Back]) {
      // 主页再按返回 = 确认退出（与 FSD 一致由菜单行退出）
    }

    if (menuFocus_) {
      if (in.pressed[(int)Btn::Down] && !games_.empty()) menuFocus_ = false;
      if (in.pressed[(int)Btn::Left]) menuSel_ = (menuSel_ + kMenuCount - 1) % kMenuCount;
      if (in.pressed[(int)Btn::Right]) menuSel_ = (menuSel_ + 1) % kMenuCount;
      if (in.pressed[(int)Btn::Ok]) activateMenu(app);
    } else {
      if (in.pressed[(int)Btn::Up] || games_.empty()) menuFocus_ = true;
      if (!games_.empty()) {
        if (in.pressed[(int)Btn::Left]) {
          sel_ = (sel_ + (int)games_.size() - 1) % (int)games_.size();
          app.covers.requestFetch(games_[(size_t)sel_]);
        }
        if (in.pressed[(int)Btn::Right]) {
          sel_ = (sel_ + 1) % (int)games_.size();
          app.covers.requestFetch(games_[(size_t)sel_]);
        }
        if (in.pressed[(int)Btn::Ok]) {
          app.launchGame(games_[(size_t)sel_]);
        }
        if (in.pressed[(int)Btn::Fav]) {
          app.library.toggleFavorite(games_[(size_t)sel_].id);
          app.library.save();
          refresh(app);
        }
      }
    }
    if (in.pressed[(int)Btn::Menu]) {
      app.pushScene(createSettingsScene());
    }
  }

  void activateMenu(App& app) {
    switch (menuSel_) {
      case 0: app.pushScene(createLibraryScene(-1)); break;
      case 1: app.pushScene(createLibraryScene(-2)); break;
      case 2: app.pushScene(createLibraryScene(-3)); break;
      case 3: app.pushScene(createSettingsScene()); break;
      case 4: app.pushScene(createAboutScene()); break;
      case 5: app.requestExit(); break;
    }
  }

  void draw(App& app) override {
    const Skin& sk = Theme::skin();
    Draw& d = app.draw;

    // 深空渐变底
    for (int i = 0; i < 40; i++) {
      const float t = (float)i / 40;
      d.rect(Rect(0, app.draw.H * t, app.draw.W, app.draw.H / 40 + 1),
             sk.bg.lerp(sk.bgDeep, t));
    }
    // 网格微光（Neon 科技感）
    for (int x = 0; x < app.draw.W; x += 80) {
      d.line((float)x, 0, (float)x, (float)app.draw.H,
             Color8(sk.line.r, sk.line.g, sk.line.b, 28), 1);
    }
    for (int y = 0; y < app.draw.H; y += 80) {
      d.line(0, (float)y, (float)app.draw.W, (float)y,
             Color8(sk.line.r, sk.line.g, sk.line.b, 22), 1);
    }

    // —— 品牌行 ——
    app.font.draw(app.sdlRenderer(), "NESSTATION", 40, 30, 26, sk.textHi, true);
    d.rect(Rect(40 + app.font.textWidth("NESSTATION", 26, true) + 12, 38, 4, 22), sk.accent);
    const std::string count =
        std::to_string(games_.size()) + " GAMES · PC EDITION";
    app.font.draw(app.sdlRenderer(), count, 40, 64, 13, sk.textDim);
    app.font.draw(app.sdlRenderer(), clockText(), app.draw.W - 100, 34, 22, sk.text, true);

    // —— 菜单行 ——
    const char* menu[kMenuCount] = {"游戏库", "收藏", "最近", "设置", "关于", "退出"};
    float mx = 40;
    const float my = 108;
    for (int i = 0; i < kMenuCount; i++) {
      const bool hot = menuFocus_ ? menuSel_ == i : false;
      const std::string label = menu[i];
      const float tw = app.font.textWidth(label, 16, hot);
      const Rect rc(mx, my, tw + 30, 36);
      if (hot) d.glowRect(rc, 6, Color8(sk.accent.r, sk.accent.g, sk.accent.b, 90), 6);
      skinPanel(d, rc, hot, 0);
      app.font.drawVCentered(app.sdlRenderer(), label, mx + 15, my + 18, 16,
                             hot ? sk.accent : sk.text, hot);
      // 鼠标
      if (rc.contains(app.input.input().mouse)) {
        if (app.input.input().mousePressed) {
          menuFocus_ = true;
          menuSel_ = i;
          activateMenu(app);
        } else {
          menuFocus_ = true;
          menuSel_ = i;
        }
      }
      mx += tw + 42;
    }
    // 菜单行焦点指示
    d.line(40, my + 44, 40 + 140, my + 44,
           menuFocus_ ? Color8(sk.accent.r, sk.accent.g, sk.accent.b, 200) : sk.line, 2);

    // —— 3D 封面流 ——
    if (!games_.empty()) {
      const float cardH = app.draw.H * 0.44f;
      const float cardW = cardH * 0.72f;
      const float cy = app.draw.H * 0.42f;
      const float cx = app.draw.W / 2;

      for (int dist = 3; dist >= 1; dist--) {
        for (int sign : {-1, 1}) {
          const int idx = sel_ + sign * dist;
          if (idx < 0 || idx >= (int)games_.size()) continue;
          drawFlowCard(app, cx, cy, cardW, cardH, sign * dist, games_[(size_t)idx]);
        }
      }
      if (!games_.empty()) drawFlowCard(app, cx, cy, cardW, cardH, 0, games_[(size_t)sel_]);

      // —— 信息区 ——
      const GameEntry& g = games_[(size_t)sel_];
      const float iy = app.draw.H * 0.70f;
      const float titleSize = 34;
      const std::string title = app.font.ellipsize(g.displayTitle(), titleSize, app.draw.W * 0.5f, true);
      app.font.draw(app.sdlRenderer(), title, 40, iy, titleSize, sk.textHi, true);

      // 平台徽章 + 年代
      const Color8 pc = Color8::hex(platformAccent(g.platform));
      float bx = 40;
      const float by = iy + 48;
      const std::string pname = platformShortName(g.platform);
      const float pw = app.font.textWidth(pname, 13, true) + 20;
      d.chamferRect(Rect(bx, by, pw, 24), 4, Color8(pc.r, pc.g, pc.b, 44));
      d.chamferRectOutline(Rect(bx, by, pw, 24), 4, pc, 1.4f);
      app.font.drawVCentered(app.sdlRenderer(), pname, bx + 10, by + 12, 13, pc, true);
      bx += pw + 12;
      app.font.drawVCentered(app.sdlRenderer(), platformEra(g.platform), bx, by + 12, 13,
                             sk.textDim);
      bx += app.font.textWidth(platformEra(g.platform), 13) + 20;
      app.font.drawVCentered(app.sdlRenderer(),
                             lastPlayedText(g.lastPlayedAt) + " · 总时长 " +
                                 playTimeText(g.playTimeSec),
                             bx, by + 12, 13, sk.textDim);

      // 操作按钮
      const Rect playRc(app.draw.W - 260, iy + 6, 220, 52);
      if (w::button(app, playRc, "▶  启动游戏", !menuFocus_ && flowHot(app, playRc), 19, true)) {
        app.launchGame(g);
      }
      const Rect favRc(app.draw.W - 260, iy + 70, 220, 40);
      const std::string favLabel = g.favorite ? "★ 已收藏" : "☆ 收藏";
      if (w::button(app, favRc, favLabel, false, 15, false)) {
        app.library.toggleFavorite(g.id);
        app.library.save();
        refresh(app);
      }
    } else {
      // 空库引导
      const std::string msg = "游戏库是空的 —— 到「游戏库 → 添加 ROM 目录」导入游戏";
      const float tw = app.font.textWidth(msg, 18);
      app.font.draw(app.sdlRenderer(), msg, app.draw.W / 2 - tw / 2, app.draw.H * 0.45f, 18,
                    sk.text);
      const Rect goRc(app.draw.W / 2 - 90, app.draw.H * 0.45f + 50, 180, 46);
      if (w::button(app, goRc, "打开游戏库", true, 17, true)) {
        app.pushScene(createLibraryScene(-1));
      }
    }

    w::hintBar(app, {{"A", "启动"}, {"X", "收藏"}, {"Start", "设置"}, {"B", "菜单行"}});
  }

  bool flowHot(App& app, const Rect& rc) {
    // 主焦点在封面流时，启动按钮高亮
    (void)app;
    (void)rc;
    return !menuFocus_;
  }

 private:
  static constexpr int kMenuCount = 6;
  std::vector<GameEntry> games_;
  int sel_ = 0;
  int menuSel_ = 0;
  bool menuFocus_ = true;
  int libCount_ = -1;
};

// ==================== FSD 经典磁贴主页 ====================
class FsdHome : public Scene {
 public:
  struct Tile {
    std::string label;
    std::string sub;
    int action;              // -1=library all, -2=fav, -3=recent, -4=settings, -5=about,
                             // -6=exit, >=0 = 平台过滤
    GamePlatform platform;   // action>=0 有效
    int count = 0;
  };

  void onEnter(App& app) override { rebuild(app); }

  void rebuild(App& app) {
    tiles_.clear();
    Tile all{"全部游戏", "", -1, GamePlatform::NES, (int)app.library.games().size()};
    tiles_.push_back(all);
    for (GamePlatform p : allPlatforms()) {
      const int n = app.library.countFor(p);
      if (n <= 0) continue;
      Tile t{platformShortName(p), platformFullName(p), (int)p, p, n};
      tiles_.push_back(t);
    }
    tiles_.push_back({"收藏", "FAVORITES", -2, GamePlatform::NES,
                      (int)app.library.favorites().size()});
    tiles_.push_back({"最近游玩", "RECENT", -3, GamePlatform::NES,
                      (int)app.library.recent(20).size()});
    tiles_.push_back({"设置", "SETTINGS", -4, GamePlatform::NES, 0});
    tiles_.push_back({"关于", "ABOUT", -5, GamePlatform::NES, 0});
    tiles_.push_back({"退出", "EXIT", -6, GamePlatform::NES, 0});
    if (sel_ >= (int)tiles_.size()) sel_ = 0;
    app.covers.requestFetchAllMissing(app.library.games());
  }

  void update(App& app, float dt, const InputState& in) override {
    (void)dt;
    if ((int)app.library.games().size() != libCount_) {
      libCount_ = (int)app.library.games().size();
      rebuild(app);
    }
    const int N = (int)tiles_.size();
    if (N > 0) {
      if (in.pressed[(int)Btn::Left]) sel_ = (sel_ + N - 1) % N;
      if (in.pressed[(int)Btn::Right]) sel_ = (sel_ + 1) % N;
      if (in.pressed[(int)Btn::Ok]) activate(app, tiles_[(size_t)sel_]);
      if (in.pressed[(int)Btn::Menu]) app.pushScene(createSettingsScene());
    }
    if (in.pressed[(int)Btn::Back]) app.requestExit();
  }

  void activate(App& app, const Tile& t) {
    switch (t.action) {
      case -1: app.pushScene(createLibraryScene(-1)); break;
      case -2: app.pushScene(createLibraryScene(-2)); break;
      case -3: app.pushScene(createLibraryScene(-3)); break;
      case -4: app.pushScene(createSettingsScene()); break;
      case -5: app.pushScene(createAboutScene()); break;
      case -6: app.requestExit(); break;
      default:
        app.pushScene(createLibraryScene((int)t.platform));
        break;
    }
  }

  void draw(App& app) override {
    const Skin& sk = Theme::skin();
    Draw& d = app.draw;
    appW_ = (float)app.draw.W;

    // FSD 深蓝渐变背景 + 斜向光带
    for (int i = 0; i < 40; i++) {
      const float t = (float)i / 40;
      d.rect(Rect(0, app.draw.H * t, app.draw.W, app.draw.H / 40 + 1),
             Color8::hex(0x081426).lerp(Color8::hex(0x123B6E), t * 0.9f));
    }
    // 斜向光带（两条半透明白蓝）
    for (int k = 0; k < 2; k++) {
      const float x0 = app.draw.W * (0.25f + 0.4f * k) - 60;
      for (int i = 0; i < 50; i++) {
        const float t = (float)i / 50;
        d.line(x0 + t * 130, 0, x0 + t * 130 + 200, (float)app.draw.H,
               Color8::hex(0x9FB6D4, 16), 26);
      }
    }

    // —— 顶部状态条 ——
    const std::string status = "NesStation PC · " + std::to_string(app.library.games().size()) +
                               " 个游戏 · " + std::to_string(app.library.favorites().size()) +
                               " 个收藏";
    app.font.draw(app.sdlRenderer(), status, 30, 22, 14, Color8::hex(0x9FB6D4));
    app.font.draw(app.sdlRenderer(), clockText(), app.draw.W - 90, 18, 20,
                  Color8::hex(0xE8F1FF), true);

    // —— 面包屑 ——
    app.font.draw(app.sdlRenderer(), "主菜单 ▪ 游戏库", 30, 50, 18, Color8::hex(0xE8F1FF),
                  true);
    d.line(30, 78, 30 + app.font.textWidth("主菜单 ▪ 游戏库", 18, true) + 20, 78,
           Color8::hex(0x2E86E0), 2);

    // —— 磁贴流 ——
    const float cy = app.draw.H * 0.46f;
    const float bigW = 240, bigH = 300;
    for (int dist = 4; dist >= 1; dist--) {
      for (int sign : {-1, 1}) {
        const int idx = sel_ + sign * dist;
        if (idx < 0 || idx >= (int)tiles_.size()) continue;
        drawTile(app, cxOf(sign * dist), cy, bigW * 0.62f, bigH * 0.62f,
                 tiles_[(size_t)idx], false);
      }
    }
    if (!tiles_.empty()) drawTile(app, cxOf(0), cy, bigW, bigH, tiles_[(size_t)sel_], true);

    // —— 底部信息 ——
    if (!tiles_.empty()) {
      const Tile& t = tiles_[(size_t)sel_];
      app.font.draw(app.sdlRenderer(), t.label, 40, app.draw.H - 110, 28,
                    Color8::hex(0xE8F1FF), true);
      std::string sub = t.sub;
      if (t.count > 0) sub += " · " + std::to_string(t.count) + " 个游戏";
      if (t.sub.empty()) sub = std::to_string(t.count) + " 个游戏";
      app.font.draw(app.sdlRenderer(), sub, 40, app.draw.H - 74, 15,
                    Color8::hex(0x9FB6D4));
      // 蓝/黄对角装饰
      d.line(40, app.draw.H - 40, 180, app.draw.H - 40, Color8::hex(0x2E86E0), 4);
      d.line(180, app.draw.H - 40, 230, app.draw.H - 40, Color8::hex(0xF8E71C), 4);
    }

    w::hintBar(app, {{"A", "打开"}, {"Start", "设置"}, {"B", "退出"}});
  }

  float cxOf(int dist) const { return appW_ / 2 + (float)dist * 175.0f; }

  void drawTile(App& app, float cx, float cy, float w, float h, const Tile& t, bool selected) {
    Draw& d = app.draw;
    const Rect rc(cx - w / 2, cy - h / 2, w, h);
    if (selected) {
      d.glowRect(rc, 10, Color8::hex(0x2E86E0, 150), 16);
      d.roundRect(rc, 10, Color8::hex(0x123B6E, 235));
      d.roundRectOutline(rc, 10, Color8::hex(0x2E86E0), 3);
    } else {
      d.roundRect(rc, 10, Color8::hex(0x0D2C55, 215));
      d.roundRectOutline(rc, 10, Color8::hex(0x2E6DB4), 1.4f);
    }
    // 对角色块（蓝/黄）
    if (selected) {
      d.rect(Rect(rc.x + 10, rc.y + 10, 44, 6), Color8::hex(0xF8E71C));
      d.rect(Rect(rc.x + 10, rc.y + 18, 26, 6), Color8::hex(0x2E86E0));
    }
    // 平台色圆 + 平台名
    const Color8 pc = selected ? Color8::hex(0xE8F1FF) : Color8::hex(0x9FB6D4);
    const float ty = rc.cy() - 16;
    app.font.drawVCentered(app.sdlRenderer(), t.label, rc.cx() - app.font.textWidth(t.label, 22, selected) / 2, ty, 22,
                           pc, selected);
    if (!t.sub.empty()) {
      const float tw2 = app.font.textWidth(t.sub, 12);
      app.font.drawVCentered(app.sdlRenderer(), t.sub, rc.cx() - tw2 / 2, ty + 26, 12,
                             Color8::hex(0x4A5568));
    }
    if (t.count > 0) {
      const std::string n = std::to_string(t.count);
      const float nw = app.font.textWidth(n, 30, true);
      app.font.drawVCentered(app.sdlRenderer(), n, rc.cx() - nw / 2, rc.bottom() - 44, 30,
                             selected ? Color8::hex(0xF8E71C) : Color8::hex(0x2E86E0), true);
    }
    // 鼠标点击
    if (rc.contains(app.input.input().mouse) && app.input.input().mousePressed) {
      activate(app, t);
    } else if (rc.contains(app.input.input().mouse)) {
      // 悬停即选中
      for (size_t i = 0; i < tiles_.size(); i++)
        if (tiles_[i].action == t.action &&
            tiles_[i].platform == t.platform) {
          sel_ = (int)i;
          break;
        }
    }
  }

 private:
  std::vector<Tile> tiles_;
  int sel_ = 0;
  int libCount_ = -1;
  float appW_ = 1280;
};

}  // namespace

std::unique_ptr<Scene> createHomeScene() {
  if (Theme::isNeon()) return std::unique_ptr<Scene>(new NeonHome());
  return std::unique_ptr<Scene>(new FsdHome());
}

}  // namespace ns
