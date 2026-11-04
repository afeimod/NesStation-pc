// NesStation PC — 游戏库（★ 1:1 移植 Android 版双风格布局）
//
//   · Neon 3D（NeonLibraryScreen.kt）：
//       左侧 220dp 核心列表（返回主页 + 模式 + 平台 SideEntry 彩点 + 数量 +
//       底部搜索框 + 导入/刷新图标按钮）
//       右侧：平台全名大标题 + 时代副标题 + 3D 封面流（NeonFlow 同参数）+
//       底部信息条（标题 + 徽章 + 目录名 + ★ 收藏 + 删除 + 开始按钮）
//   · FSD 经典（LibraryScreen.kt）：
//       顶栏（返回 + 标题 + 计数 + 导入/刷新）+ 平台过滤 chips + 方形封面网格
//   · 交互：右键卡片快捷菜单 / 双击或 Enter 启动 / F 收藏 / 拖拽滚动
#include <algorithm>
#include <cmath>

#include "emu/EmuRunner.h"
#include "ui/Scenes.h"
#include "ui/Widgets.h"
#include "util/Fs.h"

namespace ns {
namespace {

class LibraryScene : public Scene {
 public:
  explicit LibraryScene(int platformFilter) : filter_(platformFilter) {}

  void onEnter(App& app) override { rebuild(app); }

