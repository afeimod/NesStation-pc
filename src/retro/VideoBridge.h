// NesStation PC — 视频桥：libretro 帧 → SDL 纹理 → 画面呈现
#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include <SDL.h>
#include <libretro.h>

#include "core/Platform.h"
#include "ui/Ui.h"

namespace ns {

enum class ScalingMode : int {
  KeepAspect = 0,   // 保持比例（默认，Core 提供宽高比）
  Stretch = 1,      // 拉伸填充
  IntegerScale = 2, // 整数缩放（像素完美）
};

enum class ScreenEffect : int {
  None = 0,
  Scanlines = 1,   // 扫描线
  CrtMask = 2,     // CRT 荫罩 + 暗角
};

// 双屏平台（NDS/3DS）画面布局
enum class DualLayout : int {
  Both = 0,        // 上屏 + 下屏（原始堆叠）
  TopOnly = 1,     // 仅上屏放大
  BottomOnly = 2,  // 仅下屏放大
};

class VideoBridge {
 public:
  VideoBridge();
  ~VideoBridge();

  void setRenderer(SDL_Renderer* r);
  void setDualScreen(bool dual) { dual_ = dual; }

  // 核心回调（帧提交）
  void frame(const void* data, unsigned width, unsigned height, size_t pitch);
  void setPixelFormat(enum retro_pixel_format fmt);

  // AV 信息（load 后由 core 填）
  void setAvInfo(unsigned baseW, unsigned baseH, double aspect, unsigned maxW, unsigned maxH);
  void setGeometry(unsigned w, unsigned h, double aspect);

  // 呈现（游戏区 dst 内按模式缩放；dual 平台按布局）
  void draw(SDL_Renderer* r, const Rect& dst, ScalingMode mode, ScreenEffect fx,
            DualLayout dual, float smooth);

  bool hasFrame() const { return hasFrame_; }
  unsigned frameWidth() const { return frameW_; }
  unsigned frameHeight() const { return frameH_; }
  double aspect() const { return aspect_; }

  // 截图（当前帧 → PNG 落盘）
  bool screenshot(const std::string& pngPath);

 private:
  void ensureTexture(SDL_Renderer* r);
  void ensureEffectTextures(SDL_Renderer* r);
  void drawScreenPart(SDL_Renderer* r, SDL_Texture* tex, const SDL_Rect* src,
                      const Rect& dst, ScalingMode mode, ScreenEffect fx,
                      unsigned texW, unsigned texH, float gameAspect, float smooth);

  enum retro_pixel_format fmt_ = RETRO_PIXEL_FORMAT_0RGB1555;
  SDL_Texture* tex_ = nullptr;
  unsigned texW_ = 0, texH_ = 0;

  // 本帧待上传数据（video_refresh 时核心保证到下一次刷新前有效）
  const void* pendingSrc_ = nullptr;
  size_t pendingPitch_ = 0;
  bool dirty_ = false;

  // 最近一帧 ARGB 缓存（截图用）
  std::vector<uint32_t> lastFrame_;

  bool hasFrame_ = false;
  unsigned frameW_ = 0, frameH_ = 0;
  unsigned baseW_ = 256, baseH_ = 240;
  unsigned maxW_ = 256, maxH_ = 240;
  double aspect_ = 4.0 / 3.0;
  bool dual_ = false;

  // 特效叠加纹理
  SDL_Texture* scanlineTex_ = nullptr;
  SDL_Texture* crtTex_ = nullptr;
};

}  // namespace ns
