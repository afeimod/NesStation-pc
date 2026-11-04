// NesStation PC — UI 基础类型（颜色 / 矩形 / 输入动作）
#pragma once
#include <cstdint>
#include <string>

namespace ns {

struct Vec2 {
  float x = 0, y = 0;
  Vec2() = default;
  Vec2(float px, float py) : x(px), y(py) {}
};

struct Rect {
  float x = 0, y = 0, w = 0, h = 0;
  Rect() = default;
  Rect(float px, float py, float pw, float ph) : x(px), y(py), w(pw), h(ph) {}
  float left() const { return x; }
  float right() const { return x + w; }
  float top() const { return y; }
  float bottom() const { return y + h; }
  float cx() const { return x + w * 0.5f; }
  float cy() const { return y + h * 0.5f; }
  float minDim() const { return w < h ? w : h; }
  bool contains(const Vec2& p) const {
    return p.x >= x && p.x < x + w && p.y >= y && p.y < y + h;
  }
  bool contains(float px, float py) const {
    return px >= x && px < x + w && py >= y && py < y + h;
  }
  Rect shrink(float m) const { return Rect(x + m, y + m, w - m * 2, h - m * 2); }
  Rect inset(float l, float t, float r, float b) const {
    return Rect(x + l, y + t, w - l - r, h - t - b);
  }
  Rect withAspectFit(float contentAspect) const;
};

struct Color8 {
  uint8_t r = 255, g = 255, b = 255, a = 255;
  Color8() = default;
  Color8(uint8_t pr, uint8_t pg, uint8_t pb, uint8_t pa = 255)
      : r(pr), g(pg), b(pb), a(pa) {}
  // 0xRRGGBB / 0xRRGGBBAA
  static Color8 hex(uint32_t v, uint8_t a = 255) {
    return Color8((uint8_t)((v >> 16) & 0xFF), (uint8_t)((v >> 8) & 0xFF), (uint8_t)(v & 0xFF), a);
  }
  Color8 withA(uint8_t a2) const { return Color8(r, g, b, a2); }
  Color8 scale(float s) const {
    const auto f = [&](uint8_t c) -> uint8_t {
      const int v = (int)(c * s);
      return (uint8_t)(v < 0 ? 0 : (v > 255 ? 255 : v));
    };
    return Color8(f(r), f(g), f(b), a);
  }
  Color8 lerp(const Color8& o, float t) const {
    const auto f = [&](uint8_t a1, uint8_t b1) -> uint8_t {
      return (uint8_t)(a1 + (int)((b1 - a1) * t));
    };
    return Color8(f(r, o.r), f(g, o.g), f(b, o.b), f(a, o.a));
  }
};

inline Rect Rect::withAspectFit(float contentAspect) const {
  if (contentAspect <= 0 || w <= 0 || h <= 0) return *this;
  const float boxAspect = w / h;
  if (boxAspect > contentAspect) {
    const float nw = h * contentAspect;
    return Rect(x + (w - nw) * 0.5f, y, nw, h);
  }
  const float nh = w / contentAspect;
  return Rect(x, y + (h - nh) * 0.5f, w, nh);
}

// —— 统一 UI 输入动作（键盘 / 手柄 / 鼠标共同映射）———
enum class Btn : int {
  Up = 0, Down, Left, Right,
  Ok,       // Enter / 手柄A / 鼠标左键
  Back,     // Esc / 手柄B
  Menu,     // F1 / 手柄Start
  Option,   // 手柄Y / 键盘E
  Fav,      // 手柄X / 键盘F
  PageUp,   // L1 / PgUp / 滚轮上
  PageDown, // R1 / PgDn / 滚轮下
  Count
};

struct InputState {
  bool held[(int)Btn::Count] = {};
  bool pressed[(int)Btn::Count] = {};   // 本帧按下沿
  bool released[(int)Btn::Count] = {};  // 本帧释放沿

  Vec2 mouse;
  float mouseDx = 0, mouseDy = 0;  // 本帧鼠标位移增量（拖拽滚动 / 触摸滑动用）
  bool mouseHeld = false;      // 左键按住
  bool mousePressed = false;   // 左键按下沿
  bool mouseReleased = false;
  bool mouseRightPressed = false;  // 右键按下沿（游戏内呼出菜单等）
  int wheel = 0;              // 本帧滚轮（+1 上 / -1 下）

  bool anyPressed() const {
    for (int i = 0; i < (int)Btn::Count; i++)
      if (pressed[i]) return true;
    return mousePressed || mouseRightPressed || wheel != 0;
  }
};

// 常用颜色
inline Color8 colorWhite() { return Color8(255, 255, 255, 255); }
inline Color8 colorBlack() { return Color8(0, 0, 0, 255); }
inline Color8 transparent() { return Color8(0, 0, 0, 0); }

}  // namespace ns
