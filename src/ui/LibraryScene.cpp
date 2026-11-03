// NesStation PC — 游戏库：平台过滤 + 封面网格 + 详情 + ROM 导入 / 重命名 / 移除
#include <algorithm>
#include <cmath>

#include "ui/Scenes.h"
#include "ui/Widgets.h"
#include "util/Fs.h"

namespace ns {
namespace {

class LibraryScene : public Scene {
 public:
  explicit LibraryScene(int platformFilter)
      : filter_(platformFilter), focusArea_(0) /* 0=过滤列表 1=网格 2=详情*/ {}

  void onEnter(App& app) override { rebuild(app); }

  void rebuild(App& app) {
    // 平台列表（仅有游戏的平台 + 全部/收藏/最近）
    cats_.clear();
    cats_.push_back({-1, "全部"});
    cats_.push_back({-2, "收藏"});
    cats_.push_back({-3, "最近"});
    for (GamePlatform p : allPlatforms()) {
      if (app.library.countFor(p) > 0) cats_.push_back({(int)p, platformShortName(p)});
    }
    if (catSel_ >= (int)cats_.size()) catSel_ = 0;
    if (filter_ != -9) {
      // 从主页带进来的过滤
      for (size_t i = 0; i < cats_.size(); i++) {
        if (cats_[i].id == filter_) {
          catSel_ = (int)i;
          break;
        }
      }
      filter_ = -9;
    }
    refreshGames(app);
  }

  void refreshGames(App& app) {
    games_.clear();
    const int id = cats_[(size_t)catSel_].id;
    if (id == -1) {
      games_ = app.library.games();
    } else if (id == -2) {
      games_ = app.library.favorites();
    } else if (id == -3) {
      games_ = app.library.recent(100);
    } else {
      games_ = app.library.byPlatform((GamePlatform)id);
    }
    std::sort(games_.begin(), games_.end(), [](const GameEntry& a, const GameEntry& b) {
      return a.displayTitle() < b.displayTitle();
    });
    if (gridSel_ >= (int)games_.size()) gridSel_ = 0;
    // 封面排队（可见范围）
    const int cols = columns(app);
    const int rows = 3;
    for (int i = 0; i < cols * rows && i < (int)games_.size(); i++) {
      app.covers.requestFetch(games_[(size_t)i]);
    }
  }

  int columns(App& app) const {
    const float gridW = app.draw.W - 300;
    return std::max(2, (int)(gridW / 185));
  }

  void update(App& app, float dt, const InputState& in) override {
    (void)dt;
    if (in.pressed[(int)Btn::Back]) {
      if (renaming_) {
        // 重命名中：Esc 仅取消重命名，不关详情/不退出场景
        renaming_ = false;
        app.input.textInputStop();
      } else if (detailOpen_) {
        detailOpen_ = false;
      } else {
        app.popScene();
      }
      return;
    }
    if (in.pressed[(int)Btn::Menu]) app.pushScene(createSettingsScene());

    if (detailOpen_) {
      updateDetail(app, in);
      return;
    }

    // —— 区域导航 ——
    const int N = (int)games_.size();
    if (focusArea_ == 0) {
      if ((in.pressed[(int)Btn::Right] || in.pressed[(int)Btn::Down]) && N > 0) focusArea_ = 1;
      if (in.pressed[(int)Btn::Up]) catSel_ = (catSel_ + (int)cats_.size() - 1) % (int)cats_.size();
      if (in.pressed[(int)Btn::Down] && catSel_ < (int)cats_.size() - 1) {
        // Down 在列表内移动，到底后跳网格
        if (catSel_ == (int)cats_.size() - 1 && N > 0) {
          focusArea_ = 1;
        } else {
          catSel_ = (catSel_ + 1) % (int)cats_.size();
        }
      }
      if (in.pressed[(int)Btn::Ok]) {
        refreshGames(app);
        gridSel_ = 0;
        if (N > 0) focusArea_ = 1;
      }
    } else {
      if (N > 0) {
        const int cols = columns(app);
        if (in.pressed[(int)Btn::Left]) gridSel_ = (gridSel_ + N - 1) % N;
        if (in.pressed[(int)Btn::Right]) gridSel_ = (gridSel_ + 1) % N;
        if (in.pressed[(int)Btn::Up]) {
          if (gridSel_ - cols >= 0) gridSel_ -= cols;
          else focusArea_ = 0;
        }
        if (in.pressed[(int)Btn::Down]) {
          if (gridSel_ + cols < N) gridSel_ += cols;
        }
        if (in.pressed[(int)Btn::Ok]) {
          detailOpen_ = true;
          detailSel_ = 0;
        }
        if (in.pressed[(int)Btn::Fav]) {
          app.library.toggleFavorite(games_[(size_t)gridSel_].id);
          app.library.save();
          refreshGames(app);
        }
        // 滚动跟随
        ensureVisible(app);
      } else {
        focusArea_ = 0;
      }
    }
  }

