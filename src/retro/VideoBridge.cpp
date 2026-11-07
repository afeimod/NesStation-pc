#include "retro/VideoBridge.h"

#include <algorithm>
#include <cstring>

#include <stb_image_write.h>

#include "util/Log.h"

namespace ns {

VideoBridge::VideoBridge() = default;
VideoBridge::~VideoBridge() {
  if (tex_) SDL_DestroyTexture(tex_);
  if (scanlineTex_) SDL_DestroyTexture(scanlineTex_);
  if (crtTex_) SDL_DestroyTexture(crtTex_);
  if (dotTex_) SDL_DestroyTexture(dotTex_);
  if (vignetteTex_) SDL_DestroyTexture(vignetteTex_);
}

void VideoBridge::setRenderer(SDL_Renderer*) {}

void VideoBridge::beginSession() {
  // 像素格式回默认（核心声明时经 SET_PIXEL_FORMAT 覆盖）
  fmt_ = RETRO_PIXEL_FORMAT_0RGB1555;
  pending_.clear();
  pendingW_ = pendingH_ = 0;
  pendingPitch_ = 0;
  dirty_ = false;
  hasFrame_ = false;
  frameW_ = frameH_ = 0;
}

void VideoBridge::setPixelFormat(enum retro_pixel_format fmt) {
  fmt_ = fmt;
  const char* name = fmt == RETRO_PIXEL_FORMAT_XRGB8888 ? "XRGB8888"
                    : fmt == RETRO_PIXEL_FORMAT_RGB565    ? "RGB565"
                                                           : "0RGB1555";
  LOGI("核心像素格式：%s", name);
}

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
  // ★v1.3.0 帧到达即拷贝到自有缓冲（与核心内部缓冲完全解耦，
  //   渲染期间无论核心怎么写都不会读到半帧 —— 割裂闪烁根治）
  const size_t need = (size_t)pitch * height;
  if (pending_.size() < need) {
    pending_.resize(need);
    pendingPitch_ = pitch;
  } else {
    pendingPitch_ = pitch;
  }
  std::memcpy(pending_.data(), data, need);
  pendingW_ = width;
  pendingH_ = height;
  frameW_ = width;
  frameH_ = height;
  hasFrame_ = true;
  dirty_ = true;
}

// 纹理按【最终上传尺寸】（放大后）创建/扩容：★修复堆溢出 —— 旧序先建
// 纹理（原始帧尺寸）再做放大，HQ4X 后 1024×960 写进 256×256 纹理越界
void VideoBridge::ensureTexture(SDL_Renderer* r, unsigned needW, unsigned needH) {
  if (tex_ && texW_ >= needW && texH_ >= needH) return;
  if (tex_) SDL_DestroyTexture(tex_);
  texW_ = 1;
  texH_ = 1;
  while (texW_ < needW) texW_ <<= 1;
  while (texH_ < needH) texH_ <<= 1;
  tex_ = SDL_CreateTexture(r, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, texW_,
                           texH_);
  if (!tex_) LOGW("创建游戏纹理失败：%s", SDL_GetError());
}

