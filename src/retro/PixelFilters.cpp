// NesStation PC — 像素滤镜引擎实现（★v1.2.8）
//
// 2xBR / 4xBR 逐行移植自 Android 版 core/jni/shared/core_shared.h
//（Hyllian 2xBR v3.3a，RetroArch 移植版；int32 混合宏防环绕修复原样保留）。
// HQ2X / HQ4X 调用 third_party/hqx（Maxim Stepin 原版，_rb 行距变体）。
#include "retro/PixelFilters.h"

#include <cmath>

extern "C" {
#include <hqx.h>
}

namespace ns {
namespace pixelfilters {

// ---------------------------------------------------------------------------
// 2xBR — Hyllian's 2xBR v3.3a (adapted from RetroArch)
//（移植自 APK core_shared.h:326-505，宏与数值一字未改）
// ---------------------------------------------------------------------------

static constexpr uint32_t XBR_RED_MASK   = 0x00FF0000;
static constexpr uint32_t XBR_GREEN_MASK = 0x0000FF00;
static constexpr uint32_t XBR_BLUE_MASK  = 0x000000FF;
static constexpr uint32_t XBR_LBMASK     = 0xFEFEFEFE;
static constexpr uint32_t XBR_ALPHA_MASK = 0xFF000000;

static inline float xbrDf8(uint32_t A, uint32_t B) {
  int r = abs((int)((A & XBR_RED_MASK)   >> 16) - (int)((B & XBR_RED_MASK)   >> 16));
  int g = abs((int)((A & XBR_GREEN_MASK) >>  8) - (int)((B & XBR_GREEN_MASK) >>  8));
  int b = abs((int)((A & XBR_BLUE_MASK)        ) - (int)((B & XBR_BLUE_MASK)       ));
  float y = fabsf(0.299f*r + 0.587f*g + 0.114f*b);
  float u = fabsf(-0.169f*r - 0.331f*g + 0.500f*b);
  float v = fabsf(0.500f*r - 0.419f*g - 0.081f*b);
  return 48.0f*y + 7.0f*u + 6.0f*v;
}

static inline int xbrEq8(uint32_t A, uint32_t B) {
  int r = abs((int)((A & XBR_RED_MASK)   >> 16) - (int)((B & XBR_RED_MASK)   >> 16));
  int g = abs((int)((A & XBR_GREEN_MASK) >>  8) - (int)((B & XBR_GREEN_MASK) >>  8));
  int b = abs((int)((A & XBR_BLUE_MASK)        ) - (int)((B & XBR_BLUE_MASK)       ));
  float y = fabsf(0.299f*r + 0.587f*g + 0.114f*b);
  float u = fabsf(-0.169f*r - 0.331f*g + 0.500f*b);
  float v = fabsf(0.500f*r - 0.419f*g - 0.081f*b);
  return (y <= 48.0f && u <= 7.0f && v <= 6.0f) ? 1 : 0;
}

#define XBR_BLEND_128(dst, src) \
  dst = (((src & XBR_LBMASK) >> 1) + ((dst & XBR_LBMASK) >> 1)) | XBR_ALPHA_MASK

// CRITICAL: All blend macros cast to int32_t before subtraction.
// Without this, (src & MASK) - (dst & MASK) is unsigned and wraps to a
// huge value when src < dst, causing corrupted pixels (scattered dots).
// The int32_t cast makes the subtraction signed, and >> on signed values
// is an arithmetic shift that preserves the sign, giving correct blending.
#define XBR_BLEND_32(dst, src) \
  dst = ( \
      (XBR_RED_MASK & (uint32_t)((int32_t)(dst & XBR_RED_MASK) + \
          (((int32_t)(src & XBR_RED_MASK) - (int32_t)(dst & XBR_RED_MASK)) >> 3))) | \
      (XBR_GREEN_MASK & (uint32_t)((int32_t)(dst & XBR_GREEN_MASK) + \
          (((int32_t)(src & XBR_GREEN_MASK) - (int32_t)(dst & XBR_GREEN_MASK)) >> 3))) | \
      (XBR_BLUE_MASK & (uint32_t)((int32_t)(dst & XBR_BLUE_MASK) + \
          (((int32_t)(src & XBR_BLUE_MASK) - (int32_t)(dst & XBR_BLUE_MASK)) >> 3))) ) + \
          XBR_ALPHA_MASK

#define XBR_BLEND_64(dst, src) \
  dst = ( \
      (XBR_RED_MASK & (uint32_t)((int32_t)(dst & XBR_RED_MASK) + \
          (((int32_t)(src & XBR_RED_MASK) - (int32_t)(dst & XBR_RED_MASK)) >> 2))) | \
      (XBR_GREEN_MASK & (uint32_t)((int32_t)(dst & XBR_GREEN_MASK) + \
          (((int32_t)(src & XBR_GREEN_MASK) - (int32_t)(dst & XBR_GREEN_MASK)) >> 2))) | \
      (XBR_BLUE_MASK & (uint32_t)((int32_t)(dst & XBR_BLUE_MASK) + \
          (((int32_t)(src & XBR_BLUE_MASK) - (int32_t)(dst & XBR_BLUE_MASK)) >> 2))) ) + \
          XBR_ALPHA_MASK

#define XBR_BLEND_192(dst, src) \
  dst = ( \
      (XBR_RED_MASK & (uint32_t)((int32_t)(dst & XBR_RED_MASK) + \
          (((int32_t)(src & XBR_RED_MASK) - (int32_t)(dst & XBR_RED_MASK)) * 192 >> 8))) | \
      (XBR_GREEN_MASK & (uint32_t)((int32_t)(dst & XBR_GREEN_MASK) + \
          (((int32_t)(src & XBR_GREEN_MASK) - (int32_t)(dst & XBR_GREEN_MASK)) * 192 >> 8))) | \
      (XBR_BLUE_MASK & (uint32_t)((int32_t)(dst & XBR_BLUE_MASK) + \
          (((int32_t)(src & XBR_BLUE_MASK) - (int32_t)(dst & XBR_BLUE_MASK)) * 192 >> 8))) ) + \
          XBR_ALPHA_MASK

#define XBR_BLEND_224(dst, src) \
  dst = ( \
      (XBR_RED_MASK & (uint32_t)((int32_t)(dst & XBR_RED_MASK) + \
          (((int32_t)(src & XBR_RED_MASK) - (int32_t)(dst & XBR_RED_MASK)) * 224 >> 8))) | \
      (XBR_GREEN_MASK & (uint32_t)((int32_t)(dst & XBR_GREEN_MASK) + \
          (((int32_t)(src & XBR_GREEN_MASK) - (int32_t)(dst & XBR_GREEN_MASK)) * 224 >> 8))) | \
      (XBR_BLUE_MASK & (uint32_t)((int32_t)(dst & XBR_BLUE_MASK) + \
          (((int32_t)(src & XBR_BLUE_MASK) - (int32_t)(dst & XBR_BLUE_MASK)) * 224 >> 8))) ) + \
          XBR_ALPHA_MASK

#define XBR_LEFT_UP(N3, N2, N1, px) \
  XBR_BLEND_224(E[N3], px); \
  XBR_BLEND_64(E[N2], px);  \
  E[N1] = E[N2];

#define XBR_LEFT(N3, N2, px) \
  XBR_BLEND_192(E[N3], px); \
  XBR_BLEND_64(E[N2], px);

#define XBR_UP(N3, N1, px) \
  XBR_BLEND_192(E[N3], px); \
  XBR_BLEND_64(E[N1], px);

#define XBR_DIA(N3, px) \
  XBR_BLEND_128(E[N3], px);

#define XBR_FILTRO(PE, _PI, PH, PF, PG, PC, PD, PB, PA, \
    G5, C4, G0, D0, C1, B1, F4, I4, H5, I5, A0, A1, N0, N1, N2, N3) \
  ex = (PE != PH && PE != PF); \
  if (ex) { \
    e = (xbrDf8(PE,PC) + xbrDf8(PE,PG) + xbrDf8(_PI,H5) + xbrDf8(_PI,F4)) \
         + (4.0f * xbrDf8(PH,PF)); \
    i = (xbrDf8(PH,PD) + xbrDf8(PH,I5) + xbrDf8(PF,I4) + xbrDf8(PF,PB)) \
         + (4.0f * xbrDf8(PE,_PI)); \
    if ((e < i) && ( \
        (!xbrEq8(PF,PB) && !xbrEq8(PF,PC)) || \
        (!xbrEq8(PH,PD) && !xbrEq8(PH,PG)) || \
        (xbrEq8(PE,_PI) && ((!xbrEq8(PF,F4) && !xbrEq8(PF,I4)) || \
                            (!xbrEq8(PH,H5) && !xbrEq8(PH,I5)))) || \
        xbrEq8(PE,PG) || xbrEq8(PE,PC))) \
    { \
      ke = xbrDf8(PF,PG); \
      ki = xbrDf8(PH,PC); \
      ex2 = (PE != PC && PB != PC); \
      ex3 = (PE != PG && PD != PG); \
      px = (xbrDf8(PE,PF) <= xbrDf8(PE,PH)) ? PF : PH; \
      if ((ke*2 <= ki) && ex3 && (ke >= ki*2) && ex2) { \
        XBR_LEFT_UP(N3, N2, N1, px) \
      } else if ((ke*2 <= ki) && ex3) { \
        XBR_LEFT(N3, N2, px); \
      } else if ((ke >= ki*2) && ex2) { \
        XBR_UP(N3, N1, px); \
      } else { \
        XBR_DIA(N3, px); \
      } \
    } else if (e <= i) { \
      XBR_BLEND_128(E[N3], \
          (xbrDf8(PE,PF) <= xbrDf8(PE,PH)) ? PF : PH); \
    } \
  }

// 2xBR upscale: src(w x h) -> dst(2w x 2h)
void xbr2xUpscale(const uint32_t* src, unsigned sw, unsigned sh, size_t srcStride,
                  uint32_t* dst) {
  const unsigned dw = sw * 2;
  const unsigned dstStride = dw;

  auto getPx = [&](int x, int y) -> uint32_t {
    if (x < 0) x = 0; else if (x >= (int)sw) x = sw - 1;
    if (y < 0) y = 0; else if (y >= (int)sh) y = sh - 1;
    return src[y * srcStride + x];
  };

  for (unsigned y = 0; y < sh; ++y) {
    for (unsigned x = 0; x < sw; ++x) {
      uint32_t A1=getPx(x-1,y-2), B1=getPx(x,y-2), C1=getPx(x+1,y-2);
      uint32_t A0=getPx(x-2,y-1), PA=getPx(x-1,y-1), PB=getPx(x,y-1), PC=getPx(x+1,y-1), C4=getPx(x+2,y-1);
      uint32_t D0=getPx(x-2,y),   PD=getPx(x-1,y),   PE=getPx(x,y),   PF=getPx(x+1,y),   F4=getPx(x+2,y);
      uint32_t G0=getPx(x-2,y+1), PG=getPx(x-1,y+1), PH=getPx(x,y+1), _PI=getPx(x+1,y+1), I4=getPx(x+2,y+1);
      uint32_t G5=getPx(x-1,y+2), H5=getPx(x,y+2), I5=getPx(x+1,y+2);

      uint32_t E[4];
      float e, i, ke, ki;
      uint32_t ex, ex2, ex3, px;

      E[0] = E[1] = E[2] = E[3] = PE;

      XBR_FILTRO(PE, _PI, PH, PF, PG, PC, PD, PB, PA,
          G5, C4, G0, D0, C1, B1, F4, I4, H5, I5, A0, A1, 0, 1, 2, 3);
      XBR_FILTRO(PE, PC, PF, PB, _PI, PA, PH, PD, PG,
          I4, A1, I5, H5, A0, D0, B1, C1, F4, C4, G5, G0, 2, 0, 3, 1);
      XBR_FILTRO(PE, PA, PB, PD, PC, PG, PF, PH, _PI,
          C1, G0, C4, F4, G5, H5, D0, A0, B1, A1, I4, I5, 3, 2, 1, 0);
      XBR_FILTRO(PE, PG, PD, PH, PA, _PI, PB, PF, PC,
          A0, I5, A1, B1, I4, F4, H5, G5, D0, G0, C1, C4, 1, 3, 0, 2);

      const unsigned x2 = x * 2;
      const unsigned y2 = y * 2;
      dst[y2 * dstStride + x2]       = E[0];
      dst[y2 * dstStride + x2 + 1]   = E[1];
      dst[(y2+1) * dstStride + x2]   = E[2];
      dst[(y2+1) * dstStride + x2+1] = E[3];
    }
  }
}

// 4xBR cascade: two passes of 2xBR (w x h -> 2w x 2h -> 4w x 4h)
void xbr4xUpscale(const uint32_t* src, unsigned sw, unsigned sh, size_t srcStride,
                  uint32_t* dst, uint32_t* midBuffer) {
  xbr2xUpscale(src, sw, sh, srcStride, midBuffer);
  xbr2xUpscale(midBuffer, sw * 2, sh * 2, sw * 2, dst);
}

// ---------------------------------------------------------------------------
// 滤镜字符串 / 编号映射（APK 语义）
// ---------------------------------------------------------------------------

bool filterFromString(const std::string& s, Filter* out) {
  if (s == "none") { *out = None; return true; }
  if (s == "scanline") { *out = Scanline; return true; }
  if (s == "crt") { *out = Crt; return true; }
  if (s == "dot") { *out = Dot; return true; }
  if (s == "xbr") { *out = Xbr2x; return true; }
  if (s == "hq2x") { *out = Hq2x; return true; }
  if (s == "hq4x") { *out = Hq4x; return true; }
  if (s == "xbr_dot") { *out = Xbr2xDot; return true; }
  if (s == "4xbr") { *out = Xbr4x; return true; }
  if (s == "4xbr_dot") { *out = Xbr4xDot; return true; }
  if (s == "hq4x_dot") { *out = Hq4xDot; return true; }
  return false;
}

const char* filterToString(Filter f) {
  switch (f) {
    case Scanline: return "scanline";
    case Crt: return "crt";
    case Dot: return "dot";
    case Xbr2x: return "xbr";
    case Hq2x: return "hq2x";
    case Hq4x: return "hq4x";
    case Xbr2xDot: return "xbr_dot";
    case Xbr4x: return "4xbr";
    case Xbr4xDot: return "4xbr_dot";
    case Hq4xDot: return "hq4x_dot";
    case None:
    default: return "none";
  }
}

// 设置页映射（APK SettingsScreen.kt:126-176 —— 真实编号）
Filter filterIntFromSettings(const std::string& s) {
  Filter f = None;
  if (filterFromString(s, &f)) return f;
  return None;
}

// 游戏内面板映射（APK EmulatorScreen.kt:2890-2910，3.5.2 实测分支行为：
// xbr 族换 HQ2X/HQ4X —— 原生 2xBR 硬边缘色渗；叠加类照旧交给视图层）
Filter filterIntFromGamePanel(const std::string& s) {
  if (s == "xbr" || s == "hq2x" || s == "xbr_dot") return Hq2x;
  if (s == "4xbr" || s == "hq4x" || s == "4xbr_dot") return Hq4x;
  if (s == "hq4x_dot") return Hq4xDot;
  Filter f = None;
  if (filterFromString(s, &f)) return f;
  return None;
}

Filter overlayTypeFor(const std::string& s) {
  if (s == "scanline") return Scanline;
  if (s == "crt") return Crt;
  if (s == "dot") return Dot;
  // 后缀组合（APK overlayFilterTypeFor）：*_dot → dot
  if (s.size() > 4 && s.compare(s.size() - 4, 4, "_dot") == 0) return Dot;
  return None;  // none / 纯放大类（xbr/hq2x/hq4x/4xbr）无叠加
}

// ---------------------------------------------------------------------------
// 帧级放大调度（APK applyFilterAndBlit 的放大部分；blit 交还调用方）
// ---------------------------------------------------------------------------

// 复用缓冲（模拟线程每帧调用，避免反复分配；最大 4× 预算帧尺寸）
static std::vector<uint32_t> g_buf2x, g_buf4x, g_bufMid;

const uint32_t* applyUpscale(Filter f, const uint32_t* src, unsigned w, unsigned h,
                             size_t stride, unsigned budgetW, unsigned budgetH,
                             unsigned* outW, unsigned* outH) {
  *outW = w;
  *outH = h;
  if (!src || !w || !h) return nullptr;

  // 滤镜预算（APK canUpscale：帧超过预算静默跳过）
  const bool canUpscale = (w <= budgetW && h <= budgetH);
  if (!canUpscale) return nullptr;

  try {
    switch (f) {
      case Xbr2x:      // xbr（设置页真实编号）→ 2xBR 2×
      case Xbr2xDot: { // xbr+dot → 2xBR 2× + dot 叠加（叠加由视图层画）
        g_buf2x.assign((size_t)w * 2 * h * 2, 0);
        xbr2xUpscale(src, w, h, stride, g_buf2x.data());
        *outW = w * 2; *outH = h * 2;
        return g_buf2x.data();
      }
      case Xbr4x:      // 4xbr → 4xBR 4×
      case Xbr4xDot: { // 4xbr+dot → 4xBR 4× + dot 叠加
        g_buf4x.assign((size_t)w * 4 * h * 4, 0);
        g_bufMid.assign((size_t)w * 2 * h * 2, 0);
        xbr4xUpscale(src, w, h, stride, g_buf4x.data(), g_bufMid.data());
        *outW = w * 4; *outH = h * 4;
        return g_buf4x.data();
      }
      case Hq2x: {     // hq2x（及游戏内 xbr 映射）→ HQ2X 2×
        g_buf2x.assign((size_t)w * 2 * h * 2, 0);
        hq2x_32_rb(src, (uint32_t)(stride * sizeof(uint32_t)),
                   g_buf2x.data(), (uint32_t)(w * 2 * sizeof(uint32_t)),
                   (int)w, (int)h);
        *outW = w * 2; *outH = h * 2;
        return g_buf2x.data();
      }
      case Hq4x:       // hq4x（及游戏内 4xbr 映射）
      case Hq4xDot: {  // hq4x+dot → HQ4X 4× + dot 叠加
        g_buf4x.assign((size_t)w * 4 * h * 4, 0);
        hq4x_32_rb(src, (uint32_t)(stride * sizeof(uint32_t)),
                   g_buf4x.data(), (uint32_t)(w * 4 * sizeof(uint32_t)),
                   (int)w, (int)h);
        *outW = w * 4; *outH = h * 4;
        return g_buf4x.data();
      }
      default:
        return nullptr;  // 无放大需求（none/叠加类）
    }
  } catch (...) {
    return nullptr;  // 内存不足等：静默直通（APK 同款降级语义）
  }
}

// ---------------------------------------------------------------------------
// 叠加图案（APK NdsFilterPatterns.kt 逐参数）
// ---------------------------------------------------------------------------

std::vector<uint32_t> makeScanlinePattern(int* w, int* h) {
  // Scanline pattern: 2px wide, 4px tall — 3 transparent rows + 1 dark row.
  *w = 2; *h = 4;
  std::vector<uint32_t> bmp(2 * 4);
  for (int x = 0; x <= 1; x++) {
    bmp[x + 0 * 2] = 0x00000000u;                     // 透明
    bmp[x + 1 * 2] = 0x00000000u;                     // 透明
    bmp[x + 2 * 2] = 0x00000000u;                     // 透明
    bmp[x + 3 * 2] = 0x8C000000u;                     // 55% 黑（0x8C=140）
  }
  return bmp;
}

std::vector<uint32_t> makeCrtPattern(int* w, int* h) {
  // CRT pattern: 3px RGB phosphor triads, 6px tall, bottom row = scanline.
  *w = 3; *h = 6;
  std::vector<uint32_t> bmp(3 * 6);
  for (int y = 0; y <= 4; y++) {
    bmp[0 + y * 3] = 0x26FF0000u;  // red phosphor（α≈15%）
    bmp[1 + y * 3] = 0x2600FF00u;  // green phosphor
    bmp[2 + y * 3] = 0x260000FFu;  // blue phosphor
  }
  for (int x = 0; x <= 2; x++) {
    bmp[x + 5 * 3] = 0x80000000u;  // 50% 黑扫描线
  }
  return bmp;
}

std::vector<uint32_t> makeDotPattern(int* w, int* h) {
  // Dot pattern: LCD dot matrix with smoothstep circular alpha.
  const int size = 4;
  *w = size; *h = size;
  std::vector<uint32_t> bmp(size * size);
  const float center = (size - 1) / 2.0f;             // = 1.5
  const float dotRadius = 1.0f;
  const float maxDist = std::sqrt(center * center + center * center);  // ≈2.121
  for (int y = 0; y < size; y++) {
    for (int x = 0; x < size; x++) {
      const float dist = std::sqrt((x - center) * (x - center) +
                                   (y - center) * (y - center));
      float t = (dist - dotRadius) / (maxDist - dotRadius);
      if (t < 0) t = 0;
      if (t > 1) t = 1;
      const float smoothT = t * t * (3 - 2 * t);      // smoothstep
      int alpha = (int)(smoothT * 128.0f);            // 最大 50% 黑
      if (alpha < 0) alpha = 0;
      if (alpha > 255) alpha = 255;
      bmp[x + y * size] = ((uint32_t)alpha << 24);
    }
  }
  return bmp;
}

std::vector<uint32_t> makeCrtVignette(int* w, int* h) {
  // CRT 径向暗角（APK FilterOverlay crt 分支：Brush.radialGradient(
  //   Transparent → Black.copy(alpha=0.35f)，半径 min(w,h)*0.7f，全程渐变)。
  // 生成 256×256 α 图，绘制时拉伸到目标矩形。
  const int S = 256;
  *w = S; *h = S;
  std::vector<uint32_t> bmp((size_t)S * S);
  for (int y = 0; y < S; y++) {
    for (int x = 0; x < S; x++) {
      // 像素中心归一化 [-1,1]²，半径 0（中心）→ √2（角）；
      // 0.35 黑的全程渐变 = α = clamp(dist/√2 × 0.35) × 255
      const float nx = (x + 0.5f) / S * 2.0f - 1.0f;
      const float ny = (y + 0.5f) / S * 2.0f - 1.0f;
      const float dist = std::sqrt(nx * nx + ny * ny) / std::sqrt(2.0f);
      float a = dist * 0.35f;
      if (a < 0) a = 0;
      if (a > 0.35f) a = 0.35f;
      bmp[x + y * S] = ((uint32_t)(a * 255.0f + 0.5f) << 24);
    }
  }
  return bmp;
}

}  // namespace pixelfilters
}  // namespace ns