  void ensureVisible(App& app) {
    const int cols = columns(app);
    const int row = gridSel_ / cols;
    const float rowH = 250;
    if (scroll_ > row * rowH) scroll_ = (float)row * rowH;
    const float viewH = gridBounds(app).h;
    if ((row + 1) * rowH > scroll_ + viewH) scroll_ = (row + 1) * rowH - viewH;
    if (scroll_ < 0) scroll_ = 0;
  }

  Rect gridBounds(App& app) const {
    return Rect(286, 120, app.draw.W - 286 - 24, app.draw.H - 170);
  }

  void updateDetail(App& app, const InputState& in) {
    const int N = 6;  // 启动/收藏/重命名/封面刷新/移除/关闭
    if (in.pressed[(int)Btn::Up]) detailSel_ = (detailSel_ + N - 1) % N;
    if (in.pressed[(int)Btn::Down]) detailSel_ = (detailSel_ + 1) % N;
    if (in.pressed[(int)Btn::Ok]) {
      const GameEntry& g = games_[(size_t)gridSel_];
      switch (detailSel_) {
        case 0:
          detailOpen_ = false;
          app.launchGame(g);
          break;
        case 1:
          app.library.toggleFavorite(g.id);
          app.library.save();
          refreshGames(app);
          break;
        case 2:
          renaming_ = true;
          renameBuf_ = g.customTitle;
          app.input.textClear();
          app.input.textInputStart();
          break;
        case 3:
          app.library.find(g.id)->coverPath.clear();
          app.invalidateTexture(fsutil::join(app.library.coverDir(), g.id + ".png"));
          app.library.save();
          app.covers.requestFetch(g);
          app.toast("已重新排队获取封面");
          break;
        case 4:
          app.library.remove(g.id);
          app.library.save();
          detailOpen_ = false;
          refreshGames(app);
          break;
        case 5:
          detailOpen_ = false;
          break;
      }
    }
    if (renaming_) {
      renameBuf_ = app.input.textBuffer();
    }
  }

  void draw(App& app) override {
    const Skin& sk = Theme::skin();
    Draw& d = app.draw;

    // 背景
    for (int i = 0; i < 40; i++) {
      const float t = (float)i / 40;
      d.rect(Rect(0, app.draw.H * t, app.draw.W, app.draw.H / 40 + 1),
             sk.bg.lerp(sk.bgDeep, t));
    }

    if (w::header(app, "游戏库", std::to_string(games_.size()) + " 个游戏", true)) {
      if (renaming_) {
        renaming_ = false;
        app.input.textInputStop();
      } else if (detailOpen_) {
        detailOpen_ = false;
      } else {
        app.popScene();
      }
      return;
    }

    // —— 左侧过滤列表 ——
    float y = 120;
    for (size_t i = 0; i < cats_.size(); i++) {
      const Rect rc(24, y, 240, 44);
      const bool focused = focusArea_ == 0 && catSel_ == (int)i;
      std::string sub;
      if (cats_[i].id == -1) sub = std::to_string(app.library.games().size());
      else if (cats_[i].id == -2) sub = std::to_string(app.library.favorites().size());
      else if (cats_[i].id == -3) sub = std::to_string(app.library.recent(100).size());
      else sub = std::to_string(app.library.countFor((GamePlatform)cats_[i].id));
      if (w::listRow(app, rc, cats_[i].name, sub, focused)) {
        catSel_ = (int)i;
        refreshGames(app);
        focusArea_ = 1;
        gridSel_ = 0;
      }
      y += 50;
    }
    // 添加 ROM 目录按钮
    const Rect addRc(24, y + 10, 240, 44);
    if (w::button(app, addRc, "＋ 添加 ROM 目录", focusArea_ == 0 && false)) {
      startAddRomDir(app);
    }

    // —— 封面网格 ——
    const Rect grid = gridBounds(app);
    d.pushClip(grid);
    const int cols = columns(app);
    const float cw = 172, ch = 236;
    for (int idx = 0; idx < (int)games_.size(); idx++) {
      const int col = idx % cols;
      const int row = idx / cols;
      const float gx = grid.x + col * 185 + 6;
      const float gy = grid.y + row * 250 - scroll_ + 4;
      if (gy > grid.bottom() || gy + ch < grid.y - 260) continue;
      const Rect rc(gx, gy, cw, ch);
      const bool focused = focusArea_ == 1 && gridSel_ == idx;
      w::coverCard(app, rc, games_[(size_t)idx], focused);
      // 鼠标
      if (rc.contains(app.input.input().mouse)) {
        gridSel_ = idx;
        focusArea_ = 1;
        if (app.input.input().mousePressed) {
          detailOpen_ = true;
          detailSel_ = 0;
        }
      }
    }
    d.popClip();
    // 滚轮
    if (grid.contains(app.input.input().mouse)) {
      scroll_ -= (float)app.input.input().wheel * 60;
      const float maxScroll =
          std::max(0.0f, (float)((int)games_.size() + cols - 1) / cols * 250 - grid.h);
      scroll_ = std::max(0.0f, std::min(scroll_, maxScroll));
    }
    // 滚动条
    if ((int)games_.size() > cols * 3) {
      w::scrollIndicator(app, Rect(app.draw.W - 18, grid.y, 6, grid.h),
                         std::min(1.0f, grid.h / 750.0f), scroll_ / std::max(1.0f, (float)((int)games_.size() + cols - 1) / cols * 250 - grid.h));
    }

    if (detailOpen_) drawDetail(app);
    if (renaming_) drawRename(app);

    w::keyHintBar(app, {{"A", "详情"}, {"X / F", "收藏"}, {"B / Esc", "返回"},
                        {"Start", "设置"}});
  }

