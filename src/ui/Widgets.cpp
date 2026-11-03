#include "ui/Widgets.h"

#include <cmath>

#include "ui/Draw.h"
#include "ui/Font.h"
#include "ui/Theme.h"

namespace ns {
namespace w {

// —— 私有：切角横向渐变（NeonPrimaryButton 用，同 Compose 水平渐变）——
static void chamferGradientH(Draw& d, const Rect& rc, float c, Color8 a, Color8 b) {
  const int strips = 26;
  const float sw = rc.w / strips;
  for (int i = 0; i < strips; i++) {
    const float x = rc.x + i * sw;
    const float t = (i + 0.5f) / strips;
    // 切角形状内该竖条的合法 y 范围
    float y0 = rc.y, y1 = rc.bottom();
    if (x < rc.x + c) y0 = rc.y + (c - (x - rc.x));
    if (x + sw > rc.right() - c) y1 = rc.bottom() - ((x + sw) - (rc.right() - c));
    if (y1 <= y0) continue;
    d.rect(Rect(x, y0, sw + 1, y1 - y0), a.lerp(b, t));
  }
}

bool hovered(App& app, const Rect& rc) {
  return rc.contains(app.input.input().mouse);
}

bool activated(App& app, const Rect& rc, bool focused) {
  const InputState& in = app.input.input();
  const bool hov = hovered(app, rc);
  if (focused && in.pressed[(int)Btn::Ok]) return true;
  if (hov && in.mousePressed) return true;
  return false;
}

void panel(App& app, const Rect& rc, bool highlighted, float glow) {
  skinPanel(app.draw, rc, highlighted, glow);
}

bool header(App& app, const std::string& title, const std::string& subtitle,
           bool canBack) {
  const Skin& sk = Theme::skin();
  float tx = 40;
  bool backClicked = false;
  if (canBack) {
    // 返回按钮（Android NeonLibraryScreen 左栏「‹ 返回主页」同款交互）
    const Rect rc(24, 30, 112, 46);
    const bool hov = hovered(app, rc);
    if (hov) {
      app.draw.glowRect(rc, sk.panelRadius,
                        Color8(sk.accent.r, sk.accent.g, sk.accent.b, 60), 6);
    }
    skinPanel(app.draw, rc, hov, hov ? 6 : 0);
    iconArrowLeft(app, Rect(rc.x + 14, rc.y + 12, 20, 22), hov ? sk.accent : sk.text);
    app.font.drawVCentered(app.sdlRenderer(), "返回", rc.x + 42, rc.cy(), 16,
                           hov ? sk.accent : sk.text, hov);
    backClicked = activated(app, rc, false);
    tx = 156;
  }
  // 布局：标题 30..66 · 装饰线 72 · 副标题 78..96（互不遮挡）
  app.font.draw(app.sdlRenderer(), title, tx, 30, 30, sk.textHi, true);
  const float w = app.font.textWidth(title, 30, true);
  app.draw.line(tx, 72, tx + w, 72,
                Color8(sk.accent.r, sk.accent.g, sk.accent.b, 170), 2);
  if (!subtitle.empty()) {
    app.font.draw(app.sdlRenderer(), subtitle, tx, 78, 15, sk.textDim);
  }
  return backClicked;
}

bool primaryButton(App& app, const Rect& rc, const std::string& label, int iconId) {
  const Skin& sk = Theme::skin();
  const bool hov = hovered(app, rc);
  if (Theme::isNeon()) {
    if (hov) app.draw.glowRect(rc, rc.minDim() * 0.2f,
                               Color8(sk.accent.r, sk.accent.g, sk.accent.b, 110), 12);
    // 横向渐变 accentDim → accent（同 Compose Brush.horizontalGradient）
    chamferGradientH(app.draw, rc, rc.minDim() * 0.28f, sk.accentDim, sk.accent);
    app.draw.chamferRectOutline(rc, rc.minDim() * 0.28f,
                                Color8(sk.accent.r, sk.accent.g, sk.accent.b, 120), 1.2f);
  } else {
    if (hov) app.draw.glowRect(rc, 10,
                               Color8(sk.accent.r, sk.accent.g, sk.accent.b, 110), 12);
    app.draw.roundRect(rc, 10, sk.accent);
    app.draw.roundRectOutline(rc, 10, sk.gold, 1.4f);
  }
  // 深色文字 + 图标（Android： Neon.BgDeep 色文字压在亮色按钮上）
  const Color8 dark = Theme::isNeon() ? Color8::hex(0x02030A) : Color8::hex(0x081426);
  const float tw = app.font.textWidth(label, 16, true);
  const float iw = iconId > 0 ? 22 : 0;
  const float startX = rc.cx() - (tw + iw + 6) / 2;
  if (iconId > 0) icon(app, iconId, Rect(startX, rc.cy() - 9, 18, 18), dark);
  app.font.drawVCentered(app.sdlRenderer(), label, startX + iw + 6, rc.cy(), 16, dark,
                         true);
  return activated(app, rc, false);
}

bool button(App& app, const Rect& rc, const std::string& label, bool focused,
            float fontSize, bool primary) {
  const Skin& sk = Theme::skin();
  const bool hov = hovered(app, rc);
  const bool hot = focused || hov;

  if (hot && primary) {
    app.draw.glowRect(rc, sk.panelRadius,
                      Color8(sk.accent.r, sk.accent.g, sk.accent.b, 120), 10);
  }
  skinPanel(app.draw, rc, hot, hot ? 8 : 0);

  Color8 textCol = hot ? sk.textHi : sk.text;
  if (primary && hot) textCol = sk.accent;
  const float tw = app.font.textWidth(label, fontSize, primary || hot);
  app.font.drawVCentered(app.sdlRenderer(), label, rc.cx() - tw / 2, rc.cy(), fontSize,
                         textCol, primary || hot);
  return activated(app, rc, focused);
}

bool listRow(App& app, const Rect& rc, const std::string& title, const std::string& value,
             bool focused, const std::string& subtitle) {
  const Skin& sk = Theme::skin();
  const bool hot = focused || hovered(app, rc);
  if (hot) {
    app.draw.glowRect(rc, sk.panelRadius,
                      Color8(sk.accent.r, sk.accent.g, sk.accent.b, 70), 6);
  }
  skinPanel(app.draw, rc, hot, 0);
  const float pad = 16;
  if (!subtitle.empty()) {
    app.font.draw(app.sdlRenderer(), title, rc.x + pad, rc.y + rc.h * 0.18f, 17,
                  hot ? sk.textHi : sk.text, hot);
    app.font.draw(app.sdlRenderer(), subtitle, rc.x + pad, rc.y + rc.h * 0.55f, 13,
                  sk.textDim);
  } else {
    app.font.drawVCentered(app.sdlRenderer(), title, rc.x + pad, rc.cy(), 17,
                           hot ? sk.textHi : sk.text, hot);
  }
  if (!value.empty()) {
    const float vw = app.font.textWidth(value, 15);
    app.font.drawVCentered(app.sdlRenderer(), value, rc.right() - pad - vw, rc.cy(), 15,
                           hot ? sk.accent : sk.textDim);
  }
  return activated(app, rc, focused);
}

bool toggleRow(App& app, const Rect& rc, const std::string& title, bool& value,
               bool focused, const std::string& subtitle) {
  const Skin& sk = Theme::skin();
  // 行体（不画右侧值文字，避免与开关重叠）
  const bool act = listRow(app, rc, title, "", focused, subtitle);
  if (act) value = !value;
  // 开关（Material 风格：胶囊轨道 + 实心圆钮；关态也保持可读对比度）
  const float tw = 44, th = 22;
  const Rect track(rc.right() - 76, rc.cy() - th / 2, tw, th);
  const Color8 off = Color8(sk.text.r, sk.text.g, sk.text.b, 200);
  const Color8 on = value ? sk.accent : off;
  app.draw.roundRect(track, th / 2, Color8(on.r, on.g, on.b, value ? 235 : 70));
  app.draw.roundRectOutline(track, th / 2,
                           Color8(on.r, on.g, on.b, value ? 255 : 190), 1.6f);
  const float knobR = th / 2 - 3;
  const float knobX = value ? track.right() - knobR - 3 : track.x + knobR + 3;
  app.draw.circle(knobX, track.cy(), knobR, value ? Color8(255, 255, 255, 255) : off);
  return act;
}

bool cycleRow(App& app, const Rect& rc, const std::string& title,
              const std::vector<std::string>& options, int& index, bool focused,
              const std::string& subtitle) {
  if (options.empty()) return false;
  if (index < 0 || index >= (int)options.size()) index = 0;
  const Skin& sk = Theme::skin();
  bool changed = false;
  const bool hot = focused || hovered(app, rc);
  if (hot) {
    const InputState& in = app.input.input();
    // 箭头点击区与绘制位置一致
    const Rect left(rc.right() - 90, rc.y, 30, rc.h);
    const Rect right(rc.right() - 32, rc.y, 30, rc.h);
    if (in.pressed[(int)Btn::Left] || (hovered(app, left) && in.mousePressed)) {
      index = (index - 1 + (int)options.size()) % (int)options.size();
      changed = true;
    }
    if (in.pressed[(int)Btn::Right] || (hovered(app, right) && in.mousePressed)) {
      index = (index + 1) % (int)options.size();
      changed = true;
    }
  }
  const bool act = listRow(app, rc, title, "", focused, subtitle);
  // 绘制当前值 + 左右箭头（值右对齐，与箭头留白）
  const std::string& val = options[(size_t)index];
  const float pad = 16;
  const float vw = app.font.textWidth(val, 15);
  const float arrowR = rc.right() - pad - 8;
  const float arrowL = arrowR - 46;
  const float vx = arrowL - 14 - vw;  // 值右缘距左箭头 14px
  app.font.drawVCentered(app.sdlRenderer(), val, vx, rc.cy(), 15,
                         hot ? sk.accent : sk.text);
  // 箭头按钮（悬停高亮）
  const Rect leftRc(arrowL - 10, rc.y, 26, rc.h);
  const Rect rightRc(arrowR - 8, rc.y, 26, rc.h);
  app.font.drawVCentered(app.sdlRenderer(), "‹", arrowL - 4, rc.cy(), 20,
                         hot ? sk.accent : sk.textDim);
  app.font.drawVCentered(app.sdlRenderer(), "›", arrowR - 4, rc.cy(), 20,
                         hot ? sk.accent : sk.textDim);
  (void)leftRc;
  (void)rightRc;
  if (act) {
    index = (index + 1) % (int)options.size();
    changed = true;
  }
  return changed;
}

bool sliderRow(App& app, const Rect& rc, const std::string& title, float& value01,
               bool focused, const std::string& subtitle) {
  const Skin& sk = Theme::skin();
  (void)listRow(app, rc, title, "", focused, subtitle);
  const InputState& in = app.input.input();
  const bool hot = focused || hovered(app, rc);
  bool changed = false;
  if (hot) {
    if (in.pressed[(int)Btn::Left]) {
      value01 = std::max(0.0f, value01 - 0.05f);
      changed = true;
    }
    if (in.pressed[(int)Btn::Right]) {
      value01 = std::min(1.0f, value01 + 0.05f);
      changed = true;
    }
    if (hovered(app, rc) && (in.mouseHeld)) {
      const float rel = (in.mouse.x - (rc.right() - 220)) / 190.0f;
      if (rel >= -0.05f && rel <= 1.1f) {
        value01 = std::min(1.0f, std::max(0.0f, rel));
        changed = true;
      }
    }
  }
  // 轨道
  const Rect track(rc.right() - 210, rc.cy() - 3, 180, 6);
  app.draw.roundRect(track, 3, sk.line);
  const Rect fill(track.x, track.y, track.w * value01, track.h);
  app.draw.roundRect(fill, 3, hot ? sk.accent : sk.accentDim);
  app.draw.circle(track.x + track.w * value01, track.cy(), 8,
                  hot ? sk.accent : sk.accentDim);
  return changed;
}

void sectionLabel(App& app, float x, float y, const std::string& text) {
  const Skin& sk = Theme::skin();
  app.font.draw(app.sdlRenderer(), text, x, y, 14, sk.textDim, true);
}

void keyHintBar(App& app, const std::vector<KeyHint>& hints) {
  const Skin& sk = Theme::skin();
  const float y = app.draw.H - 34;
  float x = 24;
  for (const auto& h : hints) {
    const Color8 col = h.color.a == 0 ? sk.textDim : h.color;
    // 键帽 chip（Neon：切角 0.5；FSD：圆角）
    const float kw = app.font.textWidth(h.key, 12, true) + 16;
    const Rect chip(x, y, kw, 22);
    if (Theme::isNeon()) {
      app.draw.chamferRect(chip, 10, sk.bgPanelHi);
    } else {
      app.draw.roundRect(chip, 5, sk.bgPanelHi);
    }
    app.font.drawVCentered(app.sdlRenderer(), h.key, chip.x + 8, chip.cy(), 12, col,
                           true);
    x = chip.right() + 8;
    app.font.drawVCentered(app.sdlRenderer(), h.label, x, chip.cy(), 13, sk.textDim);
    x += app.font.textWidth(h.label, 13) + 22;
  }
}

void scrollIndicator(App& app, const Rect& track, float visible01, float offset01) {
  const Skin& sk = Theme::skin();
  const float h = std::max(24.0f, track.h * visible01);
  const float maxOffset = std::max(0.0f, track.h - h);
  const float y = track.y + maxOffset * offset01;
  app.draw.roundRect(Rect(track.x, y, track.w, h), track.w / 2, sk.accentDim);
}

// —— 图标 ——
void icon(App& app, int id, const Rect& rc, Color8 c) {
  switch (id) {
    case 1: iconPlay(app, rc, c); break;
    case 2: iconStar(app, rc, c, true); break;
    case 3: iconGear(app, rc, c); break;
    case 4: iconFolder(app, rc, c); break;
    case 5: iconPower(app, rc, c); break;
    case 6: iconSave(app, rc, c); break;
    case 7: iconLoad(app, rc, c); break;
    case 8: iconReset(app, rc, c); break;
    case 9: iconGamepad(app, rc, c); break;
    case 10: iconInfo(app, rc, c); break;
    case 11: iconSearch(app, rc, c); break;
    case 12: iconGrid(app, rc, c); break;
    case 13: iconHeart(app, rc, c); break;
    case 14: iconHistory(app, rc, c); break;
    case 15: iconTerminal(app, rc, c); break;
    case 16: iconChip(app, rc, c); break;
    case 17: iconHelp(app, rc, c); break;
    case 18: iconLogout(app, rc, c); break;
    case 19: iconArrowLeft(app, rc, c); break;
    case 20: iconRefresh(app, rc, c); break;
    default: break;
  }
}

void iconPlay(App& app, const Rect& rc, Color8 c) {
  Draw& d = app.draw;
  const float cx = rc.cx(), cy = rc.cy();
  const float s = rc.minDim() * 0.5f;
  d.line(cx - s * 0.5f, cy - s * 0.8f, cx + s * 0.8f, cy, c, 3);
  d.line(cx + s * 0.8f, cy, cx - s * 0.5f, cy + s * 0.8f, c, 3);
  d.line(cx - s * 0.5f, cy + s * 0.8f, cx - s * 0.5f, cy - s * 0.8f, c, 3);
}

void iconStar(App& app, const Rect& rc, Color8 c, bool filled) {
  Draw& d = app.draw;
  const float cx = rc.cx(), cy = rc.cy();
  const float R = rc.minDim() * 0.42f, r = R * 0.45f;
  float px[10], py[10];
  for (int i = 0; i < 10; i++) {
    const float a = -1.5707963f + (float)i * 0.6283185f;
    const float rad = (i % 2 == 0) ? R : r;
    px[i] = cx + std::cos(a) * rad;
    py[i] = cy + std::sin(a) * rad;
  }
  for (int i = 0; i < 10; i++) {
    const int j = (i + 1) % 10;
    d.line(px[i], py[i], px[j], py[j], c, 2);
  }
  if (filled) {
    for (int i = 0; i < 10; i++) {
      const int j = (i + 1) % 10;
      d.line(cx, cy, px[j], py[j], Color8(c.r, c.g, c.b, 120), 1.4f);
    }
    d.circle(cx, cy, R * 0.28f, c);
  }
}

void iconGear(App& app, const Rect& rc, Color8 c) {
  Draw& d = app.draw;
  const float cx = rc.cx(), cy = rc.cy();
  const float R = rc.minDim() * 0.34f;
  // 粗环 + 粗齿（小尺寸下保持可读）
  const int segs = 22;
  for (int i = 0; i < segs; i++) {
    const float a0 = (float)i / segs * 6.2831853f;
    const float a1 = (float)(i + 1) / segs * 6.2831853f + 0.03f;
    d.line(cx + std::cos(a0) * (R - 2.2f), cy + std::sin(a0) * (R - 2.2f),
           cx + std::cos(a1) * (R - 2.2f), cy + std::sin(a1) * (R - 2.2f), c, 3.0f);
  }
  for (int i = 0; i < 8; i++) {
    const float a = (float)i * 0.7853982f;
    d.line(cx + std::cos(a) * R, cy + std::sin(a) * R, cx + std::cos(a) * (R + 4.5f),
           cy + std::sin(a) * (R + 4.5f), c, 3.2f);
  }
  d.circle(cx, cy, R * 0.32f, c);
}

void iconBackArrow(App& app, const Rect& rc, Color8 c) {
  Draw& d = app.draw;
  const float cx = rc.cx(), cy = rc.cy();
  const float s = rc.minDim() * 0.4f;
  d.line(cx + s * 0.4f, cy - s, cx - s * 0.6f, cy, c, 2.5f);
  d.line(cx - s * 0.6f, cy, cx + s * 0.4f, cy + s, c, 2.5f);
}

void iconFolder(App& app, const Rect& rc, Color8 c) {
  Draw& d = app.draw;
  const float x = rc.x + rc.w * 0.18f, y = rc.y + rc.h * 0.28f;
  const float w = rc.w * 0.64f, h = rc.h * 0.5f;
  d.rectOutline(Rect(x, y, w * 0.4f, h * 0.3f), c, 2);
  d.rectOutline(Rect(x, y + h * 0.28f, w, h * 0.72f), c, 2);
}

void iconPower(App& app, const Rect& rc, Color8 c) {
  Draw& d = app.draw;
  const float cx = rc.cx(), cy = rc.cy();
  const float R = rc.minDim() * 0.34f;
  for (int i = 0; i < 12; i++) {
    if (i >= 4 && i <= 7) continue;  // 顶部缺口
    const float a0 = (float)i * 0.5235988f - 1.5707963f;
    const float a1 = a0 + 0.5235988f;
    d.line(cx + std::cos(a0) * R, cy + std::sin(a0) * R, cx + std::cos(a1) * R,
           cy + std::sin(a1) * R, c, 2.4f);
  }
  d.line(cx, cy - R - 4, cx, cy - R + 5, c, 2.6f);
}

void iconSave(App& app, const Rect& rc, Color8 c) {
  Draw& d = app.draw;
  d.rectOutline(Rect(rc.x + rc.w * 0.2f, rc.y + rc.h * 0.2f, rc.w * 0.6f, rc.h * 0.62f), c, 2);
  d.rect(Rect(rc.x + rc.w * 0.36f, rc.y + rc.h * 0.2f, rc.w * 0.28f, rc.h * 0.2f), c);
  d.rectOutline(Rect(rc.x + rc.w * 0.34f, rc.y + rc.h * 0.52f, rc.w * 0.32f, rc.h * 0.22f), c, 1.5f);
}

void iconLoad(App& app, const Rect& rc, Color8 c) {
  Draw& d = app.draw;
  d.rectOutline(Rect(rc.x + rc.w * 0.2f, rc.y + rc.h * 0.2f, rc.w * 0.6f, rc.h * 0.62f), c, 2);
  d.rect(Rect(rc.x + rc.w * 0.36f, rc.y + rc.h * 0.2f, rc.w * 0.28f, rc.h * 0.2f), c);
  d.line(rc.cx(), rc.y + rc.h * 0.5f, rc.cx(), rc.y + rc.h * 0.85f, c, 2);
  d.line(rc.cx(), rc.y + rc.h * 0.85f, rc.cx() - 5, rc.y + rc.h * 0.76f, c, 2);
  d.line(rc.cx(), rc.y + rc.h * 0.85f, rc.cx() + 5, rc.y + rc.h * 0.76f, c, 2);
}

void iconReset(App& app, const Rect& rc, Color8 c) {
  Draw& d = app.draw;
  const float cx = rc.cx(), cy = rc.cy();
  const float R = rc.minDim() * 0.34f;
  for (int i = 0; i < 20; i++) {
    const float a0 = (float)i * 0.3141592f;
    const float a1 = a0 + 0.17f;
    d.line(cx + std::cos(a0) * R, cy + std::sin(a0) * R, cx + std::cos(a1) * R,
           cy + std::sin(a1) * R, c, 2.2f);
  }
}

void iconGamepad(App& app, const Rect& rc, Color8 c) {
  Draw& d = app.draw;
  const float cx = rc.cx(), cy = rc.cy();
  const float w = rc.w * 0.7f, h = rc.h * 0.42f;
  d.roundRectOutline(Rect(cx - w / 2, cy - h / 2, w, h), h * 0.45f, c, 2);
  // 十字
  d.rect(Rect(cx - w * 0.28f, cy - 1.4f, w * 0.14f, 2.8f), c);
  d.rect(Rect(cx - w * 0.245f - 1.4f, cy - h * 0.16f, 2.8f, h * 0.32f), c);
  // AB
  d.circle(cx + w * 0.24f, cy - h * 0.14f, 3, c);
  d.circle(cx + w * 0.36f, cy + h * 0.02f, 3, c);
}

void iconInfo(App& app, const Rect& rc, Color8 c) {
  Draw& d = app.draw;
  const float cx = rc.cx(), cy = rc.cy();
  d.circleOutline(cx, cy, rc.minDim() * 0.4f, c, 2);
  d.circle(cx, cy - rc.minDim() * 0.18f, 2, c);
  d.line(cx, cy - rc.minDim() * 0.04f, cx, cy + rc.minDim() * 0.22f, c, 2.4f);
}

void iconSearch(App& app, const Rect& rc, Color8 c) {
  Draw& d = app.draw;
  const float cx = rc.cx() - 2, cy = rc.cy() - 2;
  const float R = rc.minDim() * 0.3f;
  d.circleOutline(cx, cy, R, c, 2.2f);
  d.line(cx + R * 0.72f, cy + R * 0.72f, cx + R * 1.5f, cy + R * 1.5f, c, 2.4f);
}

// —— Android 菜单 / 磁贴图标（Material Rounded 风格矢量重绘）——

void iconGrid(App& app, const Rect& rc, Color8 c) {  // 游戏库
  Draw& d = app.draw;
  const float pad = rc.minDim() * 0.12f;
  const float cw = (rc.w - pad * 3) / 2, ch = (rc.h - pad * 3) / 2;
  const float r = std::min(cw, ch) * 0.24f;
  for (int gy = 0; gy < 2; gy++) {
    for (int gx = 0; gx < 2; gx++) {
      const float x = rc.x + pad + gx * (cw + pad);
      const float y = rc.y + pad + gy * (ch + pad);
      d.roundRect(Rect(x, y, cw, ch), r, c);
    }
  }
}

void iconHeart(App& app, const Rect& rc, Color8 c) {  // 收藏
  Draw& d = app.draw;
  const float cx = rc.cx(), cy = rc.y + rc.h * 0.42f;
  const float s = rc.minDim() * 0.30f;
  // 两个上圆 + 下三角（扫描线填充）
  d.circle(cx - s * 0.52f, cy - s * 0.32f, s * 0.55f, c);
  d.circle(cx + s * 0.52f, cy - s * 0.32f, s * 0.55f, c);
  const float top = cy - s * 0.35f, bot = cy + s * 1.15f;
  const int steps = 14;
  for (int i = 0; i <= steps; i++) {
    const float t = (float)i / steps;
    const float y = top + (bot - top) * t;
    const float half = s * (0.95f - t * 0.62f);
    d.rect(Rect(cx - half, y, half * 2, (bot - top) / steps + 1), c);
  }
}

void iconHistory(App& app, const Rect& rc, Color8 c) {  // 最近
  Draw& d = app.draw;
  const float cx = rc.cx(), cy = rc.cy();
  const float R = rc.minDim() * 0.34f;
  // 粗表盘（顶部留缺口）
  const int segs = 22;
  for (int i = 0; i < segs; i++) {
    const float a0 = -1.5707963f + 0.55f + (float)i * (6.2831853f - 1.1f) / segs;
    const float a1 = a0 + (6.2831853f - 1.1f) / segs + 0.04f;
    d.line(cx + std::cos(a0) * R, cy + std::sin(a0) * R, cx + std::cos(a1) * R,
           cy + std::sin(a1) * R, c, 3.0f);
  }
  // 指针
  d.line(cx, cy, cx, cy - R * 0.55f, c, 3.0f);
  d.line(cx, cy, cx - R * 0.42f, cy + R * 0.2f, c, 3.0f);
  // 顶部逆时针箭头
  d.line(cx - R * 0.28f, cy - R * 0.85f, cx - R * 0.02f, cy - R * 1.08f, c, 3.0f);
  d.line(cx - R * 0.28f, cy - R * 0.85f, cx - R * 0.3f, cy - R * 0.55f, c, 3.0f);
}

void iconTerminal(App& app, const Rect& rc, Color8 c) {  // 核心管理（Neon chip）
  Draw& d = app.draw;
  const Rect box(rc.x + rc.w * 0.1f, rc.y + rc.h * 0.18f, rc.w * 0.8f, rc.h * 0.64f);
  d.roundRectOutline(box, 3, c, 1.8f);
  const float cx = box.x + box.w * 0.3f, cy = box.cy();
  d.line(cx, cy - box.h * 0.2f, cx - box.w * 0.14f, cy, c, 1.8f);
  d.line(cx - box.w * 0.14f, cy, cx, cy + box.h * 0.2f, c, 1.8f);
  d.line(box.x + box.w * 0.42f, cy + box.h * 0.2f, box.x + box.w * 0.72f,
         cy + box.h * 0.2f, c, 1.8f);
}

void iconChip(App& app, const Rect& rc, Color8 c) {  // 核心管理（FSD Memory）
  Draw& d = app.draw;
  const Rect body(rc.x + rc.w * 0.22f, rc.y + rc.h * 0.22f, rc.w * 0.56f, rc.h * 0.56f);
  d.roundRectOutline(body, 3, c, 1.8f);
  d.rectOutline(Rect(body.x + body.w * 0.28f, body.y + body.h * 0.28f, body.w * 0.44f,
                     body.h * 0.44f),
                c, 1.6f);
  for (int i = 0; i < 3; i++) {
    const float y = body.y + body.h * (0.18f + 0.32f * i);
    d.line(rc.x + rc.w * 0.08f, y, body.x, y, c, 1.6f);
    d.line(body.right(), y, rc.right() - rc.w * 0.08f, y, c, 1.6f);
  }
}

void iconHelp(App& app, const Rect& rc, Color8 c) {  // 关于
  Draw& d = app.draw;
  d.circleOutline(rc.cx(), rc.cy(), rc.minDim() * 0.42f, c, 2.2f);
  app.font.drawVCentered(app.sdlRenderer(), "?", rc.cx() - 5, rc.cy() + 1,
                         (float)rc.minDim() * 0.52f, c, true);
}

void iconLogout(App& app, const Rect& rc, Color8 c) {  // 退出
  Draw& d = app.draw;
  const Rect door(rc.x + rc.w * 0.12f, rc.y + rc.h * 0.14f, rc.w * 0.5f, rc.h * 0.72f);
  d.rectOutline(door, c, 2.4f);
  const float y = rc.cy();
  const float xa = door.right() + rc.w * 0.06f;
  const float xb = rc.right() - rc.w * 0.04f;
  d.line(xa, y, xb, y, c, 2.6f);
  d.line(xb, y, xb - rc.w * 0.18f, y - rc.h * 0.18f, c, 2.6f);
  d.line(xb, y, xb - rc.w * 0.18f, y + rc.h * 0.18f, c, 2.6f);
}

void iconArrowLeft(App& app, const Rect& rc, Color8 c) {  // 返回
  Draw& d = app.draw;
  const float cx = rc.cx(), cy = rc.cy();
  const float s = rc.minDim() * 0.4f;
  d.line(cx + s * 0.5f, cy - s, cx - s * 0.5f, cy, c, 2.5f);
  d.line(cx - s * 0.5f, cy, cx + s * 0.5f, cy + s, c, 2.5f);
  d.line(cx - s * 0.5f, cy, cx + s * 0.9f, cy, c, 2.5f);
}

void iconRefresh(App& app, const Rect& rc, Color8 c) {  // 刷新
  Draw& d = app.draw;
  const float cx = rc.cx(), cy = rc.cy();
  const float R = rc.minDim() * 0.34f;
  for (int i = 0; i < 24; i++) {
    const float a0 = -1.5707963f + 0.7f + (float)i * (6.2831853f - 1.4f) / 24.0f;
    const float a1 = a0 + (6.2831853f - 1.4f) / 24.0f + 0.02f;
    d.line(cx + std::cos(a0) * R, cy + std::sin(a0) * R, cx + std::cos(a1) * R,
           cy + std::sin(a1) * R, c, 2.2f);
  }
  d.line(cx + R * 0.5f, cy - R * 0.72f, cx + R * 0.9f, cy - R * 1.0f, c, 2.2f);
  d.line(cx + R * 0.5f, cy - R * 0.72f, cx + R * 0.12f, cy - R * 1.0f, c, 2.2f);
}

// —— 平台徽章（Android PlatformBadge）——
float platformBadge(App& app, float x, float cy, GamePlatform p) {
  const Color8 pc = Color8::hex(platformAccent(p));
  const std::string name = platformShortName(p);
  const float fs = 12;
  const float w = app.font.textWidth(name, fs, true) + 18;
  const Rect rc(x, cy - 12, w, 24);
  app.draw.roundRect(rc, 4, Color8(pc.r, pc.g, pc.b, 56));
  app.font.drawVCentered(app.sdlRenderer(), name, rc.x + 9, cy, fs, pc, true);
  return w;
}


// —— 封面 ——
void coverCard(App& app, const Rect& rc, const GameEntry& g, bool selected) {
  const Skin& sk = Theme::skin();
  // 选中：外围辉光（Android NeonCoverCard glow 层）
  if (selected) {
    app.draw.glowRect(rc.shrink(-6), 12,
                      Color8(sk.accent.r, sk.accent.g, sk.accent.b, 120), 14);
  }
  // 卡体（Neon 圆角 8 / FSD 圆角 10 + 描边）
  const float r = Theme::isNeon() ? 8 : 10;
  app.draw.roundRect(rc, r, sk.bgPanel);
  app.draw.roundRectOutline(rc, r,
                            selected ? sk.accent
                                     : (Theme::isNeon() ? sk.line : Color8::hex(0x3E64AF)),
                            selected ? 2.4f : 1.4f);
  const Rect inner = rc.inset(5, 5, 5, 5);
  SDL_Texture* tex = coverTexture(app, g);
  if (tex) {
    app.draw.textureCrop(tex, inner);
    // 底部渐隐标题层（0.55 → 1.0 渐变到 0xCC05070E）
    const int bands = 10;
    for (int i = 0; i < bands; i++) {
      const float t0 = 0.55f + 0.45f * i / bands;
      const float a = 204.0f * ((t0 - 0.55f) / 0.45f);
      app.draw.rect(Rect(inner.x, inner.y + inner.h * t0, inner.w, inner.h / bands + 1),
                    Color8(0x05, 0x07, 0x0E, (uint8_t)a));
    }
    const std::string title = app.font.ellipsize(g.displayTitle(), 12, inner.w - 10);
    app.font.draw(app.sdlRenderer(), title, inner.x + 5, inner.bottom() - 20, 12, sk.textHi,
                  true);
  } else {
    w::platformPlaceholder(app, inner, g.platform, g.displayTitle());
  }
  if (g.favorite) {
    app.font.draw(app.sdlRenderer(), "★", rc.right() - 26, rc.y + 6, 14, sk.gold);
  }
}

SDL_Texture* coverTexture(App& app, const GameEntry& g) {
  if (!g.coverPath.empty()) {
    SDL_Texture* t = app.texture(g.coverPath);
    if (t) return t;
  }
  return nullptr;
}

void platformPlaceholder(App& app, const Rect& rc, GamePlatform p, const std::string& title) {
  Draw& d = app.draw;
  const uint32_t acc = platformAccent(p);
  const Color8 c1 = Color8::hex(acc);
  const Color8 c2 = c1.scale(0.35f);
  // 对角渐变近似（多条横带插值）
  const int bands = 8;
  for (int i = 0; i < bands; i++) {
    const float t = (float)i / bands;
    const Color8 c = Color8::hex(acc).lerp(Color8::hex(0x0B1020), t * 0.85f);
    d.rect(Rect(rc.x, rc.y + rc.h * t, rc.w, rc.h / bands + 1), c);
  }
  // 像素网格点缀
  for (int yy = 0; yy < rc.h; yy += 14) {
    for (int xx = 0; xx < rc.w; xx += 14) {
      if (((xx / 14) + (yy / 14)) % 5 == 0)
        d.rect(Rect(rc.x + xx, rc.y + yy, 3, 3), Color8(c1.r, c1.g, c1.b, 40));
    }
  }
  // 底部标题条
  d.rect(Rect(rc.x, rc.bottom() - 34, rc.w, 34), Color8::hex(0x05070E, 210));
  d.line(rc.x, rc.bottom() - 34, rc.right(), rc.bottom() - 34, c1, 2);
  app.font.draw(app.sdlRenderer(), platformShortName(p), rc.x + 8, rc.bottom() - 30, 12, c1,
                true);
  const std::string t = app.font.ellipsize(title, 13, rc.w - 16);
  app.font.draw(app.sdlRenderer(), t, rc.x + 8, rc.bottom() - 15, 13, Color8::hex(0xEAF6FF));
  (void)c2;
}

}  // namespace w
}  // namespace ns
