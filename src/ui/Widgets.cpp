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

// —— 手势追踪（拖拽滚动 vs 点击互斥）——
// 按下记录起点，按住累计位移；位移 > 10px 判为拖拽，松开时不触发点击
namespace {
struct GestureState {
  bool down = false;
  float travel = 0;
  uint64_t frame = 0;
} g_gesture;

void gesturePump(App& app) {
  if (g_gesture.frame == app.frameNo()) return;  // 每帧只更新一次
  g_gesture.frame = app.frameNo();
  const InputState& in = app.input.input();
  if (in.mousePressed) {
    g_gesture.down = true;
    g_gesture.travel = 0;
  } else if (in.mouseHeld && g_gesture.down) {
    g_gesture.travel += std::fabs(in.mouseDx) + std::fabs(in.mouseDy);
  } else if (in.mouseReleased) {
    // ★释放帧不清除手势：activated() 本帧仍能凭 travel>10 判定「这是拖拽
    //   不是点击」；下一帧（无按下无释放）才真正复位。旧实现在释放帧立即
    //   down=false → 拖拽列表结束的瞬间被当作点击，误进目录。
  } else if (!in.mouseHeld) {
    g_gesture.down = false;
    g_gesture.travel = 0;
  }
}
}  // namespace

bool dragGestureActive() { return g_gesture.down && g_gesture.travel > 10.0f; }

bool hovered(App& app, const Rect& rc) {
  return rc.contains(app.input.input().mouse);
}

bool activated(App& app, const Rect& rc, bool focused) {
  gesturePump(app);
  const InputState& in = app.input.input();
  const bool hov = hovered(app, rc);
  if (focused && in.pressed[(int)Btn::Ok]) return true;
  // 鼠标：松开时命中且未拖拽 = 点击（拖拽交给滚动，不误触控件）
  if (hov && in.mouseReleased && !dragGestureActive()) return true;
  return false;
}

float dragScroll(App& app, const Rect& rc) {
  gesturePump(app);
  const InputState& in = app.input.input();
  if (in.mouseHeld && rc.contains(in.mouse)) {
    // 手指/鼠标下拉（dy>0）→ 内容跟随下移（scroll 减小）
    return -in.mouseDy;
  }
  return 0;
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
  // 轨道几何（固定在行右侧区域）
  const Rect track(rc.right() - 230, rc.cy() - 4, 190, 8);
  if (hot) {
    if (in.pressed[(int)Btn::Left]) {
      value01 = std::max(0.0f, value01 - 0.05f);
      changed = true;
    }
    if (in.pressed[(int)Btn::Right]) {
      value01 = std::min(1.0f, value01 + 0.05f);
      changed = true;
    }
  }
  // ★ 拖拽修复：命中区 = 轨道纵向扩展 ±18px（不要求精准点中细轨道）；
  // 按住期间持续跟随（含移出行也继续拖，与 Android SeekBar 一致）
  const Rect hit(track.x - 14, track.y - 18, track.w + 28, track.h + 36);
  static bool s_sliderHeld = false;
  if (in.mousePressed && hit.contains(in.mouse)) s_sliderHeld = true;
  if (!in.mouseHeld) s_sliderHeld = false;
  if (s_sliderHeld) {
    const float rel = (in.mouse.x - track.x) / track.w;
    const float v = std::min(1.0f, std::max(0.0f, rel));
    if (std::fabs(v - value01) > 0.001f) {
      value01 = v;
      changed = true;
    }
  }
  // 轨道：圆角胶囊 + accent 填充 + 发光滑钮
  app.draw.roundRect(track, 4, sk.line);
  const Rect fill(track.x, track.y, std::max(8.0f, track.w * value01), track.h);
  app.draw.roundRect(fill, 4, hot ? sk.accent : sk.accentDim);
  const float kx = track.x + track.w * value01;
  if (hot)
    app.draw.glowRect(Rect(kx - 12, track.cy() - 12, 24, 24), 12,
                       Color8(sk.accent.r, sk.accent.g, sk.accent.b, 70), 8);
  app.draw.circle(kx, track.cy(), hot ? 10 : 8, sk.accent);
  app.draw.circle(kx, track.cy(), hot ? 4 : 3, sk.bgDeep);
  return changed;
}

void sectionLabel(App& app, float x, float y, const std::string& text) {
  const Skin& sk = Theme::skin();
  app.font.draw(app.sdlRenderer(), text, x, y, 14, sk.textDim, true);
}

// —— NeonSettingsScreen 1:1 控件 ——

void blockTitle(App& app, float x, float y, const std::string& text) {
  const Skin& sk = Theme::skin();
  // SettingsBlock：accent 12sp 粗体（Android：Neon.Accent）
  app.font.draw(app.sdlRenderer(), text, x, y, 13, sk.accent, true);
}

void blockPanel(App& app, const Rect& rc) {
  const Skin& sk = Theme::skin();
  // NeonPanel：切角 0.10 + BgPanel 底 + 1px Line 描线（FSD 皮肤用圆角）
  if (Theme::isNeon()) {
    const float c = rc.minDim() * 0.10f;
    app.draw.chamferRect(rc, c, sk.bgPanel);
    app.draw.chamferRectOutline(rc, c, sk.line, 1.0f);
  } else {
    app.draw.roundRect(rc, 10, sk.bgPanel);
    app.draw.roundRectOutline(rc, 10, sk.line, 1.0f);
  }
}

bool blockRow(App& app, const Rect& rc, const std::string& label,
              const std::string& sub, bool focused) {
  const Skin& sk = Theme::skin();
  const bool hov = hovered(app, rc);
  const bool hot = focused || hov;
  // hover / 焦点：淡 accent 底 + 左侧 accent 竖条（Neon 切角底，FSD 圆角）
  if (hot) {
    const Color8 hl = Color8(sk.accent.r, sk.accent.g, sk.accent.b, 26);
    if (Theme::isNeon())
      app.draw.chamferRect(rc, rc.minDim() * 0.12f, hl);
    else
      app.draw.roundRect(rc, 6, hl);
    app.draw.rect(Rect(rc.x, rc.y + 4, 3, rc.h - 8), sk.accent);
  }
  const float pad = 18;
  if (!sub.empty()) {
    app.font.draw(app.sdlRenderer(), label, rc.x + pad, rc.y + rc.h * 0.16f, 15,
                  hot ? sk.textHi : sk.text, hot);
    app.font.draw(app.sdlRenderer(), sub, rc.x + pad, rc.y + rc.h * 0.56f, 12,
                  sk.textDim);
  } else {
    app.font.drawVCentered(app.sdlRenderer(), label, rc.x + pad, rc.cy(), 15,
                           hot ? sk.textHi : sk.text, hot);
  }
  return activated(app, rc, focused);
}

