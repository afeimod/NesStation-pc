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

// 主页动作（两套皮肤共用）：0 游戏库 1 收藏 2 最近 3 核心管理 4 设置 5 关于 6 退出
void homeAction(App& app, int action) {
  switch (action) {
    case 0: app.pushScene(createLibraryScene(-1)); break;
    case 1: app.pushScene(createLibraryScene(-2)); break;
    case 2: app.pushScene(createLibraryScene(-3)); break;
    case 3: app.pushScene(createSettingsScene(1)); break;  // 设置 → 核心
    case 4: app.pushScene(createSettingsScene()); break;
    case 5: app.pushScene(createAboutScene()); break;
    case 6:
      app.pushScene(createMessageScene(
          "退出 NesStation？", "确定要退出应用吗？未保存的进度将按设置自动存档。",
          false, "退出", [appPtr = &app]() { appPtr->requestExit(); }));
      break;
  }
}

// ==================== Neon 3D 主页 ====================
class NeonHome : public Scene {
 public:
  ~NeonHome() { baked_.clear(); }

  void onEnter(App& app) override {
    refresh(app);
    animSel_ = (float)sel_;
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
      // 只为可见附近的缺封面游戏排队
      for (int i = -4; i <= 4; i++) {
        const int idx = sel_ + i;
        if (idx >= 0 && idx < (int)games_.size()) app.covers.requestFetch(games_[(size_t)idx]);
      }
    }
  }

  // —— 布局 ——
  struct Layout {
    float flowTop, flowBottom, flowCy, cardW, cardH, infoY, hintY;
  };
  Layout layout(App& app) const {
    Layout L;
    L.flowTop = 118;
    L.infoY = app.draw.H - 142;   // 信息区顶
    L.hintY = app.draw.H - 44;    // 按键栏顶
    L.flowBottom = L.infoY - 8;
    L.flowCy = (L.flowTop + L.flowBottom) * 0.5f;
    const float areaH = L.flowBottom - L.flowTop;
    L.cardH = std::min(areaH * 0.86f, std::max(180.0f, app.draw.H * 0.40f));
    L.cardW = L.cardH / 1.36f;   // Android cardRatio = 1.36
    return L;
  }

  void update(App& app, float dt, const InputState& in) override {
    // 动画位（Android animateFloatAsState tween(280) 的指数近似）
    const float diff = (float)sel_ - animSel_;
    if (std::fabs(diff) < 0.003f)
      animSel_ = (float)sel_;
    else
      animSel_ += diff * std::min(1.0f, dt * 11.0f);

    // 库被外部改动（导入/删除）后刷新
    if ((int)app.library.games().size() != libCount_) {
      libCount_ = (int)app.library.games().size();
      refresh(app);
    }

    if (in.pressed[(int)Btn::Back]) {
      homeAction(app, 6);  // Esc = 退出确认
      return;
    }

    // —— 鼠标拖拽封面流（detectHorizontalDragGestures）——
    // 点击命中在 draw 内直接处理（与 chip/按钮同一立即模式交互）
    const Layout L = layout(app);
    if (in.mouseHeld &&
        in.mouse.y > L.flowTop && in.mouse.y < L.infoY) {
      if (!dragging_) {
        dragging_ = true;
        dragLastX_ = in.mouse.x;
        dragAccum_ = 0;
      } else {
        dragAccum_ += in.mouse.x - dragLastX_;
        dragLastX_ = in.mouse.x;
        const float step = L.cardW * 0.5f;
        while (dragAccum_ > step && sel_ > 0) {
          sel_--;
          dragAccum_ -= step;
        }
        while (dragAccum_ < -step && sel_ < (int)games_.size() - 1) {
          sel_++;
          dragAccum_ += step;
        }
        dragAccum_ = std::max(-step, std::min(step, dragAccum_));
      }
    } else {
      dragging_ = false;
    }

    if (menuFocus_) {
      if ((in.pressed[(int)Btn::Down] || in.pressed[(int)Btn::Up]) && !games_.empty())
        menuFocus_ = false;
      if (in.pressed[(int)Btn::Left]) menuSel_ = (menuSel_ + kMenuCount - 1) % kMenuCount;
      if (in.pressed[(int)Btn::Right]) menuSel_ = (menuSel_ + 1) % kMenuCount;
      if (in.pressed[(int)Btn::Ok]) homeAction(app, menuSel_);
    } else if (!games_.empty()) {
      // Android onKeyNav：不循环，到头即止
      if (in.pressed[(int)Btn::Left] && sel_ > 0) {
        sel_--;
        app.covers.requestFetch(games_[(size_t)sel_]);
      }
      if (in.pressed[(int)Btn::Right] && sel_ < (int)games_.size() - 1) {
        sel_++;
        app.covers.requestFetch(games_[(size_t)sel_]);
      }
      if (in.pressed[(int)Btn::Ok]) app.launchGame(games_[(size_t)sel_]);
      if (in.pressed[(int)Btn::Fav]) {
        app.library.toggleFavorite(games_[(size_t)sel_].id);
        app.library.save();
        refresh(app);
      }
    }
    if (in.pressed[(int)Btn::Menu]) app.pushScene(createSettingsScene());
  }

  // —— 封面卡烘焙（内容 + 圆角一次成纹理，流上仅做透视四边形）——
  SDL_Texture* cardTexture(App& app, const GameEntry& g) {
    const std::string key =
        g.id + "|" + g.coverPath + "|" + (g.favorite ? "F" : "-");
    auto it = baked_.find(key);
    if (it != baked_.end()) return it->second;
    // 无渲染目标支持（软件渲染器 / 旧 SDL）→ 返 nullptr，走平面绘制回退
    if (!SDL_RenderTargetSupported(app.sdlRenderer())) return nullptr;

    const int bw = 288, bh = (int)(288 * 1.36f);  // 1.5× 卡片分辨率
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

    drawCardContent(app, g, Rect(0, 0, (float)bw, (float)bh), 1.0f);

    eraseRoundedCorners(r, bw, bh, 12);
    SDL_SetRenderTarget(r, prev);
    baked_[key] = tex;
    return tex;
  }

  // 卡片内容（烘焙与无渲染目标回退共用；alpha 用于侧卡渐隐）
  void drawCardContent(App& app, const GameEntry& g, const Rect& rc, float alpha) {
    const Skin& sk = Theme::skin();
    Draw& d = app.draw;
    const uint8_t A = (uint8_t)(alpha * 255);
    // 底板（BgPanel）
    d.roundRect(rc, 8, Color8(sk.bgPanel.r, sk.bgPanel.g, sk.bgPanel.b, A));
    // 封面 / 占位
    SDL_Texture* cover = w::coverTexture(app, g);
    const Rect inner = rc.inset(4, 4, 4, 4);
    if (cover) {
      d.textureCrop(cover, inner, Color8(255, 255, 255, A));
      // 底部渐隐（0.55 → 1 渐变到 0xCC05070E）
      const int bands = 12;
      for (int i = 0; i < bands; i++) {
        const float t0 = 0.55f + 0.45f * i / bands;
        const float a = 204.0f * ((t0 - 0.55f) / 0.45f);
        d.rect(Rect(inner.x, inner.y + inner.h * t0, inner.w, inner.h / bands + 1),
               Color8(0x05, 0x07, 0x0E, (uint8_t)(a * alpha)));
      }
    } else {
      w::platformPlaceholder(app, inner, g.platform, g.displayTitle());
    }
    // 标题（12sp 粗体左下）
    const std::string title =
        app.font.ellipsize(g.displayTitle(), 13, rc.w - 20, true);
    app.font.draw(app.sdlRenderer(), title, rc.x + 10, rc.bottom() - 34, 13,
                  Color8(sk.textHi.r, sk.textHi.g, sk.textHi.b, A), true);
    // 收藏星标（右上）
    if (g.favorite)
      app.font.draw(app.sdlRenderer(), "★", rc.right() - 34, rc.y + 8, 15,
                    Color8(sk.gold.r, sk.gold.g, sk.gold.b, A));
  }

  // 透视卡片绘制（Android graphicsLayer rotationY + cameraDistance 的仿射近似）
  void drawCard3D(App& app, SDL_Texture* tex, float cx, float cy, float cardW,
                  float cardH, float animDist, int dir, float alpha) {
    if (!tex) return;
    const float tiltDeg = dir == 0 ? 0.0f : -dir * 30.0f * (0.4f + 0.12f * animDist);
    const float th = tiltDeg * 3.14159265f / 180.0f;
    const float scale = dir == 0 ? 1.0f : std::max(0.55f, 1.0f - 0.12f * animDist);
    const float W = cardW * scale * std::cos(th);
    const float H = cardH * scale;
    const float persp = 0.16f * std::fabs(std::sin(th));  // 近棱高 / 远棱矮
    const float hN = H * (1.0f + persp), hF = H * (1.0f - persp);
    float xy[8];
    if (dir <= 0) {
      // 左卡/中卡：右棱近（更高）
      xy[0] = cx - W / 2; xy[1] = cy - hF / 2;
      xy[2] = cx + W / 2; xy[3] = cy - hN / 2;
      xy[4] = cx + W / 2; xy[5] = cy + hN / 2;
      xy[6] = cx - W / 2; xy[7] = cy + hF / 2;
    } else {
      // 右卡：左棱近
      xy[0] = cx - W / 2; xy[1] = cy - hN / 2;
      xy[2] = cx + W / 2; xy[3] = cy - hF / 2;
      xy[4] = cx + W / 2; xy[5] = cy + hF / 2;
      xy[6] = cx - W / 2; xy[7] = cy + hN / 2;
    }
    const float uv[8] = {0, 0, 1, 0, 1, 1, 0, 1};
    app.draw.textureQuad(tex, xy, uv,
                        Color8(255, 255, 255, (uint8_t)(alpha * 255)));
  }

  void draw(App& app) override {
    const Skin& sk = Theme::skin();
    Draw& d = app.draw;
    const Layout L = layout(app);
    const InputState& in = app.input.input();

    // —— NeonCoverBackdrop：选中封面全屏暗化 / NeonBackdrop 深空渐变 ——
    if (!games_.empty()) {
      SDL_Texture* bg = w::coverTexture(app, games_[(size_t)sel_]);
      if (bg) {
        d.textureCrop(bg, Rect(0, 0, (float)d.W, (float)d.H),
                      Color8(255, 255, 255, 89));  // 0.35
        for (int i = 0; i < 40; i++) {
          const float t = (float)i / 40;
          d.rect(Rect(0, d.H * t, d.W, d.H / 40 + 1),
                 Color8(sk.bg.r, sk.bg.g, sk.bg.b,
                        (uint8_t)(140 + 92 * t)));  // 0.55 → 0.92
        }
      } else {
        neonBackdrop(d);
      }
    } else {
      neonBackdrop(d);
    }

    // —— NeonClockBar：品牌徽标 + 游戏数 + 时钟 ——
    {
      const float by = 18;
      const std::string brand = "NES·STATION";
      const float bw = app.font.textWidth(brand, 15, true) + 20;
      const Rect badge(28, by, bw, 30);
      d.chamferRect(badge, 14, sk.accent);
      app.font.drawVCentered(app.sdlRenderer(), brand, badge.x + 10, badge.cy(), 15,
                             sk.bgDeep, true);
      const std::string count = std::to_string(games_.size()) + " GAMES";
      const float cw = app.font.textWidth(count, 12, true);
      const std::string tm = clockText();
      const float tw = app.font.textWidth(tm, 15, true);
      app.font.draw(app.sdlRenderer(), count, d.W - 28 - tw - 20 - cw, by + 8, 12,
                    sk.textDim, true);
      app.font.draw(app.sdlRenderer(), tm, d.W - 28 - tw, by + 6, 15, sk.accent, true);
    }

    // —— NeonMenuRow：彩色 chips ——
    {
      struct Chip {
        const char* label;
        int iconId;
        Color8 col;
      };
      const Chip chips[kMenuCount] = {
          {"游戏库", 12, sk.red},    {"收藏", 13, sk.gold},   {"最近", 14, sk.blue},
          {"核心管理", 15, sk.purple}, {"设置", 3, sk.text},   {"关于", 17, sk.text},
          {"退出", 18, sk.red},
      };
      float x = 28;
      const float y = 62;
      for (int i = 0; i < kMenuCount; i++) {
        const bool hot = menuFocus_ ? menuSel_ == i : false;
        const float tw = app.font.textWidth(chips[i].label, 13, true);
        const Rect rc(x, y, tw + 20 + 21, 38);
        if (rc.contains(in.mouse)) {
          // 悬停即聚焦（鼠标优先）
          if (!menuFocus_ || menuSel_ != i) {
            menuFocus_ = true;
            menuSel_ = i;
          }
          if (in.mousePressed) homeAction(app, i);
        }
        if (hot || rc.contains(in.mouse)) {
          d.glowRect(rc, rc.minDim() * 0.4f,
                     Color8(sk.accent.r, sk.accent.g, sk.accent.b, 50), 5);
          d.chamferRect(rc, rc.minDim() * 0.4f, sk.bgPanelHi);
          d.chamferRectOutline(rc, rc.minDim() * 0.4f, sk.accent, 1.4f);
        } else {
          d.chamferRect(rc, rc.minDim() * 0.4f, sk.bgPanel);
        }
        w::icon(app, chips[i].iconId, Rect(rc.x + 10, rc.cy() - 8, 16, 16), chips[i].col);
        app.font.drawVCentered(app.sdlRenderer(), chips[i].label, rc.x + 32, rc.cy(), 13,
                               sk.text, true);
        x += rc.w + 10;
      }
    }

    // —— 3D 封面流 ——
    hitCards_.clear();
    if (!games_.empty()) {
      const int N = (int)games_.size();
      auto cardRect = [&](int i, int dir, float ad) {
        const float scale = std::max(0.55f, 1.0f - 0.12f * ad);
        const float cx = (float)d.W / 2 + dir * ad * L.cardW * 0.72f;
        const float cy = L.flowCy + ad * 24.0f;
        return std::pair<Vec2, float>(Vec2(cx, cy), scale);
      };
      // 远 → 近
      for (int dist = 4; dist >= 1; dist--) {
        for (int sgn : {-1, 1}) {
          const int i = sel_ + sgn * dist;
          if (i < 0 || i >= N) continue;
          const float ad = std::fabs((float)i - animSel_);
          const int dir = (i < animSel_) ? -1 : 1;
          const auto [c, scale] = cardRect(i, dir, ad);
          const float alpha = std::min(0.85f, std::max(0.18f, 0.95f - 0.16f * ad));
          SDL_Texture* tex = cardTexture(app, games_[(size_t)i]);
          if (tex) {
            drawCard3D(app, tex, c.x, c.y, L.cardW, L.cardH, ad, dir, alpha);
          } else {
            // 软件渲染器回退：平面卡片（无透视）
            const float w = L.cardW * scale, h = L.cardH * scale;
            drawCardContent(app, games_[(size_t)i],
                            Rect(c.x - w / 2, c.y - h / 2, w, h), alpha);
          }
          // 命中矩形（供鼠标点击选中侧卡）
          if (alpha > 0.3f) {
            const float w = L.cardW * scale, h = L.cardH * scale;
            hitCards_.push_back({Rect(c.x - w / 2, c.y - h / 2, w, h), i});
          }
        }
      }
      // 中心卡（-14px 上浮 + 辉光）
      {
        const GameEntry& g = games_[(size_t)sel_];
        const float cy = L.flowCy - 14.0f;
        const Rect glowRc((float)d.W / 2 - L.cardW / 2 - 6, cy - L.cardH / 2 - 6,
                          L.cardW + 12, L.cardH + 12);
        d.glowRect(glowRc, 14, Color8(sk.accent.r, sk.accent.g, sk.accent.b, 128), 12);
        SDL_Texture* tex = cardTexture(app, g);
        if (tex) {
          drawCard3D(app, tex, (float)d.W / 2, cy, L.cardW, L.cardH, 0, 0, 1.0f);
        } else {
          drawCardContent(app, g,
                          Rect((float)d.W / 2 - L.cardW / 2, cy - L.cardH / 2,
                               L.cardW, L.cardH), 1.0f);
        }
        hitCards_.push_back(
            {Rect((float)d.W / 2 - L.cardW / 2, cy - L.cardH / 2, L.cardW, L.cardH),
             sel_});
      }
      // 本帧命中检测（后画的上层优先）：点侧卡选中，点中卡启动；
      // 右键 = 卡片快捷菜单（Android「长按卡片」的 PC 等价交互）
      if (in.mousePressed) {
        int hit = -1;
        for (auto it = hitCards_.rbegin(); it != hitCards_.rend(); ++it) {
          if (it->first.contains(in.mouse)) {
            hit = it->second;
            break;
          }
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
        for (auto it = hitCards_.rbegin(); it != hitCards_.rend(); ++it) {
          if (it->first.contains(in.mouse)) {
            hit = it->second;
            break;
          }
        }
        if (hit >= 0) {
          sel_ = hit;
          openGameMenu(app, games_[(size_t)hit], Vec2(in.mouse));
        }
      }
    }

    // —— 信息区（BgDeep 75% 底带）——
    if (!games_.empty()) {
      const GameEntry& g = games_[(size_t)sel_];
      const Rect band(0, L.infoY, (float)d.W, L.hintY - L.infoY);
      d.rect(band, Color8(sk.bgDeep.r, sk.bgDeep.g, sk.bgDeep.b, 191));
      d.line(0, L.infoY, (float)d.W, L.infoY,
             Color8(sk.line.r, sk.line.g, sk.line.b, 160), 1);
      // 标题
      const std::string title =
          app.font.ellipsize(g.displayTitle(), 19, (float)d.W * 0.55f, true);
      app.font.draw(app.sdlRenderer(), title, 28, L.infoY + 12, 19, sk.textHi, true);
      // 徽章 + 元信息
      float mx = 28;
      const float my = L.infoY + 46;
      mx += w::platformBadge(app, mx, my + 8, g.platform) + 12;
      app.font.drawVCentered(
          app.sdlRenderer(),
          playTimeText(g.playTimeSec) + " · " + lastPlayedText(g.lastPlayedAt), mx,
          my + 8, 12, sk.textDim);
      if (g.favorite) {
        const std::string fav = "★ 已收藏";
        const float fw = app.font.textWidth(fav, 12, true);
        app.font.drawVCentered(app.sdlRenderer(), fav,
                               (float)d.W - 320 - 16 - fw, my + 8, 12, sk.gold, true);
      }
      // 开始游戏（NeonPrimaryButton）
      const Rect playRc((float)d.W - 300, L.infoY + 16, 264, 48);
      if (w::primaryButton(app, playRc, "开始游戏", 1)) {
        app.launchGame(g);
      }
    } else {
      // 空库引导（Android 空状态）
      const float cy = L.flowCy;
      w::iconGamepad(app, Rect((float)d.W / 2 - 32, cy - 76, 64, 64), sk.textDim);
      app.font.draw(app.sdlRenderer(), "还没有游戏",
                    (float)d.W / 2 - app.font.textWidth("还没有游戏", 20, true) / 2,
                    cy + 4, 20, sk.textHi, true);
      const std::string tip = "去游戏库导入 ROM 开始游玩";
      app.font.draw(app.sdlRenderer(), tip,
                    (float)d.W / 2 - app.font.textWidth(tip, 13) / 2, cy + 36, 13,
                    sk.textDim);
      const Rect goRc((float)d.W / 2 - 100, cy + 72, 200, 44);
      if (w::primaryButton(app, goRc, "进入游戏库", 12)) {
        app.pushScene(createLibraryScene(-1));
      }
    }

    // —— 按键提示栏 ——
    d.rect(Rect(0, L.hintY, (float)d.W, (float)d.H - L.hintY), sk.bgDeep);
    w::keyHintBar(app, {{"←→", "切换", sk.textDim},
                        {"Enter", "开始", sk.green},
                        {"F", "收藏", sk.gold},
                        {"Esc", "返回", sk.textDim}});
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
    items.push_back({20, "重新获取封面", "", false, [&app, g]() {
      if (GameEntry* e = app.library.find(g.id)) e->coverPath.clear();
      app.library.save();
      app.covers.requestFetch(g);
      app.toast("已重新排队获取封面");
    }});
    if (app.cores.installedCoresFor(g.platform).size() > 1) {
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

  void neonBackdrop(Draw& d) {
    const Skin& sk = Theme::skin();
    // Bg → BgDeep → #040914 三段渐变
    for (int i = 0; i < 40; i++) {
      const float t = (float)i / 40;
      Color8 c = t < 0.5f ? sk.bg.lerp(sk.bgDeep, t * 2)
                          : sk.bgDeep.lerp(Color8::hex(0x040914), (t - 0.5f) * 2);
      d.rect(Rect(0, d.H * t, d.W, d.H / 40 + 1), c);
    }
    // 顶部 / 底部霓虹光带
    for (int i = 0; i < 12; i++) {
      const float a = 26.0f * (1.0f - (float)i / 12);
      d.rect(Rect(0, i * 4, d.W, 5), Color8(sk.accent.r, sk.accent.g, sk.accent.b, (uint8_t)a));
    }
    for (int i = 0; i < 12; i++) {
      const float a = 30.0f * (1.0f - (float)i / 12);
      d.rect(Rect(0, d.H - i * 4 - 5, d.W, 5),
             Color8(sk.accentDim.r, sk.accentDim.g, sk.accentDim.b, (uint8_t)a));
    }
  }

 private:
  static constexpr int kMenuCount = 7;
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
    drawPixelBackdrop(app);

    // —— 顶部状态条 ——
    {
      // 半透明深色横带（白云漂过时保证白字可读）
      d.rect(Rect(0, 0, (float)d.W, 58), Color8(0x0D, 0x2C, 0x55, 120));
      const Color8 white = Color8(255, 255, 255, 255);
      float x = 30;
      app.font.draw(app.sdlRenderer(), "NesStation", x, 20, 22, white, true);
      x += app.font.textWidth("NesStation", 22, true) + 8;
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
