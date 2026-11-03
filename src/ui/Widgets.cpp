#include "ui/Widgets.h"

#include <cmath>

#include "ui/Draw.h"
#include "ui/Font.h"
#include "ui/Theme.h"

namespace ns {
namespace w {

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

void header(App& app, const std::string& title, const std::string& subtitle) {
  const Skin& sk = Theme::skin();
  app.font.draw(app.sdlRenderer(), title, 40, 34, 30, sk.textHi, true);
  if (!subtitle.empty()) {
    app.font.draw(app.sdlRenderer(), subtitle, 40, 72, 15, sk.textDim);
  }
  // 标题装饰线
  const float w = app.font.textWidth(title, 30, true);
  app.draw.line(40, 76, 40 + w, 76, Color8(sk.accent.r, sk.accent.g, sk.accent.b, 170), 2);
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
  const bool act = listRow(app, rc, title, value ? "开" : "关", focused, subtitle);
  if (act) value = !value;
  // 视觉指示
  const Skin& sk = Theme::skin();
  const float tw = 34, th = 18;
  const Rect track(rc.right() - 70, rc.cy() - th / 2, tw, th);
  const Color8 on = value ? sk.accent : sk.textDim;
  app.draw.roundRect(track, th / 2, Color8(on.r, on.g, on.b, 90));
  const float knobX = value ? track.right() - th / 2 - 2 : track.x + 2;
  app.draw.circle(knobX + th / 2 - 2, track.cy(), th / 2 - 3, value ? sk.accent : sk.textDim);
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
    const Rect left(rc.right() - 120, rc.y, 30, rc.h);
    const Rect right(rc.right() - 34, rc.y, 30, rc.h);
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
  // 绘制当前值 + 左右箭头
  const std::string& val = options[(size_t)index];
  const float pad = 16;
  const float vw = app.font.textWidth(val, 15);
  const float cx = rc.right() - pad - 50 - vw / 2;
  app.font.drawVCentered(app.sdlRenderer(), val, cx - vw / 2, rc.cy(), 15,
                         hot ? sk.accent : sk.text);
  app.font.drawVCentered(app.sdlRenderer(), "‹", rc.right() - pad - 110, rc.cy(), 18,
                         hot ? sk.accent : sk.textDim);
  app.font.drawVCentered(app.sdlRenderer(), "›", rc.right() - pad - 26, rc.cy(), 18,
                         hot ? sk.accent : sk.textDim);
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

void hintBar(App& app, const std::vector<std::pair<std::string, std::string>>& hints) {
  const Skin& sk = Theme::skin();
  const float y = app.draw.H - 34;
  float x = 24;
  for (const auto& [k, label] : hints) {
    app.draw.circle(x + 9, y + 9, 9, Color8(sk.textDim.r, sk.textDim.g, sk.textDim.b, 50));
    app.draw.circleOutline(x + 9, y + 9, 9, sk.textDim, 1.2f);
    app.font.drawVCentered(app.sdlRenderer(), k, x + 4, y + 9, 11, sk.text);
    x += 22;
    app.font.drawVCentered(app.sdlRenderer(), label, x, y + 9, 14, sk.textDim);
    x += app.font.textWidth(label, 14) + 20;
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
  const float R = rc.minDim() * 0.4f;
  d.circleOutline(cx, cy, R, c, 2.2f);
  for (int i = 0; i < 8; i++) {
    const float a = (float)i * 0.7853982f;
    d.line(cx + std::cos(a) * R, cy + std::sin(a) * R, cx + std::cos(a) * (R + 5),
           cy + std::sin(a) * (R + 5), c, 2.4f);
  }
  d.circleOutline(cx, cy, R * 0.42f, c, 2);
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

// —— 封面 ——
void coverCard(App& app, const Rect& rc, const GameEntry& g, bool selected) {
  const Skin& sk = Theme::skin();
  skinPanel(app.draw, rc, selected, selected ? 16 : 0);
  const Rect inner = rc.inset(6, 6, 6, 6);
  SDL_Texture* tex = coverTexture(app, g);
  if (tex) {
    app.draw.textureAspect(tex, inner);
  } else {
    platformPlaceholder(app, inner, g.platform, g.displayTitle());
  }
  if (g.favorite) {
    iconStar(app, Rect(rc.right() - 30, rc.y + 8, 22, 22), sk.gold, true);
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
