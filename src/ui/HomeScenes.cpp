// NesStation PC — 主页：Neon 3D 游戏站 / FSD 经典磁贴（按皮肤切换）
//
// 视觉与交互 1:1 移植自 Android 版：
//   · NeonHomeScreen.kt + NeonFlow.kt + NeonChrome.kt（时钟条 / 彩色 chip 菜单行 /
//     3D 封面流[透视缩放+渐隐+辉光] / 信息区 / 按键提示栏）
//   · FsdHomeScreen.kt + FsdCoverFlow.kt + PixelBackdrop.kt
//     （像素天空 / 蓝黄对角磁贴 / 底部 Dock）
// 鼠标：chip / 磁贴 / 封面卡均矩形命中；封面流支持横向拖拽换位。
#include <algorithm>
#include <cmath>
#include <ctime>
#include <map>

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
      app.toast("在线游戏需要 NesStation 在线服务（Android 版功能），PC 版暂未提供");
      break;
    case 8:
      app.toast("对战平台需要 NesStation 在线服务（Android 版功能），PC 版暂未提供");
      break;
    case 9:
      app.toast("SWF 小游戏为 Android 版专属功能，PC 版暂未提供");
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

  // —— dp：Android 密度无关像素（720p 基准；窄窗按宽约束防溢出）——
  static float dpOf(const App& app) {
    const float dH = (float)app.draw.H / 720.0f;
    const float dW = (float)app.draw.W / 1280.0f;
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
      app.pushScene(createFileBrowser(req));
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

// ==================== FSD 经典磁贴主页 ====================
class FsdHome : public Scene {
 public:
  ~FsdHome() { baked_.clear(); }

  struct Tile {
    const char* label;
    int iconId;
    std::string badge;  // 右上角徽标（空 = 无）
    int action;
  };

  void onEnter(App& app) override {
    rebuild(app);
    animSel_ = (float)sel_;
  }
  void onExit(App& app) override {
    (void)app;
    for (auto& kv : baked_)
      if (kv.second) SDL_DestroyTexture(kv.second);
    baked_.clear();
  }

  void rebuild(App& app) {
    tiles_ = {
        {"游戏库", 12, std::to_string(app.library.games().size()), 0},
        {"收藏", 13, std::to_string(app.library.favorites().size()), 1},
        {"最近", 14, "", 2},
        {"核心管理", 16, "PC", 3},
        {"设置", 3, "", 4},
        {"关于", 17, "", 5},
        {"退出", 18, "", 6},
    };
    app.covers.requestFetchAllMissing(app.library.games());
  }

  void update(App& app, float dt, const InputState& in) override {
    timeMs_ += (int64_t)(dt * 1000);
    const float diff = (float)sel_ - animSel_;
    if (std::fabs(diff) < 0.003f)
      animSel_ = (float)sel_;
    else
      animSel_ += diff * std::min(1.0f, dt * 11.0f);

    if ((int)app.library.games().size() != libCount_) {
      libCount_ = (int)app.library.games().size();
      rebuild(app);
    }

    if (in.pressed[(int)Btn::Back]) {
      homeAction(app, 6);
      return;
    }

    const int N = (int)tiles_.size();
    const float tileW = tileWidth(app);

    // 鼠标拖拽磁贴流
    if (in.mouseHeld && in.mouse.y > 70 && in.mouse.y < app.draw.H - 70) {
      if (!dragging_) {
        dragging_ = true;
        dragLastX_ = in.mouse.x;
        dragAccum_ = 0;
      } else {
        dragAccum_ += in.mouse.x - dragLastX_;
        dragLastX_ = in.mouse.x;
        const float step = tileW * 0.6f * 0.5f;
        while (dragAccum_ > step && sel_ > 0) {
          sel_--;
          dragAccum_ -= step;
        }
        while (dragAccum_ < -step && sel_ < N - 1) {
          sel_++;
          dragAccum_ += step;
        }
        dragAccum_ = std::max(-step, std::min(step, dragAccum_));
      }
    } else {
      dragging_ = false;
    }

    // 上帧磁贴点击改为 draw 内直接处理；这里只做键盘导航
    if (in.pressed[(int)Btn::Left] && sel_ > 0) sel_--;
    if (in.pressed[(int)Btn::Right] && sel_ < N - 1) sel_++;
    if (in.pressed[(int)Btn::Ok]) homeAction(app, tiles_[(size_t)sel_].action);
    if (in.pressed[(int)Btn::Menu]) app.pushScene(createSettingsScene());
  }

  float tileWidth(App& app) const {
    return std::max(170.0f, std::min(240.0f, (float)app.draw.W * 0.165f));
  }

  // —— 磁贴内容（烘焙与无渲染目标回退共用；alpha 用于侧贴渐隐）——
  void drawTileContent(App& app, const Tile& t, const Rect& rc, float alpha) {
    Draw& d = app.draw;
    const uint8_t A = (uint8_t)(alpha * 255);
    const float w = rc.w, h = rc.h;
    // 底色（安全网）
    d.roundRect(rc, 10, Color8(0x0D, 0x2C, 0x55, A));
    // 对角渐变：上蓝（右高左低斜线）、下黄；分割线 (0,0.68h)→(w,0.42h)
    const Color8 navy = Color8::hex(0x1565C0), blue = Color8::hex(0x2196F3);
    const Color8 amber = Color8::hex(0xF9A825), yellow = Color8::hex(0xFDD835);
    const int rows = (int)h;
    for (int y = 0; y < rows; y++) {
      const float fy = (float)y + rc.y;
      const float splitX = rc.x + w * (0.68f - (float)y / h) / 0.26f;
      const float tTop = std::min(1.0f, std::max(0.0f, (float)y / (h * 0.68f)));
      const float tBot =
          std::min(1.0f, std::max(0.0f, ((float)y - h * 0.42f) / (h * 0.58f)));
      Color8 cTop = navy.lerp(blue, tTop), cBot = amber.lerp(yellow, tBot);
      cTop.a = A;
      cBot.a = A;
      if (splitX < rc.x + w) {
        const float sx = std::max(rc.x, splitX);
        d.rect(Rect(sx, fy, rc.x + w - sx, 1), cTop);
      }
      if (splitX > rc.x) {
        const float ex = std::min(rc.x + w, splitX);
        d.rect(Rect(rc.x, fy, ex - rc.x, 1), cBot);
      }
      const float bandX = rc.x + w * (0.62f - 0.30f * (float)y / h);
      if (bandX < rc.x + w) {
        const float bx = std::max(rc.x, bandX);
        d.rect(Rect(bx, fy, rc.x + w - bx, 1), Color8(255, 255, 255, (uint8_t)(13 * alpha)));
      }
    }
    // 白色图标（顶部居中）
    w::icon(app, t.iconId, Rect(rc.x + w / 2 - w * 0.07f, rc.y + h * 0.14f, w * 0.14f,
                                w * 0.14f),
            Color8(255, 255, 255, A));
    // 左下标签
    app.font.draw(app.sdlRenderer(), t.label, rc.x + rc.w * 0.045f,
                  rc.bottom() - h * 0.24f, h * 0.115f,
                  Color8(255, 255, 255, A), true);
    // 右上徽标
    if (!t.badge.empty()) {
      const float tw = app.font.textWidth(t.badge, 11, true);
      const Rect brc(rc.right() - tw - 26, rc.y + 8, tw + 16, 21);
      d.roundRect(brc, 4, Color8(0x1A, 0x4E, 0x8A, (uint8_t)(204 * alpha)));
      app.font.drawVCentered(app.sdlRenderer(), t.badge, brc.x + 8, brc.cy(), 11,
                             Color8(255, 255, 255, A), true);
    }
  }

  // —— 磁贴烘焙：蓝黄对角渐变 + 高光 + 白图标 + 左下标签 + 右上徽标 + 圆角 ——
  SDL_Texture* tileTexture(App& app, const Tile& t) {
    const std::string key = std::string(t.label) + "|" + t.badge;
    auto it = baked_.find(key);
    if (it != baked_.end()) return it->second;
    // 无渲染目标支持 → nullptr，调用方走平面绘制
    if (!SDL_RenderTargetSupported(app.sdlRenderer())) return nullptr;

    const int bw = 330, bh = (int)(330 * 0.68f);  // ~1.5×
    SDL_Renderer* r = app.sdlRenderer();
    SDL_Texture* tex =
        SDL_CreateTexture(r, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_TARGET, bw, bh);
    if (!tex) return nullptr;
    SDL_Texture* prev = SDL_GetRenderTarget(r);
    SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
    SDL_SetRenderTarget(r, tex);
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
    SDL_SetRenderDrawColor(r, 0x0D, 0x2C, 0x55, 255);
    SDL_RenderClear(r);
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);

    drawTileContent(app, t, Rect(0, 0, (float)bw, (float)bh), 1.0f);

    eraseRoundedCorners(r, bw, bh, 12);
    SDL_SetRenderTarget(r, prev);
    baked_[key] = tex;
    return tex;
  }

  void drawPixelBackdrop(App& app) {
    Draw& d = app.draw;
    const float W = (float)d.W, H = (float)d.H;
    // 天空（浅蓝渐变）
    const Color8 skyTop = Color8::hex(0xB7D7F2), skyBot = Color8::hex(0xC8E2F6);
    for (int i = 0; i < 40; i++) {
      const float t = (float)i / 40;
      d.rect(Rect(0, H * t, W, H / 40 + 1), skyTop.lerp(skyBot, t));
    }
    // 像素云（漂移）
    struct Cloud {
      float x, y, scale, drift;
    };
    static const Cloud clouds[] = {
        {0.08f, 0.18f, 1.4f, 0.000020f}, {0.32f, 0.10f, 1.0f, 0.000014f},
        {0.62f, 0.22f, 1.7f, 0.000010f}, {0.86f, 0.08f, 1.2f, 0.000018f},
    };
    static const char* pattern[] = {"  ####  ", " ###### ", "########",
                                    " ###### ", "  ####  "};
    const Color8 cloudC = Color8::hex(0xF5F7FB), cloudS = Color8::hex(0xCAD3DF);
    for (const auto& c : clouds) {
      const float dx = std::fmod((float)((double)timeMs_ * c.drift), W);
      const float cx = c.x * W + dx;
      const float unit = 18.0f * c.scale;
      for (int row = 0; row < 5; row++) {
        for (int col = 0; col < 8; col++) {
          if (pattern[row][col] != '#') continue;
          float px = cx + (col - 4) * unit;
          // 环绕
          if (px > W) px -= W;
          if (px + unit < 0) px += W;
          d.rect(Rect(px, c.y * H + (row - 2) * unit, unit, unit),
                 row >= 3 ? cloudS : cloudC);
        }
      }
    }
    // 星光闪烁
    struct Sparkle {
      float x, y, size, phase;
    };
    static const Sparkle sparks[] = {
        {0.10f, 0.12f, 14, 0.6f}, {0.25f, 0.06f, 18, 0.4f}, {0.45f, 0.14f, 12, 0.8f},
        {0.78f, 0.05f, 16, 0.5f}, {0.92f, 0.16f, 14, 0.7f},
    };
    for (const auto& s : sparks) {
      const float blink =
          (std::sin((double)timeMs_ / 1000.0 * 2.0 + s.phase * 6.2831853) + 1.0f) / 2.0f;
      const float a = (60.0f + blink * 195.0f) / 255.0f;
      const Color8 col = Color8(255, 255, 255, (uint8_t)(128 * a));
      const float cx = s.x * W, cy = s.y * H;
      d.circleOutline(cx, cy, s.size / 2, col, 2);
      d.line(cx - s.size, cy, cx + s.size, cy, col, 2);
      d.line(cx, cy - s.size, cx, cy + s.size, col, 2);
    }
    // 海
    const float seaY = H * 0.74f;
    const Color8 seaTop = Color8::hex(0x4F8AC4), seaBot = Color8::hex(0x2F5C8C);
    for (int i = 0; i < 20; i++) {
      const float t = (float)i / 20;
      d.rect(Rect(0, seaY + (H - seaY) * t, W, (H - seaY) / 20 + 1),
             seaTop.lerp(seaBot, t));
    }
    for (float x = 0; x < W; x += 24) {
      d.rect(Rect(x, seaY + 4, 12, 2), Color8(255, 255, 255, 102));
    }
    // 草地
    const float grassY = H * 0.84f;
    const Color8 gTop = Color8::hex(0x7BB36A), gDeep = Color8::hex(0x4F8C4A);
    for (int i = 0; i < 16; i++) {
      const float t = (float)i / 16;
      d.rect(Rect(0, grassY + (H - grassY) * t, W, (H - grassY) / 16 + 1),
             gTop.lerp(gDeep, t));
    }
    for (float x = 0; x < W; x += 10) {
      d.rect(Rect(x, grassY - 3, 2, 6), Color8::hex(0x6BA45A));
    }
  }

  void draw(App& app) override {
    Draw& d = app.draw;
    const InputState& in = app.input.input();
    const int N = (int)tiles_.size();
    const float tileW = tileWidth(app);
    const float tileH = tileW * 0.68f;
    const float cy = app.draw.H * 0.46f;

    // —— 像素天空背景 ——
    // ★v1.2.6 自定义主页背景（设置 → 外观 → 主页背景；两套风格通用，
    //   对齐 Android AppSettings.homeBackgroundUri）：
    //   选择图片后替代像素天空（aspect-fill 不变形 + 轻压暗保证文字可读）
    if (!app.config.homeBackground.empty()) {
      SDL_Texture* bgTex = app.texture(app.config.homeBackground);
      if (bgTex) {
        d.textureCrop(bgTex, Rect(0, 0, (float)d.W, (float)d.H));
        d.rect(Rect(0, 0, (float)d.W, (float)d.H), Color8(0x0D, 0x1A, 0x30, 110));
      } else {
        drawPixelBackdrop(app);
      }
    } else {
      drawPixelBackdrop(app);
    }

    // —— 顶部状态条 ——
    {
      // 半透明深色横带（白云漂过时保证白字可读）
      d.rect(Rect(0, 0, (float)d.W, 58), Color8(0x0D, 0x2C, 0x55, 120));
      const Color8 white = Color8(255, 255, 255, 255);
      float x = 30;
      app.font.draw(app.sdlRenderer(), "NesStation", x, 20, 22, white, true);
      x += app.font.textWidth("NesStation", 22, true) + 8;
      // ★版本徽章：一眼确认所跑构建
      app.font.draw(app.sdlRenderer(), appVersionText(), x, 30, 12,
                    Color8(255, 255, 255, 235), true);
      x += app.font.textWidth(appVersionText(), 12, true) + 10;
      app.font.draw(app.sdlRenderer(), "GameBox for PC", x, 30, 12,
                    Color8(255, 255, 255, 235), true);
      const std::string right =
          std::to_string(app.library.games().size()) + " GAMES · 18 PLATFORMS";
      app.font.draw(app.sdlRenderer(), right,
                    (float)d.W - 30 - app.font.textWidth(right, 12, true), 30, 12,
                    Color8(255, 255, 255, 245), true);
    }

    // —— 磁贴流 ——
    hitTiles_.clear();
    for (int dist = 4; dist >= 1; dist--) {
      for (int sgn : {-1, 1}) {
        const int i = sel_ + sgn * dist;
        if (i < 0 || i >= N) continue;
        const float ad = std::fabs((float)i - animSel_);
        const int dir = (i < animSel_) ? -1 : 1;
        const float scale = std::max(0.62f, 1.0f - 0.11f * ad);
        const float alpha = std::min(0.85f, std::max(0.15f, 0.92f - 0.18f * ad));
        const float cx = (float)d.W / 2 + dir * ad * tileW * 0.66f;
        const float w = tileW * scale, h = tileH * scale;
        const Rect rc(cx - w / 2, cy - h / 2, w, h);
        SDL_Texture* tex = tileTexture(app, tiles_[(size_t)i]);
        if (tex) {
          d.texture(tex, rc, Color8(255, 255, 255, (uint8_t)(alpha * 255)));
        } else {
          drawTileContent(app, tiles_[(size_t)i], rc, alpha);  // 软件渲染回退
        }
        if (alpha > 0.3f) hitTiles_.push_back({rc, i});
      }
    }
    if (N > 0) {
      // 中心磁贴（选中：放大 + 外发光 + 描边）
      const Rect rc((float)d.W / 2 - tileW / 2, cy - tileH / 2, tileW, tileH);
      d.glowRect(rc, 10, Color8(0x2E, 0x86, 0xE0, 150), 16);
      SDL_Texture* tex = tileTexture(app, tiles_[(size_t)sel_]);
      if (tex) {
        d.texture(tex, rc);
      } else {
        drawTileContent(app, tiles_[(size_t)sel_], rc, 1.0f);
      }
      d.roundRectOutline(rc, 10, Color8::hex(0x2E86E0), 3);
      hitTiles_.push_back({rc, sel_});
    }
    // 命中检测（倒序：上层优先）+ 悬停跟随 + 点击直达
    if (in.mousePressed) {
      int hit = -1;
      for (auto it = hitTiles_.rbegin(); it != hitTiles_.rend(); ++it) {
        if (it->first.contains(in.mouse)) {
          hit = it->second;
          break;
        }
      }
      if (hit >= 0) {
        if (hit == sel_)
          homeAction(app, tiles_[(size_t)sel_].action);
        else
          sel_ = hit;
      }
    } else {
      for (auto it = hitTiles_.rbegin(); it != hitTiles_.rend(); ++it) {
        if (it->first.contains(in.mouse)) {
          sel_ = it->second;
          break;
        }
      }
    }

    // —— 底部 Dock（半透明玻璃条）——
    {
      const float dockY = (float)d.H - 46;
      d.rect(Rect(0, dockY, (float)d.W, 46), Color8(0x0D, 0x2C, 0x55, 136));
      app.font.draw(app.sdlRenderer(), tiles_[(size_t)sel_].label, 30, dockY + 13, 16,
                    Color8(255, 255, 255, 255), true);
      const std::string hint = "←→ 切换 · Enter 确认 · Esc 返回 · 鼠标点击 / 拖拽";
      app.font.draw(app.sdlRenderer(), hint,
                    (float)d.W - 30 - app.font.textWidth(hint, 11), dockY + 17, 11,
                    Color8(255, 255, 255, 204));
    }
  }

 private:
  std::vector<Tile> tiles_;
  std::map<std::string, SDL_Texture*> baked_;
  int sel_ = 0;
  float animSel_ = 0;
  int libCount_ = -1;
  int64_t timeMs_ = 0;
  std::vector<std::pair<Rect, int>> hitTiles_;
  bool dragging_ = false;
  float dragLastX_ = 0;
  float dragAccum_ = 0;
};

}  // namespace

std::unique_ptr<Scene> createHomeScene() {
  if (Theme::isNeon()) return std::unique_ptr<Scene>(new NeonHome());
  return std::unique_ptr<Scene>(new FsdHome());
}

}  // namespace ns
