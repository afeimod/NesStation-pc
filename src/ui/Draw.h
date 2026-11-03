// NesStation PC — 渲染助手：圆角/切角矩形、描边、辉光、纹理、裁剪栈
#pragma once
#include <vector>

#include <SDL.h>

#include "ui/Ui.h"

namespace ns {

class Draw {
 public:
  void begin(SDL_Renderer* r, int w, int h) {
    ren = r;
    W = w;
    H = h;
  }

  SDL_Renderer* ren = nullptr;
  int W = 0, H = 0;

  // —— 基础形状 ——
  void rect(const Rect& rc, Color8 c);
  void rectOutline(const Rect& rc, Color8 c, float thickness = 1);
  void roundRect(const Rect& rc, float radius, Color8 c);
  void roundRectOutline(const Rect& rc, float radius, Color8 c, float thickness = 1);
  // Neon 切角（左上 + 右下斜切）
  void chamferRect(const Rect& rc, float c, Color8 fill);
  void chamferRectOutline(const Rect& rc, float c, Color8 stroke, float thickness = 1);
  void line(float x1, float y1, float x2, float y2, Color8 c, float thickness = 1);
  void circle(float cx, float cy, float r, Color8 c);
  void circleOutline(float cx, float cy, float r, Color8 c, float thickness = 1);

  // —— 辉光（多层近似）——
  void glowRect(const Rect& rc, float radius, Color8 c, float spread = 14);
  void glowLine(float x1, float y1, float x2, float y2, Color8 c, float thickness,
                float spread);

  // —— 纹理 ——
  void texture(SDL_Texture* tex, const Rect& dst, Color8 tint = Color8(255, 255, 255, 255));
  void textureEx(SDL_Texture* tex, const Rect& src, const Rect& dst, double angle,
                 Color8 tint = Color8(255, 255, 255, 255), bool flipV = false);
  void textureAspect(SDL_Texture* tex, const Rect& box,
                    Color8 tint = Color8(255, 255, 255, 255));  // contain 适配
  // aspect-fill：裁剪源图填满 box（Neon 封面背景用），保持纹理不变形
  void textureCrop(SDL_Texture* tex, const Rect& box,
                   Color8 tint = Color8(255, 255, 255, 255));
  // 透视四边形（3D 封面流）：xy 为四角屏幕坐标（左上/右上/右下/左下），
  // uv 为对应纹理坐标（0..1）。SDL ≥ 2.0.18 走 SDL_RenderGeometry，
  // 旧版退化为普通缩放拷贝。
  void textureQuad(SDL_Texture* tex, const float xy[8], const float uv[8], Color8 tint);

  // —— 裁剪栈 ——
  void pushClip(const Rect& rc);
  void popClip();

 private:
  std::vector<SDL_Rect> clipStack_;
  SDL_Color toSdl(Color8 c) const { return SDL_Color{c.r, c.g, c.b, c.a}; }
};

}  // namespace ns