// —— 帧上传：格式转换 →（放大型滤镜 CPU 逐像素放大）→ 纹理 ——
// 放大调度对齐 APK applyFilterAndBlit（core_shared.h:517-549）：
//   预算 = 核心 AV maxW×maxH（再钳 1024×1024 防大分辨率帧卡死模拟线程）；
//   超预算 / 内存不足 → 静默直通（原帧）。
void VideoBridge::uploadFrame(SDL_Renderer* r) {
  if (!dirty_ || pending_.empty()) return;

  const unsigned srcW = pendingW_;
  const unsigned srcH = pendingH_;
  // 先转 ARGB 到 lastFrame_（格式归一，放大算法输入 0xAARRGGBB）
  lastFrame_.resize((size_t)srcW * srcH);
  {
    const uint8_t* src = pending_.data();
    const size_t srcPitch = pendingPitch_;
    uint32_t* out = lastFrame_.data();
    switch (fmt_) {
      case RETRO_PIXEL_FORMAT_XRGB8888: {
        const uint32_t* s = (const uint32_t*)src;
        for (unsigned y = 0; y < srcH; y++) {
          const uint32_t* row = s + (size_t)y * (srcPitch / 4);
          uint32_t* dst = out + (size_t)y * srcW;
          for (unsigned x = 0; x < srcW; x++) {
            // ★v1.3.0：高位 X 强制为不透明（部分核心写 0x00；
            //   若渲染路径启用混合会变透明/异色）
            dst[x] = row[x] | 0xFF000000u;
          }
        }
        break;
      }
      case RETRO_PIXEL_FORMAT_RGB565: {
        const uint16_t* s = (const uint16_t*)src;
        for (unsigned y = 0; y < srcH; y++) {
          const uint16_t* row = s + (size_t)y * (srcPitch / 2);
          uint32_t* dst = out + (size_t)y * srcW;
          for (unsigned x = 0; x < srcW; x++) {
            const uint16_t c = row[x];
            const uint8_t r5 = (uint8_t)((c >> 11) & 0x1F);
            const uint8_t g6 = (uint8_t)((c >> 5) & 0x3F);
            const uint8_t b5 = (uint8_t)(c & 0x1F);
            dst[x] = 0xFF000000u | ((uint32_t)(r5 << 3 | r5 >> 2) << 16) |
                     ((uint32_t)(g6 << 2 | g6 >> 4) << 8) | (uint32_t)(b5 << 3 | b5 >> 2);
          }
        }
        break;
      }
      case RETRO_PIXEL_FORMAT_0RGB1555:
      default: {
        const uint16_t* s = (const uint16_t*)src;
        for (unsigned y = 0; y < srcH; y++) {
          const uint16_t* row = s + (size_t)y * (srcPitch / 2);
          uint32_t* dst = out + (size_t)y * srcW;
          for (unsigned x = 0; x < srcW; x++) {
            const uint16_t c = row[x];
            const uint8_t r5 = (uint8_t)((c >> 10) & 0x1F);
            const uint8_t g5 = (uint8_t)((c >> 5) & 0x1F);
            const uint8_t b5 = (uint8_t)(c & 0x1F);
            dst[x] = 0xFF000000u | ((uint32_t)(r5 << 3 | r5 >> 2) << 16) |
                     ((uint32_t)(g5 << 3 | g5 >> 2) << 8) | (uint32_t)(b5 << 3 | b5 >> 2);
          }
        }
        break;
      }
    }
  }

  // —— 放大型滤镜（游戏内口径映射，APK EmulatorScreen 运行时行为）——
  activeFilter_ = "none";
  drawnW_ = srcW;
  drawnH_ = srcH;
  const uint32_t* uploadSrc = lastFrame_.data();
  {
    const pixelfilters::Filter f =
        pixelfilters::filterIntFromGamePanel(appFilter_);
    if (pixelfilters::isUpscale(f)) {
      const unsigned budgetW = maxW_ < 1024 ? maxW_ : 1024;
      const unsigned budgetH = maxH_ < 1024 ? maxH_ : 1024;
      unsigned outW = srcW, outH = srcH;
      const uint32_t* up = pixelfilters::applyUpscale(f, lastFrame_.data(), srcW, srcH,
                                                      srcW, budgetW, budgetH, &outW,
                                                      &outH);
      if (up) {
        uploadSrc = up;
        drawnW_ = outW;
        drawnH_ = outH;
        activeFilter_ = pixelfilters::filterToString(f);
      }
    }
  }

  // —— 上传（纹理按【放大后】尺寸创建/扩容，见 ensureTexture 修复说明）——
  ensureTexture(r, drawnW_, drawnH_);
  if (!tex_) return;
  void* pixels = nullptr;
  int pitch2 = 0;
  if (SDL_LockTexture(tex_, nullptr, &pixels, &pitch2) == 0) {
    const int dstPitch = pitch2 / (int)sizeof(uint32_t);
    uint32_t* dstRow = (uint32_t*)pixels;
    for (unsigned y = 0; y < drawnH_; y++)
      std::memcpy(dstRow + (size_t)y * dstPitch, uploadSrc + (size_t)y * drawnW_,
                  (size_t)drawnW_ * 4);
    SDL_UnlockTexture(tex_);
  }
  lastW_ = drawnW_;
  lastH_ = drawnH_;
  dirty_ = false;
  // 滤镜切换日志（变化时一条，便于确认生效与排查性能）
  if (activeFilter_ != loggedFilter_) {
    loggedFilter_ = activeFilter_;
    if (activeFilter_ == "none")
      LOGI("Video filter: none (%ux%u direct)", srcW, srcH);
    else
      LOGI("Video filter: %s (%ux%u → %ux%u)", activeFilter_.c_str(), srcW, srcH,
           drawnW_, drawnH_);
  }
}