bool switchNeon(App& app, const Rect& rc, bool& value, bool focused) {
  const Skin& sk = Theme::skin();
  const bool hov = hovered(app, rc);
  // 整块可点（点击轨道即切换）
  bool toggled = false;
  if (activated(app, rc, focused)) {
    value = !value;
    toggled = true;
  }
  // Compose NeonSwitch：开 = accent 轨道 + BgDeep 钮；关 = BgPanelHi 轨道 + TextDim 钮
  const float tw = 52, th = 28;
  const Rect track(rc.right() - tw - 8, rc.cy() - th / 2, tw, th);
  const Color8 trackOn = Color8(sk.accent.r, sk.accent.g, sk.accent.b, 235);
  const Color8 trackOff = sk.bgPanelHi;
  app.draw.roundRect(track, th / 2, value ? trackOn : trackOff);
  // 描边：开 = accent 实线；关 = Line（保证深底上可见）
  app.draw.roundRectOutline(track, th / 2,
                            value ? Color8(sk.accent.r, sk.accent.g, sk.accent.b, 255)
                                  : (hov || focused
                                         ? Color8(sk.accent.r, sk.accent.g, sk.accent.b, 110)
                                         : sk.line),
                            1.4f);
  const float knobR = th / 2 - 4;
  const float knobX = value ? track.right() - knobR - 4 : track.x + knobR + 4;
  if (value)
    app.draw.glowRect(Rect(knobX - knobR - 4, track.cy() - knobR - 4, knobR * 2 + 8,
                           knobR * 2 + 8),
                      knobR + 4, Color8(255, 255, 255, 40), 6);
  app.draw.circle(knobX, track.cy(), knobR, value ? sk.bgDeep : sk.textDim);
  return toggled;
}

bool styleCard(App& app, const Rect& rc, const std::string& title,
               const std::string& desc, bool selected, Color8 accent) {
  const Skin& sk = Theme::skin();
  // Compose StyleCard：选中 = accent 16% 底 + accent 描边；未选 = BgPanelHi + Line
  if (Theme::isNeon()) {
    const float c = rc.minDim() * 0.14f;
    app.draw.chamferRect(rc, c, selected ? Color8(accent.r, accent.g, accent.b, 41)
                                         : sk.bgPanelHi);
    app.draw.chamferRectOutline(rc, c, selected ? accent : sk.line, selected ? 1.8f : 1.0f);
  } else {
    app.draw.roundRect(rc, 10, selected ? Color8(accent.r, accent.g, accent.b, 41)
                                        : sk.bgPanelHi);
    app.draw.roundRectOutline(rc, 10, selected ? accent : sk.line, selected ? 1.8f : 1.0f);
  }
  if (selected)
    app.draw.glowRect(rc.shrink(-4), Theme::isNeon() ? rc.minDim() * 0.14f : 10,
                      Color8(accent.r, accent.g, accent.b, 36), 8);
  app.font.draw(app.sdlRenderer(), title, rc.x + 16, rc.y + 16, 17,
                selected ? accent : sk.textHi, true);
  const auto lines = app.font.wrap(desc, 12, rc.w - 32);
  float y = rc.y + 42;
  for (size_t i = 0; i < lines.size() && i < 2; i++) {
    app.font.draw(app.sdlRenderer(), lines[i], rc.x + 16, y, 12, sk.textDim);
    y += 16;
  }
  return activated(app, rc, false);
}