  void startAddRomDir(App& app) {
    BrowserRequest req;
    req.title = "选择 ROM 目录";
    req.mode = 0;
    req.startDir = app.library.romDirs().empty() ? fsutil::exeDir() : app.library.romDirs().back();
    req.onPick = [&app](const std::string& dir) {
      app.library.addRomDir(dir);
      app.library.scanDirectory(dir, GamePlatform::NES, false, app.config.scanRecursive);
      app.library.save();
      app.toast("ROM 目录已添加并扫描");
    };
    app.pushScene(createFileBrowser(req));
  }

  void drawDetail(App& app) {
    const Skin& sk = Theme::skin();
    Draw& d = app.draw;
    d.rect(Rect(0, 0, app.draw.W, app.draw.H), Color8(0, 0, 0, 150));
    const float pw = 560, ph = 360;
    Rect panel(app.draw.W / 2 - pw / 2, app.draw.H / 2 - ph / 2, pw, ph);
    skinPanel(d, panel, true, 16);

    const GameEntry& g = games_[(size_t)gridSel_];
    // 左侧封面
    const Rect cover(panel.x + 24, panel.y + 24, 172, 236);
    SDL_Texture* tex = w::coverTexture(app, g);
    if (tex) {
      d.textureAspect(tex, cover);
    } else {
      w::platformPlaceholder(app, cover, g.platform, g.displayTitle());
    }
    // 信息
    app.font.draw(app.sdlRenderer(), app.font.ellipsize(g.displayTitle(), 24, pw - 240, true),
                  panel.x + 220, panel.y + 28, 24, sk.textHi, true);
    const Color8 pc = Color8::hex(platformAccent(g.platform));
    app.font.draw(app.sdlRenderer(),
                  std::string(platformShortName(g.platform)) + " · " + platformFullName(g.platform),
                  panel.x + 220, panel.y + 62, 14, pc);
    app.font.draw(app.sdlRenderer(), "ROM：" + app.font.ellipsize(fsutil::baseOf(g.romPath), 12, pw - 250),
                  panel.x + 220, panel.y + 86, 12, sk.textDim);
    app.font.draw(app.sdlRenderer(),
                  "上次：" + (g.lastPlayedAt > 0 ? std::to_string(g.lastPlayedAt) : "从未") +
                      " · 时长 " + std::to_string(g.playTimeSec / 60) + " 分钟",
                  panel.x + 220, panel.y + 106, 12, sk.textDim);

    // 操作
    const char* items[6] = {"启动游戏", g.favorite ? "取消收藏" : "加入收藏", "重命名",
                            "重新获取封面", "从库中移除", "关闭"};
    float y = panel.y + 150;
    for (int i = 0; i < 6; i++) {
      const Rect rc(panel.x + 220, y, pw - 244, 40);
      const bool danger = i == 4;
      if (w::listRow(app, rc, items[i], "", detailSel_ == i)) {
        // 点击直接执行
        detailSel_ = i;
        detailAction(app, i);
      }
      if (danger) {
        app.font.draw(app.sdlRenderer(), "!", rc.right() - 24, rc.y + 10, 16, sk.red, true);
      }
      y += 46;
    }
    // 平台核心提示（按候选链解析，可能是替代核心）
    const PlatformCore coreInUse = app.cores.resolveCore(g.platform);
    std::string coreLine = std::string("核心：") + coreInUse.coreTitle;
    if (coreInUse.kind == CoreKind::Libretro &&
        std::string(coreInUse.coreFile) != std::string(platformCore(g.platform).coreFile)) {
      coreLine += "（替代）";
    }
    app.font.draw(app.sdlRenderer(), coreLine,
                  panel.x + 24, panel.bottom() - 34, 12, sk.textDim);
  }

