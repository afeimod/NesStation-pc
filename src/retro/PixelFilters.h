// NesStation PC — 像素滤镜引擎（★v1.2.8）
//
// 1:1 移植自 Android 版 NesStation 的原生滤镜层：
//   · 放大算法：core/jni/shared/core_shared.h 的 2xBR v3.3a（Hyllian，
//     RetroArch 移植版，含 int32 混合防环绕修复）+ core/jni/hqx 的
//     HQ2X / HQ4X（Maxim Stepin 原版，rowBytes 变体）；
//   · 编号体系与 core_shared.h 完全一致：
//       0=none, 1=scanline, 2=crt, 3=dot, 4=xbr, 5=hq2x, 6=hq4x,
//       7=xbr+dot, 8=4xbr, 9=4xbr+dot, 10=hq4x+dot
//   · 叠加类（scanline/crt/dot，以及 7/9/10 的「+dot」部分）由视图层
//     绘制（VideoBridge 的叠加纹理，参数取自 NdsFilterPatterns.kt）；
//     本文件只做像素放大（xbr/hqx 家族）——与 APK 分工完全一致。
//
// 滤镜字符串（PadLayout.videoFilter，APK 同名枚举值）→ 编号的映射语义：
//   · 设置页（SettingsScreen.kt:126-176）：真实编号 —— xbr→4、4xbr→8；
//   · 游戏内面板（EmulatorScreen.kt:2890-2910）：xbr→HQ2X(5)、4xbr→
//     HQ4X(6)（APK 3.5.2 实测分支行为：原生 2xBR 在硬边缘有色渗，运行时
//     统一换 HQ2X/HQ4X）。
//   PC 版两个入口分别使用上述两套映射（filterIntFromSettings /
//   filterIntFromGamePanel），完全跟随 APK。
#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace ns {
namespace pixelfilters {

// —— APK core_shared.h 的滤镜编号 ——
enum Filter : int {
  None = 0,
  Scanline = 1,
  Crt = 2,
  Dot = 3,
  Xbr2x = 4,
  Hq2x = 5,
  Hq4x = 6,
  Xbr2xDot = 7,
  Xbr4x = 8,
  Xbr4xDot = 9,
  Hq4xDot = 10,
};

// —— 滤镜字符串（config.videoFilter，APK 枚举值）——
// 返回值写入 &out；未知值返回 false（调用方回退 none）。
bool filterFromString(const std::string& s, Filter* out);
const char* filterToString(Filter f);

// 设置页映射（APK SettingsScreen：真实编号，xbr→4 / 4xbr→8）
Filter filterIntFromSettings(const std::string& s);
// 游戏内面板映射（APK EmulatorScreen 3.5.2 行为：xbr→HQ2X(5) / 4xbr→HQ4X(6)）
Filter filterIntFromGamePanel(const std::string& s);

// 叠加外观类型（APK overlayFilterTypeFor，EmulatorScreen.kt:9131-9140）：
// 返回需要由视图层绘制的叠加图案编号；None 表示无叠加。
//   scanline/crt/dot → 同名；*_dot → dot；其余（none/纯放大类）→ None
Filter overlayTypeFor(const std::string& s);

// 该编号是否为放大型（xbr/hqx 家族，需要逐像素处理）
inline bool isUpscale(Filter f) {
  return f == Xbr2x || f == Hq2x || f == Hq4x || f == Xbr2xDot || f == Xbr4x ||
         f == Xbr4xDot || f == Hq4xDot;
}

// 2xBR v3.3a：src(w×h, srcStride 单位=像素) → dst(2w×2h, 紧凑排列)
// —— 逐行逐字符移植自 APK core_shared.h xbr2xUpscale（含边界钳制取邻域）
void xbr2xUpscale(const uint32_t* src, unsigned sw, unsigned sh, size_t srcStride,
                  uint32_t* dst);

// 4xBR 级联：两次 2xBR（w×h → 2w×2h → 4w×4h）；midBuffer 需 ≥ 2w×2h
void xbr4xUpscale(const uint32_t* src, unsigned sw, unsigned sh, size_t srcStride,
                  uint32_t* dst, uint32_t* midBuffer);

// 帧级放大调度（APK applyFilterAndBlit 的放大部分）：
//   按 filter 对 (src, w, h)（stride 单位=像素）做 CPU 放大，返回处理后
//   缓冲（滤波关闭/超预算时原样返回 src 的拷贝由调用方处理——见实现注释）。
//   budgetW/H：放大预算（APK canUpscale：帧超过预算静默跳过——NES 核心
//   256×240 全帧、melonDS 256×386；PC 侧由调用方按核心 AV 信息给出）。
//
// 返回：处理后的缓冲指针 + 输出尺寸（outW/outH）。
//   · 无放大需求（非放大型 / 超预算 / 内存不足）→ 返回 nullptr（直通）。
const uint32_t* applyUpscale(Filter f, const uint32_t* src, unsigned w, unsigned h,
                             size_t stride, unsigned budgetW, unsigned budgetH,
                             unsigned* outW, unsigned* outH);

// —— 叠加图案生成（APK NdsFilterPatterns.kt 逐参数）——
// 每个函数生成 REPEAT 平铺用小位图（ARGB8888），坐标空间 = 屏幕（目标
// 矩形）像素，绘制时禁用平滑（isFilterBitmap=false, isAntiAlias=false）。

// 扫描线：2×4 tile —— 每 4 屏幕像素 3 行透明 + 1 行 55% 黑（0x8C）
std::vector<uint32_t> makeScanlinePattern(int* w, int* h);
// CRT 荫罩：3×6 tile —— RGB 磷光竖条（各 1px，α=0x26≈15%）×5 行 +
// 第 6 行 50% 黑扫描线（0x80）
std::vector<uint32_t> makeCrtPattern(int* w, int* h);
// 点阵 LCD：4×4 tile —— smoothstep 圆形暗点（α = smoothstep((dist-1)/(√8-1))×128）
std::vector<uint32_t> makeDotPattern(int* w, int* h);
// CRT 径向暗角图（256×256 灰度 α：中心 0 → 边缘 35% 黑，全程渐变；
// 绘制时拉伸到目标矩形，半径语义 = min(w,h)×0.7）
std::vector<uint32_t> makeCrtVignette(int* w, int* h);

}  // namespace pixelfilters
}  // namespace ns