void VideoBridge::draw(SDL_Renderer* r, const Rect& dst, const VideoDrawParams& p) {
  if (!r) return;
  if (!hasFrame_) return;

  // 记录全局滤镜字符串供 uploadFrame（帧到达即可生效——热生效，APK 同款）
  appFilter_ = p.videoFilter ? p.videoFilter : "none";
  uploadFrame(r);
  if (!tex_) return;

  SDL_SetTextureScaleMode(tex_, p.smooth > 0.5f ? SDL_ScaleModeLinear
                                                : SDL_ScaleModeNearest);

  // —— 双屏布局（APK：NDS 非 custom 忽略用户所选比例，按布局派生原生比；
  //   EmulatorScreen.kt:7840-7855 effectiveVideoScale）——
  if (dual_ && frameH_ >= frameW_ * 2) {
    // 堆叠双屏（NDS 256x384 / 3DS 400x480 等）—— 源矩形按【放大后】帧取半
    const unsigned fh = drawnH_;
    SDL_Rect top{0, 0, (int)drawnW_, (int)(fh / 2)};
    SDL_Rect bottom{0, (int)(fh / 2), (int)drawnW_, (int)(fh - fh / 2)};
    switch (p.dual) {
      case DualLayout::TopOnly:
        drawScreenPart(r, &top, dst, p, drawnW_, fh / 2);
        break;
      case DualLayout::BottomOnly:
        drawScreenPart(r, &bottom, dst, p, drawnW_, fh - fh / 2);
        break;
      case DualLayout::Both:
      default:
        drawScreenPart(r, nullptr, dst, p, drawnW_, fh);
        break;
    }
    return;
  }

  drawScreenPart(r, nullptr, dst, p, drawnW_, drawnH_);
}

// —— APK GameSurfaceView 比例数学（EmulatorScreen.kt:8599-8619 /
//    4662-4686 复刻公式）——
//   stretch → 填满（可能变形）；4:3 / 3:2 / 8:7 / 16:9 → contain 适配 +
//   双向居中（PC 恒横屏）；custom → 归一化四角矩形直接映射。
static Rect computeGameRect(const Rect& dst, const char* videoScale,
                            const float* customRect, float srcAspect) {
  if (!videoScale) return dst;
  if (strcmp(videoScale, "stretch") == 0) return dst;
  if (strcmp(videoScale, "custom") == 0) {
    if (!customRect) return dst;
    // clamp(round(r*dst), 0, dst)；w/h 至少 1
    const float l = std::min(dst.w, std::max(0.0f, customRect[0] * dst.w));
    const float t = std::min(dst.h, std::max(0.0f, customRect[1] * dst.h));
    const float r = std::min(dst.w, std::max(0.0f, customRect[2] * dst.w));
    const float b = std::min(dst.h, std::max(0.0f, customRect[3] * dst.h));
    float w = r - l, h = b - t;
    if (w < 1) w = 1;
    if (h < 1) h = 1;
    return Rect(dst.x + l, dst.y + t, w, h);
  }
  float aspect = 0;
  if (strcmp(videoScale, "4:3") == 0) aspect = 4.0f / 3.0f;
  else if (strcmp(videoScale, "3:2") == 0) aspect = 3.0f / 2.0f;
  else if (strcmp(videoScale, "8:7") == 0) aspect = 8.0f / 7.0f;
  else if (strcmp(videoScale, "16:9") == 0) aspect = 16.0f / 9.0f;
  if (aspect <= 0) return dst;
  // contain：先宽后高，水平 + 垂直居中（APK Modifier.aspectRatio 语义）
  return dst.withAspectFit(aspect);
}

