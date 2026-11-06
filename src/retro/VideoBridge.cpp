#include "retro/VideoBridge.h"

#include <cstring>

#include <stb_image_write.h>

#include "util/Log.h"

namespace ns {

VideoBridge::VideoBridge() = default;
VideoBridge::~VideoBridge() {
  if (tex_) SDL_DestroyTexture(tex_);
  if (scanlineTex_) SDL_DestroyTexture(scanlineTex_);
  if (crtTex_) SDL_DestroyTexture(crtTex_);
}

void VideoBridge::setRenderer(SDL_Renderer*) {}

void VideoBridge::setPixelFormat(enum retro_pixel_format fmt) { fmt_ = fmt; }

void VideoBridge::setAvInfo(unsigned baseW, unsigned baseH, double aspect, unsigned maxW,
                            unsigned maxH) {
  baseW_ = baseW ? baseW : 256;
  baseH_ = baseH ? baseH : 240;
  maxW_ = maxW ? maxW : baseW_;
  maxH_ = maxH ? maxH : baseH_;
  aspect_ = aspect > 0.01 ? aspect : (double)baseW_ / (double)baseH_;
}

void VideoBridge::setGeometry(unsigned w, unsigned h, double aspect) {
  if (w && h) {
    baseW_ = w;
    baseH_ = h;
  }
  if (aspect > 0.01) aspect_ = aspect;
}

void VideoBridge::frame(const void* data, unsigned width, unsigned height, size_t pitch) {
  if (!data || !width || !height) return;
  if (width > 8192 || height > 8192) return;
  frameW_ = width;
  frameH_ = height;
  hasFrame_ = true;
  pendingSrc_ = data;
  pendingPitch_ = pitch;
  dirty_ = true;
}

void VideoBridge::ensureTexture(SDL_Renderer* r) {
  if (tex_ && texW_ >= frameW_ && texH_ >= frameH_) return;
  if (tex_) SDL_DestroyTexture(tex_);
  texW_ = 1;
  texH_ = 1;
  while (texW_ < frameW_) texW_ <<= 1;
  while (texH_ < frameH_) texH_ <<= 1;
  tex_ = SDL_CreateTexture(r, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, texW_,
                           texH_);
  if (!tex_) LOGW("创建游戏纹理失败：%s", SDL_GetError());
}

void VideoBridge::draw(SDL_Renderer* r, const Rect& dst, ScalingMode mode, ScreenEffect fx,
                       DualLayout dual, float smooth) {
  if (!r) return;
  ensureTexture(r);
  if (!tex_) return;

  // —— 帧上传（若本帧有新数据）——
  if (dirty_ && pendingSrc_) {
    lastFrame_.resize((size_t)frameW_ * frameH_);
    void* pixels = nullptr;
    int pitch2 = 0;
    if (SDL_LockTexture(tex_, nullptr, &pixels, &pitch2) == 0) {
      const uint8_t* src = (const uint8_t*)pendingSrc_;
      const size_t srcPitch = pendingPitch_;
      uint32_t* dstRow = (uint32_t*)pixels;
      const int dstPitch = pitch2 / (int)sizeof(uint32_t);
      switch (fmt_) {
        case RETRO_PIXEL_FORMAT_XRGB8888: {
          const uint32_t* s = (const uint32_t*)src;
          for (unsigned y = 0; y < frameH_; y++) {
            std::memcpy(dstRow + (size_t)y * dstPitch, s + (size_t)y * (srcPitch / 4),
                        (size_t)frameW_ * 4);
          }
          break;
        }
        case RETRO_PIXEL_FORMAT_RGB565: {
          const uint16_t* s = (const uint16_t*)src;
          for (unsigned y = 0; y < frameH_; y++) {
            const uint16_t* row = s + (size_t)y * (srcPitch / 2);
            uint32_t* out = dstRow + (size_t)y * dstPitch;
            for (unsigned x = 0; x < frameW_; x++) {
              const uint16_t c = row[x];
              const uint8_t r5 = (uint8_t)((c >> 11) & 0x1F);
              const uint8_t g6 = (uint8_t)((c >> 5) & 0x3F);
              const uint8_t b5 = (uint8_t)(c & 0x1F);
              out[x] = 0xFF000000u | ((uint32_t)(r5 << 3 | r5 >> 2) << 16) |
                       ((uint32_t)(g6 << 2 | g6 >> 4) << 8) | (uint32_t)(b5 << 3 | b5 >> 2);
            }
          }
          break;
        }
        case RETRO_PIXEL_FORMAT_0RGB1555:
        default: {
          const uint16_t* s = (const uint16_t*)src;
          for (unsigned y = 0; y < frameH_; y++) {
            const uint16_t* row = s + (size_t)y * (srcPitch / 2);
            uint32_t* out = dstRow + (size_t)y * dstPitch;
            for (unsigned x = 0; x < frameW_; x++) {
              const uint16_t c = row[x];
              const uint8_t r5 = (uint8_t)((c >> 10) & 0x1F);
              const uint8_t g5 = (uint8_t)((c >> 5) & 0x1F);
              const uint8_t b5 = (uint8_t)(c & 0x1F);
              out[x] = 0xFF000000u | ((uint32_t)(r5 << 3 | r5 >> 2) << 16) |
                       ((uint32_t)(g5 << 3 | g5 >> 2) << 8) | (uint32_t)(b5 << 3 | b5 >> 2);
            }
          }
          break;
        }
      }
      // 同步缓存（截图用）
      for (unsigned y = 0; y < frameH_; y++) {
        std::memcpy(lastFrame_.data() + (size_t)y * frameW_,
                    dstRow + (size_t)y * dstPitch, (size_t)frameW_ * 4);
      }
      SDL_UnlockTexture(tex_);
    } else {
      lastFrame_.clear();
    }
    dirty_ = false;
  }

  SDL_SetTextureScaleMode(tex_, smooth > 0.5f ? SDL_ScaleModeLinear : SDL_ScaleModeNearest);

  // —— 双屏布局 ——
  if (dual_ && frameH_ >= frameW_ * 2) {
    // 堆叠双屏（NDS 256x384 / 3DS 400x480 等）
    SDL_Rect top{0, 0, (int)frameW_, (int)(frameH_ / 2)};
    SDL_Rect bottom{0, (int)(frameH_ / 2), (int)frameW_, (int)(frameH_ - frameH_ / 2)};
    switch (dual) {
      case DualLayout::TopOnly: {
        const float a = (float)frameW_ / (float)(frameH_ / 2);
        drawScreenPart(r, tex_, &top, dst, mode, fx, frameW_, frameH_ / 2, a, smooth);
        break;
      }
      case DualLayout::BottomOnly: {
        const float a = (float)frameW_ / (float)(frameH_ - frameH_ / 2);
        drawScreenPart(r, tex_, &bottom, dst, mode, fx, frameW_, frameH_ - frameH_ / 2, a,
                       smooth);
        break;
      }
      case DualLayout::Both:
      default: {
        // 堆叠绘制（保持总宽高比）
        drawScreenPart(r, tex_, nullptr, dst, mode, fx, frameW_, frameH_,
                       (float)frameW_ / (float)frameH_, smooth);
        break;
      }
    }
    return;
  }

  drawScreenPart(r, tex_, nullptr, dst, mode, fx, frameW_, frameH_, (float)aspect_, smooth);
}

void VideoBridge::drawScreenPart(SDL_Renderer* r, SDL_Texture* tex, const SDL_Rect* src,
                                 const Rect& dst, ScalingMode mode, ScreenEffect fx,
                                 unsigned texW, unsigned texH, float gameAspect,
                                 float smooth) {
  (void)smooth;
  Rect target = dst;
  // 源区域宽高比优先
  if (src) {
    gameAspect = (float)src->w / (float)src->h;
  }

  switch (mode) {
    case ScalingMode::Stretch:
      break;  // 直接填满
    case ScalingMode::KeepAspect:
      target = dst.withAspectFit(gameAspect);
      break;
    case ScalingMode::IntegerScale: {
      if (texW > 0 && texH > 0 && dst.w > 0 && dst.h > 0) {
        // ★v1.2.7：整数倍数按【设备像素】计算（dst 是设计单位，渲染时还会
        //   乘 UI 缩放；若只按设计单位取整，150%/200% 缩放下整数缩放不再
        //   像素完美）。倍数确定后再折回设计单位定位。
        const float devScale = uiscale::get();
        const int sx = (int)((dst.w * devScale) / (float)texW);
        const int sy = (int)((dst.h * devScale) / (float)texH);
        if (sx < 1 && sy < 1) {
          target = dst.withAspectFit(gameAspect);
        } else {
          const int s = sx < 1 ? sy : (sy < 1 ? sx : (sx < sy ? sx : sy));
          const float w = texW * (float)s / devScale;
          const float h = texH * (float)s / devScale;
          target = Rect(dst.cx() - w / 2, dst.cy() - h / 2, w, h);
        }
      }
      break;
    }
  }

  const SDL_Rect d{(int)target.x, (int)target.y, (int)target.w, (int)target.h};
  SDL_RenderCopy(r, tex, src, &d);

  // —— 叠加特效 ——
  if (fx != ScreenEffect::None) {
    ensureEffectTextures(r);
    SDL_Texture* overlay = fx == ScreenEffect::Scanlines ? scanlineTex_ : crtTex_;
    if (overlay) {
      SDL_SetTextureBlendMode(overlay, SDL_BLENDMODE_BLEND);
      SDL_RenderCopy(r, overlay, nullptr, &d);
    }
  }
}

void VideoBridge::ensureEffectTextures(SDL_Renderer* r) {
  if (scanlineTex_ && crtTex_) return;
  const int W = 512, H = 512;
  std::vector<uint32_t> scan((size_t)W * H, 0);
  std::vector<uint32_t> crt((size_t)W * H, 0);
  for (int y = 0; y < H; y++) {
    const float fy = (float)y / H;
    for (int x = 0; x < W; x++) {
      const int idx = y * W + x;
      // 扫描线：每两行一条半透明暗线（其余全透明）
      scan[idx] = (y % 4 < 2) ? 0x00000000u : 0x5A000000u;
      // CRT 荫罩：RGB 三色竖条暗化 + 四角暗角（均以黑色半透明叠加）
      const float fx = (float)x / W;
      const float dx = fx - 0.5f, dy = fy - 0.5f;
      const float dist = dx * dx + dy * dy;          // 0 .. 0.5
      const float vig = dist * 0.65f;                // 暗角强度
      const int m = x % 3;                            // 荫罩子像素
      float dark = 0.06f + vig;
      if (m != (y % 3)) dark += 0.10f;               // 错位荫罩条纹
      if (y % 3 == 2) dark += 0.08f;                 // 水平暗线
      const uint8_t a = (uint8_t)(dark * 255.0f);
      crt[idx] = ((uint32_t)a << 24);                // 黑色 + 变量 alpha
    }
  }
  scanlineTex_ = SDL_CreateTexture(r, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STATIC, W, H);
  if (scanlineTex_) {
    SDL_SetTextureBlendMode(scanlineTex_, SDL_BLENDMODE_BLEND);
    SDL_UpdateTexture(scanlineTex_, nullptr, scan.data(), W * 4);
  }
  crtTex_ = SDL_CreateTexture(r, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STATIC, W, H);
  if (crtTex_) {
    SDL_SetTextureBlendMode(crtTex_, SDL_BLENDMODE_BLEND);
    SDL_UpdateTexture(crtTex_, nullptr, crt.data(), W * 4);
  }
}

bool VideoBridge::screenshot(const std::string& pngPath) {
  if (!hasFrame_ || !tex_) return false;
  // 读回纹理（走渲染目标不安全，直接用最近一帧的原始数据不可靠 —— 用
  // SDL_RenderReadPixels 需要画面在后台缓冲区。这里改为缓存最近 ARGB 帧）
  if (lastFrame_.empty()) return false;
  const int stride = (int)frameW_ * 4;
  return stbi_write_png(pngPath.c_str(), (int)frameW_, (int)frameH_, 4, lastFrame_.data(),
                        stride) != 0;
}

}  // namespace ns
