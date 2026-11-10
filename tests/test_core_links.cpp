// NesStation PC — 核心链路实证测试（真实核心）
// A) fceumm：INTL 中文标签 + 持久层 Region=PAL 生效（fps≈50）+ 热改回传
// B) mGBA：自制 GBA ROM（MODE3 整屏纯蓝）→ VideoBridge 整链路颜色回读
#include <cstdio>
#include <cstring>
#include <map>
#include <string>

#include <SDL.h>

#include "retro/AudioBridge.h"
#include "retro/InputManager.h"
#include "retro/LibretroCore.h"
#include "retro/VideoBridge.h"
#include "util/Fs.h"
#include "util/Log.h"

using namespace ns;

static int fails = 0;
static void check(bool ok, const char* what) {
  printf("[%s] %s\n", ok ? "PASS" : "FAIL", what);
  if (!ok) fails++;
}

// —— 自制 GBA ROM：MODE3 整屏纯蓝 ——
// GBA 原生像素 xBBBBBGGGGGRRRRR：纯蓝 = 0x7C00。
// libretro 1555 定义（R 高位 bits14-10）下核心负责翻转；前端只收标准格式。
static void makeGbaRomSimple(const std::string& path) {
  std::vector<uint8_t> rom(0x8000, 0x00);
  auto w32 = [&](size_t off, uint32_t v) {
    rom[off] = (uint8_t)v; rom[off+1] = (uint8_t)(v>>8);
    rom[off+2] = (uint8_t)(v>>16); rom[off+3] = (uint8_t)(v>>24);
  };
  w32(0x00, 0xEA00002F);            // b 0xC4（入口）
  w32(0xC4, 0xE59F0034);            // ldr r0,[pc,#0x34] → 0x100 = 0x04000000
  w32(0xC8, 0xE59F1034);            // ldr r1,[pc,#0x34] → 0x104 = 0x403
  w32(0xCC, 0xE1C010B0);            // strh r1,[r0]      DISPCNT = MODE3|BG2
  w32(0xD0, 0xE59F0034);            // ldr r0,[pc,#0x34] → 0x10C = 0x06000000
  w32(0xD4, 0xE59F1034);            // ldr r1,[pc,#0x34] → 0x110 = 0x7C00
  w32(0xD8, 0xE59F2034);            // ldr r2,[pc,#0x34] → 0x114 = 38400
  w32(0xDC, 0xE0C010B2);            // loop: strh r1,[r0],#2
  w32(0xE0, 0xE2522001);            // subs r2,r2,#1
  w32(0xE4, 0x1AFFFFFC);            // bne 0xDC
  w32(0xE8, 0xEAFFFFFE);            // b .（死循环）
  w32(0x100, 0x04000000);
  w32(0x104, 0x00000403);
  w32(0x10C, 0x06000000);
  w32(0x110, 0x00007C00);
  w32(0x114, 0x00009600);
  fsutil::writeFile(path, std::string((const char*)rom.data(), rom.size()));
}