  void detailAction(App& app, int idx) {
    const GameEntry& g = games_[(size_t)gridSel_];
    switch (idx) {
      case 0:
        detailOpen_ = false;
        app.launchGame(g);
        break;
      case 1:
        app.library.toggleFavorite(g.id);
        app.library.save();
        refreshGames(app);
        break;
      case 2:
        renaming_ = true;
        renameBuf_ = g.customTitle;
        app.input.textClear();
        app.input.textInputStart();
        break;
      case 3: {
        if (GameEntry* e = app.library.find(g.id)) e->coverPath.clear();
        app.library.save();
        app.covers.requestFetch(g);
        app.toast("已重新排队获取封面");
        break;
      }
      case 4:
        app.library.remove(g.id);
        app.library.save();
        detailOpen_ = false;
        refreshGames(app);
        break;
      case 5:
        detailOpen_ = false;
        break;
    }
  }

  void drawRename(App& app) {
    const Skin& sk = Theme::skin();
    Draw& d = app.draw;
    d.rect(Rect(0, 0, app.draw.W, app.draw.H), Color8(0, 0, 0, 170));
    const float pw = 480;
    Rect panel(app.draw.W / 2 - pw / 2, app.draw.H / 2 - 80, pw, 160);
    skinPanel(d, panel, true, 12);
    app.font.draw(app.sdlRenderer(), "重命名（显示名，不改文件）", panel.x + 24, panel.y + 20,
                  17, sk.textHi, true);
    const Rect input(panel.x + 24, panel.y + 60, pw - 48, 44);
    d.roundRect(input, 6, Color8(sk.bgDeep.r, sk.bgDeep.g, sk.bgDeep.b, 220));
    d.roundRectOutline(input, 6, sk.accent, 1.6f);
    app.font.drawVCentered(app.sdlRenderer(), renameBuf_.empty() ? "输入名称…" : renameBuf_,
                           input.x + 12, input.cy(), 16,
                           renameBuf_.empty() ? sk.textDim : sk.text);
    const Rect ok(panel.right() - 120, panel.bottom() - 56, 100, 40);
    if (w::button(app, ok, "确定", true, 16, true) ||
        (app.input.input().pressed[(int)Btn::Ok] && !renameBuf_.empty())) {
      if (GameEntry* e = app.library.find(games_[(size_t)gridSel_].id)) {
        app.library.rename(e->id, renameBuf_);
        app.library.save();
        refreshGames(app);
      }
      renaming_ = false;
      app.input.textInputStop();
    }
    w::keyHintBar(app, {{"Enter / 退格", "输入"}, {"Esc", "取消"}});
  }

 private:
  struct Cat {
    int id;
    std::string name;
  };

  int filter_;
  std::vector<Cat> cats_;
  std::vector<GameEntry> games_;
  int catSel_ = 0;
  int gridSel_ = 0;
  int focusArea_ = 0;
  bool detailOpen_ = false;
  int detailSel_ = 0;
  bool renaming_ = false;
  std::string renameBuf_;
  float scroll_ = 0;
};

}  // namespace

std::unique_ptr<Scene> createLibraryScene(int platformFilter) {
  return std::unique_ptr<Scene>(new LibraryScene(platformFilter));
}

}  // namespace ns