void VideoBridge::drawScreenPart(SDL_Renderer* r, const SDL_Rect* src, const Rect& dst,
                                 const VideoDrawParams& p, unsigned srcW, unsigned srcH) {
  Rect target = dst;
  // 源区域宽高比（单屏 = 核心 AV 宽高比；双屏 = 源矩形派生）
  float gameAspect = src ? (float)src->w / (float)src->h : (float)aspect_;

  const bool wantCustom = p.videoScale && strcmp(p.videoScale, "custom") == 0;
  if (wantCustom && p.customRect) {
    target = computeGameRect(dst, "custom", p.customRect, gameAspect);
  } else if (!wantCustom) {
    target = computeGameRect(dst, p.videoScale, nullptr, gameAspect);
  }
  // wantCustom 但无矩形（异常配置）→ 保持 dst（拉伸兜底）

  // —— PC 增强：整数缩放（像素完美）——
  //   按设备像素对源分辨率取整倍数（v1.2.7 起 uiscale 感知），倍数确定后
  //   折回设计单位居中。整数缩放优先于画面比例（像素完美语义）。
  if (p.integerScale && srcW > 0 && srcH > 0 && dst.w > 0 && dst.h > 0) {
    const float devScale = uiscale::get();
    if (devScale > 0) {
      const int sx = (int)((target.w * devScale) / (float)srcW);
      const int sy = (int)((target.h * devScale) / (float)srcH);
      if (sx >= 1 || sy >= 1) {
        const int s = (sx < 1) ? sy : ((sy < 1) ? sx : (sx < sy ? sx : sy));
        const float w = srcW * (float)s / devScale;
        const float h = srcH * (float)s / devScale;
        target = Rect(dst.cx() - w / 2, dst.cy() - h / 2, w, h);
      }
    }
  }

  const SDL_Rect d{(int)target.x, (int)target.y, (int)target.w, (int)target.h};
  // ★修复：单屏/整帧路径显式传源矩形 —— 旧版 src=nullptr 会把整张 2 的幂
  //   纹理（含帧外填充区）铺进目标矩形，非 2 的幂分辨率（如 SNES 512×448）
  //   画面被纵向压扁/出现边缘杂色。
  const SDL_Rect fullSrc{0, 0, (int)srcW, (int)srcH};
  SDL_RenderCopy(r, tex_, src ? src : &fullSrc, &d);

  // —— 叠加类滤镜（APK FilterOverlay：图案平铺到目标矩形）——
  const pixelfilters::Filter overlay = pixelfilters::overlayTypeFor(
      p.videoFilter ? p.videoFilter : "none");
  if (overlay != pixelfilters::None) {
    drawOverlay(r, target, overlay, overlay == pixelfilters::Crt);
  }
}