bool spinnerChip(App& app, const Rect& rc, const std::vector<std::string>& options,
                 int& index, bool focused) {
  const Skin& sk = Theme::skin();
  if (options.empty()) return false;
  if (index < 0 || index >= (int)options.size()) index = 0;
  const std::string& val = options[(size_t)index];
  // Chip 宽度按文字自适应（右侧留 30px 边距）
  const float fs = 14;
  const float cw = std::max(86.0f, app.font.textWidth(val, fs, true) + 34);
  const Rect chip(rc.right() - cw - 8, rc.cy() - 16, cw, 32);
  const bool hov = hovered(app, chip) || (focused && hovered(app, rc));
  bool changed = false;
  if (activated(app, chip, focused) || (focused && app.input.input().pressed[(int)Btn::Ok])) {
    index = (index + 1) % (int)options.size();
    changed = true;
  }
  if (focused) {
    const InputState& in = app.input.input();
    if (in.pressed[(int)Btn::Left]) {
      index = (index - 1 + (int)options.size()) % (int)options.size();
      changed = true;
    }
    if (in.pressed[(int)Btn::Right]) {
      index = (index + 1) % (int)options.size();
      changed = true;
    }
  }
  if (Theme::isNeon()) {
    app.draw.chamferRect(chip, chip.minDim() * 0.4f, sk.bgPanelHi);
    if (hov || focused)
      app.draw.chamferRectOutline(chip, chip.minDim() * 0.4f, sk.accent, 1.4f);
  } else {
    app.draw.roundRect(chip, 6, sk.bgPanelHi);
    if (hov || focused) app.draw.roundRectOutline(chip, 6, sk.accent, 1.4f);
  }
  // ‹ 值 ›（accent 粗体，同 SpinnerRow）
  app.font.drawVCentered(app.sdlRenderer(), "‹", chip.x + 8, chip.cy(), 14, sk.textDim);
  app.font.drawVCentered(app.sdlRenderer(), val,
                         chip.cx() - app.font.textWidth(val, fs, true) / 2, chip.cy(), fs,
                         sk.accent, true);
  app.font.drawVCentered(app.sdlRenderer(), "›", chip.right() - 16, chip.cy(), 14, sk.textDim);
  return changed;
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

// —— 滚动条（绘制 + ★直接拖拽交互）——
// 返回调整后的 offset01（未交互时原样返回）。调用方：
//   scroll_ = w::scrollIndicator(app, rc, visible01, scroll_ / maxScroll) * maxScroll;
// 交互：点击轨道任意位置跳转；按住滑块（或轨道）持续跟手 —— 桌面滚动条
// 惯例 + 触屏跟手。命中区横向外扩 10px，细条也好点中。
float scrollIndicator(App& app, const Rect& track, float visible01, float offset01) {
  const Skin& sk = Theme::skin();
  const InputState& in = app.input.input();
  const float h = std::max(24.0f, track.h * visible01);
  const float maxOffset = std::max(1.0f, track.h - h);
  auto clamp01 = [](float v) { return v < 0 ? 0.0f : (v > 1 ? 1.0f : v); };

  static bool s_barHeld = false;
  const Rect hit(track.x - 10, track.y - 6, track.w + 20, track.h + 12);
  if (in.mousePressed && hit.contains(in.mouse)) {
    s_barHeld = true;
    // 点击轨道：滑块中心跳到指针位置
    offset01 = clamp01((in.mouse.y - h * 0.5f - track.y) / maxOffset);
  }
  if (!in.mouseHeld) s_barHeld = false;
  if (s_barHeld) offset01 = clamp01((in.mouse.y - h * 0.5f - track.y) / maxOffset);

  offset01 = clamp01(offset01);
  const float y = track.y + maxOffset * offset01;
  // 拖拽中高亮 + 轻微加宽
  app.draw.roundRect(Rect(track.x, y, track.w, h), track.w / 2,
                     s_barHeld ? sk.accent : sk.accentDim);
  if (s_barHeld)
    app.draw.glowRect(Rect(track.x - 4, y, track.w + 8, h), 8,
                      Color8(sk.accent.r, sk.accent.g, sk.accent.b, 60), 10);
  return offset01;
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
    case 21: iconPercent(app, rc, c); break;
    case 22: iconGlobe(app, rc, c); break;
    case 23: iconBolt(app, rc, c); break;
    // ★v1.2.9 FSD 平台磁贴（对齐 APK platformIcon / FsdToolButton）
    case 24: iconConsole(app, rc, c); break;
    case 25: iconPhone(app, rc, c); break;
    case 26: iconPhoneWide(app, rc, c); break;
    case 27: iconComputer(app, rc, c); break;
    case 28: iconRadio(app, rc, c); break;
    case 29: iconDisc(app, rc, c); break;
    case 30: iconBook(app, rc, c); break;
    case 31: iconTicket(app, rc, c); break;
    case 32: iconCup(app, rc, c); break;
    case 33: iconSports(app, rc, c); break;
    case 34: iconTablet(app, rc, c); break;
    case 35: iconTv(app, rc, c); break;
    case 36: iconDisplay(app, rc, c); break;
    case 37: iconDevices(app, rc, c); break;
    case 38: iconImage(app, rc, c); break;
    case 39: iconDownload(app, rc, c); break;
    case 40: iconNetwork(app, rc, c); break;
    // ★v1.3.5 游戏内横排菜单（APK MenuOverlay 图标）
    case 41: iconPause(app, rc, c); break;
    case 42: iconFastForward(app, rc, c); break;
    case 43: iconCamera(app, rc, c); break;
    case 44: iconWrench(app, rc, c); break;
    case 45: iconSliders(app, rc, c); break;
    case 46: iconFullscreen(app, rc, c); break;
    case 47: iconClose(app, rc, c); break;
    case 48: iconUpload(app, rc, c); break;
    case 49: iconHome(app, rc, c); break;
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

void iconPercent(App& app, const Rect& rc, Color8 c) {  // 百分比（性能）
  Draw& d = app.draw;
  const float s = rc.minDim() * 0.36f;
  // 左上圆 + 右下圆（实心）+ 对角线
  d.circle(rc.x + rc.w * 0.32f, rc.y + rc.h * 0.32f, s * 0.42f, c);
  d.circle(rc.x + rc.w * 0.68f, rc.y + rc.h * 0.68f, s * 0.42f, c);
  d.line(rc.x + rc.w * 0.62f, rc.y + rc.h * 0.16f, rc.x + rc.w * 0.38f,
         rc.y + rc.h * 0.84f, c, 2.6f);
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

void iconGlobe(App& app, const Rect& rc, Color8 c) {  // 在线游戏（地球）
  Draw& d = app.draw;
  const float cx = rc.cx(), cy = rc.cy();
  const float R = rc.minDim() * 0.38f;
  const int segs = 22;
  for (int i = 0; i < segs; i++) {  // 外圆
    const float a0 = (float)i / segs * 6.2831853f;
    const float a1 = (float)(i + 1) / segs * 6.2831853f + 0.03f;
    d.line(cx + std::cos(a0) * R, cy + std::sin(a0) * R, cx + std::cos(a1) * R,
           cy + std::sin(a1) * R, c, 2.2f);
  }
  d.line(cx - R, cy, cx + R, cy, c, 1.8f);                       // 赤道
  d.line(cx, cy - R, cx, cy + R, c, 1.8f);                       // 经线
  for (int i = 0; i < segs; i++) {  // 两条回纬线（扁椭圆近似）
    const float t0 = (float)i / segs * 6.2831853f;
    const float t1 = (float)(i + 1) / segs * 6.2831853f + 0.03f;
    d.line(cx + std::cos(t0) * R, cy + std::sin(t0) * R * 0.45f - R * 0.45f,
           cx + std::cos(t1) * R, cy + std::sin(t1) * R * 0.45f - R * 0.45f, c, 1.6f);
    d.line(cx + std::cos(t0) * R, cy + std::sin(t0) * R * 0.45f + R * 0.45f,
           cx + std::cos(t1) * R, cy + std::sin(t1) * R * 0.45f + R * 0.45f, c, 1.6f);
  }
}

void iconBolt(App& app, const Rect& rc, Color8 c) {  // SWF（闪电）
  Draw& d = app.draw;
  const float cx = rc.cx(), cy = rc.cy();
  const float s = rc.minDim() * 0.42f;
  // 六边形闪电（Material Bolt 路径近似）
  const float px[7] = {cx + s * 0.18f, cx - s * 0.42f, cx + s * 0.06f,
                       cx - s * 0.06f, cx + s * 0.42f, cx - s * 0.18f,
                       cx + s * 0.18f};
  const float py[7] = {cy - s,        cy - s * 0.08f,  cy - s * 0.08f,
                       cy + s * 0.08f, cy + s * 0.08f,  cy + s,
                       cy - s};
  d.line(px[0], py[0], px[1], py[1], c, 2.6f);
  d.line(px[1], py[1], px[2], py[2], c, 2.6f);
  d.line(px[2], py[2], px[4], py[4], c, 2.6f);
  d.line(px[4], py[4], px[5], py[5], c, 2.6f);
  d.line(px[5], py[5], px[6], py[6], c, 2.6f);
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
  // ★ 1:1 移植 Android PlaceholderCover：
  //   平台色垂直渐变（c1→c2）+ 内边距 10dp + 居中白色手柄图标 34dp +
  //   平台名（白 13sp 粗体）+ 游戏名（白 85% 10sp，最多 2 行）
  uint32_t c1h = 0, c2h = 0;
  platformGradient(p, &c1h, &c2h);
  const Color8 c1 = Color8::hex(c1h);
  const Color8 c2 = Color8::hex(c2h);
  // 垂直渐变（分段插值）
  const int bands = 14;
  for (int i = 0; i < bands; i++) {
    const float t = (float)i / bands;
    d.rect(Rect(rc.x, rc.y + rc.h * t, rc.w, rc.h / bands + 1), c1.lerp(c2, t));
  }
  const float pad = 10;
  const Rect inner = rc.inset(pad, pad, pad, pad);
  // 手柄图标（居中偏上，34dp）
  const float iconSize = std::min(52.0f, inner.w * 0.34f);
  const Rect iconRc(inner.cx() - iconSize / 2, inner.cy() - iconSize * 0.78f, iconSize,
                    iconSize);
  iconGamepad(app, iconRc, Color8(255, 255, 255, 230));
  // 平台名（白 13sp 粗体，居中）
  const std::string pname = platformShortName(p);
  const float pw = app.font.textWidth(pname, 14, true);
  app.font.draw(app.sdlRenderer(), pname, inner.cx() - pw / 2,
                iconRc.bottom() + 8, 14, Color8(255, 255, 255, 255), true);
  // 游戏名（白 85% 10sp，最多两行，居中）
  const auto lines = app.font.wrap(title, 12, inner.w);
  float ty = iconRc.bottom() + 32;
  for (size_t i = 0; i < lines.size() && i < 2; i++) {
    const float lw = app.font.textWidth(lines[i], 12);
    app.font.draw(app.sdlRenderer(), app.font.ellipsize(lines[i], 12, inner.w),
                  inner.cx() - lw / 2, ty, 12, Color8(255, 255, 255, 217));
    ty += 16;
  }
}

// —— ★v1.2.9 FSD 主页平台磁贴专属图标（1:1 对齐 APK FsdHomeScreen.platformIcon）——
// Material Rounded 风格矢量重绘：每个平台类别一枚可辨识图标，不再千篇一律手柄。

void iconConsole(App& app, const Rect& rc, Color8 c) {  // 24 VideogameAsset（SFC/Switch）
  Draw& d = app.draw;
  const float x = rc.x, y = rc.y + rc.h * 0.28f, w = rc.w, h = rc.h * 0.44f;
  d.roundRectOutline(Rect(x, y, w, h), h * 0.32f, c, 2.2f);
  const float u = std::min(w, h);
  // 左十字键
  const float ccx = x + w * 0.22f, ccy = y + h / 2;
  d.line(ccx - u * 0.13f, ccy, ccx + u * 0.13f, ccy, c, 2.2f);
  d.line(ccx, ccy - u * 0.13f, ccx, ccy + u * 0.13f, c, 2.2f);
  // 右双按钮
  d.circle(x + w * 0.72f, y + h * 0.34f, u * 0.075f, c);
  d.circle(x + w * 0.84f, y + h * 0.62f, u * 0.075f, c);
}

void iconPhone(App& app, const Rect& rc, Color8 c) {  // 25 Smartphone（GB 竖向掌机）
  Draw& d = app.draw;
  const float w = rc.w * 0.56f, h = rc.h * 0.9f;
  const float x = rc.cx() - w / 2, y = rc.y + rc.h * 0.05f;
  d.roundRectOutline(Rect(x, y, w, h), w * 0.18f, c, 2.2f);
  d.line(x + w * 0.32f, y + h * 0.09f, x + w * 0.68f, y + h * 0.09f, c, 2);
  d.circle(x + w / 2, y + h * 0.93f, std::max(1.5f, w * 0.045f), c);
  d.line(x + w * 0.28f, y + h * 0.2f, x + w * 0.72f, y + h * 0.2f, c, 1.4f);
}

void iconPhoneWide(App& app, const Rect& rc, Color8 c) {  // 26 MobileFriendly（GBA 横向掌机）
  Draw& d = app.draw;
  const float w = rc.w * 0.92f, h = rc.h * 0.56f;
  const float x = rc.x + rc.w * 0.04f, y = rc.y + rc.h * 0.22f;
  d.roundRectOutline(Rect(x, y, w, h), h * 0.34f, c, 2.2f);
  const float u = std::min(w, h);
  d.line(x + w * 0.16f, y + h / 2, x + w * 0.3f, y + h / 2, c, 2);
  d.line(x + w * 0.19f, y + h * 0.3f, x + w * 0.19f, y + h * 0.7f, c, 2);
  d.circle(x + w * 0.74f, y + h * 0.36f, u * 0.06f, c);
  d.circle(x + w * 0.85f, y + h * 0.64f, u * 0.06f, c);
}

void iconComputer(App& app, const Rect& rc, Color8 c) {  // 27 Computer（MD 主机）
  Draw& d = app.draw;
  const float w = rc.w * 0.9f, h = rc.h * 0.58f;
  const float x = rc.x + rc.w * 0.05f, y = rc.y + rc.h * 0.08f;
  d.roundRectOutline(Rect(x, y, w, h), h * 0.16f, c, 2.2f);
  d.line(x + w * 0.08f, y + h * 0.26f, x + w * 0.6f, y + h * 0.26f, c, 1.6f);
  d.line(x + w * 0.08f, y + h * 0.48f, x + w * 0.42f, y + h * 0.48f, c, 1.6f);
  d.circle(x + w * 0.82f, y + h * 0.37f, h * 0.07f, c);
  // 底座
  d.line(x + w * 0.38f, y + h, x + w * 0.34f, y + h * 1.24f, c, 2.2f);
  d.line(x + w * 0.62f, y + h, x + w * 0.66f, y + h * 1.24f, c, 2.2f);
  d.line(x + w * 0.24f, y + h * 1.24f, x + w * 0.76f, y + h * 1.24f, c, 2.2f);
}

void iconRadio(App& app, const Rect& rc, Color8 c) {  // 28 Radio（PCE 小白机）
  Draw& d = app.draw;
  // 天线
  d.line(rc.x + rc.w * 0.72f, rc.y + rc.h * 0.3f, rc.x + rc.w * 0.9f, rc.y + rc.h * 0.04f, c, 2);
  const float w = rc.w * 0.86f, h = rc.h * 0.52f;
  const float x = rc.x + rc.w * 0.07f, y = rc.y + rc.h * 0.3f;
  d.roundRectOutline(Rect(x, y, w, h), h * 0.18f, c, 2.2f);
  d.circleOutline(x + w * 0.3f, y + h / 2, h * 0.28f, c, 2);
  d.circle(x + w * 0.3f, y + h / 2, h * 0.07f, c);
  d.line(x + w * 0.64f, y + h * 0.3f, x + w * 0.88f, y + h * 0.3f, c, 1.8f);
  d.line(x + w * 0.64f, y + h * 0.56f, x + w * 0.88f, y + h * 0.56f, c, 1.8f);
}

void iconDisc(App& app, const Rect& rc, Color8 c) {  // 29 Album（PSX 光盘）
  Draw& d = app.draw;
  const float R = rc.minDim() * 0.44f;
  d.circleOutline(rc.cx(), rc.cy(), R, c, 2.2f);
  d.circleOutline(rc.cx(), rc.cy(), R * 0.34f, c, 2);
  d.circle(rc.cx(), rc.cy(), R * 0.08f, c);
  d.arc(rc.cx(), rc.cy(), R * 0.78f, -0.5f, 0.9f, c, 1.6f);
}

void iconBook(App& app, const Rect& rc, Color8 c) {  // 30 MenuBook（NDS 翻盖双屏）
  Draw& d = app.draw;
  // 左右两页
  d.line(rc.cx(), rc.y + rc.h * 0.14f, rc.cx(), rc.y + rc.h * 0.92f, c, 2.2f);
  d.line(rc.x + rc.w * 0.08f, rc.y + rc.h * 0.2f, rc.cx() - rc.w * 0.03f, rc.y + rc.h * 0.14f, c, 2);
  d.line(rc.x + rc.w * 0.08f, rc.y + rc.h * 0.2f, rc.x + rc.w * 0.08f, rc.y + rc.h * 0.9f, c, 2);
  d.line(rc.x + rc.w * 0.08f, rc.y + rc.h * 0.9f, rc.cx(), rc.y + rc.h * 0.92f, c, 2);
  d.line(rc.x + rc.w * 0.92f, rc.y + rc.h * 0.2f, rc.cx() + rc.w * 0.03f, rc.y + rc.h * 0.14f, c, 2);
  d.line(rc.x + rc.w * 0.92f, rc.y + rc.h * 0.2f, rc.x + rc.w * 0.92f, rc.y + rc.h * 0.9f, c, 2);
  d.line(rc.x + rc.w * 0.92f, rc.y + rc.h * 0.9f, rc.cx(), rc.y + rc.h * 0.92f, c, 2);
  // 页面行
  d.line(rc.x + rc.w * 0.17f, rc.y + rc.h * 0.4f, rc.cx() - rc.w * 0.08f, rc.y + rc.h * 0.38f, c, 1.4f);
  d.line(rc.x + rc.w * 0.17f, rc.y + rc.h * 0.58f, rc.cx() - rc.w * 0.08f, rc.y + rc.h * 0.56f, c, 1.4f);
  d.line(rc.x + rc.w * 0.83f, rc.y + rc.h * 0.4f, rc.cx() + rc.w * 0.08f, rc.y + rc.h * 0.38f, c, 1.4f);
  d.line(rc.x + rc.w * 0.83f, rc.y + rc.h * 0.58f, rc.cx() + rc.w * 0.08f, rc.y + rc.h * 0.56f, c, 1.4f);
}

void iconTicket(App& app, const Rect& rc, Color8 c) {  // 31 LocalPlay（街机厅票券）
  Draw& d = app.draw;
  const float x = rc.x + rc.w * 0.04f, y = rc.y + rc.h * 0.22f;
  const float w = rc.w * 0.92f, h = rc.h * 0.56f;
  d.roundRectOutline(Rect(x, y, w, h), h * 0.16f, c, 2.2f);
  // 两侧半圆缺口（近似：贴背景色圆盖住边线两侧）—— 用两段竖虚线表达撕边
  const float u = std::min(w, h);
  d.line(x + w * 0.18f, y + h * 0.22f, x + w * 0.18f, y + h * 0.78f, c, 1.6f);
  d.line(x + w * 0.82f, y + h * 0.22f, x + w * 0.82f, y + h * 0.78f, c, 1.6f);
  // 中央星形（街机星星）
  const float R = u * 0.2f, r2 = R * 0.45f;
  for (int i = 0; i < 10; i++) {
    const float a0 = -1.5707963f + (float)i * 0.6283185f;
    const float a1 = -1.5707963f + (float)(i + 1) * 0.6283185f;
    const float r0 = (i % 2 == 0) ? R : r2, r1 = (i % 2 == 0) ? r2 : R;
    d.line(x + w * 0.5f + std::cos(a0) * r0, y + h / 2 + std::sin(a0) * r0,
           x + w * 0.5f + std::cos(a1) * r1, y + h / 2 + std::sin(a1) * r1, c, 1.8f);
  }
}

void iconCup(App& app, const Rect& rc, Color8 c) {  // 32 LocalCafe（Java 咖啡）
  Draw& d = app.draw;
  const float x = rc.x + rc.w * 0.18f, y = rc.y + rc.h * 0.24f;
  const float w = rc.w * 0.5f, h = rc.h * 0.46f;
  // 杯体
  d.line(x, y, x + w * 0.08f, y + h, c, 2.2f);
  d.line(x + w * 0.08f, y + h, x + w * 0.92f, y + h, c, 2.2f);
  d.line(x + w * 0.92f, y + h, x + w, y, c, 2.2f);
  d.line(x, y, x + w, y, c, 2.2f);
  // 杯柄
  d.arc(x + w * 1.02f, y + h * 0.35f, w * 0.26f, -1.2f, 1.2f, c, 2.2f);
  // 热气
  d.arc(x + w * 0.3f, rc.y + rc.h * 0.1f, rc.h * 0.06f, 0.3f, 2.6f, c, 1.5f);
  d.arc(x + w * 0.62f, rc.y + rc.h * 0.1f, rc.h * 0.06f, 0.3f, 2.6f, c, 1.5f);
}

void iconSports(App& app, const Rect& rc, Color8 c) {  // 33 SportsEsports（DC/对战平台）
  Draw& d = app.draw;
  const float w = rc.w * 0.94f, h = rc.h * 0.5f;
  const float x = rc.x + rc.w * 0.03f, y = rc.y + rc.h * 0.26f;
  // 两侧握柄圆 + 中央连接（手柄轮廓）
  d.circleOutline(x + h * 0.5f, y + h * 0.5f, h * 0.5f, c, 2.2f);
  d.circleOutline(x + w - h * 0.5f, y + h * 0.5f, h * 0.5f, c, 2.2f);
  d.line(x + h * 0.5f, y, x + w - h * 0.5f, y, c, 2.2f);
  d.line(x + h * 0.5f, y + h, x + w - h * 0.5f, y + h, c, 2.2f);
  // 左十字键
  const float ccx = x + w * 0.3f, ccy = y + h * 0.5f;
  const float u = h * 0.3f;
  d.line(ccx - u, ccy, ccx + u, ccy, c, 2);
  d.line(ccx, ccy - u, ccx, ccy + u, c, 2);
  // 右四键
  const float bcx = x + w * 0.72f;
  d.circle(bcx, ccy - h * 0.24f, u * 0.22f, c);
  d.circle(bcx, ccy + h * 0.24f, u * 0.22f, c);
  d.circle(bcx - h * 0.26f, ccy, u * 0.22f, c);
  d.circle(bcx + h * 0.26f, ccy, u * 0.22f, c);
}

void iconTablet(App& app, const Rect& rc, Color8 c) {  // 34 TabletAndroid（3DS）
  Draw& d = app.draw;
  const float w = rc.w * 0.74f, h = rc.h * 0.86f;
  const float x = rc.cx() - w / 2, y = rc.y + rc.h * 0.07f;
  d.roundRectOutline(Rect(x, y, w, h), w * 0.12f, c, 2.2f);
  // 双屏分割线（3DS 特征）
  d.line(x + w * 0.1f, y + h * 0.46f, x + w * 0.9f, y + h * 0.46f, c, 2);
  d.line(x + w * 0.22f, y + h * 0.22f, x + w * 0.78f, y + h * 0.22f, c, 1.4f);
  d.line(x + w * 0.22f, y + h * 0.68f, x + w * 0.78f, y + h * 0.68f, c, 1.4f);
  d.circle(x + w / 2, y + h * 0.96f, std::max(1.5f, w * 0.03f), c);
}

void iconTv(App& app, const Rect& rc, Color8 c) {  // 35 Tv（NGC/Wii 电视主机）
  Draw& d = app.draw;
  const float w = rc.w * 0.94f, h = rc.h * 0.6f;
  const float x = rc.x + rc.w * 0.03f, y = rc.y + rc.h * 0.06f;
  d.roundRectOutline(Rect(x, y, w, h), h * 0.12f, c, 2.2f);
  // 天线
  d.line(x + w * 0.24f, y, x + w * 0.1f, rc.y, c, 2);
  d.line(x + w * 0.76f, y, x + w * 0.9f, rc.y, c, 2);
  // 底座
  d.line(x + w * 0.44f, y + h, x + w * 0.4f, y + h * 1.18f, c, 2.2f);
  d.line(x + w * 0.56f, y + h, x + w * 0.6f, y + h * 1.18f, c, 2.2f);
  d.line(x + w * 0.28f, y + h * 1.18f, x + w * 0.72f, y + h * 1.18f, c, 2.2f);
}

void iconDisplay(App& app, const Rect& rc, Color8 c) {  // 36 SmartDisplay（PSP 宽屏）
  Draw& d = app.draw;
  const float w = rc.w * 0.96f, h = rc.h * 0.56f;
  const float x = rc.x + rc.w * 0.02f, y = rc.y + rc.h * 0.22f;
  d.roundRectOutline(Rect(x, y, w, h), h * 0.26f, c, 2.2f);
  // 中央播放三角
  const float cx = x + w / 2, cy = y + h / 2, s = h * 0.26f;
  d.line(cx - s * 0.7f, cy - s, cx - s * 0.7f, cy + s, c, 2.2f);
  d.line(cx - s * 0.7f, cy - s, cx + s, cy, c, 2.2f);
  d.line(cx - s * 0.7f, cy + s, cx + s, cy, c, 2.2f);
}

void iconDevices(App& app, const Rect& rc, Color8 c) {  // 37 Devices（Wii U 双设备）
  Draw& d = app.draw;
  // 后方显示器
  const float mw = rc.w * 0.62f, mh = rc.h * 0.44f;
  const float mx = rc.x + rc.w * 0.04f, my = rc.y + rc.h * 0.08f;
  d.roundRectOutline(Rect(mx, my, mw, mh), mh * 0.12f, c, 2.2f);
  d.line(mx + mw * 0.5f, my + mh, mx + mw * 0.5f, my + mh * 1.16f, c, 2);
  d.line(mx + mw * 0.28f, my + mh * 1.16f, mx + mw * 0.72f, my + mh * 1.16f, c, 2);
  // 前方掌机
  const float pw = rc.w * 0.34f, ph = rc.h * 0.4f;
  const float px = rc.x + rc.w * 0.6f, py = rc.y + rc.h * 0.5f;
  d.roundRectOutline(Rect(px, py, pw, ph), ph * 0.2f, c, 2.2f);
  d.circle(px + pw / 2, py + ph * 0.86f, std::max(1.2f, pw * 0.04f), c);
}

void iconImage(App& app, const Rect& rc, Color8 c) {  // 38 ImageIcon（获取封面）
  Draw& d = app.draw;
  d.roundRectOutline(rc.inset(rc.w * 0.06f, rc.h * 0.06f, rc.w * 0.06f, rc.h * 0.06f),
                     rc.minDim() * 0.12f, c, 2.2f);
  const float x = rc.x + rc.w * 0.2f, y = rc.y + rc.h * 0.62f;
  d.line(x, y, x + rc.w * 0.14f, y - rc.h * 0.16f, c, 2.2f);
  d.line(x + rc.w * 0.14f, y - rc.h * 0.16f, x + rc.w * 0.28f, y, c, 2.2f);
  d.line(x + rc.w * 0.28f, y, x + rc.w * 0.42f, y - rc.h * 0.22f, c, 2.2f);
  d.line(x + rc.w * 0.42f, y - rc.h * 0.22f, x + rc.w * 0.6f, y + rc.h * 0.04f, c, 2.2f);
  d.circle(rc.x + rc.w * 0.66f, rc.y + rc.h * 0.28f, rc.minDim() * 0.09f, c);
}

void iconDownload(App& app, const Rect& rc, Color8 c) {  // 39 Download（安装CIA）
  Draw& d = app.draw;
  const float cx = rc.cx(), top = rc.y + rc.h * 0.12f, bot = rc.y + rc.h * 0.62f;
  d.line(cx, top, cx, bot, c, 2.4f);
  d.line(cx - rc.w * 0.16f, bot - rc.h * 0.16f, cx, bot, c, 2.4f);
  d.line(cx + rc.w * 0.16f, bot - rc.h * 0.16f, cx, bot, c, 2.4f);
  d.line(rc.x + rc.w * 0.2f, rc.y + rc.h * 0.82f, rc.x + rc.w * 0.8f, rc.y + rc.h * 0.82f, c, 2.4f);
}

void iconUpload(App& app, const Rect& rc, Color8 c) {  // 48 UploadFile（导入ROM）
  Draw& d = app.draw;
  const float cx = rc.cx(), top = rc.y + rc.h * 0.30f, bot = rc.y + rc.h * 0.80f;
  d.line(cx, top, cx, bot, c, 2.4f);
  d.line(cx - rc.w * 0.16f, top + rc.h * 0.16f, cx, top, c, 2.4f);
  d.line(cx + rc.w * 0.16f, top + rc.h * 0.16f, cx, top, c, 2.4f);
  d.line(rc.x + rc.w * 0.2f, rc.y + rc.h * 0.92f, rc.x + rc.w * 0.8f, rc.y + rc.h * 0.92f, c, 2.4f);
}

void iconHome(App& app, const Rect& rc, Color8 c) {  // 49 Home（主页）
  Draw& d = app.draw;
  const float cx = rc.cx(), bot = rc.y + rc.h * 0.88f;
  // 屋顶（三角）
  d.line(cx, rc.y + rc.h * 0.10f, rc.x + rc.w * 0.10f, rc.y + rc.h * 0.48f, c, 2.4f);
  d.line(cx, rc.y + rc.h * 0.10f, rc.x + rc.w * 0.90f, rc.y + rc.h * 0.48f, c, 2.4f);
  // 屋身（方形）
  d.rectOutline(Rect(rc.x + rc.w * 0.20f, rc.y + rc.h * 0.46f, rc.w * 0.60f, bot - (rc.y + rc.h * 0.46f)), c, 2.2f);
  // 门
  d.line(cx, bot, cx, bot - rc.h * 0.22f, c, 2.0f);
}

void iconNetwork(App& app, const Rect& rc, Color8 c) {  // 40 NetworkCheck（网络状态）
  Draw& d = app.draw;
  const float cx = rc.cx(), cy = rc.bottom() - rc.h * 0.12f;
  d.arc(cx, cy, rc.minDim() * 0.16f, -2.356f, -0.785f, c, 2);
  d.arc(cx, cy, rc.minDim() * 0.32f, -2.356f, -0.785f, c, 2);
  d.arc(cx, cy, rc.minDim() * 0.48f, -2.356f, -0.785f, c, 2);
  d.circle(cx, cy, std::max(1.4f, rc.minDim() * 0.05f), c);
}

// ★v1.3.5 游戏内横排菜单图标（对齐 APK MenuOverlay Material 图标的视觉语义）

void iconPause(App& app, const Rect& rc, Color8 c) {  // 41 Pause（双竖条）
  Draw& d = app.draw;
  const float cx = rc.cx(), cy = rc.cy();
  const float h = rc.minDim() * 0.52f, w = std::max(2.6f, rc.minDim() * 0.13f);
  d.rect(Rect(cx - w - w * 0.6f, cy - h / 2, w, h), c);
  d.rect(Rect(cx + w * 0.6f, cy - h / 2, w, h), c);
}

void iconFastForward(App& app, const Rect& rc, Color8 c) {  // 42 FastForward（▶▶）
  Draw& d = app.draw;
  const float cy = rc.cy();
  const float s = rc.minDim() * 0.30f;  // 半高
  const float x0 = rc.cx() - s * 1.35f;
  for (int k = 0; k < 2; k++) {
    const float x = x0 + (float)k * s * 1.15f;
    d.line(x, cy - s, x + s * 0.9f, cy, c, 2.6f);
    d.line(x + s * 0.9f, cy, x, cy + s, c, 2.6f);
  }
}

void iconCamera(App& app, const Rect& rc, Color8 c) {  // 43 CameraAlt（相机）
  Draw& d = app.draw;
  const float cx = rc.cx(), cy = rc.cy();
  const float w = rc.minDim() * 0.66f, h = rc.minDim() * 0.44f;
  // 机身
  d.roundRectOutline(Rect(cx - w / 2, cy - h / 2 + rc.minDim() * 0.07f, w, h),
                      2.5f, c, 2.0f);
  // 顶部取景器凸起
  d.rectOutline(Rect(cx - w * 0.14f, cy - h / 2 - rc.minDim() * 0.02f, w * 0.28f,
                     rc.minDim() * 0.09f), c, 2.0f);
  // 镜头
  d.circleOutline(cx, cy + rc.minDim() * 0.07f, rc.minDim() * 0.13f, c, 2.0f);
}

void iconWrench(App& app, const Rect& rc, Color8 c) {  // 44 Build（扳手）
  Draw& d = app.draw;
  const float cx = rc.cx(), cy = rc.cy();
  const float s = rc.minDim() * 0.30f;
  // 扳手头（开口圆弧）+ 柄（45° 斜线）
  d.arc(cx - s * 0.55f, cy - s * 0.55f, s * 0.62f, -0.6f, 2.2f, c, 3.2f);
  d.line(cx - s * 0.25f, cy - s * 0.25f, cx + s * 0.85f, cy + s * 0.85f, c, 3.4f);
}

void iconSliders(App& app, const Rect& rc, Color8 c) {  // 45 Tune（三道滑杆）
  Draw& d = app.draw;
  const float cx = rc.cx(), cy = rc.cy();
  const float w = rc.minDim() * 0.68f;
  const float ys[3] = {cy - rc.minDim() * 0.20f, cy, cy + rc.minDim() * 0.20f};
  const float knobX[3] = {cx - w * 0.18f, cx + w * 0.10f, cx - w * 0.05f};
  for (int i = 0; i < 3; i++) {
    d.line(cx - w / 2, ys[i], cx + w / 2, ys[i], c, 2.0f);
    d.circle(knobX[i], ys[i], rc.minDim() * 0.085f, c);
  }
}

void iconFullscreen(App& app, const Rect& rc, Color8 c) {  // 46 Fullscreen（四角括弧）
  Draw& d = app.draw;
  const float cx = rc.cx(), cy = rc.cy();
  const float s = rc.minDim() * 0.30f;
  const float t = std::max(2.0f, rc.minDim() * 0.09f);
  // 四角 L 形括弧
  const float k = s * 0.6f;
  d.line(cx - s, cy - s, cx - s + k, cy - s, c, t);
  d.line(cx - s, cy - s, cx - s, cy - s + k, c, t);
  d.line(cx + s - k, cy - s, cx + s, cy - s, c, t);
  d.line(cx + s, cy - s, cx + s, cy - s + k, c, t);
  d.line(cx - s, cy + s - k, cx - s, cy + s, c, t);
  d.line(cx - s, cy + s, cx - s + k, cy + s, c, t);
  d.line(cx + s, cy + s - k, cx + s, cy + s, c, t);
  d.line(cx + s - k, cy + s, cx + s, cy + s, c, t);
}

void iconClose(App& app, const Rect& rc, Color8 c) {  // 47 Close（X）
  Draw& d = app.draw;
  const float cx = rc.cx(), cy = rc.cy();
  const float s = rc.minDim() * 0.30f;
  d.line(cx - s, cy - s, cx + s, cy + s, c, 3.0f);
  d.line(cx - s, cy + s, cx + s, cy - s, c, 3.0f);
}

// —— ★v1.2.9 共享背景（1:1 对齐 APK）——

// FsdBackdrop（FsdScaffold.kt drawFsdBackdrop）：
//   深蓝三段渐变（0A1D3A→123B6E→081426）+ 中央放射高光（2E6DB4 35%）+
//   5 条 24° 斜向光带（白 4%~8%）+ 3 根细亮线 + 底部 04101F 渐暗。
void fsdWallpaper(App& app) {
  Draw& d = app.draw;
  const float W = (float)d.W, H = (float)d.H;
  const Color8 bgTop = Color8::hex(0x0A1D3A), bgMid = Color8::hex(0x123B6E),
               bgBot = Color8::hex(0x081426);
  // 基底：三段垂直渐变（0 → 0.45 → 1）
  for (int i = 0; i < 48; i++) {
    const float t = (float)i / 48;
    const Color8 c = t < 0.45f ? bgTop.lerp(bgMid, t / 0.45f)
                               : bgMid.lerp(bgBot, (t - 0.45f) / 0.55f);
    d.rect(Rect(0, H * t, W, H / 48 + 1), c);
  }
  // 中央放射高光（concentric 圆近似 radialGradient）
  {
    const float cx = W * 0.5f, cy = H * 0.35f;
    const float R = std::max(W, H) * 0.75f;
    const int layers = 14;
    for (int i = layers; i >= 1; i--) {
      const float r = R * (float)i / layers;
      const uint8_t a = (uint8_t)(0.35f * 255.0f * (1.0f - (float)i / (layers + 1.0f)));
      d.circle(cx, cy, r, Color8(0x2E, 0x6D, 0xB4, a));
    }
  }
  // 斜向光带（24° 旋转平行扫光）：每条 = 沿带宽方向的半透明平行斜线
  {
    struct Streak { float xFrac, widthFrac, alpha; };
    static const Streak streaks[] = {
        {0.12f, 0.22f, 0.05f}, {0.38f, 0.10f, 0.07f}, {0.52f, 0.28f, 0.04f},
        {0.72f, 0.12f, 0.08f}, {0.88f, 0.20f, 0.05f},
    };
    const float slope = std::tan(24.0f * 3.14159265f / 180.0f);  // dx/dy
    for (const auto& s : streaks) {
      const float cx = W * s.xFrac, half = W * s.widthFrac * 0.5f;
      const int lines = (int)(W * s.widthFrac / 6.0f) + 2;
      for (int i = 0; i <= lines; i++) {
        const float t = (float)i / lines;              // 0..1 横跨带宽
        const float profile = 1.0f - std::fabs(t - 0.5f) * 2.0f;  // 中间亮两侧暗
        const uint8_t a = (uint8_t)(255.0f * s.alpha * profile);
        if (a == 0) continue;
        const float x0 = cx - half + W * s.widthFrac * t;
        d.line(x0, -H * 0.2f, x0 - slope * H * 1.3f, H * 1.1f,
               Color8(255, 255, 255, a), 5.5f);
      }
    }
    // 细亮线（3 根，白 10%）
    static const float xf[] = {0.30f, 0.62f, 0.83f};
    for (float x : xf)
      d.line(W * x, -H * 0.2f, W * x - slope * H * 1.3f, H * 1.1f,
             Color8(255, 255, 255, 26), 2.6f);
  }
  // 底部渐暗（0.72H → 1：04101F 0 → 55%）
  for (int i = 0; i < 12; i++) {
    const float t = (float)i / 12;
    const float y = H * (0.72f + 0.28f * t);
    const uint8_t a = (uint8_t)(0.55f * 255.0f * t);
    d.rect(Rect(0, y, W, H * 0.28f / 12 + 1), Color8(0x04, 0x10, 0x1F, a));
  }
}

// PixelBackdrop（PixelBackdrop.kt）：浅色像素天空（B7D7F2→C8E2F6）+ 4 朵漂移
// 像素云 + 5 点星光闪烁 + 海（4F8AC4→2F5C8C）+ 草地（7BB36A→4F8C4A）。
void pixelSky(App& app, int64_t timeMs) {
  Draw& d = app.draw;
  const float W = (float)d.W, H = (float)d.H;
  // 天空
  const Color8 skyTop = Color8::hex(0xB7D7F2), skyBot = Color8::hex(0xC8E2F6);
  for (int i = 0; i < 40; i++) {
    const float t = (float)i / 40;
    d.rect(Rect(0, H * t, W, H / 40 + 1), skyTop.lerp(skyBot, t));
  }
  // 像素云（漂移）
  struct Cloud { float x, y, scale, drift; };
  static const Cloud clouds[] = {
      {0.08f, 0.18f, 1.4f, 0.000020f}, {0.32f, 0.10f, 1.0f, 0.000014f},
      {0.62f, 0.22f, 1.7f, 0.000010f}, {0.86f, 0.08f, 1.2f, 0.000018f},
  };
  static const char* pattern[] = {"  ####  ", " ###### ", "########",
                                  " ###### ", "  ####  "};
  const Color8 cloudC = Color8::hex(0xF5F7FB), cloudS = Color8::hex(0xCAD3DF);
  for (const auto& c : clouds) {
    const float dx = std::fmod((float)((double)timeMs * c.drift), W);
    const float cx = c.x * W + dx;
    const float unit = 18.0f * c.scale;
    for (int row = 0; row < 5; row++) {
      for (int col = 0; col < 8; col++) {
        if (pattern[row][col] != '#') continue;
        float px = cx + (col - 4) * unit;
        if (px > W) px -= W;
        if (px + unit < 0) px += W;
        d.rect(Rect(px, c.y * H + (row - 2) * unit, unit, unit),
               row >= 3 ? cloudS : cloudC);
      }
    }
  }
  // 星光闪烁
  struct Sparkle { float x, y, size, phase; };
  static const Sparkle sparks[] = {
      {0.10f, 0.12f, 14, 0.6f}, {0.25f, 0.06f, 18, 0.4f}, {0.45f, 0.14f, 12, 0.8f},
      {0.78f, 0.05f, 16, 0.5f}, {0.92f, 0.16f, 14, 0.7f},
  };
  for (const auto& s : sparks) {
    const float blink =
        (std::sin((double)timeMs / 1000.0 * 2.0 + s.phase * 6.2831853) + 1.0f) / 2.0f;
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

}  // namespace w
}  // namespace ns
