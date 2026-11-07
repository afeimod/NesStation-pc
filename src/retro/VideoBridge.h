// NesStation PC — 视频桥：libretro 帧 → SDL 纹理 → 画面呈现
// ★v1.2.8：画面比例与滤镜完全对齐 Android 版（PadLayout.videoScale /
//   videoFilter）—— 比例数学取自 EmulatorScreen.kt GameSurfaceView，滤镜
//   算法/图案参数取自 core_shared.h 与 NdsFilterPatterns.kt（详见
//   retro/PixelFilters.h）。
#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include <SDL.h>
#include <libretro.h>

#include "core/Platform.h"
#include "retro/PixelFilters.h"
#include "ui/Ui.h"

namespace ns {

// 画面比例模式（APK video_scale 字符串；保留 int 枚举便于旧代码迁移断言）
enum class VideoScaleMode : int {
  Stretch = 0,  // 全屏拉伸（默认，APK "stretch"）
  Ratio43 = 1,  // 4:3（CRT 显示比）
  Ratio32 = 2,  // 3:2（GBA 原生 240×160）
  Ratio87 = 3,  // 8:7（NES 像素比，方形像素）
  Ratio169 = 4, // 16:9
  Custom = 5,   // 自定义（拖动四角，customLayout 归一化矩形）
};

// 双屏平台（NDS/3DS）画面布局
enum class DualLayout : int {
  Both = 0,        // 上屏 + 下屏（原始堆叠）
  TopOnly = 1,     // 仅上屏放大
  BottomOnly = 2,  // 仅下屏放大
};

// —— 画面呈现参数（对齐 APK：一次打包传入，热生效无副作用）——
struct VideoDrawParams {
  const char* videoScale = "stretch";  // APK video_scale 枚举字符串
  const float* customRect = nullptr;   // 归一化 [l,t,r,b]（custom 模式）
  const char* videoFilter = "none";    // APK video_filter 枚举字符串
  bool integerScale = false;           // PC 增强：整数缩放（像素完美）
  DualLayout dual = DualLayout::Both;
  float smooth = 1.0f;                 // 线性采样（0 = 最近邻）
};

class VideoBridge {
 public:
  VideoBridge();
  ~VideoBridge();

  void setRenderer(SDL_Renderer* r);
  void setDualScreen(bool dual) { dual_ = dual; }

  // ★v1.3.0 新会话重置：每次加载核心前调用 —— 像素格式/帧状态回到默认。
  //   旧版跨会话残留上一核心的 fmt_（如上一核心 XRGB8888、本核心未声明时
  //   按默认 0RGB1555 输出），格式错位 → 画面颜色错乱（GBA 反色类问题）。
  void beginSession();

  // 核心回调（帧提交）
  void frame(const void* data, unsigned width, unsigned height, size_t pitch);
  void setPixelFormat(enum retro_pixel_format fmt);

  // AV 信息（load 后由 core 填）
  void setAvInfo(unsigned baseW, unsigned baseH, double aspect, unsigned maxW,
                 unsigned maxH);
  void setGeometry(unsigned w, unsigned h, double aspect);

  // 呈现：在 dst 内按 APK videoScale 模式计算目标矩形；放大型滤镜在
  // 帧上传前做 CPU 逐像素放大；叠加类滤镜（扫描线/CRT/点阵）按 APK
  // 图案参数平铺到目标矩形之上。
  void draw(SDL_Renderer* r, const Rect& dst, const VideoDrawParams& p);

  bool hasFrame() const { return hasFrame_; }
  unsigned frameWidth() const { return frameW_; }
  unsigned frameHeight() const { return frameH_; }
  double aspect() const { return aspect_; }

  // 截图（当前帧 → PNG 落盘；含滤镜效果所见即所得）
  bool screenshot(const std::string& pngPath);

 private:
  void ensureTexture(SDL_Renderer* r, unsigned needW, unsigned needH);
  void uploadFrame(SDL_Renderer* r);  // 格式转换 + 放大滤镜 + 纹理上传
  void drawScreenPart(SDL_Renderer* r, const SDL_Rect* src, const Rect& dst,
                      const VideoDrawParams& p, unsigned srcW, unsigned srcH);
  void drawOverlay(SDL_Renderer* r, const Rect& target, pixelfilters::Filter overlay,
                   bool crtVignette);
  SDL_Texture* ensurePatternTexture(SDL_Renderer* r, pixelfilters::Filter kind, int w,
                                    int h);

  enum retro_pixel_format fmt_ = RETRO_PIXEL_FORMAT_0RGB1555;
  SDL_Texture* tex_ = nullptr;
  unsigned texW_ = 0, texH_ = 0;

  // ★v1.3.0 帧快照：video_refresh 时立即拷贝到自有缓冲。
  //   旧版只存核心内部指针（pendingSrc_），依赖「到下一次刷新前有效」的
  //   隐式约定；一旦核心内部分配/复用行为变化（或补帧循环多跑几帧），
  //   渲染读到中途被改写的缓冲 → 画面出现闪烁割裂块。
  //   现在每帧到达即拷贝（256×240 仅 ~123KB，60fps 代价可忽略），
  //   uploadFrame 只读自有数据，与核心完全解耦。
  std::vector<uint8_t> pending_;
  unsigned pendingW_ = 0, pendingH_ = 0;
  size_t pendingPitch_ = 0;
  bool dirty_ = false;

  // 最近一帧 ARGB 缓存（截图用；含放大滤镜后的最终画面）
  std::vector<uint32_t> lastFrame_;
  unsigned lastW_ = 0, lastH_ = 0;

  // 帧上传时的放大滤镜（游戏内口径：xbr→HQ2X / 4xbr→HQ4X，APK 运行时行为）
  std::string appFilter_ = "none";   // draw() 每帧刷新（设置热生效）
  std::string activeFilter_ = "none";  // 本帧实际生效的滤镜（日志/调试用）
  std::string loggedFilter_;           // 上次日志输出（切换才打，防刷屏）
  unsigned drawnW_ = 0, drawnH_ = 0;  // 本帧实际进入纹理的尺寸（放大后）

  bool hasFrame_ = false;
  unsigned frameW_ = 0, frameH_ = 0;
  unsigned baseW_ = 256, baseH_ = 240;
  unsigned maxW_ = 256, maxH_ = 240;
  double aspect_ = 4.0 / 3.0;
  bool dual_ = false;

  // 叠加纹理（按目标尺寸整幅重生成：SDL2 无平铺寻址，逐 tile 复制过慢；
  // 图案随目标矩形像素网格 1:1 平铺 —— 与 APK BitmapShader REPEAT 等价）
  SDL_Texture* scanlineTex_ = nullptr;
  SDL_Texture* crtTex_ = nullptr;
  SDL_Texture* dotTex_ = nullptr;
  SDL_Texture* vignetteTex_ = nullptr;
  int patternW_ = 0, patternH_ = 0;          // 当前叠加纹理尺寸
  pixelfilters::Filter patternKind_ = pixelfilters::None;
};

}  // namespace ns
