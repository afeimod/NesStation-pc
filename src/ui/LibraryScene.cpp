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
#include "ui/FsdChrome.h"
#include "ui/Scenes.h"
#include "ui/Widgets.h"
#include "util/Fs.h"

namespace ns {
namespace {

class LibraryScene : public Scene {
 public:
  explicit LibraryScene(int platformFilter) : filter_(platformFilter) {}

  void onEnter(App& app) override {
    ip_ = fsdchrome::localIp();
    statTimer_ = 999;  // 首帧即采样
    rebuild(app);
  }
  void onExit(App& app) override {
    (void)app;
    for (auto& kv : baked_)
      if (kv.second) SDL_DestroyTexture(kv.second);
    baked_.clear();
  }

  void rebuild(App& app) {
    cats_.clear();
    // ★v1.3.4 对齐 APK（严格同源同序）：
    //   Neon 侧栏 = NeonLibraryScreen.kt coreOrder：
    //     「全部」+ NES, SFC, GB, GBA, MD, PCE, PSX, PS2, NDS, N3DS,
    //     NGCWII, WIIU, DC, ARCADE, DOS, JAVA, PSP, NSWITCH（全量含 0 计数）
    //   FSD 平台 chips = LibraryScreen.kt 平台标签行：
    //     NES, SFC, GB, GBA, DOS, ARCADE, MD, PCE, NDS, N3DS, NGCWII, WIIU,
    //     PSX, PS2, JAVA, DC, PSP, NSWITCH（两套顺序不同，不得混用）
    //   收藏/最近不再是侧栏伪条目 —— 对齐 APK：主页深链进来改的是「模式」
    //   （NeonLibraryScreen mode = all/fav/recent），侧栏始终只有 全部+18 核心。
    cats_.push_back({-1, "全部", "ALL"});
    static const GamePlatform kNeonCoreOrder[] = {
        GamePlatform::NES,  GamePlatform::SFC,   GamePlatform::GB,
        GamePlatform::GBA,  GamePlatform::MD,    GamePlatform::PCE,
        GamePlatform::PSX,  GamePlatform::PS2,   GamePlatform::NDS,
        GamePlatform::N3DS, GamePlatform::NGCWII, GamePlatform::WIIU,
        GamePlatform::DC,   GamePlatform::ARCADE, GamePlatform::DOS,
        GamePlatform::JAVA, GamePlatform::PSP,  GamePlatform::NSWITCH,
    };
    for (GamePlatform p : kNeonCoreOrder)
      cats_.push_back({(int)p, platformShortName(p), ""});
    fsdChips_.clear();
    static const GamePlatform kFsdChipOrder[] = {
        GamePlatform::NES,   GamePlatform::SFC,  GamePlatform::GB,
        GamePlatform::GBA,   GamePlatform::DOS,  GamePlatform::ARCADE,
        GamePlatform::MD,    GamePlatform::PCE,  GamePlatform::NDS,
        GamePlatform::N3DS,  GamePlatform::NGCWII, GamePlatform::WIIU,
        GamePlatform::PSX,   GamePlatform::PS2,  GamePlatform::JAVA,
        GamePlatform::DC,    GamePlatform::PSP,  GamePlatform::NSWITCH,
    };
    for (GamePlatform p : kFsdChipOrder) fsdChips_.push_back((int)p);
    if (catSel_ >= (int)cats_.size()) catSel_ = 0;
    // 深链：-2 收藏 / -3 最近 → 模式（不选侧栏伪条目）；>= 0 → 选对应平台
    if (filter_ != -9) {
      if (filter_ == -2) mode_ = 1;
      else if (filter_ == -3) mode_ = 2;
      for (size_t i = 0; i < cats_.size(); i++) {
        if (cats_[i].id == (filter_ >= 0 ? filter_ : -1)) {
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
    // ★v1.3.4 对齐 APK displayGames：modeGames（模式基底）→ 搜索（跨全部
    //   平台）/ 平台过滤（选中核心）→ 排序。收藏/最近来自主页深链的
    //   「模式」（APK mode = fav/recent），不再是侧栏条目。
    std::vector<GameEntry> modeGames;
    if (mode_ == 1)
      modeGames = app.library.favorites();
    else if (mode_ == 2)
      modeGames = app.library.recent(100);
    else
      modeGames = app.library.games();
    const int id = cats_.empty() ? -1 : cats_[(size_t)catSel_].id;
    if (!searchBuf_.empty()) {
      // 搜索态：跨全部平台（APK 同款）
      std::string q = searchBuf_;
      std::transform(q.begin(), q.end(), q.begin(),
                     [](unsigned char c) { return (char)std::tolower(c); });
      for (auto& g : modeGames) {
        std::string t = g.displayTitle();
        std::transform(t.begin(), t.end(), t.begin(), [](unsigned char c) {
          return (char)std::tolower(c);
        });
        if (t.find(q) != std::string::npos) games_.push_back(g);
      }
    } else if (id >= 0) {
      for (auto& g : modeGames)
        if (g.platform == (GamePlatform)id) games_.push_back(g);
    } else {
      games_ = modeGames;
    }
    if (mode_ != 2) {  // 最近游玩保持时间序（APK sortedByDescending lastPlayedAt）
      std::sort(games_.begin(), games_.end(), [](const GameEntry& a, const GameEntry& b) {
        return a.displayTitle() < b.displayTitle();
      });
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
        // ★v1.2.9 FSD：↑↓ 在平台 chips 间切换（APK chips = 18 平台，无「全部」；
        //   ★v1.3.4 按 FSD chips 顺序走，不再用侧栏 coreOrder 顺序）
        if (in.pressed[(int)Btn::Up] || in.pressed[(int)Btn::Down]) {
          const int dir = in.pressed[(int)Btn::Down] ? 1 : -1;
          const int curId = cats_[(size_t)catSel_].id;
          int pos = -1;
          for (size_t i = 0; i < fsdChips_.size(); i++)
            if (fsdChips_[i] == curId) {
              pos = (int)i;
              break;
            }
          if (pos < 0)
            pos = 0;  // 当前在「全部」→ 从首个 chip 开始
          else
            pos = std::max(0, std::min((int)fsdChips_.size() - 1, pos + dir));
          const int want = fsdChips_[(size_t)pos];
          for (size_t i = 0; i < cats_.size(); i++) {
            if (cats_[i].id == want) {
              catSel_ = (int)i;
              break;
            }
          }
          refreshGames(app);
          gridSel_ = 0;
        }
      }
      if (in.pressed[(int)Btn::Ok]) app.launchGame(games_[(size_t)gridSel_]);
      if (Theme::isNeon()) {
        if (in.pressed[(int)Btn::Fav]) {
          app.library.toggleFavorite(games_[(size_t)gridSel_].id);
          app.library.save();
          refreshGames(app);
        }
      } else {
        // ★v1.2.9 FSD（对齐 APK 按键）：X = 搜索开关，Y = 选项（卡片菜单）
        if (in.pressed[(int)Btn::Fav]) {
          if (searching_) {
            stopEditing(app);
          } else {
            searching_ = true;
            app.input.textClear();
            app.input.textInputStart();
          }
        }
        if (in.pressed[(int)Btn::Option])
          openCardMenu(app, gridSel_, Vec2((float)app.draw.W / 2, (float)app.draw.H / 2));
      }
    }
    animSel_ += ((float)gridSel_ - animSel_) * std::min(1.0f, dt * 11.0f);
    // ★v1.2.9 FSD 顶栏系统状态采样（2.5s，对齐 APK FsdTopBar）
    statTimer_ += dt;
    if (statTimer_ > 2.5f) {
      statTimer_ = 0;
      stats_ = fsdchrome::sampleStats(app.dataDir(), &cpuPrevTotal_, &cpuPrevIdle_);
    }
  }

  void stopEditing(App& app) {
    searching_ = false;
    app.input.textInputStop();
    refreshGames(app);
  }

  // ★v1.2.11 按平台导入文件夹（APK 导入文件夹按钮：当前平台页 hint）
  //   不再进全局扫描目录 —— 文件夹注册到 importedFolders（按平台隔离记忆），
  //   后台枚举 + APK 过滤链（ROM_EXTENSIONS / CD 附属去重 / Switch 限定 / DOS 启动器），
  //   完成时主循环 toast「从文件夹导入 N 个ROM文件」。
  void startImportFolder(App& app, GamePlatform tabHint) {
    BrowserRequest req;
    req.title = std::string("导入文件夹 → ") + platformShortName(tabHint);
    req.mode = 0;
    // 起始目录：该平台最近导入的文件夹 → 任一最近导入 → 扫描目录 → 主目录
    std::string startDir;
    for (auto it = app.library.importedFolders().rbegin();
         it != app.library.importedFolders().rend(); ++it) {
      if (it->platform == tabHint) { startDir = it->dir; break; }
    }
    if (startDir.empty() && !app.library.importedFolders().empty())
      startDir = app.library.importedFolders().back().dir;
    if (startDir.empty() && !app.library.romDirs().empty())
      startDir = app.library.romDirs().back();
    if (startDir.empty()) startDir = fsutil::homeDir();
    req.startDir = startDir;
    req.onPick = [&app, tabHint](const std::string& dir) {
      if (!fsutil::isDir(dir)) return;
      if (app.library.scanRunning()) {
        // APK importing 守卫文案：「上一次导入还在进行中，请稍候再试」
        app.toast("上一次导入还在进行中，请稍候再试");
        return;
      }
      app.library.startBackgroundImport(dir, tabHint);
      app.library.save();  // 文件夹注册表落盘
      app.toast(std::string("正在扫描并导入到「") + platformShortName(tabHint) +
                "」页…（大目录可能需要一些时间）");
    };
    req.onCancel = [] {};
    browsePath(app, req);
  }

  // ★v1.2.11 导入单个 ROM 文件（APK 导入ROM 按钮：当前平台页 hint）
  void startImportRomFile(App& app, GamePlatform tabHint) {
    BrowserRequest req;
    req.title = std::string("导入 ROM → ") + platformShortName(tabHint);
    req.mode = 1;
    req.startDir = app.library.romDirs().empty() ? fsutil::homeDir()
                                                 : app.library.romDirs().back();
    req.onPick = [&app, tabHint](const std::string& file) {
      importRomFile(app, file, tabHint);
    };
    req.onCancel = [] {};
    browsePath(app, req);
  }

  // ★v1.2.11 刷新（APK refreshList 核心隔离）：平台页只重扫该平台导入的
  //   文件夹（严格识别，移除已不存在的条目）；全部/收藏/最近 → 全部注册
  //   文件夹 + 全局扫描目录后台重扫（保留设置页目录的老行为）
  void refreshCurrentTab(App& app) {
    const int id = cats_[(size_t)catSel_].id;
    if (id >= 0) {
      const auto p = (GamePlatform)id;
      if (app.library.importedFoldersFor(p).empty()) {
        app.toast(std::string(platformShortName(p)) +
                  " 平台还没有导入过文件夹（导入文件夹后会记住目录，刷新时自动同步）");
        return;
      }
      const auto r = app.library.refreshImportedFolders(p, false);
      rebuild(app);
      if (r.first > 0 || r.second > 0) {
        app.toast("刷新完成：新增 " + std::to_string(r.first) + " 个，移除 " +
                  std::to_string(r.second) + " 个");
      } else {
        app.toast("已是最新（无新增/移除）");
      }
      return;
    }
    // 全部 / 收藏 / 最近
    int added = 0, removed = 0;
    if (!app.library.importedFolders().empty()) {
      const auto r = app.library.refreshImportedFolders(GamePlatform::NES, true);
      added += r.first;
      removed += r.second;
    }
    if (!app.library.romDirs().empty()) {
      if (!app.library.scanRunning()) {
        app.library.startBackgroundRescan(app.config.scanRecursive);
      }
    }
    rebuild(app);
    if (added > 0 || removed > 0) {
      app.toast("刷新完成：新增 " + std::to_string(added) + " 个，移除 " +
                std::to_string(removed) + " 个");
    } else if (app.library.romDirs().empty() && app.library.importedFolders().empty()) {
      app.toast("还没有导入过文件夹，也没有添加扫描目录");
    } else if (app.library.romDirs().empty()) {
      app.toast("已是最新（无新增/移除）");
    }
    // （有全局目录时，后台重扫完成由主循环另行 toast）
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
  // ==================== Neon 3D 风格（★v1.3.5 顶栏横排工具栏 1:1 对齐
  //   APK NeonLibraryScreen.kt 577-812：顶栏 = 返回箭头 + 模式标题 +
  //   「GAME LIBRARY · N GAMES」副题 + 右侧横排工具按钮；工具按钮按
  //   当前核心差异（Java=安装JAR；3DS=+安装CIA；Switch=+小程序；
  //   其余=导入ROM/导入文件夹）+ 刷新/获取封面/主页；搜索行为内嵌行；
  //   左侧核心列表全高；底部 = 透明信息条（游戏名+平台徽章+N of M）
  //   + A/B/Y/X 按键提示条。
  void drawNeon(App& app) {
    const Skin& sk = Theme::skin();
    Draw& d = app.draw;
    const InputState& in = app.input.input();
    drawBackdrop(app);

    const float W = (float)d.W, H = (float)d.H;
    const GamePlatform tabHint = currentTabPlatform();
    const int curId = cats_[(size_t)catSel_].id;

    // —— 顶栏（APK：Row padding 8/6，返回 38dp + 标题 19sp ExtraBold +
    //    副题 9sp Accent letterSpacing2 + 右侧工具按钮）——
    const float topH = 56;
    {
      // 返回箭头（38dp 命中区）
      const Rect back(8, 9, 38, 38);
      if (back.contains(in.mouse)) {
        d.chamferRect(back, 10, sk.bgPanelHi);
        d.chamferRectOutline(back, 10, sk.accent, 1.2f);
      }
      w::iconArrowLeft(app, Rect(back.x + 9, back.cy() - 9, 18, 18), sk.textHi);
      if (w::activated(app, back, false)) {
        app.popScene();
        return;
      }
      // 标题 + 副题（APK modeTitle / modeSub）
      const std::string modeTitle = mode_ == 1 ? "收藏游戏"
                                    : mode_ == 2 ? "最近游玩" : "游戏库";
      app.font.draw(app.sdlRenderer(), modeTitle, 54, 8, 19, sk.textHi, true);
      std::string sub;
      if (!searchBuf_.empty())
        sub = std::string("GAME LIBRARY · ") + std::to_string((int)games_.size()) + " 结果";
      else {
        const size_t base = mode_ == 1 ? app.library.favorites().size()
                            : mode_ == 2 ? app.library.recent(100).size()
                                         : app.library.games().size();
        sub = std::string(mode_ == 1 ? "FAVORITES · " : mode_ == 2 ? "RECENT · " : "GAME LIBRARY · ") +
              std::to_string((int)base) + " GAMES";
      }
      app.font.draw(app.sdlRenderer(), sub, 54, 36, 9, sk.accent);

      // —— 右侧横排工具按钮（APK toolbarContent；NeonToolbarButton：
      //    BgPanel60% 切角 0.22 + Line 边 + 图标 20 + 下标 9sp）——
      auto neonTool = [&app, &d, &in, &sk](const Rect& rc, int iconId,
                                           const std::string& label,
                                           Color8 tint) -> bool {
        const bool hov = rc.contains(in.mouse);
        d.chamferRect(rc, rc.h * 0.22f, Color8(sk.bgPanel.r, sk.bgPanel.g, sk.bgPanel.b, 153));
        if (hov) d.chamferRectOutline(rc, rc.h * 0.22f, sk.accent, 1.2f);
        else d.chamferRectOutline(rc, rc.h * 0.22f, sk.line, 1.0f);
        w::icon(app, iconId, Rect(rc.cx() - 10, rc.y + 6, 20, 20), tint);
        app.font.draw(app.sdlRenderer(), label,
                      rc.cx() - app.font.textWidth(label, 9) / 2, rc.bottom() - 13,
                      9, tint);
        return w::activated(app, rc, false);
      };
      const float tbW = 58, tbH = 48;
      std::vector<std::pair<int, std::string>> tools;
      tools.push_back({11, "搜索"});
      if (curId == (int)GamePlatform::JAVA) {
        tools.push_back({48, "安装JAR"});  // APK：Java 页只显示安装JAR
      } else {
        tools.push_back({48, "导入ROM"});
        tools.push_back({4, "导入文件夹"});
        if (curId == (int)GamePlatform::N3DS)
          tools.push_back({39, "安装CIA"});   // APK：3DS 专属
        if (curId == (int)GamePlatform::NSWITCH)
          tools.push_back({12, "小程序"});    // APK v9：Switch 专属
      }
      tools.push_back({20, "刷新"});
      tools.push_back({38, "获取封面"});
      tools.push_back({49, "主页"});
      // 从右往左排布
      float tx = W - 10;
      for (int i = (int)tools.size() - 1; i >= 0; i--) {
        tx -= tbW;
        const Rect rc(tx, 4, tbW, tbH);
        const int idx = i;
        const bool active = (tools[(size_t)idx].second == "搜索" && searching_) ||
                            (tools[(size_t)idx].second == "主页");
        if (neonTool(rc, tools[(size_t)idx].first, tools[(size_t)idx].second,
                     active ? sk.accent : sk.text)) {
          const std::string& label = tools[(size_t)idx].second;
          if (label == "搜索") {
            if (searching_) stopEditing(app);
            else {
              searching_ = true;
              app.input.textClear();
              app.input.textInputStart();
            }
          } else if (label == "安装JAR") {
            startImportRomFile(app, GamePlatform::JAVA);
          } else if (label == "导入ROM") {
            startImportRomFile(app, tabHint);
          } else if (label == "导入文件夹") {
            startImportFolder(app, tabHint);
          } else if (label == "安装CIA") {
            startImportRomFile(app, GamePlatform::N3DS);  // .cia 安装包导入
          } else if (label == "小程序") {
            app.toast("Switch 小程序需固件支持（设置 → 核心设置 → Switch）");
          } else if (label == "刷新") {
            refreshCurrentTab(app);
          } else if (label == "获取封面") {
            app.covers.requestFetchAllMissing(games_);
            app.toast("已排队获取 " + std::to_string(games_.size()) + " 个游戏的封面");
          } else if (label == "主页") {
            app.popScene();
            return;
          }
        }
        tx -= 6;
      }
    }

    // —— 搜索行（APK NeonSearchField：切角 0.4 深底 + 搜索图标 + 清除钮）——
    float y = topH;
    if (searching_) {
      const Rect field(16, y + 2, W - 32, 30);
      d.chamferRect(field, 12, Color8(5, 7, 14, 204));
      d.chamferRectOutline(field, 12,
                           searchBuf_.empty() ? sk.line : sk.accentDim, 1.2f);
      w::iconSearch(app, Rect(field.x + 10, field.cy() - 7, 15, 15), sk.textDim);
      const std::string txt = searchBuf_.empty() ? "搜索游戏…" : searchBuf_;
      app.font.drawVCentered(app.sdlRenderer(), txt, field.x + 32, field.cy(), 11,
                             searchBuf_.empty() ? sk.textDim : sk.textHi);
      if (!searchBuf_.empty()) {
        const Rect clr(field.right() - 26, field.cy() - 8, 16, 16);
        d.circleOutline(clr.cx(), clr.cy(), 8, sk.line, 1.4f);
        app.font.draw(app.sdlRenderer(), "✕", clr.x + 5, clr.y + 3, 9, sk.text);
        if (clr.contains(in.mouse) && in.mouseReleased) {
          searchBuf_.clear();
          app.input.textClear();
          refreshGames(app);
        }
      }
      y += 36;
    }

    // —— 主体：左侧核心列表（172dp，APK NeonCoreSidebar 全高）——
    const float navW = 176;
    const float listTop = y + 4;
    const float listBottom = H - 44;  // 底部按键提示条留位
    {
      d.rect(Rect(0, 0, navW, (float)d.H), Color8(sk.bgPanel.r, sk.bgPanel.g, sk.bgPanel.b, 153));
      float sy = listTop;
      const Rect listRc(0, listTop, navW, listBottom - listTop);
      sideScroll_ += w::dragScroll(app, listRc);
      if (in.wheel != 0 && listRc.contains(in.mouse)) sideScroll_ -= (float)in.wheel * 40;
      const float total = (float)cats_.size() * 36;
      const float maxScroll = total > listRc.h ? total - listRc.h : 0;
      sideScroll_ = std::max(0.0f, std::min(sideScroll_, maxScroll));
      d.pushClip(Rect(0, listTop, navW, listBottom - listTop));
      sy = listTop - sideScroll_;
      for (size_t i = 0; i < cats_.size(); i++) {
        const Rect rc(8, sy, navW - 16, 34);
        if (rc.bottom() < listTop - 40 || rc.y > listBottom + 40) {
          sy += 36;
          continue;
        }
        const bool selected = catSel_ == (int)i;
        if (selected) d.chamferRect(rc, 10, sk.bgPanelHi);
        uint32_t c1 = 0, c2 = 0;
        if (cats_[i].id >= 0) platformGradient((GamePlatform)cats_[i].id, &c1, &c2);
        const Color8 dotCol = cats_[i].id >= 0 ? Color8::hex(c1) : sk.accent;
        d.rect(Rect(rc.x + 12, rc.cy() - 3, 6, 6), selected ? dotCol : sk.textDim);
        app.font.drawVCentered(app.sdlRenderer(), cats_[i].name, rc.x + 30, rc.cy(), 13,
                               selected ? sk.textHi : sk.text, selected);
        std::string cnt;
        if (cats_[i].id == -1) {
          cnt = std::to_string(mode_ == 1 ? app.library.favorites().size()
                                : mode_ == 2 ? app.library.recent(100).size()
                                             : app.library.games().size());
        } else {
          cnt = std::to_string(app.library.countFor((GamePlatform)cats_[i].id));
        }
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

    // —— 右侧 3D 封面流（NeonFlow：178x238 + 下沉 26 + 倒影区）——
    const float rx = navW + 16;
    const float rw = W - rx - 16;
    hitCards_.clear();
    const float flowTop = y + 8, flowBottom = H - 76;
    const float flowCy = (flowTop + flowBottom) * 0.5f;
    const float cardW = 170;
    const float cardH = cardW * 1.36f;
    if (games_.empty()) {
      const std::string main = searchBuf_.empty() ? "该分类还没有游戏"
                                 : "未找到匹配「" + searchBuf_ + "」的游戏";
      app.font.draw(app.sdlRenderer(), main,
                    rx + rw / 2 - app.font.textWidth(main, 15, true) / 2, flowCy - 14,
                    15, sk.textHi, true);
      const std::string tip2 = searchBuf_.empty()
                                   ? "点击右上角「导入ROM / 导入文件夹」添加游戏"
                                   : "换个关键词试试，或点 ✕ 关闭搜索";
      app.font.draw(app.sdlRenderer(), app.font.ellipsize(tip2, 12, (int)rw),
                    rx + rw / 2 - app.font.textWidth(app.font.ellipsize(tip2, 12, (int)rw), 12) / 2,
                    flowCy + 14, 12, sk.textDim);
    } else {
      const int N = (int)games_.size();
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
          const float w2 = cardW * scale, h2 = cardH * scale;
          w::coverCard(app, Rect(cx - w2 / 2, cy - h2 / 2, w2, h2), games_[(size_t)i], false);
          d.rect(Rect(cx - w2 / 2, cy - h2 / 2, w2, h2),
                 Color8(sk.bg.r, sk.bg.g, sk.bg.b, (uint8_t)(255 * (1.0f - alpha) * 0.55f)));
          if (alpha > 0.3f) hitCards_.push_back({Rect(cx - w2 / 2, cy - h2 / 2, w2, h2), i});
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
            if (it->second == gridSel_) app.launchGame(games_[(size_t)gridSel_]);
            else gridSel_ = it->second;
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
      if (in.mouseHeld && in.mouse.y > flowTop && in.mouse.y < flowBottom &&
          in.mouse.x > rx) {
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

      // —— 信息条（APK NeonLibraryInfoBar：透明底 + 游戏名 13sp +
      //    平台徽章（Accent16% 切角 + AccentDim 边）+ 「N of M」）——
      {
        const GameEntry& g = games_[(size_t)gridSel_];
        const float iy = H - 70;
        const float ix = rx;
        const std::string title =
            app.font.ellipsize(g.displayTitle(), 13, (int)(rw - 240), true);
        app.font.draw(app.sdlRenderer(), title, ix, iy, 13, sk.textHi, true);
        float mx = ix + app.font.textWidth(title, 13, true) + 12;
        // 平台徽章
        {
          const std::string pname = platformShortName(g.platform);
          const float bw2 = app.font.textWidth(pname, 9, true) + 14;
          const Rect brc(mx, iy - 1, bw2, 15);
          d.chamferRect(brc, 7, Color8(sk.accent.r, sk.accent.g, sk.accent.b, 41));
          d.chamferRectOutline(brc, 7, sk.accentDim, 1.0f);
          app.font.drawVCentered(app.sdlRenderer(), pname, brc.x + 7, brc.cy(), 9,
                                 sk.accent, true);
          mx = brc.right() + 8;
        }
        const std::string cnt = std::to_string(gridSel_ + 1) + " of " +
                                std::to_string((int)games_.size());
        app.font.drawVCentered(app.sdlRenderer(), cnt, rx + rw - app.font.textWidth(cnt, 11, true),
                               iy + 6, 11, sk.accent, true);
      }
    }

    // —— 底部按键提示条（APK NeonHintsBar：A 启动 / B 返回 / Y 选项 / X 搜索）——
    {
      struct Hint {
        char letter;
        const char* label;
        Color8 col;
      };
      const Hint hints[] = {
          {'A', "启动", Color8::hex(0x6DBE28)},
          {'B', "返回", Color8::hex(0xE23B3B)},
          {'Y', "选项", Color8::hex(0xF2C200)},
          {'X', "搜索", Color8::hex(0x2F7BD9)},
      };
      float hx = W / 2 - 130;
      const float hy = H - 34;
      for (const auto& h : hints) {
        d.circle(hx + 9, hy + 10, 9, h.col);
        app.font.draw(app.sdlRenderer(), std::string(1, h.letter), hx + 6, hy + 5, 10,
                      Color8(255, 255, 255), true);
        app.font.drawVCentered(app.sdlRenderer(), h.label, hx + 22, hy + 10, 11, sk.text);
        hx += app.font.textWidth(h.label, 11) + 44;
      }
    }
  }


  // ==================== FSD 经典（★v1.2.9 重制：1:1 对齐 APK LibraryScreen） ====================
  //   FsdTopBar（CPU/内存/存储 + 时钟）
  //   工具行：面包屑（游戏库 ▪ 当前平台）+ FsdToolButton（搜索/导入ROM/导入文件夹/
  //           刷新/获取封面）
  //   搜索框（展开时，深色圆角 + 放大镜）
  //   平台 chips（固定 18 平台全量，APK 同序；选中蓝渐变白字）
  //   封面流主体（FsdCoverFlow 186×252dp / 间距 30 / 透视 18° / 步进缩放 0.26 /
  //               步进淡出 0.32 / 窗口 ±4）：FsdGameCover（深蓝底 + 白 35% 边框 +
  //               满幅封面 + 左上平台徽标 + 底部渐变 + 游戏名）
  //   底部三件套：A 启动 / B 主页 / Y 选项 / X 搜索（左）+ 标题横幅（中）+
  //               N of M 计数（右）
  //   FsdBottomBar（状态：游戏库 / 空）
  static float fsdDp(const App& app) {
    // ★v1.2.10：540/960 基准（对齐逻辑画布与 APK 横屏密度；旧 720/1280 恒缩 0.75）
    const float dH = (float)app.draw.H / 540.0f;
    const float dW = (float)app.draw.W / 960.0f;
    return std::max(0.7f, std::min(dH, dW));
  }

  // 圆角擦除（写 alpha=0 像素）
  static void eraseCorners(SDL_Renderer* r, int w, int h, int R) {
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
    SDL_SetRenderDrawColor(r, 0, 0, 0, 0);
    for (int dy = 0; dy < R; dy++) {
      const float dyF = (float)dy + 0.5f;
      const float dx = (float)std::sqrt((double)R * R - dyF * dyF);
      const int e = (int)(R - dx);
      if (e <= 0) continue;
      SDL_Rect tl{0, dy, e, 1}, tr{w - e, dy, e, 1};
      SDL_Rect bl{0, h - 1 - dy, e, 1}, br{w - e, h - 1 - dy, e, 1};
      SDL_RenderFillRect(r, &tl);
      SDL_RenderFillRect(r, &tr);
      SDL_RenderFillRect(r, &bl);
      SDL_RenderFillRect(r, &br);
    }
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
  }

  // —— FsdGameCover 烘焙（满幅封面 + 边框 + 徽标 + 底部渐变 + 游戏名 + 圆角）——
  SDL_Texture* coverCardTexture(App& app, const GameEntry& g, int bakeW) {
    const std::string key =
        g.id + "|" + g.coverPath + "|" + g.displayTitle() + "|" + std::to_string(bakeW);
    auto it = baked_.find(key);
    if (it != baked_.end()) return it->second;
    if (baked_.size() > 80) {  // 容量上限：整表回收（避免长会话膨胀）
      for (auto& kv : baked_)
        if (kv.second) SDL_DestroyTexture(kv.second);
      baked_.clear();
    }
    if (!SDL_RenderTargetSupported(app.sdlRenderer())) return nullptr;
    const float D = fsdDp(app);
    const int bw = bakeW, bh = (int)(bakeW * 252.0f / 186.0f);
    SDL_Renderer* r = app.sdlRenderer();
    SDL_Texture* tex = SDL_CreateTexture(r, SDL_PIXELFORMAT_ARGB8888,
                                         SDL_TEXTUREACCESS_TARGET, bw, bh);
    if (!tex) return nullptr;
    SDL_Texture* prev = SDL_GetRenderTarget(r);
    SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
    SDL_SetRenderTarget(r, tex);
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
    SDL_SetRenderDrawColor(r, 0x0D, 0x2C, 0x55, 255);
    SDL_RenderClear(r);
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    const Rect rc(0, 0, (float)bw, (float)bh);
    const float s = bw / (186 * D);  // 烘焙比例
    Draw& d = app.draw;
    // 满幅封面（Crop；无封面 → 平台占位）
    if (SDL_Texture* cov = w::coverTexture(app, g)) {
      d.textureCrop(cov, rc);
    } else {
      w::platformPlaceholder(app, rc, g.platform, g.displayTitle());
    }
    // 底部渐变（透明 → 黑 72%）+ 游戏名（白 12sp 半粗体）
    {
      const float gh = 64 * D * s;
      const int bands = 20;
      for (int i = 0; i < bands; i++) {
        const float t = (float)i / bands;
        d.rect(Rect(rc.x, rc.bottom() - gh + gh * t, rc.w, gh / bands + 1),
               Color8(0, 0, 0, (uint8_t)(0.72f * 255.0f * t)));
      }
      const float fs = 12 * D * s;
      const std::string title =
          app.font.ellipsize(g.displayTitle(), (int)fs, (int)(rc.w - 20 * D * s));
      app.font.draw(app.sdlRenderer(), title, rc.x + 10 * D * s,
                    rc.bottom() - 6 * D * s - fs, fs, Color8(255, 255, 255), true);
    }
    // 左上平台徽标（黑 55% 圆角 5 + 白 9sp 粗体：FC/SFC/GB/...）
    {
      const char* label = fsdchrome::platformBadgeLabel(g.platform);
      const float fs = 9 * D * s;
      const float tw = app.font.textWidth(label, fs, true);
      const Rect brc(6 * D * s, 6 * D * s, tw + 12 * D * s, fs + 6 * D * s);
      d.roundRect(brc, 5 * D * s, Color8(0, 0, 0, 140));
      app.font.drawVCentered(app.sdlRenderer(), label, brc.x + 6 * D * s, brc.cy(), fs,
                             Color8(255, 255, 255), true);
    }
    // 圆角擦除 + 白 35% 2dp 边框
    eraseCorners(r, bw, bh, (int)(10 * D * s));
    d.roundRectOutline(rc, 10 * D * s, Color8(255, 255, 255, 89), 2 * D * s);
    SDL_SetRenderTarget(r, prev);
    baked_[key] = tex;
    return tex;
  }

  void drawClassic(App& app) {
    Draw& d = app.draw;
    const InputState& in = app.input.input();
    const float D = fsdDp(app);
    const float W = (float)d.W, H = (float)d.H;

    // —— 背景：FsdBackdrop（自定义主页背景覆盖时带压暗遮罩）——
    if (!app.config.homeBackground.empty()) {
      if (SDL_Texture* bgTex = app.texture(app.config.homeBackground)) {
        d.textureCrop(bgTex, Rect(0, 0, W, H));
        d.rect(Rect(0, 0, W, H), Color8(0x0A, 0x1D, 0x3A, 140));
      } else {
        w::fsdWallpaper(app);
      }
    } else {
      w::fsdWallpaper(app);
    }

    // —— FsdTopBar ——
    w::fsdTopBar(app, stats_);
    float y = w::fsdTopBarH(app);

    // —— 工具行：面包屑 + 工具按钮 ——
    const float toolRowH = 38 * D;
    {
      const int curId = cats_[(size_t)catSel_].id;
      std::string seg2;
      if (!searchBuf_.empty())
        seg2 = "搜索「" + searchBuf_ + "」";
      else if (curId >= 0)
        seg2 = platformShortName((GamePlatform)curId);
      else
        seg2 = mode_ == 1   ? "收藏游戏"
              : mode_ == 2 ? "最近游玩"
              : "游戏库";
      w::fsdBreadcrumb(app, 34 * D, y + toolRowH * 0.5f, "游戏库", seg2);
      // 右侧工具按钮（tools[i]：i 越往右越靠屏幕右侧）
      // ★v1.3.5 对齐 APK FSD 工具行（LibraryScreen.kt:1383-1419）：
      //   搜索 / 导入ROM / [3DS:安装CIA] / 导入文件夹 / 本地浏览 /
      //   [Switch:小程序] / 刷新 / 获取封面 / 主页（按当前平台差异）
      const float tw = 52 * D, th = toolRowH - 4 * D;
      struct Tool {
        int icon;
        const char* label;
      };
      std::vector<Tool> tools;
      tools.push_back({11, "搜索"});
      if (curId == (int)GamePlatform::JAVA) {
        tools.push_back({48, "安装JAR"});  // APK：Java 页专属入口
      } else {
        tools.push_back({48, "导入ROM"});
        if (curId == (int)GamePlatform::N3DS)
          tools.push_back({39, "安装CIA"});   // APK：3DS 专属
        tools.push_back({4, "导入文件夹"});
        tools.push_back({15, "本地浏览"});    // APK Storage（内置浏览器）
        if (curId == (int)GamePlatform::NSWITCH)
          tools.push_back({12, "小程序"});    // APK v9：Switch 专属
      }
      tools.push_back({20, "刷新"});
      tools.push_back({38, "获取封面"});
      tools.push_back({49, "主页"});
      // ★v1.2.11 导入ROM/导入文件夹/刷新 都按「当前平台页」hint（APK 按钮组）
      const GamePlatform tabHint = currentTabPlatform();
      const int nTools = (int)tools.size();
      for (int i = nTools - 1; i >= 0; i--) {
        const float tx = W - 24 * D - tw - (float)(nTools - 1 - i) * (tw + 6 * D);
        const Rect rc(tx, y + 2 * D, tw, th);
        if (w::fsdToolButton(app, rc, tools[(size_t)i].icon,
                             tools[(size_t)i].label)) {
          const std::string label = tools[(size_t)i].label;
          if (label == "搜索") {  // 搜索开关（APK X 键同款）
            if (searching_) {
              stopEditing(app);
            } else {
              searching_ = true;
              app.input.textClear();
              app.input.textInputStart();
            }
          } else if (label == "安装JAR") {
            startImportRomFile(app, GamePlatform::JAVA);
          } else if (label == "导入ROM") {  // 选文件（当前平台页 hint）
            startImportRomFile(app, tabHint);
          } else if (label == "安装CIA") {
            startImportRomFile(app, GamePlatform::N3DS);
          } else if (label == "导入文件夹") {  // 注册到当前平台页
            startImportFolder(app, tabHint);
          } else if (label == "本地浏览") {  // 内置文件浏览器
            BrowserRequest req;
            req.title = std::string("本地浏览（") + platformShortName(tabHint) + "）";
            req.mode = 0;
            req.startDir = fsutil::homeDir();
            req.onPick = [&app, tabHint](const std::string& dir) {
              if (!fsutil::isDir(dir)) return;
              if (app.library.scanRunning()) {
                app.toast("上一次导入还在进行中，请稍候再试");
                return;
              }
              app.library.startBackgroundImport(dir, tabHint);
              app.library.save();
            };
            req.onCancel = [] {};
            app.pushScene(createFileBrowser(req));
          } else if (label == "小程序") {
            app.toast("Switch 小程序需固件支持（设置 → 核心设置 → Switch）");
          } else if (label == "刷新") {  // 核心隔离：只重扫当前平台页
            refreshCurrentTab(app);
          } else if (label == "获取封面") {
            app.covers.requestFetchAllMissing(games_);
            app.toast("已排队获取 " + std::to_string(games_.size()) + " 个游戏的封面");
          } else if (label == "主页") {
            app.popScene();
            return;
          }
        }
      }
    }
    y += toolRowH;

    // —— 搜索框（展开时）——
    if (searching_) {
      const float fh = 36 * D;
      const Rect field(34 * D, y + 2 * D, W - 68 * D, fh);
      d.roundRect(field, 10 * D, Color8(0, 0, 0, 90));
      d.roundRectOutline(field, 10 * D, Color8::hex(0x4F8AC4), 1.4f);
      w::iconSearch(app, Rect(field.x + 10 * D, field.cy() - 9 * D, 18 * D, 18 * D),
                    Color8::hex(0x9FB6D4));
      const std::string txt = searchBuf_.empty() ? "搜索游戏名称…" : searchBuf_;
      app.font.drawVCentered(
          app.sdlRenderer(), txt, field.x + 36 * D, field.cy(), 13 * D,
          searchBuf_.empty() ? Color8::hex(0x9FB6D4) : Color8(255, 255, 255));
      y += fh + 4 * D;
    }

    // —— 平台 chips（★v1.3.4 APK FSD 顺序：18 平台 +「全部」在首位，可横向拖动滚动）——
    {
      const float chipH = 30 * D;
      const Rect rowRc(0, y, W, chipH + 4 * D);
      chipScroll_ += w::dragScroll(app, rowRc);
      d.pushClip(rowRc);
      float x = 34 * D - chipScroll_;
      // chips 列表 = 「全部」+ 18 平台（fsdChips_ 顺序，APK LibraryScreen 同序）
      auto chipList = [&]() -> std::vector<int> {
        std::vector<int> out{-1};
        out.insert(out.end(), fsdChips_.begin(), fsdChips_.end());
        return out;
      }();
      const int selId = cats_[(size_t)catSel_].id;
      for (int id : chipList) {
        const std::string label =
            id < 0 ? "全部" : platformShortName((GamePlatform)id);
        const float fs = 12 * D;
        const float cw = app.font.textWidth(label, fs, true) + 28 * D;
        if (x + cw > 0 && x < W) {
          const bool selected = id == selId;
          const Rect rc(x, y + 2 * D, cw, chipH);
          if (selected) {  // 蓝渐变（2E86E0 → 0D4FA8）
            for (int b = 0; b < 8; b++) {
              const float t = (float)b / 8;
              d.rect(Rect(rc.x, rc.y + rc.h * t, rc.w, rc.h / 8 + 1),
                     Color8::hex(0x2E86E0).lerp(Color8::hex(0x0D4FA8), t));
            }
          } else {
            d.roundRect(rc, chipH * 0.5f, Color8(255, 255, 255, 31));
          }
          d.roundRectOutline(rc, chipH * 0.5f,
                             selected ? Color8(255, 255, 255, 166)
                                      : Color8(255, 255, 255, 56),
                             1);
          app.font.drawVCentered(app.sdlRenderer(), label,
                                 rc.cx() - app.font.textWidth(label, fs, selected) / 2,
                                 rc.cy(), fs,
                                 selected ? Color8(255, 255, 255)
                                          : Color8::hex(0x9FB6D4),
                                 selected);
          if (w::activated(app, rc, false)) {
            for (size_t i = 0; i < cats_.size(); i++) {
              if (cats_[i].id == id) {
                catSel_ = (int)i;
                break;
              }
            }
            refreshGames(app);
            gridSel_ = 0;
            chipScroll_ = 0;
          }
        }
        x += cw + 8 * D;
      }
      d.popClip();
      const float totalW = x + chipScroll_ - 34 * D;
      const float maxScroll = totalW > W - 68 * D ? totalW - (W - 68 * D) : 0;
      chipScroll_ = std::max(0.0f, std::min(chipScroll_, maxScroll));
      y += chipH + 6 * D;
    }

    // —— 封面流主体（FsdCoverFlow：186×252 / gap30 / tilt18° / fade0.32 / scale0.26）——
    const float flowBottom = H - w::fsdBottomBarH(app);
    const Rect flowRc(0, y, W, flowBottom - y);
    hitCards_.clear();
    if (games_.empty()) {
      const std::string main =
          searchBuf_.empty() ? "该平台还没有游戏"
                             : "未找到匹配「" + searchBuf_ + "」的游戏";
      app.font.draw(app.sdlRenderer(), main,
                    W / 2 - app.font.textWidth(main, 17 * D, true) / 2,
                    flowRc.cy() - 20 * D, 17 * D, Color8::hex(0xE8F1FF), true);
      const std::string hint = "点击右上角「导入ROM / 导入文件夹」添加游戏";
      app.font.draw(app.sdlRenderer(), hint,
                    W / 2 - app.font.textWidth(hint, 13 * D) / 2, flowRc.cy() + 10 * D,
                    13 * D, Color8::hex(0x9FB6D4));
    } else {
      const float cardW = 186 * D, cardH = 252 * D, gap = 30 * D;
      const float step = cardW + gap;
      const float cx = W / 2, cy = flowRc.cy() - 14 * D;
      const float dCam = 2200 * D;
      const int N = (int)games_.size();
      // 鼠标拖拽翻页（松手半步阈值 0.45，上限 16）
      if (in.mouseHeld && flowRc.contains(in.mouse)) {
        if (!fsdDragging_) {
          fsdDragging_ = true;
          fsdDragLastX_ = in.mouse.x;
          fsdDragAccum_ = 0;
        } else {
          fsdDragAccum_ += in.mouse.x - fsdDragLastX_;
          fsdDragLastX_ = in.mouse.x;
        }
      } else if (fsdDragging_) {
        int steps = -(int)std::lround(fsdDragAccum_ / (step * 0.45f));
        steps = std::max(-16, std::min(16, steps));
        gridSel_ = std::max(0, std::min(N - 1, gridSel_ + steps));
        app.covers.requestFetch(games_[(size_t)gridSel_]);
        fsdDragging_ = false;
      }
      auto drawCard = [&](int i) {
        const float pos = (float)i - animSel_;
        const float ap = std::fabs(pos);
        const float scale = std::max(0.5f, 1.0f - 0.26f * ap);
        const float alpha = std::max(0.25f, std::min(1.0f, 1.0f - 0.32f * ap));
        const float hw = cardW * 0.5f * scale, hh = cardH * 0.5f * scale;
        const float cxx = cx + pos * step;
        SDL_Texture* tex = coverCardTexture(app, games_[(size_t)i], (int)(cardW * 1.5f));
        if (std::fabs(pos) < 0.05f) {
          const Rect rc(cxx - hw, cy - hh, hw * 2, hh * 2);
          if (tex) {
            d.texture(tex, rc, Color8(255, 255, 255, (uint8_t)(alpha * 255)));
          } else {
            w::coverCard(app, rc, games_[(size_t)i], true);
          }
          hitCards_.push_back({rc, i});
        } else {
          const float a = (float)(pos * 18.0f * 3.14159265f / 180.0f);
          const float ca = std::cos(a), sa = std::sin(a);
          const float fr = dCam / (dCam + hw * sa);
          const float fl = dCam / (dCam - hw * sa);
          const float xr = cxx + hw * ca * fr;
          const float xl = cxx - hw * ca * fl;
          const float yl = cy - hh * fl, ybl = cy + hh * fl;
          const float yr = cy - hh * fr, ybr = cy + hh * fr;
          const float xy[8] = {xl, yl, xr, yr, xr, ybr, xl, ybl};
          static const float uv[8] = {0, 0, 1, 0, 1, 1, 0, 1};
          if (tex) {
            d.textureQuad(tex, xy, uv, Color8(255, 255, 255, (uint8_t)(alpha * 255)));
          } else {
            const Rect rc(cxx - hw, cy - hh, hw * 2, hh * 2);
            w::coverCard(app, rc, games_[(size_t)i], false);
          }
          if (alpha > 0.3f) {
            const float left = std::min(xl, xr), right = std::max(xl, xr);
            const float topY = std::min(yl, yr), botY = std::max(ybl, ybr);
            hitCards_.push_back({Rect(left, topY, right - left, botY - topY), i});
          }
        }
      };
      for (int dist = 4; dist >= 1; dist--) {
        for (int sgn : {-1, 1}) {
          const int i = (int)std::lround(animSel_) + sgn * dist;
          if (i >= 0 && i < N) drawCard(i);
        }
      }
      drawCard(gridSel_);

      // 鼠标交互：点击非中心卡 = 选中；点击中心卡 = 启动；右键 = 快捷菜单
      if (in.mousePressed) {
        for (auto it = hitCards_.rbegin(); it != hitCards_.rend(); ++it) {
          if (it->first.contains(in.mouse)) {
            if (it->second == gridSel_)
              app.launchGame(games_[(size_t)gridSel_]);
            else {
              gridSel_ = it->second;
              app.covers.requestFetch(games_[(size_t)gridSel_]);
            }
            break;
          }
        }
      } else if (in.mouseRightPressed) {
        for (auto it = hitCards_.rbegin(); it != hitCards_.rend(); ++it) {
          if (it->first.contains(in.mouse)) {
            gridSel_ = it->second;
            openCardMenu(app, gridSel_, in.mouse);
            break;
          }
        }
      } else {
        for (auto it = hitCards_.rbegin(); it != hitCards_.rend(); ++it) {
          if (it->first.contains(in.mouse)) {
            gridSel_ = it->second;
            break;
          }
        }
      }

      // —— 底部三件套（左：按键提示 / 中：标题横幅 / 右：N of M）——
      {
        const float baseY = flowBottom;
        float hx = 34 * D;
        const struct {
          const char* l;
          const char* t;
          uint32_t c;
        } hints[] = {{"A", "启动", 0x6DBE28}, {"B", "主页", 0xE23B3B},
                     {"Y", "选项", 0xF2C200}, {"X", "搜索", 0x2F7BD9}};
        for (const auto& h : hints) {
          const float r = 10 * D;
          d.circle(hx + r, baseY - 24 * D, r, Color8::hex(h.c).withA(140));
          d.circle(hx + r, baseY - 24 * D, r * 0.8f, Color8::hex(h.c));
          app.font.drawVCentered(app.sdlRenderer(), h.l,
                                 hx + r - app.font.textWidth(h.l, 10 * D, true) / 2,
                                 baseY - 24 * D, 10 * D, Color8(255, 255, 255), true);
          hx += 20 * D + 5 * D;
          app.font.drawVCentered(app.sdlRenderer(), h.t, hx, baseY - 24 * D, 11 * D,
                                 Color8::hex(0xE8F1FF));
          hx += app.font.textWidth(h.t, 11 * D) + 14 * D;
        }
        // 中：标题横幅（平台 游戏名）
        const GameEntry& g = games_[(size_t)gridSel_];
        const std::string title =
            std::string(platformShortName(g.platform)) + "  " + g.displayTitle();
        const float bnW = std::min(380 * D, W * 0.5f);
        w::fsdTitleBanner(app,
                          Rect(W / 2 - bnW / 2, baseY - 38 * D - 30 * D, bnW, 30 * D),
                          title);
        // 右：N of M
        const std::string cnt = std::to_string(gridSel_ + 1) + " / " + std::to_string(N);
        const float cw = app.font.textWidth(cnt, 13 * D, true) + 28 * D;
        w::fsdCounter(app, Rect(W - 34 * D - cw, baseY - 38 * D - 30 * D, cw, 26 * D),
                      gridSel_ + 1, N);
      }
    }

    // —— FsdBottomBar ——
    w::fsdBottomBar(app, ip_, games_.empty() ? "空" : "游戏库");
  }

  // 导入单个 ROM 文件（导入ROM 工具按钮）
  // ★v1.2.10：改走手动导入宽松判定（APK detectFromUri）——当前平台页作
  //   hint：CD 镜像按 hint 消歧（内容嗅探优先）、zip 探头、7z/gz→街机、
  //   未知兑底 hint（不再全给 NES）
  // ★v1.2.11：先过 APK ROM_EXTENSIONS 白名单（importPickedFiles 语义：
  //   不支持的扩展名直接提示，不入库）
  static void importRomFile(App& app, const std::string& file, GamePlatform tabHint) {
    const std::string ext = fsutil::extOf(file);
    bool extOk = false;
    for (const auto& m : romExtensionsMaster()) {
      if (m == ext) {
        extOk = true;
        break;
      }
    }
    if (!extOk) {
      app.toast("不支持的文件类型 ." + ext + "（支持 nes/sfc/gba/md/… 与 zip/7z）");
      return;
    }
    if (app.library.addRom(file, tabHint, true)) {
      app.library.save();
      app.toast("已导入：" + fsutil::baseOf(file));
    } else {
      app.toast("导入失败（文件已入库或无法识别平台）");
    }
  }

  // 当前分类页对应的平台（全部/收藏/最近 → NES，即 APK 无 hint 语义）
  GamePlatform currentTabPlatform() const {
    if (cats_.empty()) return GamePlatform::NES;
    const int id = cats_[(size_t)catSel_].id;
    return id >= 0 ? (GamePlatform)id : GamePlatform::NES;
  }

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
    // ★v1.2.6 自定义封面（对齐 Android「长按卡片 → 自定义封面」）
    items.push_back({5, "自定义封面…", "", false, [this, &app, g]() {
      ns::BrowserRequest req;
      req.title = "选择封面图片（" + g.displayTitle() + "）";
      req.mode = 3;  // 图片选择（png/jpg/jpeg/bmp/webp）
      req.startDir = fsutil::homeDir();
      req.onPick = [this, &app, g](const std::string& picked) {
        const std::string err = setCustomCover(app.library, g.id, picked);
        if (err.empty()) {
          app.invalidateTexture(g.coverPath);  // 旧封面缓存失效
          refreshGames(app);                   // 重载条目 → 重绘封面
          app.toast("已设置自定义封面：" + g.displayTitle());
        } else {
          app.pushScene(createMessageScene("无法设置封面", err));
        }
      };
      browsePath(app, req);
    }});
    // ★v1.2.6 核心选项（该平台已装核心的选项面板 —— 对齐 Android 核心设置）
    items.push_back({16, "核心选项", "", false, [&app, g]() {
      app.pushScene(createCoreOptionsScene(g.platform));
    }});
    if (!g.coverPath.empty()) {
      items.push_back({20, "恢复默认封面", "", false, [&app, g]() {
        if (GameEntry* e = app.library.find(g.id)) {
          app.invalidateTexture(e->coverPath);
          e->coverPath.clear();
        }
        app.library.save();
        app.covers.requestFetch(g);
        app.toast("已恢复默认封面（重新获取官方封面）");
      }});
    } else {
      items.push_back({20, "重新获取封面", "", false, [&app, g]() {
        if (GameEntry* e = app.library.find(g.id)) e->coverPath.clear();
        app.library.save();
        app.covers.requestFetch(g);
        app.toast("已重新排队获取封面");
      }});
    }
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
  int mode_ = 0;  // ★v1.3.4 APK NeonLibraryScreen.mode：0=游戏库 1=收藏 2=最近
                  //   （主页深链进来；不再是侧栏伪条目）
  std::vector<Cat> cats_;
  std::vector<int> fsdChips_;  // ★v1.3.4 FSD 平台 chips（APK LibraryScreen 顺序）
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
  // ★v1.2.9 FSD 封面流（对齐 APK LibraryScreen）
  float chipScroll_ = 0;          // 平台 chips 横向滚动
  bool fsdDragging_ = false;      // 封面流鼠标拖拽翻页
  float fsdDragLastX_ = 0;
  float fsdDragAccum_ = 0;
  std::map<std::string, SDL_Texture*> baked_;  // FsdGameCover 烘焙缓存
  fsdchrome::SysStats stats_;                  // 顶栏系统状态
  uint64_t cpuPrevTotal_ = 0, cpuPrevIdle_ = 0;
  float statTimer_ = 999;
  std::string ip_;                              // 底栏 IP（onEnter 缓存）
};

}  // namespace

std::unique_ptr<Scene> createLibraryScene(int platformFilter) {
  return std::unique_ptr<Scene>(new LibraryScene(platformFilter));
}

}  // namespace ns
