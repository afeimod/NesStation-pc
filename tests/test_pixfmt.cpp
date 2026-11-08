// VideoBridge 像素格式转换回归测试（★v1.3.2 针对用户反馈「GBA 画面反色」）
//
// 验证三种 libretro 像素格式 → ARGB8888 的通道序与跨会话重置：
//   1. XRGB8888（mGBA/GBA 主用）：红输入 → 红输出（R/B 不得交换 = 反色回归）
//      高位 X 强制不透明（|0xFF000000）
//   2. RGB565：5-6-5 位域展开到 8-8-8，通道序不交换
//   3. 0RGB1555（默认格式）：同上
//   4. beginSession() 跨会话重置：上一核心 XRGB8888 → 新核心未声明时回默认 1555，
//      不得残留 XRGB8888（v1.2.9「换核心后颜色错乱」根因）
//
// 编译（仓库根目录）：
//   g++ -std=c++17 -O1 -I src -I third_party -I third_party/libretro \
//       $(sdl2-config --cflags) tests/test_pixfmt.cpp src/retro/PixelFilters.cpp \
//       third_party/stb/stb_impl.cpp $(sdl2-config --libs) -ldl -lpthread \
//       -o /tmp/test_pixfmt && SDL_VIDEODRIVER=dummy /tmp/test_pixfmt
#include <cstdio>
#include <cstring>

#include <SDL.h>

// 仅本翻译单元放开访问权（仓库测试惯例：test_externals.cpp 同款 include 模式）
#define private public
#include "retro/VideoBridge.h"
#undef private

using namespace ns;

static int failures = 0;
#define CHECK(cond, msg)                                         \
  do {                                                           \
    if (cond) { printf("  ✓ %s\n", msg); }                       \
    else { printf("  ✗ FAIL: %s\n", msg); failures++; }          \
  } while (0)

// 喂一帧并返回转换后左上角像素（ARGB8888）
static uint32_t feedAndGet(VideoBridge& vb, SDL_Renderer* r,
                           enum retro_pixel_format fmt, uint32_t pixelLE) {
  vb.beginSession();
  vb.setPixelFormat(fmt);
  uint32_t buf[4] = {pixelLE, pixelLE, pixelLE, pixelLE};  // 2x2 帧
  vb.frame(buf, 2, 2, 8);
  vb.uploadFrame(r);
  return vb.lastFrame_.empty() ? 0 : vb.lastFrame_[0];
}

int main() {
  if (SDL_Init(SDL_INIT_VIDEO) != 0) {
    printf("SDL 初始化失败：%s\n", SDL_GetError());
    return 1;
  }
  SDL_Window* win =
      SDL_CreateWindow("t", 0, 0, 64, 64, SDL_WINDOW_HIDDEN);
  SDL_Renderer* r = SDL_CreateSoftwareRenderer(SDL_CreateRGBSurfaceWithFormat(0, 64, 64, 32, SDL_PIXELFORMAT_ARGB8888));
  (void)win;
  if (!r) { printf("软件渲染器创建失败\n"); return 1; }

  // —— 1. XRGB8888：通道序（反色回归核心断言） ——
  // 小端内存 0x00RRGGBB：红 0x00FF0000 → 期望 ARGB 0xFFFF0000（红）
  {
    VideoBridge vb;
    const uint32_t got = feedAndGet(vb, r, RETRO_PIXEL_FORMAT_XRGB8888, 0x00FF0000u);
    CHECK((got & 0xFF000000u) == 0xFF000000u, "XRGB8888 高位 X 强制不透明（|0xFF000000）");
    CHECK((got & 0x00FF0000u) != 0 && (got & 0x000000FFu) == 0,
          "XRGB8888 红 → 红（R/B 未交换 = 反色已修复）");
  }
  {
    VideoBridge vb;
    const uint32_t got = feedAndGet(vb, r, RETRO_PIXEL_FORMAT_XRGB8888, 0x000000FFu);
    CHECK((got & 0x000000FFu) != 0 && (got & 0x00FF0000u) == 0,
          "XRGB8888 蓝 → 蓝（R/B 未交换）");
  }

  // —— 2. RGB565 位域展开 ——
  {
    VideoBridge vb;
    // 0xF800 = 纯红（r5=31,g6=0,b5=0）→ 0xFFFF0000（r8=255）
    const uint32_t got = feedAndGet(vb, r, RETRO_PIXEL_FORMAT_RGB565, 0xF800u);
    CHECK((got & 0x00FF0000u) == 0x00FF0000u && (got & 0x0000FF00u) == 0,
          "RGB565 纯红 → 纯红（位域展开正确）");
  }
  {
    VideoBridge vb;
    // 0x07E0 = 纯绿（g6=63）→ 0xFF00FF00 附近（g8=255）
    const uint32_t got = feedAndGet(vb, r, RETRO_PIXEL_FORMAT_RGB565, 0x07E0u);
    CHECK((got & 0x0000FF00u) == 0x0000FF00u && (got & 0x00FF0000u) == 0,
          "RGB565 纯绿 → 纯绿");
  }

  // —— 3. 0RGB1555（默认格式）——
  {
    VideoBridge vb;
    // 0x7C00 = 纯红（r5=31）
    const uint32_t got = feedAndGet(vb, r, RETRO_PIXEL_FORMAT_0RGB1555, 0x7C00u);
    CHECK((got & 0x00FF0000u) == 0x00FF0000u, "0RGB1555 纯红 → 纯红");
  }

  // —— 4. 跨会话重置（v1.2.9「换核心后颜色错乱」根因） ——
  {
    VideoBridge vb;
    vb.beginSession();
    vb.setPixelFormat(RETRO_PIXEL_FORMAT_XRGB8888);
    // 结束会话 → 新会话（新核心假定默认 1555，不调 SET_PIXEL_FORMAT）
    vb.beginSession();
    CHECK(vb.fmt_ == RETRO_PIXEL_FORMAT_0RGB1555,
          "beginSession 后像素格式回默认 1555（不残留上一核心的 XRGB8888）");
    // 1555 数据按 1555 解析（红 0x7C00 → 红）
    uint16_t buf[4] = {0x7C00, 0x7C00, 0x7C00, 0x7C00};
    vb.frame(buf, 2, 2, 4);
    vb.uploadFrame(r);
    CHECK((vb.lastFrame_[0] & 0x00FF0000u) == 0x00FF0000u,
          "新会话 1555 帧按 1555 解析（颜色正确）");
  }

  SDL_DestroyRenderer(r);
  SDL_DestroyWindow(win);
  SDL_Quit();
  printf(failures ? "存在失败项\n" : "全部通过\n");
  return failures ? 1 : 0;
}
