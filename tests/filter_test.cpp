// 像素滤镜单元验证：放大算法维度/保真 + 叠加图案参数（APK 逐项断言）
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include "retro/PixelFilters.h"

using namespace ns;
using namespace ns::pixelfilters;

static int failures = 0;
#define CHECK(cond, msg)                                              \
  do {                                                                \
    if (cond) { printf("  ✓ %s\n", msg); }                            \
    else { printf("  ✗ FAIL: %s\n", msg); failures++; }               \
  } while (0)

int main() {
  // —— 1. 映射表（APK 语义）——
  printf("[映射]\n");
  CHECK(filterIntFromSettings("xbr") == Xbr2x, "设置页 xbr → 真实编号 4（2xBR）");
  CHECK(filterIntFromSettings("4xbr") == Xbr4x, "设置页 4xbr → 真实编号 8（4xBR）");
  CHECK(filterIntFromGamePanel("xbr") == Hq2x, "游戏内 xbr → HQ2X(5)（APK 3.5.2 行为）");
  CHECK(filterIntFromGamePanel("4xbr") == Hq4x, "游戏内 4xbr → HQ4X(6)");
  CHECK(filterIntFromGamePanel("hq2x") == Hq2x, "游戏内 hq2x → 5");
  CHECK(filterIntFromGamePanel("hq4x_dot") == Hq4xDot, "游戏内 hq4x_dot → 10");
  CHECK(overlayTypeFor("xbr_dot") == Dot, "xbr_dot 叠加 = dot");
  CHECK(overlayTypeFor("hq4x_dot") == Dot, "hq4x_dot 叠加 = dot");
  CHECK(overlayTypeFor("scanline") == Scanline, "scanline 叠加 = scanline");
  CHECK(overlayTypeFor("crt") == Crt, "crt 叠加 = crt");
  CHECK(overlayTypeFor("dot") == Dot, "dot 叠加 = dot");
  CHECK(overlayTypeFor("xbr") == None, "纯放大类无叠加");
  CHECK(overlayTypeFor("none") == None, "none 无叠加");
  CHECK(overlayTypeFor("hq2x") == None, "hq2x 无叠加");

  // —— 2. 放大算法维度与保真 ——
  printf("[放大]\n");
  const unsigned W = 64, H = 48;
  std::vector<uint32_t> src(W * H);
  for (unsigned y = 0; y < H; y++)
    for (unsigned x = 0; x < W; x++) {
      // 棋盘格 + 渐变（有硬边缘也有平滑区）
      const bool chk = ((x / 8) + (y / 8)) % 2 == 0;
      src[y * W + x] = 0xFF000000u | (chk ? 0xFF3020u : (uint32_t)(x * 4) << 8 | 0x40u);
    }
  unsigned ow = 0, oh = 0;
  // 2xBR
  const uint32_t* out = applyUpscale(Xbr2x, src.data(), W, H, W, 1024, 1024, &ow, &oh);
  CHECK(out && ow == W * 2 && oh == H * 2, "2xBR 输出 2× 尺寸");
  if (out) {
    // 源像素应保留在偶数格点：2xBR 无边缘时 E[0] = PE 原像素。
    // 采样块中心（x%8==4）避开块间硬边缘
    bool preserve = true;
    for (unsigned y = 4; y < H && preserve; y += 8)
      for (unsigned x = 4; x < W && preserve; x += 8)
        if (out[(y * 2) * ow + x * 2] != src[y * W + x]) preserve = false;
    CHECK(preserve, "2xBR 平坦区源像素保留在 (2x,2y) 格点");
  }
  // HQ2X
  out = applyUpscale(Hq2x, src.data(), W, H, W, 1024, 1024, &ow, &oh);
  CHECK(out && ow == W * 2 && oh == H * 2, "HQ2X 输出 2× 尺寸");
  if (out) {
    bool preserve = out[0] == src[0];
    CHECK(preserve, "HQ2X 平坦区 (0,0) 原色保留");
  }
  // HQ4X
  out = applyUpscale(Hq4x, src.data(), W, H, W, 1024, 1024, &ow, &oh);
  CHECK(out && ow == W * 4 && oh == H * 4, "HQ4X 输出 4× 尺寸");
  // 4xBR 级联
  out = applyUpscale(Xbr4x, src.data(), W, H, W, 1024, 1024, &ow, &oh);
  CHECK(out && ow == W * 4 && oh == H * 4, "4xBR 级联输出 4× 尺寸");
  // 预算跳过（APK canUpscale）：64×48 帧 vs 预算 32×32 → 直通
  out = applyUpscale(Hq2x, src.data(), W, H, W, 32, 32, &ow, &oh);
  CHECK(out == nullptr, "超预算静默直通（canUpscale=false）");
  // 非放大型 → 直通
  out = applyUpscale(Scanline, src.data(), W, H, W, 1024, 1024, &ow, &oh);
  CHECK(out == nullptr, "叠加类（scanline）不走放大通道");

  // —— 3. 叠加图案参数（APK NdsFilterPatterns 逐值断言）——
  printf("[图案]\n");
  {
    int w = 0, h = 0;
    auto p = makeScanlinePattern(&w, &h);
    CHECK(w == 2 && h == 4, "扫描线 tile 2×4");
    CHECK(p[0 * 2] == 0x00000000u && p[1 * 2] == 0x00000000u && p[2 * 2] == 0x00000000u,
          "扫描线前 3 行全透明");
    CHECK(p[3 * 2] == 0x8C000000u, "扫描线第 4 行 α=0x8C（55% 黑）");
  }
  {
    int w = 0, h = 0;
    auto p = makeCrtPattern(&w, &h);
    CHECK(w == 3 && h == 6, "CRT tile 3×6");
    CHECK(p[0] == 0x26FF0000u && p[1] == 0x2600FF00u && p[2] == 0x260000FFu,
          "CRT RGB 磷光竖条 α=0x26");
    CHECK(p[5 * 3] == 0x80000000u && p[5 * 3 + 1] == 0x80000000u && p[5 * 3 + 2] == 0x80000000u,
          "CRT 第 6 行 50% 黑扫描线 α=0x80");
  }
  {
    int w = 0, h = 0;
    auto p = makeDotPattern(&w, &h);
    CHECK(w == 4 && h == 4, "点阵 tile 4×4");
    // (1,1) 距中心 (1.5,1.5) 最近（dist≈0.707<1）→ α=0（亮核）
    CHECK(p[1 * 4 + 1] == 0x00000000u, "点阵中心 (1,1) 全透明（亮）");
    // (0,0) 距中心 dist=maxDist≈2.121 → t=1 → α=128（50% 黑四角）
    CHECK(p[0] == ((uint32_t)(128) << 24), "点阵 (0,0) α=128（50% 黑）");
  }
  {
    int w = 0, h = 0;
    auto p = makeCrtVignette(&w, &h);
    CHECK(w == 256 && h == 256, "暗角图 256×256");
    // 中心 = (128,128) 全透明；角落 (0,0) α≈35%×255≈89
    CHECK(p[128 * 256 + 128] == 0x00000000u, "暗角中心全透明");
    const uint32_t corner = p[255 * 256 + 255];
    CHECK((corner >> 24) >= 80 && (corner >> 24) <= 92,
          "暗角角落 α≈35%×255（≈89）");
  }

  printf("\n%s（失败 %d 项）\n", failures ? "== 有失败 ==" : "== 全部通过 ==", failures);
  return failures ? 1 : 0;
}
