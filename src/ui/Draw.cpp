#include "ui/Draw.h"

#include <algorithm>
#include <cmath>

namespace ns {

namespace {
// 1×1 白纹理：旋转矩形画粗线用
SDL_Texture* g_whiteTex = nullptr;
SDL_Texture* whiteTex(SDL_Renderer* r) {
  if (!g_whiteTex) {
    g_whiteTex = SDL_CreateTexture(r, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STATIC, 1, 1);
    if (g_whiteTex) {
      const uint32_t px = 0xFFFFFFFFu;
      SDL_UpdateTexture(g_whiteTex, nullptr, &px, 4);
      SDL_SetTextureBlendMode(g_whiteTex, SDL_BLENDMODE_BLEND);
    }
  }
  return g_whiteTex;
}
}  // namespace

static SDL_Color toSdlColor(Color8 c) { return SDL_Color{c.r, c.g, c.b, c.a}; }

void Draw::rect(const Rect& rc, Color8 c) {
  if (c.a == 0 || rc.w <= 0 || rc.h <= 0) return;
  SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
  SDL_SetRenderDrawColor(ren, c.r, c.g, c.b, c.a);
  SDL_Rect r{(int)rc.x, (int)rc.y, (int)rc.w, (int)rc.h};
  SDL_RenderFillRect(ren, &r);
}

void Draw::rectOutline(const Rect& rc, Color8 c, float thickness) {
  const float t = thickness;
  rect(Rect(rc.x, rc.y, rc.w, t), c);
  rect(Rect(rc.x, rc.bottom() - t, rc.w, t), c);
  rect(Rect(rc.x, rc.y + t, t, rc.h - t * 2), c);
  rect(Rect(rc.right() - t, rc.y + t, t, rc.h - t * 2), c);
}

void Draw::roundRect(const Rect& rc, float radius, Color8 c) {
  if (c.a == 0 || rc.w <= 0 || rc.h <= 0) return;
  const float rad = std::min({radius, rc.w * 0.5f, rc.h * 0.5f});
  if (rad <= 1.0f) {
    rect(rc, c);
    return;
  }
  SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
  SDL_SetRenderDrawColor(ren, c.r, c.g, c.b, c.a);
  // 中部矩形
  SDL_Rect mid{(int)(rc.x + rad), (int)rc.y, (int)(rc.w - rad * 2), (int)rc.h};
  SDL_RenderFillRect(ren, &mid);
  SDL_Rect mid2{(int)rc.x, (int)(rc.y + rad), (int)rc.w, (int)(rc.h - rad * 2)};
  SDL_RenderFillRect(ren, &mid2);
  // 四角：按行近似 1/4 圆
  const int R = (int)rad;
  for (int dy = 0; dy < R; dy++) {
    const float dyF = (float)dy + 0.5f;
    const float dx = (float)std::sqrt((double)rad * rad - dyF * dyF);
    const int wPix = (int)dx;
    // 左上
    SDL_Rect lt{(int)(rc.x + rad - wPix), (int)(rc.y + rad - dyF - 1), wPix * 2, 1};
    SDL_RenderFillRect(ren, &lt);
    (void)dy;
  }
  // 复用上半圆结果画下半（对称）
  for (int dy = 0; dy < R; dy++) {
    const float dyF = (float)dy + 0.5f;
    const float dx = (float)std::sqrt((double)rad * rad - dyF * dyF);
    const int wPix = (int)dx;
    SDL_Rect lb{(int)(rc.x + rad - wPix), (int)(rc.bottom() - rad + dyF), wPix * 2, 1};
    SDL_RenderFillRect(ren, &lb);
  }
}

void Draw::roundRectOutline(const Rect& rc, float radius, Color8 c, float thickness) {
  const float rad = std::min({radius, rc.w * 0.5f, rc.h * 0.5f});
  const float t = std::max(1.0f, thickness);
  if (rad <= 1.0f) {
    rectOutline(rc, c, t);
    return;
  }
  rect(Rect(rc.x + rad, rc.y, rc.w - rad * 2, t), c);
  rect(Rect(rc.x + rad, rc.bottom() - t, rc.w - rad * 2, t), c);
  rect(Rect(rc.x, rc.y + rad, t, rc.h - rad * 2), c);
  rect(Rect(rc.right() - t, rc.y + rad, t, rc.h - rad * 2), c);
  // 四角圆弧（16 段）
  const int segs = 8;
  for (int i = 0; i < segs; i++) {
    const float a0 = (float)i / segs * 1.5707963f;
    const float a1 = (float)(i + 1) / segs * 1.5707963f;
    struct { float x, y; } p0{}, p1{};
    // 左上角
    p0 = {rc.x + rad - std::cos(a0) * rad, rc.y + rad - std::sin(a0) * rad};
    p1 = {rc.x + rad - std::cos(a1) * rad, rc.y + rad - std::sin(a1) * rad};
    line(p0.x, p0.y, p1.x, p1.y, c, t);
    // 右上
    p0 = {rc.right() - rad + std::cos(a0) * rad, rc.y + rad - std::sin(a0) * rad};
    p1 = {rc.right() - rad + std::cos(a1) * rad, rc.y + rad - std::sin(a1) * rad};
    line(p0.x, p0.y, p1.x, p1.y, c, t);
    // 左下
    p0 = {rc.x + rad - std::cos(a0) * rad, rc.bottom() - rad + std::sin(a0) * rad};
    p1 = {rc.x + rad - std::cos(a1) * rad, rc.bottom() - rad + std::sin(a1) * rad};
    line(p0.x, p0.y, p1.x, p1.y, c, t);
    // 右下
    p0 = {rc.right() - rad + std::cos(a0) * rad, rc.bottom() - rad + std::sin(a0) * rad};
    p1 = {rc.right() - rad + std::cos(a1) * rad, rc.bottom() - rad + std::sin(a1) * rad};
    line(p0.x, p0.y, p1.x, p1.y, c, t);
  }
}

void Draw::chamferRect(const Rect& rc, float c, Color8 fill) {
  if (fill.a == 0 || rc.w <= 0 || rc.h <= 0) return;
  const float ch = std::min({c, rc.w * 0.5f, rc.h * 0.5f});
  if (ch <= 1.0f) {
    rect(rc, fill);
    return;
  }
  rect(Rect(rc.x + ch, rc.y, rc.w - ch, rc.h), fill);
  rect(Rect(rc.x, rc.y + ch, ch, rc.h - ch), fill);
  rect(Rect(rc.x, rc.y + ch, rc.w, rc.h - ch * 2), fill);
  rect(Rect(rc.right() - ch, rc.y + ch, ch, rc.h - ch), fill);
  rect(Rect(rc.x, rc.bottom() - ch, rc.w - ch, ch), fill);
}

void Draw::chamferRectOutline(const Rect& rc, float c, Color8 stroke, float thickness) {
  const float ch = std::min({c, rc.w * 0.5f, rc.h * 0.5f});
  const float t = std::max(1.0f, thickness);
  // 上边（从左上斜切结束处到右上角）
  line(rc.x + ch, rc.y, rc.right(), rc.y, stroke, t);
  // 右边（右上到右下斜切起点）
  line(rc.right() - t * 0.5f, rc.y, rc.right() - t * 0.5f, rc.bottom() - ch, stroke, t);
  // 右下斜切
  line(rc.right(), rc.bottom() - ch, rc.right() - ch, rc.bottom(), stroke, t);
  // 下边
  line(rc.right() - ch, rc.bottom() - t * 0.5f, rc.x, rc.bottom() - t * 0.5f, stroke, t);
  // 左边
  line(rc.x + t * 0.5f, rc.bottom() - ch, rc.x + t * 0.5f, rc.y + ch, stroke, t);
  // 左上斜切
  line(rc.x, rc.y + ch, rc.x + ch, rc.y, stroke, t);
}

void Draw::line(float x1, float y1, float x2, float y2, Color8 c, float thickness) {
  if (c.a == 0) return;
  SDL_Texture* tex = whiteTex(ren);
  if (!tex) return;
  SDL_SetTextureColorMod(tex, c.r, c.g, c.b);
  SDL_SetTextureAlphaMod(tex, c.a);
  // 用旋转矩形近似粗线
  const float dx = x2 - x1, dy = y2 - y1;
  const float len = std::sqrt(dx * dx + dy * dy);
  if (len < 0.01f) return;
  const double angle = std::atan2((double)dy, (double)dx) * 180.0 / 3.14159265358979323846;
  SDL_Rect r{(int)x1, (int)y1 - thickness / 2, (int)len, (int)std::max(1.0f, thickness)};
  SDL_Point pivot{0, (int)(thickness / 2)};
  SDL_RenderCopyEx(ren, tex, nullptr, &r, angle, &pivot, SDL_FLIP_NONE);
}

void Draw::circle(float cx, float cy, float r, Color8 c) {
  if (c.a == 0 || r <= 0) return;
  SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
  SDL_SetRenderDrawColor(ren, c.r, c.g, c.b, c.a);
  const int R = (int)r;
  for (int dy = -R; dy <= R; dy++) {
    const float dyF = (float)dy;
    const float dx = (float)std::sqrt((double)r * r - dyF * dyF);
    SDL_Rect row{(int)(cx - dx), (int)(cy + dyF), (int)(dx * 2), 1};
    SDL_RenderFillRect(ren, &row);
  }
}

void Draw::circleOutline(float cx, float cy, float r, Color8 c, float thickness) {
  const int segs = 40;
  const float step = 6.2831853f / segs;
  for (int i = 0; i < segs; i++) {
    const float a0 = i * step, a1 = (i + 1) * step;
    line(cx + std::cos(a0) * r, cy + std::sin(a0) * r, cx + std::cos(a1) * r,
         cy + std::sin(a1) * r, c, thickness);
  }
}

void Draw::glowRect(const Rect& rc, float radius, Color8 c, float spread) {
  for (int i = 4; i >= 1; i--) {
    const float m = spread * (float)i / 4.0f;
    const uint8_t a = (uint8_t)(c.a * (5 - i) / 24);
    roundRect(rc.shrink(-m), radius + m, Color8(c.r, c.g, c.b, a));
  }
}

void Draw::glowLine(float x1, float y1, float x2, float y2, Color8 c, float thickness,
                    float spread) {
  for (int i = 4; i >= 1; i--) {
    const float m = spread * (float)i / 4.0f;
    const uint8_t a = (uint8_t)(c.a * (5 - i) / 20);
    line(x1, y1, x2, y2, Color8(c.r, c.g, c.b, a), thickness + m * 2);
  }
  line(x1, y1, x2, y2, c, thickness);
}

void Draw::texture(SDL_Texture* tex, const Rect& dst, Color8 tint) {
  if (!tex) return;
  SDL_SetTextureColorMod(tex, tint.r, tint.g, tint.b);
  SDL_SetTextureAlphaMod(tex, tint.a);
  SDL_Rect r{(int)dst.x, (int)dst.y, (int)dst.w, (int)dst.h};
  SDL_RenderCopy(ren, tex, nullptr, &r);
}

void Draw::textureEx(SDL_Texture* tex, const Rect& src, const Rect& dst, double angle,
                     Color8 tint, bool flipV) {
  if (!tex) return;
  SDL_SetTextureColorMod(tex, tint.r, tint.g, tint.b);
  SDL_SetTextureAlphaMod(tex, tint.a);
  SDL_Rect s{(int)src.x, (int)src.y, (int)src.w, (int)src.h};
  SDL_Rect d{(int)dst.x, (int)dst.y, (int)dst.w, (int)dst.h};
  const SDL_RendererFlip flip = flipV ? SDL_FLIP_VERTICAL : SDL_FLIP_NONE;
  SDL_RenderCopyEx(ren, tex, &s, &d, angle, nullptr, flip);
}

void Draw::textureAspect(SDL_Texture* tex, const Rect& box, Color8 tint) {
  if (!tex) return;
  int w = 0, h = 0;
  SDL_QueryTexture(tex, nullptr, nullptr, &w, &h);
  if (w <= 0 || h <= 0) return;
  const float scale = std::min(box.w / (float)w, box.h / (float)h);
  const float dw = w * scale, dh = h * scale;
  texture(tex, Rect(box.cx() - dw / 2, box.cy() - dh / 2, dw, dh), tint);
}

void Draw::textureCrop(SDL_Texture* tex, const Rect& box, Color8 tint) {
  if (!tex) return;
  int w = 0, h = 0;
  SDL_QueryTexture(tex, nullptr, nullptr, &w, &h);
  if (w <= 0 || h <= 0) return;
  // aspect-fill：源图裁剪出与 box 同比例的居中区域
  const float boxAspect = box.w / box.h;
  SDL_Rect src;
  if ((float)w / h > boxAspect) {
    src.h = h;
    src.w = (int)(h * boxAspect);
    src.x = (w - src.w) / 2;
    src.y = 0;
  } else {
    src.w = w;
    src.h = (int)(w / boxAspect);
    src.x = 0;
    src.y = (h - src.h) / 2;
  }
  SDL_SetTextureColorMod(tex, tint.r, tint.g, tint.b);
  SDL_SetTextureAlphaMod(tex, tint.a);
  SDL_Rect dst{(int)box.x, (int)box.y, (int)box.w, (int)box.h};
  SDL_RenderCopy(ren, tex, &src, &dst);
}

void Draw::textureQuad(SDL_Texture* tex, const float xy[8], const float uv[8],
                       Color8 tint) {
  if (!tex) return;
  SDL_SetTextureColorMod(tex, tint.r, tint.g, tint.b);
  SDL_SetTextureAlphaMod(tex, tint.a);
#if SDL_VERSION_ATLEAST(2, 0, 18)
  // 左上 / 右上 / 右下 / 左下 → 两个三角形（0-1-2, 0-2-3）
  SDL_Vertex vx[4];
  for (int i = 0; i < 4; i++) {
    vx[i].position.x = xy[i * 2];
    vx[i].position.y = xy[i * 2 + 1];
    vx[i].color = SDL_Color{255, 255, 255, 255};
    vx[i].tex_coord.x = uv[i * 2];
    vx[i].tex_coord.y = uv[i * 2 + 1];
  }
  const int idx[6] = {0, 1, 2, 0, 2, 3};
  SDL_RenderGeometry(ren, tex, vx, 4, idx, 6);
#else
  // 旧版 SDL：退化为包围盒缩放拷贝（无透视，但布局不乱）
  const float minX = std::min({xy[0], xy[2], xy[4], xy[6]});
  const float minY = std::min({xy[1], xy[3], xy[5], xy[7]});
  const float maxX = std::max({xy[0], xy[2], xy[4], xy[6]});
  const float maxY = std::max({xy[1], xy[3], xy[5], xy[7]});
  SDL_Rect dst{(int)minX, (int)minY, (int)(maxX - minX), (int)(maxY - minY)};
  SDL_RenderCopy(ren, tex, nullptr, &dst);
#endif
}


void Draw::pushClip(const Rect& rc) {
  SDL_Rect r{(int)rc.x, (int)rc.y, (int)rc.w, (int)rc.h};
  SDL_RenderSetClipRect(ren, &r);
  clipStack_.push_back(r);
}

void Draw::popClip() {
  if (!clipStack_.empty()) clipStack_.pop_back();
  if (clipStack_.empty()) {
    SDL_RenderSetClipRect(ren, nullptr);
  } else {
    SDL_RenderSetClipRect(ren, &clipStack_.back());
  }
}

}  // namespace ns
