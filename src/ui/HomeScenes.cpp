// NesStation PC — 主页：Neon 3D 游戏站 / FSD 经典磁贴（按皮肤切换）
//
// 视觉与交互 1:1 移植自 Android 版：
//   · NeonHomeScreen.kt + NeonFlow.kt + NeonChrome.kt（时钟条 / 彩色 chip 菜单行 /
//     3D 封面流[透视缩放+渐隐+辉光] / 信息区 / 按键提示栏）
//   · FsdHomeScreen.kt + FsdScaffold.kt + FsdTileMenu.kt + FsdCoverFlow.kt
//     （★v1.2.9：FsdBackdrop 深蓝壁纸 + 三分区磁贴流 + 顶/底状态条）
// 鼠标：chip / 磁贴 / 封面卡均矩形命中；封面流支持横向拖拽换位。
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <map>
#include <string>

#include "ui/FsdChrome.h"
#include "ui/Scenes.h"
#include "ui/Widgets.h"
#include "util/Fs.h"

namespace ns {
namespace {

std::string clockText() {
  const time_t now = time(nullptr);
  const struct tm* t = localtime(&now);
  char buf[16];
  if (t) strftime(buf, sizeof(buf), "%H:%M", t);
  return buf;
}

// APK GamePlatform.kt 格式化已下沉至 GameLibrary.cpp（formatPlayTime / formatLastPlayed）
#define lastPlayedText formatLastPlayed
#define playTimeText formatPlayTime

// 排序：最近游玩优先（与 Android NeonHomeScreen flowGames 一致）
std::vector<GameEntry> recentFirst(const std::vector<GameEntry>& games) {
  std::vector<GameEntry> out = games;
  std::sort(out.begin(), out.end(), [](const GameEntry& a, const GameEntry& b) {
    const bool ap = a.lastPlayedAt > 0, bp = b.lastPlayedAt > 0;
    if (ap != bp) return ap;
    return a.lastPlayedAt > b.lastPlayedAt;
  });
  return out;
}

// —— 烘焙工具：把矢量内容画进纹理目标并切圆角（供 3D 流 / 磁贴复用）——
// eraseCorners：SDL_BLENDMODE_NONE 写 alpha=0 像素 = 擦除
void eraseRoundedCorners(SDL_Renderer* r, int w, int h, int R) {
  SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
  SDL_SetRenderDrawColor(r, 0, 0, 0, 0);
  for (int dy = 0; dy < R; dy++) {
    const float dyF = (float)dy + 0.5f;
    const float dx = (float)std::sqrt((double)R * R - dyF * dyF);
    const int e = (int)(R - dx);  // 该行角落要擦除的宽度
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

// —— Neon 切角擦除（neonChamfer：左上 + 右下对角斜切，其余直角）——
// c = 斜切边长（像素）。SDL_BLENDMODE_NONE 写 alpha=0 = 擦除
void eraseChamferCorners(SDL_Renderer* r, int w, int h, int c) {
  if (c <= 0) return;
  SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
  SDL_SetRenderDrawColor(r, 0, 0, 0, 0);
  for (int y = 0; y < c; y++) {
    const int e = c - y;  // 该行斜切宽度
    if (e <= 0) continue;
    SDL_Rect tl{0, y, e, 1};
    SDL_Rect br{w - e, h - 1 - y, e, 1};
    SDL_RenderFillRect(r, &tl);  // 左上斜切
    SDL_RenderFillRect(r, &br);  // 右下斜切
  }
  SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
}

// 主页动作（两套皮肤共用）：
//   0 游戏库 1 收藏 2 最近 3 核心管理(FSD) 4 设置 5 关于 6 退出
//   7 在线游戏 8 对战平台 9 SWF（★Neon 菜单行 1:1 对齐 APK NeonHomeScreen：
//     APK 主页九宫格 = 游戏库/收藏/最近/在线游戏/对战平台/SWF/设置/关于/退出，
//     其中在线三项依赖 Android 在线服务，PC 版给出明确提示而非隐藏）
void homeAction(App& app, int action) {
  switch (action) {
    case 0: app.pushScene(createLibraryScene(-1)); break;
    case 1: app.pushScene(createLibraryScene(-2)); break;
    case 2: app.pushScene(createLibraryScene(-3)); break;
    case 3: app.pushScene(createSettingsScene(6)); break;  // 核心管理 → 核心设置
    case 4: app.pushScene(createSettingsScene()); break;
    case 5: app.pushScene(createAboutScene()); break;
    case 6:
      app.pushScene(createMessageScene(
          "退出 NesStation？", "确定要退出应用吗？未保存的进度将按设置自动存档。",
          false, "退出", [appPtr = &app]() { appPtr->requestExit(); }));
      break;
    case 7:
      // ★v1.3.1 真实场景（APK OnlineGamesScreen 信息架构；不再 toast 占位）
      app.pushScene(createOnlineGamesScene());
      break;
    case 8:
      // ★v1.3.1 真实场景（APK BattleScreen：服务器/登录/房间/对战 ROM 下载）
      app.pushScene(createBattleScene());
      break;
    case 9:
      // ★v1.3.1 真实场景（APK SwfListScreen：目录扫描 + ruffle/浏览器播放）
      app.pushScene(createSwfScene());
      break;
  }
}

// ==================== Neon 3D 主页 ====================
// ★v1.2.3 逐行对照 APK 源码完全重制（NeonHomeScreen.kt / NeonFlow.kt /
//   NeonComponents.kt / NeonUi.kt / NeonChrome.kt），横屏分支像素级对齐：
//   · NeonHomeTopBar：切角小牌[手柄icon+NESSTATION] + "N GAMES" + 时钟(textDim)
//   · NeonMenuRow：九枚切角 chip（游戏库/收藏/最近/在线游戏/对战平台/SWF/
//     设置/关于/退出）—— BgPanel 62% 底 + Line 描边 + 15dp 彩色图标
//   · 主视觉（横屏）：3D 封面流居左（centerBiasX=72dp 贴近信息面板）+
//     右侧 300dp 竖排信息面板（PS5 风格，透明底 + 文字描影）：
//     标题+★ / 平台徽章 / 最近+时长 / 继续游戏(主按钮) / 游戏库(幽灵按钮)
//   · NeonFlow 弧形导轨：aPos² 逐级下沉 + scale 1-0.19/步 + ±34° 透视 +
//     alpha 1-0.20/步 + 0.28 高度倒影渐隐 + 选中辉光浮起
//   · NeonHintsBar：底部居中切角胶囊（A 启动 / Y 收藏 / X 游戏库）
class NeonHome : public Scene {
 public:
  ~NeonHome() { baked_.clear(); }

  void onEnter(App& app) override {
    refresh(app);
    animSel_ = (float)sel_;
    bgPrev_ = nullptr;
    bgCur_ = nullptr;
    bgFade_ = 1.0f;
  }

  void onExit(App& app) override {
    (void)app;
    for (auto& kv : baked_)
      if (kv.second) SDL_DestroyTexture(kv.second);
    baked_.clear();
  }

  void refresh(App& app) {
    games_ = recentFirst(app.library.games());
    if (sel_ >= (int)games_.size()) sel_ = 0;
    if (games_.empty()) {
      app.covers.requestFetchAllMissing(app.library.games());
    } else {
      // 只为可见附近的缺封面游戏排队（APK visibleHalfWindow=5）
      for (int i = -5; i <= 5; i++) {
        const int idx = sel_ + i;
        if (idx >= 0 && idx < (int)games_.size())
          app.covers.requestFetch(games_[(size_t)idx]);
      }
    }
  }

  // —— dp：Android 密度无关像素（★v1.2.10 基准修正：逻辑画布 960×540 = APK
  //   横屏 960×540dp 的 1:1 密度 —— 旧版 720/1280 基准在 540 基准画布上
  //   恒乘 0.75，主页/游戏库文字整体缩小 25%，这是「字体都非常小」的主因；
  //   窄窗按宽约束防溢出）——
  static float dpOf(const App& app) {
    const float dH = (float)app.draw.H / 540.0f;
    const float dW = (float)app.draw.W / 960.0f;
    return std::max(0.7f, std::min(dH, dW));
  }

  static Color8 ca(const Color8& c, float a01) {
    return Color8(c.r, c.g, c.b, (uint8_t)(a01 * 255.0f));
  }

  // —— 布局（APK 横屏分支：封面流左 + 信息面板右）——
  struct Layout {
    float D;                 // dp 比例
    float mainTop, mainBottom;
    float panelX, panelW;    // 右侧信息面板（宽 300dp + end 边距 14dp）
    float flowCx, flowCy;    // 封面流中心（含 centerBiasX=72dp）
    float cardW, cardH, step, shift, fitScale;
    float menuY, menuH;      // 菜单行
  };
  Layout layout(App& app) const {
    Layout L;
    L.D = dpOf(app);
    const float D = L.D;
    // 顶栏（v10dp + 牌高27dp）+ 菜单行（v2dp + chip高32dp）+ 4dp 间距
    L.menuY = (10 + 27 + 2) * D;
    L.menuH = 32 * D;
    L.mainTop = L.menuY + L.menuH + 4 * D;
    const float hintsH = (10 + 32) * D;  // 底部提示栏（bottom 10dp + 高32dp）
    L.mainBottom = (float)app.draw.H - hintsH - 8 * D;
    L.panelW = 300 * D;
    L.panelX = (float)app.draw.W - (14 + 300) * D;
    const float flowRight = L.panelX - 10 * D;
    L.flowCx = flowRight * 0.5f + 72 * D;  // ★centerBiasX：贴近右侧游戏名
    L.flowCy = (L.mainTop + L.mainBottom) * 0.5f;
    L.cardW = 188 * D;
    L.cardH = 250 * D;
    L.shift = 14 * D;  // APK 横屏 verticalShift
    const float areaH = L.mainBottom - L.mainTop;
    // ★下移不缩小：fitScale 只按卡片本体高度自保（下限 0.78）
    L.fitScale =
        std::min(1.0f, std::max(0.78f, (areaH - L.shift - 8 * D) / L.cardH));
    L.step = (188 + 20) * D * L.fitScale;
    return L;
  }

  void update(App& app, float dt, const InputState& in) override {
    appDt_ = dt;
    // 动画位（APK animateFloatAsState tween(260) 的指数近似）
    const float diff = (float)sel_ - animSel_;
    if (std::fabs(diff) < 0.003f)
      animSel_ = (float)sel_;
    else
      animSel_ += diff * std::min(1.0f, dt * 11.0f);

    // 松手后 dragPx 平滑归零（APK settling 200ms）
    if (!dragging_) {
      if (std::fabs(dragPx_) > 0.5f)
        dragPx_ *= std::max(0.0f, 1.0f - dt * 9.0f);
      else
        dragPx_ = 0;
    }

    // 库被外部改动（导入/删除）后刷新
    if ((int)app.library.games().size() != libCount_) {
      libCount_ = (int)app.library.games().size();
      refresh(app);
    }

    if (in.pressed[(int)Btn::Back]) {
      homeAction(app, 6);  // Esc = 退出确认
      return;
    }

    // —— 封面流拖拽（APK detectHorizontalDragGestures：跟手 + 松手按步翻页）——
    const Layout L = layout(app);
    const bool inFlow = in.mouse.y > L.mainTop && in.mouse.y < L.mainBottom &&
                        in.mouse.x < L.panelX && !games_.empty();
    if (in.mouseHeld && inFlow) {
      if (!dragging_) {
        dragging_ = true;
        dragLastX_ = in.mouse.x;
        dragAccum_ = 0;
        dragPx_ = 0;
      } else {
        const float dx = in.mouse.x - dragLastX_;
        dragLastX_ = in.mouse.x;
        dragAccum_ += dx;
        dragPx_ += dx;  // 跟手平移
      }
    } else if (dragging_) {
      // 松手：半步即翻页（0.45），单次上限 16 步（APK maxSwipeSteps）
      int steps = -(int)std::lround(dragAccum_ / (L.step * 0.45f));
      steps = std::max(-16, std::min(16, steps));
      sel_ = std::max(0, std::min((int)games_.size() - 1, sel_ + steps));
      if (!games_.empty()) app.covers.requestFetch(games_[(size_t)sel_]);
      dragging_ = false;
    }

    if (menuFocus_) {
      if ((in.pressed[(int)Btn::Down] || in.pressed[(int)Btn::Up]) && !games_.empty())
        menuFocus_ = false;
      if (in.pressed[(int)Btn::Left]) menuSel_ = (menuSel_ + kMenuCount - 1) % kMenuCount;
      if (in.pressed[(int)Btn::Right]) menuSel_ = (menuSel_ + 1) % kMenuCount;
      if (in.pressed[(int)Btn::Ok]) homeAction(app, kMenuAction[menuSel_]);
    } else if (!games_.empty()) {
      // APK onKeyNav：不循环，到头即止
      if (in.pressed[(int)Btn::Left] && sel_ > 0) {
        sel_--;
        app.covers.requestFetch(games_[(size_t)sel_]);
      }
      if (in.pressed[(int)Btn::Right] && sel_ < (int)games_.size() - 1) {
        sel_++;
        app.covers.requestFetch(games_[(size_t)sel_]);
      }
      if (in.pressed[(int)Btn::Ok]) app.launchGame(games_[(size_t)sel_]);
      // APK 按键：Y=收藏（手柄Y/键盘E），X=游戏库（手柄X/键盘F）
      if (in.pressed[(int)Btn::Option]) {
        app.library.toggleFavorite(games_[(size_t)sel_].id);
        app.library.save();
        refresh(app);
      }
      if (in.pressed[(int)Btn::Fav]) app.pushScene(createLibraryScene(-1));
    }
    if (in.pressed[(int)Btn::Menu]) app.pushScene(createSettingsScene());
  }

  // —— 封面卡烘焙（APK NeonCoverCard：BgPanelHi 底 + 满幅封面 + 顶部斜面
  //    高光 + 收藏角标 + 1px Line/2px Accent 边框 + chamfer(0.10) 切角）。
  //    按显示尺寸 1.5× 烘焙 —— 大窗口不模糊（修复"主页发糊"）。——
  SDL_Texture* cardTexture(App& app, const GameEntry& g, bool selected, int bakeW) {
    const std::string key = g.id + "|" + g.coverPath + "|" + (g.favorite ? "F" : "-") +
                            (selected ? "|S" : "|N") + "|" + std::to_string(bakeW);
    auto it = baked_.find(key);
    if (it != baked_.end()) return it->second;
    if (!SDL_RenderTargetSupported(app.sdlRenderer())) return nullptr;

    const int bw = bakeW, bh = (int)(bakeW * 250.0f / 188.0f);
    SDL_Renderer* r = app.sdlRenderer();
    SDL_Texture* tex =
        SDL_CreateTexture(r, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_TARGET, bw, bh);
    if (!tex) return nullptr;
    SDL_Texture* prev = SDL_GetRenderTarget(r);
    SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
    SDL_SetRenderTarget(r, tex);
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
    SDL_SetRenderDrawColor(r, 0, 0, 0, 0);
    SDL_RenderClear(r);
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);

    drawCardContent(app, g, Rect(0, 0, (float)bw, (float)bh), selected);

    eraseChamferCorners(r, bw, bh, (int)(std::min(bw, bh) * 0.10f));
    SDL_SetRenderTarget(r, prev);
    baked_[key] = tex;
    return tex;
  }

  // 卡片内容（烘焙与软件渲染回退共用）。selected 控制边框 2px Accent / 1px Line
  void drawCardContent(App& app, const GameEntry& g, const Rect& rc, bool selected) {
    const Skin& sk = Theme::skin();
    Draw& d = app.draw;
    // BgPanelHi 底（APK background(Neon.BgPanelHi)）
    d.rect(rc, sk.bgPanelHi);
    // 封面满幅裁剪（ContentScale.Crop，无内边距）
    SDL_Texture* cover = w::coverTexture(app, g);
    if (cover) {
      d.textureCrop(cover, rc);
    } else {
      w::platformPlaceholder(app, rc, g.platform, g.displayTitle());
    }
    // 顶部斜面高光（28dp 白色渐变，选中 0.22 / 未选 0.12）
    const float hiH = rc.w * (28.0f / 188.0f);
    const int bands = 10;
    for (int i = 0; i < bands; i++) {
      const float t = (float)i / bands;
      const uint8_t a = (uint8_t)((selected ? 0.22f : 0.12f) * 255.0f * (1.0f - t));
      d.rect(Rect(rc.x, rc.y + hiH * t, rc.w, hiH / bands + 1), Color8(255, 255, 255, a));
    }
    // 收藏星标（右上 14dp 圆形黑底 60% + 金边 80% + ★ 金）
    if (g.favorite) {
      const float R = rc.w * (7.0f / 188.0f);
      const float cx = rc.right() - rc.w * (13.0f / 188.0f);
      const float cy = rc.y + rc.w * (13.0f / 188.0f);
      d.circle(cx, cy, R, Color8(0, 0, 0, 153));
      d.circleOutline(cx, cy, R, ca(sk.gold, 0.8f), 1.2f);
      const float ss = rc.w * (11.0f / 188.0f);
      app.font.drawVCentered(app.sdlRenderer(), "★", cx - ss / 2, cy, ss, sk.gold, true);
    }
    // 边框（烘焙进纹理，随透视四边形一起变换）
    d.chamferRectOutline(rc, rc.minDim() * 0.10f, selected ? sk.accent : sk.line,
                         selected ? 2.6f : 1.4f);
  }

  // 透视卡片（APK graphicsLayer rotationY = pos*34° coerce ±54.4° 的仿射近似；
  // cameraDistance 9*density 近相机 = 强透视）。flipV = 倒影
  void drawCard3D(App& app, SDL_Texture* tex, float cx, float cy, float w, float h,
                  float tiltDeg, float alpha, bool flipV = false) {
    if (!tex) return;
    const float th = tiltDeg * 3.14159265f / 180.0f;
    const float W = w * std::cos(th);
    const float persp = 0.18f * std::fabs(std::sin(th));  // 近棱高 / 远棱矮
    const float hN = h * (1.0f + persp), hF = h * (1.0f - persp);
    float xy[8];
    if (tiltDeg <= 0) {
      // 左卡/中卡：右棱（内棱朝中心）近
      xy[0] = cx - W / 2; xy[1] = cy - hF / 2;
      xy[2] = cx + W / 2; xy[3] = cy - hN / 2;
      xy[4] = cx + W / 2; xy[5] = cy + hN / 2;
      xy[6] = cx - W / 2; xy[7] = cy + hF / 2;
    } else {
      // 右卡：左棱（内棱朝中心）近
      xy[0] = cx - W / 2; xy[1] = cy - hN / 2;
      xy[2] = cx + W / 2; xy[3] = cy - hF / 2;
      xy[4] = cx + W / 2; xy[5] = cy + hF / 2;
      xy[6] = cx - W / 2; xy[7] = cy + hN / 2;
    }
    const float uv[8] = {0, 0, 1, 0, 1, 1, 0, 1};
    const float uvF[8] = {0, 1, 1, 1, 1, 0, 0, 0};  // 垂直翻转（倒影）
    app.draw.textureQuad(tex, xy, flipV ? uvF : uv,
                         Color8(255, 255, 255, (uint8_t)(alpha * 255)));
  }

  // 倒影渐隐遮罩（APK：Transparent → 0.72 处 Bg@0.72 → BgDeep）
  void drawReflectionFade(Draw& d, const Rect& rc, const Skin& sk) {
    const int bands = 16;
    for (int i = 0; i < bands; i++) {
      const float t = (float)i / bands;  // 0=倒影顶（贴卡片底） 1=倒影底
      Color8 c;
      if (t < 0.72f) {
        const float k = t / 0.72f;
        c = Color8(sk.bg.r, sk.bg.g, sk.bg.b, (uint8_t)(0.72f * 255.0f * k));
      } else {
        c = sk.bgDeep;
      }
      d.rect(Rect(rc.x, rc.y + rc.h * t, rc.w, rc.h / bands + 1), c);
    }
  }

  // —— NeonBackdrop（无封面时）：#0A1428→Bg→BgDeep 渐变 + 12 条网格线 +
  //    底部 2dp 地平线微光 ——
  void neonBackdrop(Draw& d) {
    const Skin& sk = Theme::skin();
    const Color8 top = Color8::hex(0x0A1428);
    const int bands = 40;
    for (int i = 0; i < bands; i++) {
      const float t = (float)i / bands;
      Color8 c = t < 0.45f ? top.lerp(sk.bg, t / 0.45f)
                           : sk.bg.lerp(sk.bgDeep, (t - 0.45f) / 0.55f);
      d.rect(Rect(0, d.H * t, (float)d.W, (float)d.H / bands + 1), c);
    }
    for (int i = 0; i < 12; i++)  // 网格微纹（Line 14%，12 竖列）
      d.rect(Rect((float)d.W * i / 12, 0, (float)d.W / 12, (float)d.H),
             ca(sk.line, 0.14f));
    // 底部地平线微光（accentDim 55% 横向渐变，8 段近似）
    for (int i = 0; i < 8; i++) {
      const float t = (float)i / 7;
      const float a = 0.55f * std::sin(t * 3.14159265f);
      d.rect(Rect((float)d.W * i / 8, (float)d.H - 2, (float)d.W / 8 + 1, 2),
             ca(sk.accentDim, a));
    }
  }

  // —— NeonCoverBackdrop：选中封面全屏 + Crossfade(420ms) + 四段压暗层 ——
  void drawBackground(App& app, float dt) {
    const Skin& sk = Theme::skin();
    Draw& d = app.draw;
    const Rect full(0, 0, (float)d.W, (float)d.H);
    // ★v1.2.6 自定义主页背景（对齐 Android AppSettings.homeBackgroundUri）：
    //   设置 → 外观 → 主页背景 选择图片后，替代封面流背景（aspect-fill 不变形）
    if (!app.config.homeBackground.empty()) {
      SDL_Texture* bgTex = app.texture(app.config.homeBackground);
      if (bgTex) {
        d.textureCrop(bgTex, full);
        // 与封面背景同款四段压暗层（保证前景文字可读）
        const Color8 s0(0x02, 0x04, 0x0C, 0xCC), s1(0x03, 0x05, 0x12, 0x8C);
        const Color8 s2(0x03, 0x06, 0x14, 0x99), s3(0x02, 0x03, 0x0A, 0xE6);
        const int bands = 32;
        for (int i = 0; i < bands; i++) {
          const float t = (float)i / bands;
          Color8 c = t < 0.30f ? s0.lerp(s1, t / 0.30f)
                    : t < 0.62f ? s1.lerp(s2, (t - 0.30f) / 0.32f)
                                : s2.lerp(s3, (t - 0.62f) / 0.38f);
          d.rect(Rect(0, (float)d.H * t, (float)d.W, (float)d.H / bands + 1), c);
        }
        for (int i = 0; i < 8; i++) {  // 底部地平线微光（2dp）
          const float t = (float)i / 7;
          d.rect(Rect((float)d.W * i / 8, (float)d.H - 2, (float)d.W / 8 + 1, 2),
                 ca(sk.accentDim, 0.55f * std::sin(t * 3.14159265f)));
        }
        bgCur_ = nullptr;
        bgPrev_ = nullptr;
        return;
      }
    }
    SDL_Texture* bg =
        games_.empty() ? nullptr : w::coverTexture(app, games_[(size_t)sel_]);
    if (bg != bgCur_) {
      bgPrev_ = bgCur_;
      bgCur_ = bg;
      bgFade_ = 0;
    }
    if (!bgCur_ && !bgPrev_) {
      neonBackdrop(d);
      return;
    }
    if (bgPrev_ && bgPrev_ != bgCur_ && bgFade_ < 1.0f) {
      d.textureCrop(bgPrev_, full);
      d.textureCrop(bgCur_, full, Color8(255, 255, 255, (uint8_t)(bgFade_ * 255)));
    } else {
      if (bgCur_) d.textureCrop(bgCur_, full);
      bgPrev_ = nullptr;
    }
    // 压暗层（APK：0xCC02040C → 0x8C030512@30% → 0x99030614@62% → 0xE602030A）
    const Color8 s0(0x02, 0x04, 0x0C, 0xCC), s1(0x03, 0x05, 0x12, 0x8C);
    const Color8 s2(0x03, 0x06, 0x14, 0x99), s3(0x02, 0x03, 0x0A, 0xE6);
    const int bands = 32;
    for (int i = 0; i < bands; i++) {
      const float t = (float)i / bands;
      Color8 c = t < 0.30f ? s0.lerp(s1, t / 0.30f)
                : t < 0.62f ? s1.lerp(s2, (t - 0.30f) / 0.32f)
                            : s2.lerp(s3, (t - 0.62f) / 0.38f);
      d.rect(Rect(0, (float)d.H * t, (float)d.W, (float)d.H / bands + 1), c);
    }
    // 底部地平线微光（2dp）
    for (int i = 0; i < 8; i++) {
      const float t = (float)i / 7;
      d.rect(Rect((float)d.W * i / 8, (float)d.H - 2, (float)d.W / 8 + 1, 2),
             ca(sk.accentDim, 0.55f * std::sin(t * 3.14159265f)));
    }
    bgFade_ = std::min(1.0f, bgFade_ + dt / 0.42f);
  }

  // —— NeonHomeTopBar：品牌切角牌 + N GAMES + 时钟 ——
  void drawTopBar(App& app, const Layout& L) {
    const Skin& sk = Theme::skin();
    Draw& d = app.draw;
    const float D = L.D;
    const float pad = 10 * D, brandH = 27 * D;
    const float iconS = 17 * D, innerPad = 12 * D;
    const float textW = app.font.textWidth("NESSTATION", 14 * D, true);
    const Rect pill(20 * D, pad, innerPad * 2 + iconS + 7 * D + textW, brandH);
    d.chamferRect(pill, pill.minDim() * 0.35f, ca(sk.accent, 0.12f));
    d.chamferRectOutline(pill, pill.minDim() * 0.35f, sk.accent, 1.2f);
    w::icon(app, 9, Rect(pill.x + innerPad, pill.cy() - iconS / 2, iconS, iconS),
            sk.accent);
    app.font.drawVCentered(app.sdlRenderer(), "NESSTATION",
                           pill.x + innerPad + iconS + 7 * D, pill.cy(), 14 * D,
                           sk.textHi, true);
    // "N GAMES"（12sp Bold accent）
    const std::string count = std::to_string(games_.size()) + " GAMES";
    app.font.drawVCentered(app.sdlRenderer(), count, pill.right() + 14 * D, pill.cy(),
                           12 * D, sk.accent, true);
    // 时钟（13sp Medium textDim，右侧）
    const std::string tm = clockText();
    const float tw = app.font.textWidth(tm, 13 * D);
    app.font.drawVCentered(app.sdlRenderer(), tm, (float)d.W - 20 * D - tw, pill.cy(),
                           13 * D, sk.textDim);
  }

  // —— NeonMenuRow：九枚切角 chip（图标 15dp + 12sp SemiBold）——
  void drawMenuRow(App& app, const Layout& L, const InputState& in) {
    const Skin& sk = Theme::skin();
    Draw& d = app.draw;
    const float D = L.D;
    struct Chip {
      const char* label;
      int iconId;
      Color8 col;
      int action;
    };
    const Chip chips[kMenuCount] = {
        {"游戏库", 12, sk.red, 0},     {"收藏", 13, sk.gold, 1},
        {"最近", 14, sk.blue, 2},      {"在线游戏", 22, sk.accent, 7},
        {"对战平台", 9, sk.purple, 8}, {"SWF", 23, sk.gold, 9},
        {"设置", 3, sk.text, 4},       {"关于", 17, sk.text, 5},
        {"退出", 18, sk.red, 6},
    };
    float x = 20 * D;
    const float iconS = 15 * D;
    const float hPad = 13 * D, vPad = 7 * D;
    for (int i = 0; i < kMenuCount; i++) {
      const float tw = app.font.textWidth(chips[i].label, 12 * D, true);
      const float w = hPad + iconS + 6 * D + tw + hPad;
      const float h = vPad * 2 + std::max(iconS, app.font.lineHeight(12 * D));
      const Rect rc(x, L.menuY, w, h);
      const bool hover = rc.contains(in.mouse);
      if (hover) {  // 悬停即聚焦（鼠标优先）
        if (!menuFocus_ || menuSel_ != i) {
          menuFocus_ = true;
          menuSel_ = i;
        }
        if (in.mousePressed) homeAction(app, chips[i].action);
      }
      const bool hot = (menuFocus_ && menuSel_ == i) || hover;
      d.chamferRect(rc, rc.minDim() * 0.30f, ca(sk.bgPanel, 0.62f));
      d.chamferRectOutline(rc, rc.minDim() * 0.30f, hot ? sk.accent : sk.line,
                           hot ? 1.4f : 1.1f);
      if (hot) d.glowRect(rc, rc.minDim() * 0.30f, ca(sk.accent, 0.20f), 5);
      w::icon(app, chips[i].iconId, Rect(rc.x + hPad, rc.cy() - iconS / 2, iconS, iconS),
              chips[i].col);
      app.font.drawVCentered(app.sdlRenderer(), chips[i].label,
                             rc.x + hPad + iconS + 6 * D, rc.cy(), 12 * D, sk.text,
                             true);
      x += w + 9 * D;
    }
  }

  // —— NeonFlow 主视觉 + 倒影（弧形导轨数学 1:1 APK）——
  void drawFlow(App& app, const Layout& L, const InputState& in) {
    const Skin& sk = Theme::skin();
    Draw& d = app.draw;
    const float D = L.D;
    hitCards_.clear();
    if (games_.empty()) return;
    const int N = (int)games_.size();
    const int bakeW = (int)(L.cardW * 1.5f);
    const int from = std::max(0, sel_ - 5), to = std::min(N - 1, sel_ + 5);
    // APK 绘制顺序：索引升序（右侧后画 = 叠在上方）
    for (int i = from; i <= to; i++) {
      const float pos = (float)i - animSel_;   // 动画位（连续）
      const float aPos = std::fabs(pos);
      const int dist = std::abs(i - sel_);     // 静态距离（命中判定）
      // 弧形导轨：X 平移 + dragPx 跟手；Y = shift + aPos²·5.5 + aPos·3（dp）
      const float cx = L.flowCx + pos * L.step + dragPx_;
      const float cy = L.flowCy + L.shift + (aPos * aPos * 5.5f + aPos * 3.0f) * D;
      // 缩放：中心 1.12 / 两侧 1-0.19/步（下限 0.5）× fitScale
      const float sideScale = std::max(0.5f, 1.0f - 0.19f * aPos);
      const float s = (aPos < 0.5f ? 1.12f : sideScale) * L.fitScale;
      const float w = L.cardW * s, h = L.cardH * s;
      // 透视：rotationY = pos*34°（±54.4°）
      const float tilt = std::max(-54.4f, std::min(54.4f, pos * 34.0f));
      // 逐级变暗：1-0.20/步（下限 0.30）
      const float alpha = std::max(0.30f, 1.0f - 0.20f * aPos);
      const bool selected = dist == 0 && aPos < 0.5f;
      SDL_Texture* tex = cardTexture(app, games_[(size_t)i], selected, bakeW);
      // 选中卡：青色辉光 + 上浮 6px（APK shadowElevation18 + translationY-6）
      if (selected) {
        const float glow = 18 * D;
        const Rect gr(cx - w / 2 - glow, cy - h / 2 - glow - 6 * D, w + glow * 2,
                      h + glow * 2);
        d.glowRect(gr, w * 0.10f, ca(sk.accent, 0.30f), glow);
      }
      const float liftY = selected ? -6 * D : 0;
      if (tex) {
        drawCard3D(app, tex, cx, cy + liftY, w, h, tilt, alpha);
        // 倒影（showReflection：高度 0.28，alpha 0.24，渐隐到深空底）
        const float refH = h * 0.28f;
        drawCard3D(app, tex, cx, cy + liftY + h / 2 + refH / 2, w, refH, tilt,
                   alpha * 0.24f, true);
        // 倒影渐隐遮罩（覆盖在倒影区上）
        const float rw = w * std::cos(tilt * 3.14159265f / 180.0f);
        drawReflectionFade(d, Rect(cx - rw / 2, cy + liftY + h / 2, rw, refH), sk);
      } else {
        // 软件渲染器回退：平面卡片（无透视/倒影）
        drawCardContent(app, games_[(size_t)i],
                        Rect(cx - w / 2, cy + liftY - h / 2, w, h), selected);
      }
      // 命中矩形（鼠标：点侧卡居中 / 点中卡启动）
      if (alpha > 0.3f)
        hitCards_.push_back({Rect(cx - w / 2, cy + liftY - h / 2, w, h), i});
    }
    // 本帧命中（后画的上层优先）：dist==0 启动，否则选中
    if (in.mousePressed && !dragging_) {
      int hit = -1;
      for (auto it = hitCards_.rbegin(); it != hitCards_.rend(); ++it)
        if (it->first.contains(in.mouse)) {
          hit = it->second;
          break;
        }
      if (hit >= 0) {
        if (hit == sel_)
          app.launchGame(games_[(size_t)sel_]);
        else
          sel_ = hit;
      }
    }
    if (in.mouseRightPressed) {
      int hit = -1;
      for (auto it = hitCards_.rbegin(); it != hitCards_.rend(); ++it)
        if (it->first.contains(in.mouse)) {
          hit = it->second;
          break;
        }
      if (hit >= 0) {
        sel_ = hit;
        openGameMenu(app, games_[(size_t)hit], Vec2(in.mouse));
      }
    }
  }

  // —— 右侧信息面板（APK NeonGameInfoPanel compact=true：透明底 + 描影）——
  void drawInfoPanel(App& app, const Layout& L, const InputState& in) {
    const Skin& sk = Theme::skin();
    Draw& d = app.draw;
    const float D = L.D;
    if (games_.empty()) return;
    const GameEntry& g = games_[(size_t)sel_];

    // 内容总高（垂直居中，APK Arrangement.Center）
    const float titleSize = 19 * D, metaSize = 11 * D;
    const float titleLH = app.font.lineHeight(titleSize);
    auto lines = app.font.wrap(g.displayTitle(), titleSize, L.panelW - 34 * D, true);
    if (lines.size() > 2) {  // APK maxLines=2 + Ellipsis
      lines.resize(2);
      lines[1] = app.font.ellipsize(lines[1], titleSize, L.panelW - 34 * D, true);
    }
    const float starS = 18 * D;
    const float badgeH = 3 * D * 2 + app.font.lineHeight(10 * D);
    const float btnH = 9 * D * 2 + app.font.lineHeight(13 * D);
    const float ghostH = 9 * D * 2 + app.font.lineHeight(12 * D);
    const float metaLH = app.font.lineHeight(metaSize);
    const float total = lines.size() * titleLH + 9 * D + badgeH + 9 * D + metaLH * 2 +
                        4 * D + 16 * D + btnH + 9 * D + ghostH;
    float y = L.flowCy - total / 2;
    const float x0 = L.panelX;
    const float titleTop = y;

    // 标题（最多 2 行，19sp Bold textHi，描影 0xF2000000/12f）
    for (auto& ln : lines) {
      app.font.draw(app.sdlRenderer(), ln, x0 + 1.5f, y + 1.5f, titleSize,
                    Color8(0, 0, 0, 184), true);
      app.font.draw(app.sdlRenderer(), ln, x0, y, titleSize, sk.textHi, true);
      y += titleLH;
    }
    // ★/☆（18sp 金，右侧同行，点击切换收藏 —— APK clickable）
    {
      const Rect starRc(x0 + L.panelW - starS - 8 * D, titleTop, starS + 16 * D,
                        lines.size() * titleLH);
      const std::string st = g.favorite ? "★" : "☆";
      app.font.draw(app.sdlRenderer(), st, starRc.x, titleTop, starS, sk.gold, true);
      if (starRc.contains(in.mouse) && in.mousePressed) {
        app.library.toggleFavorite(g.id);
        app.library.save();
        refresh(app);
      }
    }
    y += 9 * D;
    // 平台徽章（accent 14% 底 + AccentDim 边 + chamfer 0.5 + 10sp Bold accent）
    {
      const std::string pn = platformShortName(g.platform);
      const float tw = app.font.textWidth(pn, 10 * D, true);
      const Rect brc(x0, y, 9 * D * 2 + tw, badgeH);
      d.chamferRect(brc, brc.minDim() * 0.5f, ca(sk.accent, 0.14f));
      d.chamferRectOutline(brc, brc.minDim() * 0.5f, sk.accentDim, 1.1f);
      app.font.drawVCentered(app.sdlRenderer(), pn, brc.x + 9 * D, brc.cy(), 10 * D,
                             sk.accent, true);
      y += badgeH + 9 * D;
    }
    // 最近 / 时长（11sp Medium text，描影）
    const std::string l1 = "最近 " + lastPlayedText(g.lastPlayedAt);
    const std::string l2 = "时长 " + playTimeText(g.playTimeSec);
    app.font.draw(app.sdlRenderer(), l1, x0 + 1.2f, y + 1.2f, metaSize,
                  Color8(0, 0, 0, 184));
    app.font.draw(app.sdlRenderer(), l1, x0, y, metaSize, sk.text);
    y += metaLH + 4 * D;
    app.font.draw(app.sdlRenderer(), l2, x0 + 1.2f, y + 1.2f, metaSize,
                  Color8(0, 0, 0, 184));
    app.font.draw(app.sdlRenderer(), l2, x0, y, metaSize, sk.text);
    y += metaLH + 16 * D;
    // 主按钮：继续游戏 / 开始游戏（NeonPrimaryButton：横渐变 accentDim→accent）
    {
      const std::string label = g.lastPlayedAt > 0 ? "继续游戏" : "开始游戏";
      const float tw = app.font.textWidth(label, 13 * D, true);
      const Rect rc(x0, y, 20 * D + 16 * D + 7 * D + tw, btnH);
      if (w::primaryButton(app, rc, label, 1)) app.launchGame(g);
      y += btnH + 9 * D;
    }
    // 幽灵按钮：游戏库（NeonGhostButton：BgPanel 55% 底 + Line 边）
    {
      const std::string label = "游戏库";
      const float tw = app.font.textWidth(label, 12 * D, true);
      const Rect rc(x0, y, 16 * D + 15 * D + 6 * D + tw, ghostH);
      const bool hover = rc.contains(in.mouse);
      d.chamferRect(rc, rc.minDim() * 0.30f, ca(sk.bgPanel, 0.55f));
      d.chamferRectOutline(rc, rc.minDim() * 0.30f, hover ? sk.accent : sk.line, 1.1f);
      w::icon(app, 12, Rect(rc.x + 16 * D, rc.cy() - 7.5 * D, 15 * D, 15 * D),
              sk.text);
      app.font.drawVCentered(app.sdlRenderer(), label,
                             rc.x + 16 * D + 15 * D + 6 * D, rc.cy(), 12 * D, sk.text,
                             true);
      if (hover && in.mousePressed) app.pushScene(createLibraryScene(-1));
    }
  }

  // —— 空库引导（APK：手柄图标 + 还没有游戏 + 提示 + 进入游戏库）——
  void drawEmpty(App& app, const Layout& L) {
    const Skin& sk = Theme::skin();
    Draw& d = app.draw;
    const float D = L.D;
    const float cx = L.flowCx;
    const float cy = L.flowCy;
    w::iconGamepad(app, Rect(cx - 32 * D, cy - 76 * D, 64 * D, 64 * D), sk.textDim);
    const std::string t1 = "还没有游戏";
    app.font.draw(app.sdlRenderer(), t1, cx - app.font.textWidth(t1, 20 * D, true) / 2,
                  cy + 4 * D, 20 * D, sk.textHi, true);
    const std::string t2 = "去游戏库导入 ROM 开始游玩";
    app.font.draw(app.sdlRenderer(), t2, cx - app.font.textWidth(t2, 12 * D) / 2,
                  cy + 36 * D, 12 * D, sk.textDim);
    const float tw = app.font.textWidth("进入游戏库", 13 * D, true);
    const Rect go(cx - (20 * D + tw + 20 * D) / 2, cy + 72 * D,
                  20 * D + tw + 20 * D, 9 * D * 2 + app.font.lineHeight(13 * D));
    if (w::primaryButton(app, go, "进入游戏库", 12))
      app.pushScene(createLibraryScene(-1));
  }

  // —— NeonHintsBar：底部居中切角胶囊（A 启动 / Y 收藏 / X 游戏库）——
  void drawHintsBar(App& app, const Layout& L) {
    const Skin& sk = Theme::skin();
    Draw& d = app.draw;
    const float D = L.D;
    struct Hint {
      const char letter[2];
      const char* label;
      Color8 col;
    };
    const Hint hints[3] = {
        {"A", "启动", sk.btnA}, {"Y", "收藏", sk.btnY}, {"X", "游戏库", sk.btnX}};
    // 胶囊宽 = 14dp×2 + Σ[圆16dp + 5dp + 文字] + 16dp×2（组间距）
    float inner = 0;
    for (int i = 0; i < 3; i++)
      inner += 16 * D + 5 * D + app.font.textWidth(hints[i].label, 11 * D);
    inner += 16 * D * 2;
    const float h = 7 * D * 2 + std::max(16 * D, app.font.lineHeight(11 * D));
    const Rect bar(((float)d.W - (14 * D * 2 + inner)) / 2, (float)d.H - 10 * D - h,
                   14 * D * 2 + inner, h);
    d.chamferRect(bar, bar.minDim() * 0.5f, Color8(0x05, 0x07, 0x0E, 204));
    d.chamferRectOutline(bar, bar.minDim() * 0.5f, sk.line, 1.1f);
    float x = bar.x + 14 * D;
    for (int i = 0; i < 3; i++) {
      const Color8& c = hints[i].col;
      // 圆形字母键帽（color 16% 底 + color 边 + 10sp Black）
      const float r = 8 * D;
      d.circle(x + r, bar.cy(), r, ca(c, 0.16f));
      d.circleOutline(x + r, bar.cy(), r, c, 1.1f);
      app.font.drawVCentered(app.sdlRenderer(), hints[i].letter, x, bar.cy(), 10 * D,
                             c, true);
      x += 16 * D + 5 * D;
      x += app.font.drawVCentered(app.sdlRenderer(), hints[i].label, x, bar.cy(),
                                  11 * D, sk.text) +
           16 * D;
    }
  }

  void draw(App& app) override {
    const Layout L = layout(app);
    const InputState& in = app.input.input();

    // 背景（封面 Crossfade / NeonBackdrop）
    drawBackground(app, appDt_ > 0 ? appDt_ : 0.016f);

    // 顶部品牌条 + 菜单行
    drawTopBar(app, L);
    drawMenuRow(app, L, in);

    // 主视觉 + 信息面板（APK 横屏：Row[Flow(weight 1f), InfoPanel(300dp)]）
    drawFlow(app, L, in);
    if (!games_.empty())
      drawInfoPanel(app, L, in);
    else
      drawEmpty(app, L);

    // 底部按键提示
    drawHintsBar(app, L);
  }

  // —— 封面卡右键快捷菜单（Android「长按卡片」的 PC 等价交互）——
  // 按值捕获 GameEntry（回调执行时 games_ 可能已刷新，不能持引用）
  void openGameMenu(App& app, const GameEntry& gIn, Vec2 at) {
    const GameEntry g = gIn;  // 显式拷贝，防悬垂引用
    std::vector<ContextMenuItem> items;
    items.push_back({1, "启动游戏", "Enter", false, [&app, g]() {
      app.launchGame(g);
    }});
    items.push_back({2, g.favorite ? "取消收藏" : "加入收藏", "F", false,
                     [this, &app, g]() {
                       app.library.toggleFavorite(g.id);
                       app.library.save();
                       refresh(app);
                     }});
    // ★v1.2.6 自定义封面（对齐 Android「长按卡片 → 自定义封面」）
    items.push_back({5, "自定义封面…", "", false, [this, &app, g]() {
      BrowserRequest req;
      req.title = "选择封面图片（" + g.displayTitle() + "）";
      req.mode = 3;  // 图片选择（png/jpg/jpeg/bmp/webp）
      req.startDir = fsutil::homeDir();
      req.onPick = [this, &app, g](const std::string& picked) {
        const std::string err = setCustomCover(app.library, g.id, picked);
        if (err.empty()) {
          app.invalidateTexture(g.coverPath);  // 旧封面缓存失效
          refresh(app);                        // 重载条目 → 重烘封面卡
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
      items.push_back({20, "恢复默认封面", "", false, [this, &app, g]() {
        if (GameEntry* e = app.library.find(g.id)) {
          app.invalidateTexture(e->coverPath);
          e->coverPath.clear();
        }
        app.library.save();
        app.covers.requestFetch(g);
        refresh(app);
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
    // 平台多核心（进程内 + 已就绪外置独立核心）→ 重新选择入口
    if (app.cores.installedCoresFor(g.platform).size() +
            (size_t)app.externals.availableCount(g.platform) >
        1) {
      items.push_back({16, "重新选择核心", "", false, [&app, g]() {
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
      refresh(app);
      app.toast("已从库中移除：" + g.displayTitle());
    }});
    pushContextMenu(app, at, g.displayTitle(),
                    std::string(platformShortName(g.platform)) + " · " +
                        platformFullName(g.platform) +
                        (g.favorite ? " · ★ 已收藏" : ""),
                    std::move(items));
  }

 private:
  static constexpr int kMenuCount = 9;
  static constexpr int kMenuAction[kMenuCount] = {0, 1, 2, 7, 8, 9, 4, 5, 6};
  std::vector<GameEntry> games_;
  std::map<std::string, SDL_Texture*> baked_;
  int sel_ = 0;
  float animSel_ = 0;
  int menuSel_ = 0;
  bool menuFocus_ = false;   // 默认焦点在封面流（Android 主交互）
  int libCount_ = -1;
  std::vector<std::pair<Rect, int>> hitCards_;
  bool dragging_ = false;
  float dragLastX_ = 0;
  float dragAccum_ = 0;
  float dragPx_ = 0;         // 拖拽跟手偏移（松手平滑归零）
  float appDt_ = 0.016f;     // 最近帧间隔（背景 Crossfade 计时）
  // 背景淡入淡出（APK Crossfade 420ms）
  SDL_Texture* bgPrev_ = nullptr;
  SDL_Texture* bgCur_ = nullptr;
  float bgFade_ = 1.0f;
};

// ==================== FSD 经典磁贴主页（★v1.2.9 完全重制） ====================
// 1:1 对齐 APK FsdHomeScreen.kt + FsdScaffold.kt + FsdTileMenu.kt + FsdCoverFlow.kt：
//   · 背景 FsdBackdrop（深蓝三段渐变 + 中央放射高光 + 斜向光带 + 底部渐暗；
//     自定义主页背景图仍可覆盖）—— 不再误用像素天空（那是 APK 设置页的背景）
//   · FsdTopBar：CPU / 内存 / 存储 实时状态（标签 + 数值 + 迷你进度条，
//     绿 <70% / 黄 <90% / 红 ≥90%）+ NES STATION + 时钟
//   · 3 个分区行（垂直封面流：选中行居中放大 1.0，上下行 0.66/0.32 缩放 +
//     0.55/0.10 变暗，260ms 平滑动画；↑↓ / 滚轮 / 纵向拖拽切换）：
//       ① 游戏库 —— 全部游戏 + 各平台磁贴（仅显示有游戏的平台；APK 同序 16 平台；
//          每平台专属图标 + 数量徽标；点击直达该平台封面流）
//       ② 在线 · 对战 · SWF
//       ③ 设置 · 关于 · 退出
//   · 分区标题条：黄色圆点 + 白色粗体大标题（FsdSectionHeader）
//   · 行内横向封面流：磁贴 220×136dp、步进 244dp、每步缩放 0.18 / 淡出 0.20 /
//     透视 ±10°（textureQuad 梯形近似）；选中分区中当前磁贴黄色 3dp 高亮边框
//   · FsdButtonHints：A 选择（绿）/ B 返回（红）彩色圆钮
//   · FsdBottomBar：网络图标 + 局域网 IP + 状态 + 日期时间（每秒刷新）
//   · 磁贴（FsdTile）：蓝(2E86E0→0D4FA8)/黄(F7B500→DE8A00)对角分割 +
//     白色平台图标（可自定义图片）+ 右上数量徽标 + 左下深色粗体标签 +
//     玻璃斜向高光；整体透明度可调（APK homeTileIconAlphas）
//   · 磁贴选项（右键 / E 键）：自定义图标（选图片）/ 卡片透明度滑杆 /
//     应用到全部 / 恢复默认 —— 对齐 APK 长按磁贴对话框，持久化到 config
// （fsdhome 系统状态/图标映射已移至 ui/FsdChrome.h —— 主页与游戏库共用）

// —— 磁贴选项对话框（对齐 APK「磁贴选项」AlertDialog：透明度滑杆 +
//    自定义图标 + 应用到全部 + 恢复默认）——
class FsdTileOptionsScene : public Scene {
 public:
  FsdTileOptionsScene(std::string tileKey, std::string tileLabel)
      : key_(std::move(tileKey)), label_(std::move(tileLabel)) {}

  void onEnter(App& app) override {
    auto it = app.config.homeTileIconAlphas.find(key_);
    alpha_ = it == app.config.homeTileIconAlphas.end() ? 1.0f : it->second;
    custom_ = app.config.homeTileIcons.count(key_) > 0;
  }

  void update(App& app, float dt, const InputState& in) override {
    (void)dt;
    if (in.pressed[(int)Btn::Back] || in.pressed[(int)Btn::Menu]) {
      app.popScene();
      return;
    }
    if (in.pressed[(int)Btn::Up]) {
      sliderFocus_ = false;
      btnFocus_ = (btnFocus_ + 3) % 4;
      if (btnFocus_ == 2 && !custom_) btnFocus_ = (btnFocus_ + 3) % 4;
    }
    if (in.pressed[(int)Btn::Down] && !sliderFocus_) {
      btnFocus_ = (btnFocus_ + 1) % 4;
      if (btnFocus_ == 2 && !custom_) btnFocus_ = 3;
    }
    if ((in.pressed[(int)Btn::Left] || in.pressed[(int)Btn::Right]) && sliderFocus_) {
      alpha_ += in.pressed[(int)Btn::Right] ? 0.05f : -0.05f;
      alpha_ = std::max(0.05f, std::min(1.0f, alpha_));
      app.config.homeTileIconAlphas[key_] = alpha_;
      app.saveConfig();
    }
    if (in.pressed[(int)Btn::Ok]) {
      if (sliderFocus_) {
        sliderFocus_ = false;
        btnFocus_ = 0;
      } else if (btnFocus_ == 0) {
        pickIcon(app);
      } else if (btnFocus_ == 1) {
        applyAll(app);
      } else if (btnFocus_ == 2 && custom_) {
        resetIcon(app);
      } else {
        app.popScene();
      }
    }
  }

  void pickIcon(App& app) {
    App* appPtr = &app;
    std::string key = key_;
    app.popScene();  // 关闭自身，避免文件浏览器叠两层
    BrowserRequest req;
    req.title = "选择磁贴图标（" + label_ + "）";
    req.mode = 3;  // 选图片
    req.startDir = app.dataDir();
    req.onPick = [appPtr, key](const std::string& path) {
      // 拷贝到数据目录 icons/（对齐 APK：filesDir/icons/tile_<key>_<ts>.png）
      std::string data;
      if (!fsutil::readFile(path, &data)) {
        appPtr->toast("无法读取所选图片");
        return;
      }
      std::string safeKey = key;
      for (char& c : safeKey)
        if (c == ':' || c == '/' || c == '\\') c = '_';
      std::string ext = "png";
      const size_t dot = path.find_last_of('.');
      if (dot != std::string::npos) {
        ext = path.substr(dot + 1);
        for (char& c : ext) c = (char)tolower((unsigned char)c);
      }
      const std::string dir = fsutil::join(appPtr->dataDir(), "icons");
      fsutil::makeDirs(dir);
      const std::string dest = fsutil::join(
          dir, "tile_" + safeKey + "_" + std::to_string(time(nullptr)) + "." + ext);
      if (!fsutil::writeFile(dest, data)) {
        appPtr->toast("图标保存失败");
        return;
      }
      appPtr->config.homeTileIcons[key] = dest;
      appPtr->saveConfig();
      appPtr->toast("已设置磁贴图标（返回主页生效）");
    };
    req.onCancel = [] {};
    browsePath(app, req);
  }

  void applyAll(App& app) {
    // 一键把当前透明度同步到所有磁贴（对齐 APK「将此透明度应用到所有卡片」）
    const float v = alpha_;
    app.config.homeTileIconAlphas["all"] = v;
    for (GamePlatform p : fsdchrome::kHomePlatformOrder)
      app.config.homeTileIconAlphas[std::string("platform:") + platformShortName(p)] = v;
    for (const char* k : {"online", "battle", "swf", "settings", "about", "exit"})
      app.config.homeTileIconAlphas[k] = v;
    app.saveConfig();
    app.toast("已将透明度应用到所有磁贴（返回主页生效）");
  }

  void resetIcon(App& app) {
    app.config.homeTileIcons.erase(key_);
    app.saveConfig();
    custom_ = false;
    app.toast("已恢复默认图标（返回主页生效）");
  }

  void draw(App& app) override {
    Draw& d = app.draw;
    const InputState& in = app.input.input();
    // 遮罩
    d.rect(Rect(0, 0, (float)d.W, (float)d.H), Color8(0, 0, 0, 132));
    // 面板（★v1.2.10 dp 基准 540/960，与 NeonHome::dpOf 一致）
    const float D = std::max(0.75f, std::min((float)d.H / 540.0f, (float)d.W / 960.0f));
    const float pw = std::min(470.0f * D, (float)d.W - 40);
    const float ph = 330 * D;
    const Rect panel((float)d.W / 2 - pw / 2, (float)d.H / 2 - ph / 2, pw, ph);
    d.roundRect(panel, 12 * D, Color8::hex(0x0D2C55, 0xF2));
    d.roundRectOutline(panel, 12 * D, Color8::hex(0x2E86E0), 1.4f);
    float y = panel.y + 18 * D;
    app.font.draw(app.sdlRenderer(), "磁贴选项 · " + label_, panel.x + 20 * D, y, 17 * D,
                  Color8(255, 255, 255), true);
    y += 30 * D;
    app.font.draw(app.sdlRenderer(),
                  "可为本磁贴设置自定义图标（支持图片）。图标立即生效并持久保存。",
                  panel.x + 20 * D, y, 12 * D, Color8(0x9F, 0xB6, 0xD4));
    y += 24 * D;
    // 透明度行
    {
      const Rect row(panel.x + 16 * D, y, pw - 32 * D, 58 * D);
      d.roundRect(row, 8 * D, Color8(0x06, 0x12, 0x25, 200));
      app.font.draw(app.sdlRenderer(), "卡片透明度", row.x + 14 * D, row.y + 10 * D,
                    14 * D, Color8(0xE8, 0xF1, 0xFF), true);
      char pct[16];
      snprintf(pct, sizeof(pct), "%d%%", (int)(alpha_ * 100 + 0.5f));
      app.font.draw(app.sdlRenderer(), pct,
                    row.right() - 14 * D - app.font.textWidth(pct, 13 * D), row.y + 10 * D,
                    13 * D, Color8(0x9F, 0xB6, 0xD4));
      const Rect slider(row.x + 14 * D, row.y + 34 * D, row.w - 28 * D, 16 * D);
      float v01 = (alpha_ - 0.05f) / 0.95f;
      const bool hadFocus = sliderFocus_;
      if (w::sliderRow(app, slider, "", v01, hadFocus)) {
        alpha_ = 0.05f + v01 * 0.95f;
        app.config.homeTileIconAlphas[key_] = alpha_;
        app.saveConfig();
      }
      // 鼠标/键盘焦点切换
      if (in.mousePressed && row.contains(in.mouse)) sliderFocus_ = true;
      if (in.pressed[(int)Btn::Down] && sliderFocus_) {
        sliderFocus_ = false;
        btnFocus_ = 0;
      }
      y += 66 * D;
    }
    // 按钮：自定义图标 / 应用到全部 / 恢复默认(有自定义时) / 关闭
    const float bw = (pw - 32 * D - 10 * D) / 2;
    pickRect_ = Rect(panel.x + 16 * D, y, bw, 40 * D);
    allRect_ = Rect(pickRect_.right() + 10 * D, y, bw, 40 * D);
    resetRect_ = Rect(panel.x + 16 * D, y + 48 * D, bw, 40 * D);
    closeRect_ = Rect(resetRect_.right() + 10 * D, y + 48 * D, bw, 40 * D);
    if (w::primaryButton(app, pickRect_, "自定义图标") || w::activated(app, pickRect_, btnFocus_ == 0 && !sliderFocus_))
      pickIcon(app);
    if (w::button(app, allRect_, "应用到全部磁贴", btnFocus_ == 1 && !sliderFocus_))
      applyAll(app);
    if (custom_) {
      if (w::button(app, resetRect_, "恢复默认图标", btnFocus_ == 2 && !sliderFocus_))
        resetIcon(app);
    } else {
      d.roundRect(resetRect_, 8 * D, Color8(0x06, 0x12, 0x25, 140));
      app.font.drawVCentered(app.sdlRenderer(), "（无自定义图标）", resetRect_.x + 14 * D,
                             resetRect_.cy(), 12 * D, Color8(0x5E, 0x6F, 0x85));
    }
    if (w::button(app, closeRect_, "关闭", btnFocus_ == 3 && !sliderFocus_)) app.popScene();
  }

 private:
  std::string key_, label_;
  float alpha_ = 1.0f;
  bool custom_ = false;
  bool sliderFocus_ = true;
  int btnFocus_ = 0;
  Rect pickRect_, allRect_, resetRect_, closeRect_;
};

class FsdHome : public Scene {
 public:
  ~FsdHome() { baked_.clear(); }

  struct Tile {
    std::string key;    // "all" / "platform:NES" / "online" / ...
    std::string label;  // 显示名
    int iconId = 9;     // 矢量图标
    std::string badge;  // 右上徽标（空=无）
  };
  struct Section {
    std::string key, title;
    std::vector<Tile> tiles;
    int sel = 0;
    float animSel = 0;  // 行内动画位
  };

  void onEnter(App& app) override {
    timeMs_ = 0;
    rebuild(app);
    animSec_ = (float)secSel_;
    ip_ = fsdchrome::localIp();
    lastStats_ = 0;
  }
  void onExit(App& app) override {
    (void)app;
    for (auto& kv : baked_)
      if (kv.second) SDL_DestroyTexture(kv.second);
    baked_.clear();
  }

  static float dpOf(const App& app) {
    // ★v1.2.10：540/960 基准（对齐逻辑画布与 APK 横屏密度；旧 720/1280 恒缩 0.75）
    const float dH = (float)app.draw.H / 540.0f;
    const float dW = (float)app.draw.W / 960.0f;
    return std::max(0.7f, std::min(dH, dW));
  }

  float tileAlpha(const App& app, const std::string& key) const {
    auto it = app.config.homeTileIconAlphas.find(key);
    if (it == app.config.homeTileIconAlphas.end()) return 1.0f;
    return std::max(0.05f, std::min(1.0f, it->second));
  }

  void rebuild(App& app) {
    // —— 分区结构（对齐 APK sections）——
    sections_.clear();
    Section lib;
    lib.key = "library";
    lib.title = "游戏库";
    Tile all;
    all.key = "all";
    all.label = "全部游戏";
    all.iconId = 12;
    all.badge = std::to_string(app.library.games().size());
    lib.tiles.push_back(all);
    for (GamePlatform p : fsdchrome::kHomePlatformOrder) {
      const int n = app.library.countFor(p);
      if (n <= 0) continue;
      Tile t;
      t.key = std::string("platform:") + platformShortName(p);
      t.label = platformShortName(p);
      t.iconId = fsdchrome::platformTileIcon(p);
      t.badge = std::to_string(n);
      lib.tiles.push_back(t);
    }
    lib.sel = std::min(libSel_, (int)lib.tiles.size() - 1);
    if (lib.sel < 0) lib.sel = 0;
    lib.animSel = (float)lib.sel;
    sections_.push_back(std::move(lib));

    Section online;
    online.key = "online";
    online.title = "在线 · 对战 · SWF";
    online.tiles = {{"online", "在线游戏", 22, ""},
                    {"battle", "对战平台", 33, ""},
                    {"swf", "SWF/Flash", 1, ""}};
    online.sel = std::min(onlineSel_, (int)online.tiles.size() - 1);
    online.animSel = (float)online.sel;
    sections_.push_back(std::move(online));

    Section sys;
    sys.key = "system";
    sys.title = "设置 · 关于 · 退出";
    sys.tiles = {{"settings", "设置", 3, ""},
                 {"about", "关于", 17, ""},
                 {"exit", "退出", 18, ""}};
    sys.sel = std::min(sysSel_, (int)sys.tiles.size() - 1);
    sys.animSel = (float)sys.sel;
    sections_.push_back(std::move(sys));

    libCount_ = (int)app.library.games().size();
    // 个性化指纹（图标/透明度改动 → 重建）
    persoFp_ = personalizationFp(app);
    if (secSel_ >= (int)sections_.size()) secSel_ = 0;
    app.covers.requestFetchAllMissing(app.library.games());
  }

  static std::string personalizationFp(const App& app) {
    std::string fp;
    for (const auto& kv : app.config.homeTileIcons) fp += kv.first + ">" + kv.second + ";";
    for (const auto& kv : app.config.homeTileIconAlphas)
      fp += kv.first + "~" + std::to_string(kv.second) + ";";
    return fp;
  }

  void activate(App& app, const Tile& t) {
    if (t.key == "all") {
      // 对齐 APK：FSD 模式「全部游戏」进入游戏库（默认 NES 平台页 ——
      // LibraryScreen initialPlatform ?: GamePlatform.NES）
      app.pushScene(createLibraryScene((int)GamePlatform::NES));
    } else if (t.key.rfind("platform:", 0) == 0) {
      const std::string pn = t.key.substr(9);
      app.pushScene(createLibraryScene((int)platformFromString(pn)));
    } else if (t.key == "online") {
      // ★v1.3.1 真实场景（不再 toast 占位）
      app.pushScene(createOnlineGamesScene());
    } else if (t.key == "battle") {
      app.pushScene(createBattleScene());
    } else if (t.key == "swf") {
      app.pushScene(createSwfScene());
    } else if (t.key == "settings") {
      app.pushScene(createSettingsScene());
    } else if (t.key == "about") {
      app.pushScene(createAboutScene());
    } else if (t.key == "exit") {
      app.pushScene(createMessageScene(
          "退出 NesStation？", "确定要退出应用吗？未保存的进度将按设置自动存档。",
          false, "退出", [appPtr = &app]() { appPtr->requestExit(); }));
    }
  }

  // ============ 输入 ============
  void update(App& app, float dt, const InputState& in) override {
    timeMs_ += (int64_t)(dt * 1000);
    animSec_ += ((float)secSel_ - animSec_) * std::min(1.0f, dt * 10.0f);
    if (std::fabs((float)secSel_ - animSec_) < 0.003f) animSec_ = (float)secSel_;
    for (auto& s : sections_) {
      s.animSel += ((float)s.sel - s.animSel) * std::min(1.0f, dt * 10.0f);
      if (std::fabs((float)s.sel - s.animSel) < 0.003f) s.animSel = (float)s.sel;
    }
    // 库或个性化变化 → 重建（记住三个分区的选中位）
    {
      const int cnt = (int)app.library.games().size();
      if (cnt != libCount_) {
        libSel_ = sections_.empty() ? 0 : sections_[0].sel;
        onlineSel_ = sections_.size() > 1 ? sections_[1].sel : 0;
        sysSel_ = sections_.size() > 2 ? sections_[2].sel : 0;
        rebuild(app);
      } else if (personalizationFp(app) != persoFp_) {
        libSel_ = sections_.empty() ? 0 : sections_[0].sel;
        onlineSel_ = sections_.size() > 1 ? sections_[1].sel : 0;
        sysSel_ = sections_.size() > 2 ? sections_[2].sel : 0;
        rebuild(app);
        // 重建后烘焙表里旧磁贴纹理已在 rebuild 前清理？—— 烘焙键含图标路径，
        // 个性化改动只会新增键；旧纹理留在缓存无害（数量有限）。
      }
    }

    if (in.pressed[(int)Btn::Back]) {
      app.pushScene(createMessageScene(
          "退出 NesStation？", "确定要退出应用吗？未保存的进度将按设置自动存档。",
          false, "退出", [appPtr = &app]() { appPtr->requestExit(); }));
      return;
    }

    Section& cur = sections_[(size_t)secSel_];
    const int N = (int)cur.tiles.size();

    // 键盘导航：↑↓ 切分区，←→ 切磁贴，Enter 激活，E 磁贴选项
    if (in.pressed[(int)Btn::Up] && secSel_ > 0) secSel_--;
    if (in.pressed[(int)Btn::Down] && secSel_ < (int)sections_.size() - 1) secSel_++;
    if (in.pressed[(int)Btn::Left] && cur.sel > 0) cur.sel--;
    if (in.pressed[(int)Btn::Right] && cur.sel < N - 1) cur.sel++;
    if (in.pressed[(int)Btn::Ok]) activate(app, cur.tiles[(size_t)cur.sel]);
    if (in.pressed[(int)Btn::Option])
      app.pushScene(std::unique_ptr<Scene>(
          new FsdTileOptionsScene(cur.tiles[(size_t)cur.sel].key,
                                  cur.tiles[(size_t)cur.sel].label)));
    if (in.pressed[(int)Btn::Menu]) app.pushScene(createSettingsScene());
    // 滚轮：切分区
    if (in.wheel != 0) {
      int next = secSel_ - in.wheel;
      secSel_ = std::max(0, std::min((int)sections_.size() - 1, next));
    }

    // 鼠标拖拽：横向 → 行内翻页；纵向（>48dp）→ 切分区
    const float D = dpOf(app);
    if (in.mouseHeld && flowArea(app).contains(in.mouse)) {
      if (!dragging_) {
        dragging_ = true;
        dragLastX_ = in.mouse.x;
        dragLastY_ = in.mouse.y;
        dragAccX_ = 0;
        dragAccY_ = 0;
      } else {
        dragAccX_ += in.mouse.x - dragLastX_;
        dragAccY_ += in.mouse.y - dragLastY_;
        dragLastX_ = in.mouse.x;
        dragLastY_ = in.mouse.y;
        // 纵向：拖过 48dp 立即切分区（一次性，需回拖再触发）
        if (std::fabs(dragAccY_) > 48 * D && std::fabs(dragAccY_) > std::fabs(dragAccX_)) {
          if (dragAccY_ < 0 && secSel_ < (int)sections_.size() - 1) secSel_++;
          else if (dragAccY_ > 0 && secSel_ > 0) secSel_--;
          dragAccY_ = 0;
          dragAccX_ = 0;
        }
      }
    } else if (dragging_) {
      // 松手：横向按半步翻页（0.45，上限 16 步 —— APK maxSwipeSteps）
      const float step = 244 * D;
      int steps = -(int)std::lround(dragAccX_ / (step * 0.45f));
      steps = std::max(-16, std::min(16, steps));
      Section& s = sections_[(size_t)secSel_];
      s.sel = std::max(0, std::min((int)s.tiles.size() - 1, s.sel + steps));
      dragging_ = false;
    }
  }

  Rect flowArea(App& app) const {
    const float D = dpOf(app);
    const float top = w::fsdTopBarH(app) + 10 * D;
    // ★v1.2.10：底部预留 = 底栏 + 按键提示条（32dp + 上下边距 20dp）——
    //   旧值 44dp 没算提示条，磁贴标题与 A/B 提示条贴边叠压
    const float bottom = (float)app.draw.H - w::fsdBottomBarH(app) - 52 * D;
    return Rect(0, top, (float)app.draw.W, bottom - top);
  }

  // —— 磁贴烘焙：蓝/黄对角渐变 + 高光 + 图标 + 徽标 + 深色标签 + 圆角 ——
  SDL_Texture* tileTexture(App& app, const Tile& t) {
    auto iit = app.config.homeTileIcons.find(t.key);
    const std::string customIcon =
        iit != app.config.homeTileIcons.end() ? iit->second : "";
    const std::string key = t.key + "|" + t.badge + "|" + customIcon;
    auto it = baked_.find(key);
    if (it != baked_.end()) return it->second;
    if (!SDL_RenderTargetSupported(app.sdlRenderer())) return nullptr;

    const float D = dpOf(app);
    const int bw = std::max(8, (int)(220 * D * 1.5f));
    const int bh = std::max(8, (int)(136 * D * 1.5f));
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
    drawTileContent(app, t, customIcon, Rect(0, 0, (float)bw, (float)bh), 1.0f);
    eraseRoundedCorners(r, bw, bh, (int)(10 * D * 1.5f));
    SDL_SetRenderTarget(r, prev);
    baked_[key] = tex;
    return tex;
  }

  // —— 磁贴内容（烘焙与软件渲染回退共用；alpha 用于侧贴渐隐）——
  void drawTileContent(App& app, const Tile& t, const std::string& customIcon,
                       const Rect& rc, float alpha) {
    Draw& d = app.draw;
    const float D = dpOf(app);
    const uint8_t A = (uint8_t)(alpha * 255);
    const float w = rc.w, h = rc.h;
    const float s = w / (220 * D);  // 烘焙比例（相对设计 220dp）
    // 底色
    d.roundRect(rc, 10 * D * s, Color8(0x0D, 0x2C, 0x55, A));
    // 1. 蓝色基底（垂直渐变 2E86E0 → 0D4FA8）
    const Color8 blueTop = Color8::hex(0x2E86E0), blueBot = Color8::hex(0x0D4FA8);
    for (int i = 0; i < 24; i++) {
      const float t2 = (float)i / 24;
      Color8 c = blueTop.lerp(blueBot, t2);
      c.a = A;
      d.rect(Rect(rc.x, rc.y + h * t2, w, h / 24 + 1), c);
    }
    // 2. 黄色对角块（左低右高斜线 0.62h → 0.38h；F7B500 → DE8A00）
    const Color8 yTop = Color8::hex(0xF7B500), yBot = Color8::hex(0xDE8A00);
    const int rows = (int)h;
    for (int y = 0; y < rows; y++) {
      const float fy = (float)y;
      const float splitX = rc.x + w * (0.38f + (1.0f - fy / h) * 0.24f);
      if (splitX < rc.x + w) {
        const float sx = std::max(rc.x, splitX);
        const float tBot = std::min(1.0f, std::max(0.0f, fy / h));
        Color8 c = yTop.lerp(yBot, tBot);
        c.a = A;
        d.rect(Rect(sx, rc.y + fy, rc.x + w - sx, 1), c);
      }
    }
    // 3. 对角分界高光细线（白 35%，2px）
    {
      Color8 c(255, 255, 255, (uint8_t)(0.35f * A));
      d.line(rc.x, rc.y + h * 0.62f, rc.x + w, rc.y + h * 0.38f, c, 2 * s);
    }
    // 4. 玻璃斜向高光（左上白 6% 平行四边形）
    for (int y = 0; y < (int)h; y++) {
      const float t2 = (float)y / h;
      // 左边从 x0 向 x2 收缩，右边从 x1 向 x0 收缩
      const float ga = rc.x + (w * 0.10f) * t2;
      const float ge = rc.x + (w * 0.45f) * (1.0f - t2);
      if (ge > ga)
        d.rect(Rect(ga, rc.y + y, ge - ga, 1),
               Color8(255, 255, 255, (uint8_t)(0.06f * A)));
    }
    // 图标：自定义图片优先（顶部居中，44dp，Fit）；否则白色矢量图标
    const float iconSize = 44 * D * s;
    const Rect iconRc(rc.x + w / 2 - iconSize / 2, rc.y + 18 * D * s, iconSize, iconSize);
    bool drewCustom = false;
    if (!customIcon.empty()) {
      SDL_Texture* tex = app.texture(customIcon);
      if (tex) {
        d.textureAspect(tex, iconRc, Color8(255, 255, 255, A));
        drewCustom = true;
      }
    }
    if (!drewCustom) w::icon(app, t.iconId, iconRc, Color8(255, 255, 255, A));
    // 徽标（右上：黑 35% 圆角底 + 白粗体 11sp）
    if (!t.badge.empty()) {
      const float fs = 11 * D * s;
      const float tw = app.font.textWidth(t.badge, fs, true);
      const Rect brc(rc.right() - tw - 16 * D * s - 10 * D * s, rc.y + 10 * D * s,
                     tw + 16 * D * s, 17 * D * s);
      d.roundRect(brc, 10 * D * s, Color8(0, 0, 0, (uint8_t)(0.35f * A)));
      app.font.drawVCentered(app.sdlRenderer(), t.badge, brc.x + 8 * D * s, brc.cy(), fs,
                             Color8(255, 255, 255, A), true);
    }
    // 标签（左下：深色 0x1A1206 粗体 15sp —— 落在黄色块上，APK 同色）
    {
      const float fs = 15 * D * s;
      const std::string label = app.font.ellipsize(t.label, (int)fs, (int)(w - 24 * D * s));
      app.font.draw(app.sdlRenderer(), label, rc.x + 12 * D * s,
                    rc.bottom() - 8 * D * s - fs, fs, Color8(0x1A, 0x12, 0x06, A), true);
    }
  }

  // ============ 绘制 ============
  void draw(App& app) override {
    Draw& d = app.draw;
    const InputState& in = app.input.input();
    const float D = dpOf(app);
    const float W = (float)d.W, H = (float)d.H;

    // —— 背景（FsdBackdrop；自定义背景图覆盖）——
    if (!app.config.homeBackground.empty()) {
      SDL_Texture* bgTex = app.texture(app.config.homeBackground);
      if (bgTex) {
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

    // —— 分区垂直封面流 ——
    hitTiles_.clear();
    {
      const Rect area = flowArea(app);
      // ★v1.2.10：步距钳制 —— APK 固定 rowH+gapY=218dp 是给高屏（平板/TV）的；
      //   960×540 基准画布上 218dp 步距会把相邻行（缩放 0.5）推到底部
      //   状态栏/按键提示下面（磁贴标题被裁切、提示叠印磁贴）。按可用高度
      //   收缩步距，保证相邻行完整可见。
      const float rawStep = (190 + 28) * D;  // APK rowH + gapY
      const float adjacentHalf = (30 + 10 + 136) * 0.5f * D * 0.5f;  // 相邻行半高
      const float maxStep = area.h * 0.5f - adjacentHalf - 6 * D;
      const float flowStep = std::min(rawStep, std::max(120.0f * D, maxStep));
      const float cy = area.cy();
      // 距离远的先画：dist 2,1 再 0（选中行最上层）
      for (int dist = 2; dist >= 0; dist--) {
        const int sgnLo = -1, sgnHi = 1, sgnSelf = 0;
        const int sgns[3] = {sgnHi, sgnLo, sgnSelf};
        for (int si = 0; si < 3; si++) {
          if (dist == 0 && si != 2) continue;
          if (dist != 0 && si == 2) continue;
          const int i = secSel_ + sgns[si] * dist;
          if (i < 0 || i >= (int)sections_.size()) continue;
          const float pos = (float)i - animSec_;
          const float ap = std::fabs(pos);
          const float scale = std::max(0.5f, 1.0f - 0.34f * ap);
          const float alpha = std::max(0.35f, std::min(1.0f, 1.0f - 0.45f * ap));
          const float rowCy = cy + pos * flowStep;
          drawSectionRow(app, sections_[(size_t)i], i == secSel_, rowCy, scale, alpha);
        }
      }
    }

    // 命中处理
    handleMouse(app, in);

    // —— FsdButtonHints（A 选择 / B 返回）——
    w::fsdButtonHints(app, H - w::fsdBottomBarH(app) - 32 * D,
                       {{"A", "选择", 0x6DBE28}, {"B", "返回", 0xE23B3B}});

    // —— FsdBottomBar ——
    w::fsdBottomBar(app, ip_, "主菜单");

    // 2.5s 刷新系统状态
    sampleTimers(app);
  }

  void drawSectionRow(App& app, Section& s, bool focused, float rowCy, float scale,
                      float alpha) {
    Draw& d = app.draw;
    const float D = dpOf(app);
    const float itemW = 220 * D, itemH = 136 * D;
    const float headerH = 30 * D;
    // 行整体内容：header + 10dp + tiles → 垂直居中于 rowCy
    const float contentH = headerH + 10 * D + itemH;
    const float top = rowCy - contentH * 0.5f * scale;

    // 分区标题条（黄点 + 白粗体大标题 —— FsdSectionHeader，与中心磁贴左缘对齐）
    {
      const float dotR = 6 * D * scale;
      const float hy = top + headerH * 0.5f * scale;
      const float x = (float)d.W * 0.5f - itemW * 0.5f * scale;
      d.circle(x + dotR, hy, dotR,
               Color8(0xF7, 0xB5, 0x00, (uint8_t)(255 * alpha)));
      app.font.draw(app.sdlRenderer(), s.title, x + dotR * 2 + 12 * D * scale,
                    hy - 10 * D * scale, 20 * D * scale,
                    Color8(255, 255, 255, (uint8_t)(255 * alpha)), true);
    }

    // 行内横向封面流（visibleHalfWindow=5）
    const float tilesTop = top + (headerH + 10 * D) * scale;
    const float cyT = tilesTop + itemH * 0.5f * scale;
    const int N = (int)s.tiles.size();
    const float cx0 = (float)d.W * 0.5f;
    const float dCam = 2200 * D;  // 透视相机距离（px）
    for (int dist = 5; dist >= 1; dist--) {
      for (int sgn : {-1, 1}) {
        const int i = (int)std::lround(s.animSel) + sgn * dist;
        if (i < 0 || i >= N) continue;
        drawFlowTile(app, s, i, cx0, cyT, scale, alpha, focused, dCam);
      }
    }
    if (N > 0) {
      drawFlowTile(app, s, s.sel, cx0, cyT, scale, alpha, focused, dCam);
    }
  }

  void drawFlowTile(App& app, Section& s, int i, float cx0, float cyT, float rowScale,
                    float rowAlpha, bool focused, float dCam) {
    Draw& d = app.draw;
    const float D = dpOf(app);
    const float itemW = 220 * D, itemH = 136 * D, gap = 24 * D;
    const float step = itemW + gap;
    const float pos = (float)i - s.animSel;
    const float ap = std::fabs(pos);
    const float cardScale = std::max(0.5f, 1.0f - 0.18f * ap);
    const float scale = cardScale * rowScale;
    const float alpha = std::max(0.25f, std::min(1.0f, 1.0f - 0.20f * ap)) * rowAlpha;
    const Tile& t = s.tiles[(size_t)i];
    const float tAlpha = alpha * tileAlpha(app, t.key);
    const float hw = itemW * 0.5f * scale, hh = itemH * 0.5f * scale;
    const float cxx = cx0 + pos * step * rowScale;

    SDL_Texture* tex = tileTexture(app, t);
    const int secIdx = (int)(&s - sections_.data());
    if (std::fabs(pos) < 0.05f) {
      // 中心磁贴：正对用户
      const Rect rc(cxx - hw, cyT - hh, hw * 2, hh * 2);
      // 选中分区中当前磁贴：黄色 3dp 高亮边框 + 辉光
      if (focused && i == s.sel && tAlpha > 0.25f)
        d.glowRect(rc, 10 * D, Color8(0xF7, 0xB5, 0x00, 110), 12);
      if (tex) {
        d.texture(tex, rc, Color8(255, 255, 255, (uint8_t)(tAlpha * 255)));
      } else {
        drawTileContent(app, t, "", rc, tAlpha);
      }
      if (focused && i == s.sel && tAlpha > 0.25f)
        d.roundRectOutline(rc, 10 * D, Color8::hex(0xF7B500), 3 * D);
      if (tAlpha > 0.25f) hitTiles_.push_back({rc, {secIdx, i}});
    } else {
      // 侧贴：透视梯形（rotationY = pos*10°，cameraDistance 近似）
      const float a = (float)(pos * 10.0f * 3.14159265f / 180.0f);
      const float ca = std::cos(a), sa = std::sin(a);
      const float fr = dCam / (dCam + hw * sa);
      const float fl = dCam / (dCam - hw * sa);
      const float xr = cxx + hw * ca * fr;
      const float xl = cxx - hw * ca * fl;
      const float yl = cyT - hh * fl, ybl = cyT + hh * fl;
      const float yr = cyT - hh * fr, ybr = cyT + hh * fr;
      const float xy[8] = {xl, yl, xr, yr, xr, ybr, xl, ybl};
      static const float uv[8] = {0, 0, 1, 0, 1, 1, 0, 1};
      if (tex) {
        d.textureQuad(tex, xy, uv, Color8(255, 255, 255, (uint8_t)(tAlpha * 255)));
      } else {
        const Rect rc(cxx - hw, cyT - hh, hw * 2, hh * 2);
        drawTileContent(app, t, "", rc, tAlpha);
      }
      if (tAlpha > 0.30f) {
        const float left = std::min(xl, xr), right = std::max(xl, xr);
        const float topY = std::min(yl, yr), botY = std::max(ybl, ybr);
        hitTiles_.push_back({Rect(left, topY, right - left, botY - topY), {secIdx, i}});
      }
    }
  }

  void handleMouse(App& app, const InputState& in) {
    if (in.mousePressed) {
      // 倒序：上层（选中行/中心贴）优先
      for (auto it = hitTiles_.rbegin(); it != hitTiles_.rend(); ++it) {
        if (it->first.contains(in.mouse)) {
          const int sec = it->second.first, tile = it->second.second;
          if (sec != secSel_) {
            secSel_ = sec;  // 先选中该行（APK：点击未选中行任意磁贴）
            sections_[(size_t)sec].sel = tile;
          } else if (tile == sections_[(size_t)sec].sel) {
            activate(app, sections_[(size_t)sec].tiles[(size_t)tile]);
          } else {
            sections_[(size_t)sec].sel = tile;
          }
          return;
        }
      }
    } else if (in.mouseRightPressed) {
      for (auto it = hitTiles_.rbegin(); it != hitTiles_.rend(); ++it) {
        if (it->first.contains(in.mouse)) {
          const int sec = it->second.first, tile = it->second.second;
          secSel_ = sec;
          sections_[(size_t)sec].sel = tile;
          const Tile& t = sections_[(size_t)sec].tiles[(size_t)tile];
          app.pushScene(std::unique_ptr<Scene>(new FsdTileOptionsScene(t.key, t.label)));
          return;
        }
      }
    } else {
      // 悬停跟随选中（仅当前分区）
      for (auto it = hitTiles_.rbegin(); it != hitTiles_.rend(); ++it) {
        if (it->first.contains(in.mouse) && it->second.first == secSel_) {
          sections_[(size_t)secSel_].sel = it->second.second;
          break;
        }
      }
    }
  }

  void sampleTimers(App& app) {
    if (timeMs_ - lastStats_ > 2500 || lastStats_ == 0) {
      lastStats_ = timeMs_;
      stats_ = fsdchrome::sampleStats(app.dataDir(), &cpuPrevTotal_, &cpuPrevIdle_);
    }
  }

 private:
  std::vector<Section> sections_;
  int secSel_ = 0;
  float animSec_ = 0;
  int libSel_ = 0, onlineSel_ = 0, sysSel_ = 0;
  std::map<std::string, SDL_Texture*> baked_;
  int libCount_ = -1;
  std::string persoFp_;
  int64_t timeMs_ = 0;
  int64_t lastStats_ = 0;
  uint64_t cpuPrevTotal_ = 0, cpuPrevIdle_ = 0;
  fsdchrome::SysStats stats_;
  std::string ip_;
  std::vector<std::pair<Rect, std::pair<int, int>>> hitTiles_;
  bool dragging_ = false;
  float dragLastX_ = 0, dragLastY_ = 0, dragAccX_ = 0, dragAccY_ = 0;
};

}  // namespace

std::unique_ptr<Scene> createHomeScene() {
  if (Theme::isNeon()) return std::unique_ptr<Scene>(new NeonHome());
  return std::unique_ptr<Scene>(new FsdHome());
}

}  // namespace ns