int main(int argc, char** argv) {
  const std::string cores = argc > 1 ? argv[1] : "/tmp/cores";
  const std::string dataDir = "/tmp/ns-kt";
  fsutil::makeDirs(dataDir);
  fsutil::makeDirs(fsutil::join(dataDir, "system"));
  fsutil::makeDirs(fsutil::join(dataDir, "saves"));
  logInit(fsutil::join(dataDir, "test.log"));

  // ========== A. fceumm：INTL 中文 + Region 选项生效 ==========
  {
    printf("========== A. fceumm ==========\n");
    std::map<std::string, std::string> stored;
    stored["fceumm_region"] = "PAL";
    auto resolverA = [&](const std::string& key) {
      auto it = stored.find(key);
      return it != stored.end() ? it->second : std::string();
    };

    AudioBridge audio; VideoBridge video; InputManager input;
    LibretroCore core(audio, video, input);
    check(core.load(fsutil::join(cores, "fceumm_libretro.so"),
                    fsutil::join(dataDir, "system"), fsutil::join(dataDir, "saves"),
                    resolverA), "fceumm 加载");
    if (core.loaded()) {
      bool hasZh = false;
      std::string sampleDesc;
      for (const auto& o : core.options()) {
        if (sampleDesc.empty()) sampleDesc = o.desc;
        if (hasZh) break;
        for (const auto& lb : o.labels)
          for (unsigned char c : lb)
            if (c >= 0xE4 && c <= 0xE9) { hasZh = true; break; }
      }
      check(hasZh, "选项含中文标签（INTL local 被采用）");
      printf("     首选项 desc：%.60s\n", sampleDesc.c_str());
      printf("     选项数：%zu\n", core.options().size());

      std::vector<uint8_t> nes(16 + 16*1024 + 8*1024, 0);
      nes[0]=0x4E; nes[1]=0x45; nes[2]=0x53; nes[3]=0x1A; nes[4]=1; nes[5]=1;
      const std::string nesPath = fsutil::join(dataDir, "t.nes");
      fsutil::writeFile(nesPath, std::string((const char*)nes.data(), nes.size()));
      check(core.loadGame(nesPath), "fceumm 载入 ROM");
      const double fps = core.timingFps();
      printf("     timing fps = %.3f（PAL 期望 ≈50，NTSC ≈60.1）\n", fps);
      check(fps > 49.0 && fps < 51.0, "持久层 fceumm_region=PAL 生效（fps≈50）");
      core.setOptionValue("fceumm_region", "NTSC");
      for (int i = 0; i < 5; i++) core.runFrame();
      printf("     已热改 region=NTSC（GET_VARIABLE 回传见日志）\n");
    }
  }

  // ========== B. mGBA：GBA 纯蓝整链路颜色 ==========
  {
    printf("========== B. mGBA ==========\n");
    // ★v1.3.5：SDL_HINT_VIDEODRIVER 是 SDL 2.24+/SDL3 的宏，ubuntu-22.04 的
    //   SDL 2.0.20 没有它（linux-x64 job 编译挂点）；SDL2 全系可用 SDL_setenv
    //   在视频子系统初始化前选 dummy 驱动，等价且无版本差异
    SDL_setenv("SDL_VIDEODRIVER", "dummy", 1);
    if (SDL_InitSubSystem(SDL_INIT_VIDEO) != 0) {
      printf("     [跳过] SDL init：%s\n", SDL_GetError());
    } else {
      AudioBridge audio; VideoBridge video; InputManager input;
      LibretroCore core(audio, video, input);
      auto resolverB = [](const std::string&) { return std::string(); };
      check(core.load(fsutil::join(cores, "mgba_libretro.so"),
                      fsutil::join(dataDir, "system"), fsutil::join(dataDir, "saves"),
                      resolverB), "mgba 加载");
      if (core.loaded()) {
        const std::string gbaPath = fsutil::join(dataDir, "t.gba");
        makeGbaRomSimple(gbaPath);
        check(core.loadGame(gbaPath), "mgba 载入自制纯蓝 GBA ROM");
        for (int i = 0; i < 30; i++) core.runFrame();
        check(video.hasFrame(), "mGBA video_refresh 到帧");
        printf("     帧尺寸：%ux%u\n", video.frameWidth(), video.frameHeight());

        SDL_Surface* surf =
            SDL_CreateRGBSurfaceWithFormat(0, 240, 160, 32, SDL_PIXELFORMAT_ARGB8888);
        SDL_Renderer* ren = SDL_CreateSoftwareRenderer(surf);
        if (ren) {
          video.draw(ren, Rect(0, 0, 240, 160), VideoDrawParams{});
          const Uint32 px = ((Uint32*)surf->pixels)[(160/2)*240 + 240/2];
          printf("     中心像素 ARGB = 0x%08X（期望 0xFF0000FF 纯蓝；0xFFFF0000=红→反色）\n", px);
          const Uint32 pr = (px >> 16) & 0xFF, pb = px & 0xFF;
          check(pb > 200 && pr < 50, "GBA 纯蓝帧 → 屏幕纯蓝（无反色/染色）");
          SDL_DestroyRenderer(ren);
        }
        if (surf) SDL_FreeSurface(surf);
      }
    }
  }

  printf("\n结果：%d 项失败\n", fails);
  return fails ? 1 : 0;
}