  void rebuild(App& app) {
    cats_.clear();
    cats_.push_back({-1, "全部游戏", "ALL"});
    cats_.push_back({-2, "收藏", "FAV"});
    cats_.push_back({-3, "最近游玩", "RECENT"});
    for (GamePlatform p : allPlatforms()) {
      if (app.library.countFor(p) > 0)
        cats_.push_back({(int)p, platformShortName(p), ""});
    }
    if (catSel_ >= (int)cats_.size()) catSel_ = 0;
    if (filter_ != -9) {
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
    if (id != -3) {
      std::sort(games_.begin(), games_.end(), [](const GameEntry& a, const GameEntry& b) {
        return a.displayTitle() < b.displayTitle();
      });
    }
    if (!searchBuf_.empty()) {
      std::string q = searchBuf_;
      std::transform(q.begin(), q.end(), q.begin(),
                     [](unsigned char c) { return (char)std::tolower(c); });
      games_.erase(std::remove_if(games_.begin(), games_.end(),
                                  [&q](const GameEntry& g) {
                                    std::string t = g.displayTitle();
                                    std::transform(t.begin(), t.end(), t.begin(),
                                                   [](unsigned char c) {
                                                     return (char)std::tolower(c);
                                                   });
                                    return t.find(q) == std::string::npos;
                                  }),
                   games_.end());
    }
    if (gridSel_ >= (int)games_.size()) gridSel_ = 0;
    const int want = 12;
    for (int i = 0; i < want && i < (int)games_.size(); i++) {
      app.covers.requestFetch(games_[(size_t)i]);
    }
  }

  int columns(App& app) const {
    const float gridW = app.draw.W - 320;
    return std::max(2, (int)(gridW / 150));
  }

  // ============ 输入 ============
  void update(App& app, float dt, const InputState& in) override {
    (void)dt;
    if (searching_) {
      searchBuf_ = app.input.textBuffer();
      if (in.pressed[(int)Btn::Back]) {
        stopEditing(app);
        return;
      }
      if (in.pressed[(int)Btn::Ok]) {
        stopEditing(app);
        return;
      }
      static std::string lastBuf;
      if (searchBuf_ != lastBuf) {
        lastBuf = searchBuf_;
        refreshGames(app);
      }
      return;
    }
    static std::string lastBuf;
    lastBuf.clear();

    if (in.pressed[(int)Btn::Back]) {
      if (renaming_) {
        renaming_ = false;
        app.input.textInputStop();
      } else if (detailOpen_) {
        detailOpen_ = false;
      } else if (!searchBuf_.empty()) {
        searchBuf_.clear();
        refreshGames(app);
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

    const int N = (int)games_.size();
    // 封面流横向：←→（Neon 风格也是 ←→ 换卡）
    if (N > 0) {
      if (in.pressed[(int)Btn::Left] && gridSel_ > 0) {
        gridSel_--;
        app.covers.requestFetch(games_[(size_t)gridSel_]);
      }
      if (in.pressed[(int)Btn::Right] && gridSel_ < N - 1) {
        gridSel_++;
        app.covers.requestFetch(games_[(size_t)gridSel_]);
      }
      if (Theme::isNeon()) {
        if (in.pressed[(int)Btn::Up] && catSel_ > 0) {
          catSel_--;
          refreshGames(app);
          gridSel_ = 0;
        }
        if (in.pressed[(int)Btn::Down] && catSel_ < (int)cats_.size() - 1) {
          catSel_++;
          refreshGames(app);
          gridSel_ = 0;
        }
      } else {
        const int cols = columns(app);
        if (in.pressed[(int)Btn::Up] && gridSel_ - cols >= 0) gridSel_ -= cols;
        if (in.pressed[(int)Btn::Down] && gridSel_ + cols < N) gridSel_ += cols;
      }
      if (in.pressed[(int)Btn::Ok]) app.launchGame(games_[(size_t)gridSel_]);
      if (in.pressed[(int)Btn::Fav]) {
        app.library.toggleFavorite(games_[(size_t)gridSel_].id);
        app.library.save();
        refreshGames(app);
      }
    }
    animSel_ += ((float)gridSel_ - animSel_) * std::min(1.0f, dt * 11.0f);
  }

  void stopEditing(App& app) {
    searching_ = false;
    app.input.textInputStop();
    refreshGames(app);
  }

  void startAddRomDir(App& app) {
    BrowserRequest req;
    req.title = "导入游戏文件夹";
    req.mode = 0;
    req.startDir = app.library.romDirs().empty() ? fsutil::homeDir()
                                                 : app.library.romDirs().back();
    req.onPick = [&app](const std::string& dir) {
      app.library.addRomDir(dir);
      const int n = app.library.scanDirectory(dir, GamePlatform::NES, false,
                                              app.config.scanRecursive);
      app.library.save();
      app.toast("已添加目录，本次扫描新增 " + std::to_string(n) + " 个游戏");
    };
    app.pushScene(createFileBrowser(req));
  }

  void rescanAll(App& app) {
    int n = 0;
    for (const auto& dir : app.library.romDirs())
      n += app.library.scanDirectory(dir, GamePlatform::NES, false,
                                     app.config.scanRecursive);
    app.library.save();
    rebuild(app);
    app.toast("重新扫描完成：新增 " + std::to_string(n) + " · 库内共 " +
              std::to_string(app.library.games().size()) + " 个游戏");
  }

  // ============ 绘制 ============
  void draw(App& app) override {
    if (Theme::isNeon()) {
      drawNeon(app);
    } else {
      drawClassic(app);
    }
    if (detailOpen_) drawDetail(app);
    if (renaming_) drawRename(app);
  }

  // —— NeonBackdrop（深空渐变 + 光带）——
  void drawBackdrop(App& app) {
    const Skin& sk = Theme::skin();
    Draw& d = app.draw;
    for (int i = 0; i < 40; i++) {
      const float t = (float)i / 40;
      d.rect(Rect(0, d.H * t, d.W, d.H / 40 + 1), sk.bg.lerp(sk.bgDeep, t));
    }
    for (int i = 0; i < 12; i++) {
      const float a = 26.0f * (1.0f - (float)i / 12);
      d.rect(Rect(0, i * 4, d.W, 5),
             Color8(sk.accent.r, sk.accent.g, sk.accent.b, (uint8_t)a));
    }
    for (int i = 0; i < 12; i++) {
      const float a = 30.0f * (1.0f - (float)i / 12);
      d.rect(Rect(0, d.H - i * 4 - 5, d.W, 5),
             Color8(sk.accentDim.r, sk.accentDim.g, sk.accentDim.b, (uint8_t)a));
    }
  }

  // ==================== Neon 3D 风格 ====================
  void drawNeon(App& app) {
    const Skin& sk = Theme::skin();
    Draw& d = app.draw;
    const InputState& in = app.input.input();
    drawBackdrop(app);

    // —— 左侧 220dp 核心列表（BgPanel 60%）——
    const float navW = 230;
    d.rect(Rect(0, 0, navW, (float)d.H), Color8(sk.bgPanel.r, sk.bgPanel.g, sk.bgPanel.b, 153));
    float y = 14;
    // 返回主页
    {
      const Rect rc(12, y, navW - 24, 38);
      const bool hov = rc.contains(in.mouse);
      if (hov) d.chamferRect(rc, 11, sk.bgPanelHi);
      w::iconArrowLeft(app, Rect(rc.x + 12, rc.cy() - 8, 16, 16), hov ? sk.accent : sk.accent);
      app.font.drawVCentered(app.sdlRenderer(), "返回主页", rc.x + 36, rc.cy(), 14,
                             hov ? sk.textHi : sk.text);
      if (w::activated(app, rc, false)) app.popScene();
      y += 46;
    }
    // 模式标签
    {
      const std::string mode = cats_[(size_t)catSel_].id == -2
                                   ? "收藏"
                                   : (cats_[(size_t)catSel_].id == -3 ? "最近游玩" : "全部游戏");
      app.font.draw(app.sdlRenderer(), mode, 28, y + 2, 11, sk.textDim);
      y += 18;
    }
    // 平台 SideEntry 列表（可滚动：滚轮 + 拖拽）
    {
      const float listTop = y;
      const float listBottom = (float)d.H - 214;  // 给底部搜索/按钮区留位
      const Rect listRc(0, listTop, navW, listBottom - listTop);
      sideScroll_ += w::dragScroll(app, listRc);
      if (in.wheel != 0 && listRc.contains(in.mouse)) sideScroll_ -= (float)in.wheel * 40;
      const float total = (float)cats_.size() * 36;
      const float maxScroll = total > listRc.h ? total - listRc.h : 0;
      sideScroll_ = std::max(0.0f, std::min(sideScroll_, maxScroll));
      d.pushClip(listRc);
      float sy = listTop - sideScroll_;
      for (size_t i = 0; i < cats_.size(); i++) {
        const Rect rc(8, sy, navW - 16, 34);
        if (rc.bottom() < listTop - 40 || rc.y > listBottom + 40) {
          sy += 36;
          continue;
        }
        const bool selected = catSel_ == (int)i;
        // SideEntry：选中 BgPanelHi 底 + 彩点 + 文字 + 数量
        if (selected) d.chamferRect(rc, 10, sk.bgPanelHi);
        uint32_t c1 = 0, c2 = 0;
        if (cats_[i].id >= 0) {
          platformGradient((GamePlatform)cats_[i].id, &c1, &c2);
        }
        const Color8 dotCol = cats_[i].id >= 0
                                  ? Color8::hex(c1)
                                  : (cats_[i].id == -2 ? sk.gold : sk.accent);
        d.rect(Rect(rc.x + 12, rc.cy() - 3, 6, 6),
               selected ? dotCol : sk.textDim);
        app.font.drawVCentered(app.sdlRenderer(), cats_[i].name, rc.x + 30, rc.cy(), 13,
                               selected ? sk.textHi : sk.text, selected);
        std::string cnt;
        if (cats_[i].id == -1) cnt = std::to_string(app.library.games().size());
        else if (cats_[i].id == -2) cnt = std::to_string(app.library.favorites().size());
        else if (cats_[i].id == -3) cnt = std::to_string(app.library.recent(100).size());
        else cnt = std::to_string(app.library.countFor((GamePlatform)cats_[i].id));
        app.font.drawVCentered(app.sdlRenderer(), cnt,
                               rc.right() - 14 - app.font.textWidth(cnt, 11), rc.cy(), 11,
                               sk.textDim);
        if (w::activated(app, rc, false)) {
          catSel_ = (int)i;
          refreshGames(app);
          gridSel_ = 0;
        }
        sy += 36;
      }
      d.popClip();
    }
    // 底部：搜索框 + 导入 / 刷新（图标按钮；避开底部信息条 H-92）
    {
      const float by = (float)d.H - 202;
      const Rect src(14, by, navW - 28, 38);
      const bool hov = src.contains(in.mouse) || searching_;
      d.chamferRect(src, 10, hov ? sk.bgPanelHi : sk.bgPanel);
      d.chamferRectOutline(src, 10, searching_ ? sk.accent : sk.line, 1.2f);
      w::iconSearch(app, Rect(src.x + 10, src.y + 9, 18, 18), sk.textDim);
      const std::string& shown =
          searchBuf_.empty() && !searching_ ? std::string("搜索游戏…") : searchBuf_;
      app.font.drawVCentered(app.sdlRenderer(), shown.empty() && searching_ ? "|" : shown,
                             src.x + 36, src.cy(), 13,
                             searchBuf_.empty() && !searching_ ? sk.textDim : sk.textHi);
      if (hov && in.mousePressed && !searching_) {
        searching_ = true;
        app.input.textClear();
        app.input.textInputStart();
      }
      const float bw = (navW - 28 - 8) / 2;
      const Rect imp(14, by + 46, bw, 38);
      const bool impHov = imp.contains(in.mouse);
      if (impHov) {
        d.chamferRect(imp, 10, sk.bgPanelHi);
        d.chamferRectOutline(imp, 10, sk.accent, 1.2f);
      } else {
        d.chamferRect(imp, 10, sk.bgPanelHi);
      }
      w::iconFolder(app, Rect(imp.cx() - 8, imp.cy() - 8, 16, 16), sk.green);
      if (w::activated(app, imp, false)) startAddRomDir(app);
      const Rect rfs(14 + bw + 8, by + 46, bw, 38);
      const bool rfsHov = rfs.contains(in.mouse);
      if (rfsHov) {
        d.chamferRect(rfs, 10, sk.bgPanelHi);
        d.chamferRectOutline(rfs, 10, sk.accent, 1.2f);
      } else {
        d.chamferRect(rfs, 10, sk.bgPanelHi);
      }
      w::iconRefresh(app, Rect(rfs.cx() - 8, rfs.cy() - 8, 16, 16), sk.blue);
      if (w::activated(app, rfs, false)) rescanAll(app);
    }

    // —— 右侧内容 ——
    const float rx = navW + 26;
    const float rw = (float)d.W - rx - 26;
    // 顶部平台标题 + 时代副标题
    {
      const int id = cats_[(size_t)catSel_].id;
      std::string title;
      Color8 subCol = sk.accent;
      std::string sub;
      if (id >= 0) {
        const GamePlatform p = (GamePlatform)id;
        title = platformFullName(p);
        sub = platformEra(p);
        uint32_t c1 = 0, c2 = 0;
        platformGradient(p, &c1, &c2);
        subCol = Color8::hex(c1);
      } else {
        title = cats_[(size_t)catSel_].id == -2
                    ? "收藏的游戏"
                    : (cats_[(size_t)catSel_].id == -3 ? "最近游玩" : "全部游戏");
        sub = "LIBRARY · " + std::to_string(games_.size()) + " GAMES";
      }
      app.font.draw(app.sdlRenderer(), title, rx, 18, 19, sk.textHi, true);
      app.font.draw(app.sdlRenderer(), sub, rx, 46, 11, subCol);
    }

    // —— 3D 封面流（NeonFlow 参数）——
    hitCards_.clear();
    const float flowTop = 66, flowBottom = (float)d.H - 108;
    const float flowCy = (flowTop + flowBottom) * 0.5f;
    const float cardW = 170;              // NeonLibraryScreen cardWidth = 170dp
    const float cardH = cardW * 1.36f;
    if (games_.empty()) {
      app.font.draw(app.sdlRenderer(), "这里还没有游戏",
                    rx + rw / 2 - app.font.textWidth("这里还没有游戏", 14) / 2, flowCy - 12,
                    14, sk.textDim);
      const std::string tip = "左下角导入 ROM 文件夹，或在 设置 → 存储 添加扫描目录";
      app.font.draw(app.sdlRenderer(), app.font.ellipsize(tip, 12, rw),
                    rx + rw / 2 - app.font.textWidth(app.font.ellipsize(tip, 12, rw), 12) / 2,
                    flowCy + 14, 12, Color8(sk.textDim.r, sk.textDim.g, sk.textDim.b, 180));
    } else {
      const int N = (int)games_.size();
      // 远 → 近（scale 0.12 衰减 / alpha 0.16 衰减 / 间距 0.72 卡宽 / 下沉 24）
      for (int dist = 4; dist >= 1; dist--) {
        for (int sgn : {-1, 1}) {
          const int i = gridSel_ + sgn * dist;
          if (i < 0 || i >= N) continue;
          const float ad = std::fabs((float)i - animSel_);
          const int dir = (i < animSel_) ? -1 : 1;
          const float scale = std::max(0.55f, 1.0f - 0.12f * ad);
          const float alpha = std::min(0.85f, std::max(0.18f, 0.95f - 0.16f * ad));
          const float cx = rx + rw / 2 + dir * ad * cardW * 0.72f;
          const float cy = flowCy + ad * 24.0f;
          const float w = cardW * scale, h = cardH * scale;
          w::coverCard(app, Rect(cx - w / 2, cy - h / 2, w, h), games_[(size_t)i], false)
              ;
          // 侧卡透明度由 coverCard 内部不支持 → 用暗化叠加近似
          d.rect(Rect(cx - w / 2, cy - h / 2, w, h),
                 Color8(sk.bg.r, sk.bg.g, sk.bg.b, (uint8_t)(255 * (1.0f - alpha) * 0.55f)));
          if (alpha > 0.3f) hitCards_.push_back({Rect(cx - w / 2, cy - h / 2, w, h), i});
        }
      }
      // 中心卡（上浮 14 + 辉光）
      {
        const GameEntry& g = games_[(size_t)gridSel_];
        const float cy = flowCy - 14.0f;
        const Rect rc(rx + rw / 2 - cardW / 2, cy - cardH / 2, cardW, cardH);
        w::coverCard(app, rc, g, true);
        hitCards_.push_back({rc, gridSel_});
      }
      // 点击：侧卡选中 / 中卡启动；右键快捷菜单
      if (in.mousePressed) {
        for (auto it = hitCards_.rbegin(); it != hitCards_.rend(); ++it) {
          if (it->first.contains(in.mouse)) {
            if (it->second == gridSel_)
              app.launchGame(games_[(size_t)gridSel_]);
            else
              gridSel_ = it->second;
            break;
          }
        }
      }
      if (in.mouseRightPressed) {
        for (auto it = hitCards_.rbegin(); it != hitCards_.rend(); ++it) {
          if (it->first.contains(in.mouse)) {
            gridSel_ = it->second;
            openCardMenu(app, it->second, Vec2(in.mouse));
            break;
          }
        }
      }
      // 拖拽换卡（detectHorizontalDragGestures）
      if (in.mouseHeld && in.mouse.y > flowTop && in.mouse.y < flowBottom) {
        if (!dragging_) {
          dragging_ = true;
          dragLastX_ = in.mouse.x;
        } else {
          const float step = cardW * 0.5f;
          float dd = in.mouse.x - dragLastX_;
          dragLastX_ = in.mouse.x;
          dragAccum_ += dd;
          while (dragAccum_ > step && gridSel_ > 0) {
            gridSel_--;
            dragAccum_ -= step;
          }
          while (dragAccum_ < -step && gridSel_ < N - 1) {
            gridSel_++;
            dragAccum_ += step;
          }
          dragAccum_ = std::max(-step, std::min(step, dragAccum_));
        }
      } else {
        dragging_ = false;
      }
    }

    // —— 底部信息条（BgDeep 80%）——
    if (!games_.empty()) {
      const GameEntry& g = games_[(size_t)gridSel_];
      const float by = (float)d.H - 92;
      d.rect(Rect(0, by, (float)d.W, 44), Color8(sk.bgDeep.r, sk.bgDeep.g, sk.bgDeep.b, 204));
      const float ix = rx;
      app.font.draw(app.sdlRenderer(), app.font.ellipsize(g.displayTitle(), 15, rw - 320, true),
                    ix, by + 6, 15, sk.textHi, true);
      float mx = ix;
      mx += w::platformBadge(app, mx, by + 30, g.platform) + 10;
      app.font.drawVCentered(app.sdlRenderer(),
                             fsutil::baseOf(fsutil::dirOf(g.romPath)), mx, by + 30, 11,
                             sk.textDim);
      // ★ 收藏 / ✗ 删除
      {
        const Rect fav(rw + rx - 240, by + 10, 36, 24);
        app.font.draw(app.sdlRenderer(), "★", fav.x, by + 12, 17,
                      g.favorite ? sk.gold : sk.textDim);
        if (fav.contains(in.mouse) && in.mouseReleased && !w::dragGestureActive()) {
          app.library.toggleFavorite(g.id);
          app.library.save();
          refreshGames(app);
        }
      }
      {
        const Rect del(rw + rx - 196, by + 10, 36, 24);
        app.font.draw(app.sdlRenderer(), "✕", del.x + 4, by + 12, 15, sk.red);
        if (del.contains(in.mouse) && in.mouseReleased && !w::dragGestureActive()) {
          app.library.remove(g.id);
          app.library.save();
          refreshGames(app);
          app.toast("已从库中移除");
        }
      }
      // 开始按钮（NeonPrimaryButton）
      const Rect playRc(rx + rw - 150, by + 6, 150, 32);
      if (w::primaryButton(app, playRc, "开始", 1)) app.launchGame(g);
    }

    w::keyHintBar(app, {{"←→", "切换"}, {"Enter / 双击", "开始"}, {"F", "收藏"},
                        {"右键", "快捷菜单"}, {"B / Esc", "返回"}});
  }

  // ==================== FSD 经典风格 ====================
  void drawClassic(App& app) {
    const Skin& sk = Theme::skin();
    Draw& d = app.draw;
    const InputState& in = app.input.input();
    // 背景（NesTheme 深蓝渐变）
    for (int i = 0; i < 40; i++) {
      const float t = (float)i / 40;
      d.rect(Rect(0, d.H * t, d.W, d.H / 40 + 1), sk.bg.lerp(sk.bgDeep, t));
    }
    // 顶栏（surface 90%）：返回 + 标题 + 计数 + 导入/刷新
    {
      d.rect(Rect(0, 0, (float)d.W, 54), Color8(sk.bgPanel.r, sk.bgPanel.g, sk.bgPanel.b, 230));
      const Rect back(14, 12, 40, 30);
      w::iconArrowLeft(app, Rect(back.x + 8, back.cy() - 9, 18, 18),
                       back.contains(in.mouse) ? sk.accent : sk.text);
      if (w::activated(app, back, false)) app.popScene();
      float x = 62;
      const char* modeTitle = cats_[(size_t)catSel_].id == -2
                                  ? "收藏"
                                  : (cats_[(size_t)catSel_].id == -3 ? "最近游玩" : "游戏库");
      app.font.draw(app.sdlRenderer(), modeTitle, x, 15, 18, sk.textHi, true);
      x += app.font.textWidth(modeTitle, 18, true) + 12;
      app.font.draw(app.sdlRenderer(), std::to_string(games_.size()), x, 19, 14, sk.accent,
                    true);
      // 右侧图标
      const Rect imp((float)d.W - 88, 12, 34, 30);
      const Rect rfs((float)d.W - 46, 12, 34, 30);
      w::iconFolder(app, Rect(imp.cx() - 9, imp.cy() - 9, 18, 18),
                    imp.contains(in.mouse) ? sk.accent : sk.text);
      if (w::activated(app, imp, false)) startAddRomDir(app);
      w::iconRefresh(app, Rect(rfs.cx() - 9, rfs.cy() - 9, 18, 18),
                     rfs.contains(in.mouse) ? sk.accent : sk.text);
      if (w::activated(app, rfs, false)) rescanAll(app);
    }
    // 平台过滤 chips（横滑）
    {
      float x = 20;
      const float y = 64;
      for (size_t i = 0; i < cats_.size(); i++) {
        const bool selected = catSel_ == (int)i;
        const std::string& label = cats_[i].name;
        const float cw = app.font.textWidth(label, 12, true) + 24;
        const Rect rc(x, y, cw, 28);
        if (rc.right() > d.W - 20) break;
        d.roundRect(rc, 14, selected ? sk.accent : sk.bgPanelHi);
        app.font.drawVCentered(app.sdlRenderer(), label,
                               rc.cx() - app.font.textWidth(label, 12, selected) / 2, rc.cy(),
                               12, selected ? Color8::hex(0x081426) : sk.text, selected);
        if (w::activated(app, rc, false)) {
          catSel_ = (int)i;
          refreshGames(app);
          gridSel_ = 0;
        }
        x += cw + 8;
      }
    }
    // 方形封面网格（Adaptive 150dp，间距 14）
    {
      const Rect grid(20, 102, (float)d.W - 40, (float)d.H - 148);
      d.pushClip(grid);
      const int cols = columns(app);
      const float cw = (grid.w - 14 * (cols - 1)) / cols;
      const float cellH = cw + 58;  // 方形封面 + 标题/徽章区
      // 滚动：滚轮 + 拖拽
      scroll_ += w::dragScroll(app, grid);
      if (in.wheel != 0 && grid.contains(in.mouse)) scroll_ -= (float)in.wheel * 60;
      const float totalH = ((float)games_.size() + cols - 1) / cols * (cellH + 14);
      const float maxScroll = totalH > grid.h ? totalH - grid.h : 0;
      scroll_ = std::max(0.0f, std::min(scroll_, maxScroll));
      for (int idx = 0; idx < (int)games_.size(); idx++) {
        const int col = idx % cols;
        const int row = idx / cols;
        const float gx = grid.x + col * (cw + 14);
        const float gy = grid.y + row * (cellH + 14) - scroll_;
        if (gy > grid.bottom() || gy + cellH < grid.y - 60) continue;
        const GameEntry& g = games_[(size_t)idx];
        const bool selected = gridSel_ == idx;
        const Rect card(gx, gy, cw, cellH);
        d.roundRect(card, 10, sk.bgPanel);
        d.roundRectOutline(card, 10, selected ? sk.accent : sk.line, selected ? 2.0f : 1.0f);
        const Rect cover(gx, gy, cw, cw);
        SDL_Texture* tex = w::coverTexture(app, g);
        if (tex) {
          d.textureCrop(tex, cover);
        } else {
          w::platformPlaceholder(app, cover, g.platform, g.displayTitle());
        }
        // 收藏星
        if (g.favorite)
          app.font.draw(app.sdlRenderer(), "★", gx + cw - 24, gy + 6, 15,
                        Color8(0xF2, 0xC2, 0x00));
        // 标题 + 徽章
        app.font.draw(app.sdlRenderer(),
                      app.font.ellipsize(g.displayTitle(), 12, cw - 14), gx + 8, gy + cw + 8,
                      12, sk.textHi, true);
        w::platformBadge(app, gx + 8, gy + cw + 32, g.platform);
        // 鼠标交互
        if (card.contains(in.mouse)) {
          gridSel_ = idx;
          if (in.mousePressed) {
            // 单击选中；双击（间隔内二次点击）启动
            const Uint32 now = SDL_GetTicks();
            if (selected && now - lastClickMs_ < 400)
              app.launchGame(games_[(size_t)gridSel_]);
            lastClickMs_ = now;
          }
          if (in.mouseRightPressed) openCardMenu(app, idx, Vec2(in.mouse));
        }
      }
      d.popClip();
      if (totalH > grid.h) {
        w::scrollIndicator(app, Rect((float)d.W - 14, grid.y, 6, grid.h), grid.h / totalH,
                           scroll_ / (totalH - grid.h));
      }
    }
    w::keyHintBar(app, {{"双击", "开始"}, {"F", "收藏"}, {"右键", "快捷菜单"},
                        {"B / Esc", "返回"}});
  }

  // —— 详情 / 重命名 / 右键菜单（PC 扩展，双风格共用）——
  void updateDetail(App& app, const InputState& in) {
    const int N = 6;
    if (in.pressed[(int)Btn::Up]) detailSel_ = (detailSel_ + N - 1) % N;
    if (in.pressed[(int)Btn::Down]) detailSel_ = (detailSel_ + 1) % N;
    if (in.pressed[(int)Btn::Ok]) detailAction(app, detailSel_);
    if (renaming_) renameBuf_ = app.input.textBuffer();
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

  void drawDetail(App& app) {
    const Skin& sk = Theme::skin();
    Draw& d = app.draw;
    d.rect(Rect(0, 0, app.draw.W, app.draw.H), Color8(0, 0, 0, 150));
    const float pw = 560, ph = 360;
    Rect panel(app.draw.W / 2 - pw / 2, app.draw.H / 2 - ph / 2, pw, ph);
    skinPanel(d, panel, true, 16);

    const GameEntry& g = games_[(size_t)gridSel_];
    const Rect cover(panel.x + 24, panel.y + 24, 172, 236);
    SDL_Texture* tex = w::coverTexture(app, g);
    if (tex) {
      d.textureAspect(tex, cover);
    } else {
      w::platformPlaceholder(app, cover, g.platform, g.displayTitle());
    }
    app.font.draw(app.sdlRenderer(),
                  app.font.ellipsize(g.displayTitle(), 24, pw - 240, true), panel.x + 220,
                  panel.y + 28, 24, sk.textHi, true);
    const Color8 pc = Color8::hex(platformAccent(g.platform));
    app.font.draw(app.sdlRenderer(),
                  std::string(platformShortName(g.platform)) + " · " +
                      platformFullName(g.platform),
                  panel.x + 220, panel.y + 62, 14, pc);
    app.font.draw(app.sdlRenderer(),
                  "ROM：" + app.font.ellipsize(fsutil::baseOf(g.romPath), 12, pw - 250),
                  panel.x + 220, panel.y + 86, 12, sk.textDim);
    app.font.draw(app.sdlRenderer(),
                  "上次：" + formatLastPlayed(g.lastPlayedAt) + " · 时长 " +
                      formatPlayTime(g.playTimeSec),
                  panel.x + 220, panel.y + 106, 12, sk.textDim);

    const char* items[6] = {"启动游戏", g.favorite ? "取消收藏" : "加入收藏", "重命名",
                            "重新获取封面", "从库中移除", "关闭"};
    float y = panel.y + 150;
    for (int i = 0; i < 6; i++) {
      const Rect rc(panel.x + 220, y, pw - 244, 40);
      const bool danger = i == 4;
      if (w::listRow(app, rc, items[i], "", detailSel_ == i)) {
        detailSel_ = i;
        detailAction(app, i);
      }
      if (danger)
        app.font.draw(app.sdlRenderer(), "!", rc.right() - 24, rc.y + 10, 16, sk.red, true);
      y += 46;
    }
    const PlatformCore coreInUse = app.cores.resolveCore(g.platform);
    app.font.draw(app.sdlRenderer(), std::string("核心：") + coreInUse.coreTitle,
                  panel.x + 24, panel.bottom() - 34, 12, sk.textDim);
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

  // —— 右键快捷菜单（Android「长按卡片」的 PC 等价交互）——
  void openCardMenu(App& app, int idx, Vec2 at) {
    if (idx < 0 || idx >= (int)games_.size()) return;
    gridSel_ = idx;
    const GameEntry& g = games_[(size_t)idx];
    std::vector<ContextMenuItem> items;
    items.push_back({1, "启动游戏", "Enter", false, [this, &app, g]() {
      detailOpen_ = false;
      app.launchGame(g);
    }});
    items.push_back({2, g.favorite ? "取消收藏" : "加入收藏", "F", false,
                     [this, &app, g]() {
                       app.library.toggleFavorite(g.id);
                       app.library.save();
                       refreshGames(app);
                     }});
    items.push_back({0, "重命名（显示名）", "", false, [this, &app, g]() {
      renaming_ = true;
      renameBuf_ = g.customTitle;
      app.input.textClear();
      app.input.textInputStart();
    }});
    items.push_back({20, "重新获取封面", "", false, [&app, g]() {
      if (GameEntry* e = app.library.find(g.id)) e->coverPath.clear();
      app.library.save();
      app.covers.requestFetch(g);
      app.toast("已重新排队获取封面");
    }});
    if (app.cores.installedCoresFor(g.platform).size() +
            (size_t)app.externals.availableCount(g.platform) >
        1) {
      std::string hint;
      if (GameEntry* e = app.library.find(g.id); e && !e->preferredCore.empty()) {
        hint = "已记忆";
      }
      items.push_back({16, "重新选择核心", hint, false, [this, &app, g]() {
        if (GameEntry* e = app.library.find(g.id)) {
          e->preferredCore.clear();
          app.library.save();
        }
        app.launchGame(g);
      }});
    }
    items.push_back({0, "从库中移除", "", true, [this, &app, g]() {
      app.library.remove(g.id);
      app.library.save();
      detailOpen_ = false;
      refreshGames(app);
      app.toast("已从库中移除：" + g.displayTitle());
    }});
    pushContextMenu(app, at, g.displayTitle(),
                    std::string(platformShortName(g.platform)) + " · " +
                        platformFullName(g.platform) +
                        (g.favorite ? " · ★ 已收藏" : ""),
                    std::move(items));
  }

 private:
  struct Cat {
    int id;
    std::string name;
    std::string badge;
  };

  int filter_ = -9;
  std::vector<Cat> cats_;
  std::vector<GameEntry> games_;
  int catSel_ = 0;
  int gridSel_ = 0;
  float animSel_ = 0;
  bool detailOpen_ = false;
  int detailSel_ = 0;
  bool renaming_ = false;
  std::string renameBuf_;
  float scroll_ = 0;       // FSD 网格滚动
  float sideScroll_ = 0;   // Neon 侧栏滚动
  bool searching_ = false;
  std::string searchBuf_;
  Uint32 lastClickMs_ = 0;
  std::vector<std::pair<Rect, int>> hitCards_;
  bool dragging_ = false;
  float dragLastX_ = 0;
  float dragAccum_ = 0;
};

}  // namespace

std::unique_ptr<Scene> createLibraryScene(int platformFilter) {
  return std::unique_ptr<Scene>(new LibraryScene(platformFilter));
}

}  // namespace ns