// —— 叠加层绘制 ——
//   图案按目标矩形整幅重生成（SDL2 无纹理平铺寻址；逐 tile RenderCopy 在
//   1080p 下需要数十万次调用不可行）。图案周期 = 2/3/4 设备像素，与 APK
//   「屏幕像素空间平铺」一致；尺寸变化时才重建（非每帧）。
void VideoBridge::drawOverlay(SDL_Renderer* r, const Rect& target,
                              pixelfilters::Filter overlay, bool crtVignette) {
  const int w = (int)target.w, h = (int)target.h;
  if (w <= 0 || h <= 0) return;
  SDL_Texture* tex = ensurePatternTexture(r, overlay, w, h);
  if (tex) {
    SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
    const SDL_Rect d{(int)target.x, (int)target.y, w, h};
    SDL_RenderCopy(r, tex, nullptr, &d);
  }
  if (crtVignette) {
    // CRT 径向暗角（APK：透明→35% 黑，半径 min(w,h)×0.7，全程渐变）
    if (!vignetteTex_) {
      int vw = 0, vh = 0;
      std::vector<uint32_t> v = pixelfilters::makeCrtVignette(&vw, &vh);
      vignetteTex_ = SDL_CreateTexture(r, SDL_PIXELFORMAT_ARGB8888,
                                       SDL_TEXTUREACCESS_STATIC, vw, vh);
      if (vignetteTex_) {
        SDL_SetTextureBlendMode(vignetteTex_, SDL_BLENDMODE_BLEND);
        SDL_UpdateTexture(vignetteTex_, nullptr, v.data(), vw * 4);
      }
    }
    if (vignetteTex_) {
      const SDL_Rect d{(int)target.x, (int)target.y, w, h};
      SDL_RenderCopy(r, vignetteTex_, nullptr, &d);
    }
  }
}

SDL_Texture* VideoBridge::ensurePatternTexture(SDL_Renderer* r, pixelfilters::Filter kind,
                                               int w, int h) {
  // 同尺寸 + 同类型直接复用（平移不变：图案锚定矩形左上，重绘等效）
  if (scanlineTex_ && patternKind_ == kind && patternW_ == w && patternH_ == h)
    return scanlineTex_;  // 任意一张代表当前缓存（见下方指针复用约定）

  // 生成整幅平铺位图
  int pw = 0, ph = 0;
  std::vector<uint32_t> pat;
  switch (kind) {
    case pixelfilters::Scanline: pat = pixelfilters::makeScanlinePattern(&pw, &ph); break;
    case pixelfilters::Crt:      pat = pixelfilters::makeCrtPattern(&pw, &ph); break;
    case pixelfilters::Dot:      pat = pixelfilters::makeDotPattern(&pw, &ph); break;
    default: return nullptr;
  }
  if (pw <= 0 || ph <= 0) return nullptr;
  std::vector<uint32_t> full((size_t)w * h, 0);
  for (int y = 0; y < h; y++) {
    const uint32_t* row = &pat[(size_t)(y % ph) * pw];
    uint32_t* out = &full[(size_t)y * w];
    for (int x = 0; x < w; x++) out[x] = row[x % pw];
  }

  // 释放旧缓存，统一挂在 scanlineTex_ 槽位（kind 由 patternKind_ 区分）
  if (scanlineTex_) SDL_DestroyTexture(scanlineTex_);
  if (crtTex_) { SDL_DestroyTexture(crtTex_); crtTex_ = nullptr; }
  if (dotTex_) { SDL_DestroyTexture(dotTex_); dotTex_ = nullptr; }
  SDL_Texture* tex = SDL_CreateTexture(r, SDL_PIXELFORMAT_ARGB8888,
                                       SDL_TEXTUREACCESS_STATIC, w, h);
  if (!tex) return nullptr;
  SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
  SDL_UpdateTexture(tex, nullptr, full.data(), w * 4);
  scanlineTex_ = tex;  // 当前活动图案缓存（crtTex_/dotTex_ 保留为空）
  patternKind_ = kind;
  patternW_ = w;
  patternH_ = h;
  return tex;
}

bool VideoBridge::screenshot(const std::string& pngPath) {
  if (!hasFrame_) return false;
  if (lastFrame_.empty() || !lastW_ || !lastH_) return false;
  const int stride = (int)lastW_ * 4;
  return stbi_write_png(pngPath.c_str(), (int)lastW_, (int)lastH_, 4, lastFrame_.data(),
                        stride) != 0;
}

}  // namespace ns
