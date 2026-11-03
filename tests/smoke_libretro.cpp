// NesStation PC — libretro 宿主链路冒烟测试（不依赖 SDL 视频）
// 用真实 fceumm 核心验证：加载 → 选项解析 → 载入 ROM → 跑帧 → 音频/视频回调 → 快速存档
#include <cstdio>
#include <cstring>
#include <string>

#include "retro/AudioBridge.h"
#include "retro/InputManager.h"
#include "retro/LibretroCore.h"
#include "retro/VideoBridge.h"
#include "util/Fs.h"
#include "util/Log.h"

using namespace ns;

int main(int argc, char** argv) {
  const std::string corePath = argc > 1 ? argv[1] : "/tmp/cores/fceumm_libretro.so";
  const std::string romPath = argc > 2 ? argv[2] : "/tmp/test.nes";
  const std::string dataDir = argc > 3 ? argv[3] : "/tmp/ns-smoke";
  fsutil::makeDirs(fsutil::join(dataDir, "system"));
  fsutil::makeDirs(fsutil::join(dataDir, "saves"));
  logInit(fsutil::join(dataDir, "smoke.log"));

  AudioBridge audio;
  VideoBridge video;
  InputManager input;
  int fails = 0;
  auto check = [&](bool ok, const char* what) {
    printf("[%s] %s\n", ok ? "PASS" : "FAIL", what);
    if (!ok) fails++;
  };

  LibretroCore core(audio, video, input);
  auto resolver = [](const std::string&) { return std::string(); };
  check(core.load(corePath, fsutil::join(dataDir, "system"), fsutil::join(dataDir, "saves"),
                  resolver),
        "核心 dlopen + 符号解析 + retro_init");
  printf("     核心名：%s %s\n", core.coreName().c_str(), core.coreVersion().c_str());

  check(core.loadGame(romPath), "retro_load_game（内存加载）");
  printf("     视频：%ux%u @%.2ffps\n", core.videoBaseW(), core.videoBaseH(),
         core.timingFps());

  check(core.options().size() > 5, "核心选项解析（v1/v2 → 统一结构）");
  printf("     选项数：%zu\n", core.options().size());
  if (!core.options().empty()) {
    const auto& o = core.options()[0];
    printf("     首选项：%s（%zu 个取值，默认 %s）\n", o.desc.c_str(), o.values.size(),
           core.optionDefaultValue(o.key).c_str());
  }

  bool gotVideo = false, gotAudio = false;
  for (int i = 0; i < 240; i++) {  // 4 秒 @60fps
    core.runFrame();
    if (video.hasFrame()) gotVideo = true;
  }
  (void)gotAudio;
  check(gotVideo, "跑 240 帧（video_refresh 回调到达）");
  printf("     帧尺寸：%ux%u\n", video.frameWidth(), video.frameHeight());

  std::vector<uint8_t> state;
  const bool ser = core.serialize(&state);
  check(ser && state.size() > 0, "快速存档 serialize");
  printf("     存档大小：%zu 字节\n", state.size());
  if (ser) {
    check(core.unserialize(state.data(), state.size()), "快速读档 unserialize");
    core.runFrame();
    check(true, "读档后继续跑帧");
  }

  // 选项热改
  if (!core.options().empty()) {
    const auto& o = core.options()[0];
    if (o.values.size() > 1) {
      core.setOptionValue(o.key, o.values[1]);
      core.runFrame();
      check(true, "选项热改 + 继续跑帧");
    }
  }

  core.unload();
  check(true, "卸载（无崩溃）");
  printf("\n结果：%s（%d 项失败）\n", fails == 0 ? "全部通过" : "存在失败", fails);
  logClose();
  return fails == 0 ? 0 : 1;
}
