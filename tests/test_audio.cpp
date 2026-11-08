// AudioBridge 回归测试（★v1.3.2 针对用户反馈「所有核心音频乱七八糟、像快进」）
//
// v1.2.9 两大根因的回归断言：
//   A. 核心采样率 ≠ 设备采样率时无重采样（44100 数据按 48000 播 → 音调升高
//      「像快进」）→ v1.3.0 SDL_AudioStream 重采样：验证异率路径产出正确
//      时长且不中断
//   B. 帧/样本单位混用导致下溢读垃圾 → v1.3.0 环形缓冲统一按「立体声帧」
//      计量：验证等率直拷读写一致、欠载补零（静音而非噪声）、溢出丢最旧
//
// 编译（仓库根目录）：
//   g++ -std=c++17 -O1 -I src -I third_party -I third_party/libretro \
//       $(sdl2-config --cflags) tests/test_audio.cpp $(sdl2-config --libs) \
//       -ldl -lpthread -o /tmp/test_audio && SDL_AUDIODRIVER=dummy /tmp/test_audio
#include <cstdio>
#include <cstring>
#include <vector>

#include <SDL.h>

// 仅本翻译单元放开访问权（仓库测试惯例）
#define private public
#include "retro/AudioBridge.h"
#undef private

using namespace ns;

static int failures = 0;
#define CHECK(cond, msg)                                         \
  do {                                                           \
    if (cond) { printf("  ✓ %s\n", msg); }                       \
    else { printf("  ✗ FAIL: %s\n", msg); failures++; }          \
  } while (0)

int main() {
  if (SDL_Init(SDL_INIT_AUDIO) != 0) {
    printf("SDL 音频初始化失败：%s\n", SDL_GetError());
    return 1;
  }
  // 测试全程暂停设备回调：start() 立即启动回调线程（SDL_PauseAudioDevice(dev_,0)），
  // dummy 驱动也会并发消费缓冲 —— 不暂停则断言与回调线程竞态（首个版本测试
  // 误报重采样失败的真相：独立 SDL_AudioStream 验证完全正常）
  auto pauseCb = [](AudioBridge& ab) { SDL_PauseAudioDevice(ab.dev_, 1); };

  // —— 1. 等率直拷（核心 48000 = 设备 48000）——
  {
    AudioBridge ab;
    if (!ab.start(48000, 64)) {
      printf("（dummy 驱动未开启音频设备，跳过回放断言）\n");
      SDL_Quit();
      return 0;
    }
    pauseCb(ab);
    ab.setCoreFormat(48000, 2);
    // 推 100 帧（帧 = 立体声样本对），值 = 帧号
    std::vector<int16_t> buf(200);
    for (int i = 0; i < 100; i++) { buf[i*2] = (int16_t)i; buf[i*2+1] = (int16_t)(i + 1000); }
    ab.push(buf.data(), 100);
    CHECK(ab.ringSize_ == 100, "等率 push 100 帧 → ringSize_ = 100（帧单位，非样本）");
    // mix 取 60 帧
    std::vector<int16_t> out(120);
    ab.mix((Uint8*)out.data(), 60 * 4);
    bool ordered = true;
    for (int i = 0; i < 60; i++)
      if (out[i*2] != (int16_t)i || out[i*2+1] != (int16_t)(i + 1000)) { ordered = false; break; }
    CHECK(ordered, "等率 mix 按先进先出取帧（左右声道序完整）");
    CHECK(ab.ringSize_ == 40, "取后余 40 帧（读写计量一致）");
    // 欠载：再取 60 帧（只有 40 可用）→ 尾部 20 帧补零
    std::vector<int16_t> out2(120, 0x7FFF);
    ab.mix((Uint8*)out2.data(), 60 * 4);
    CHECK(ab.ringSize_ == 0, "欠载后缓冲空");
    bool tailSilent = true;
    for (int i = 40; i < 60; i++)
      if (out2[i*2] != 0 || out2[i*2+1] != 0) { tailSilent = false; break; }
    CHECK(tailSilent, "欠载部分补零（静音，非垃圾数据 —— 「乱七八糟」根因回归）");
    bool headOk = true;
    for (int i = 0; i < 40; i++)
      if (out2[i*2] != (int16_t)(60 + i) || out2[i*2+1] != (int16_t)(60 + i + 1000)) { headOk = false; break; }
    CHECK(headOk, "欠载帧前段数据仍连续正确");
    // 溢出：推超容量 → 丢最旧保最新（低延迟语义）
    const size_t cap = ab.kMaxFrames;
    std::vector<int16_t> big((cap + 100) * 2);
    for (size_t i = 0; i < cap + 100; i++) { big[i*2] = (int16_t)(i % 32000); big[i*2+1] = 0; }
    ab.push(big.data(), cap + 100);
    CHECK(ab.ringSize_ == cap, "溢出后缓冲钳制在容量上限");
    std::vector<int16_t> out3(8);
    ab.mix((Uint8*)out3.data(), 4 * 4);
    CHECK((int)out3[0] == 100, "溢出丢最旧 → 首帧为第 100 帧（保最新数据，v1.3.2 前此处 size_t 下溢）");
    ab.stop();
  }

  // —— 2. 异率重采样（核心 44100 → 设备 48000：mGBA/多数核心实况） ——
  {
    AudioBridge ab;
    if (!ab.start(48000, 64)) return 1;
    pauseCb(ab);
    ab.setCoreFormat(44100, 2);
    CHECK(ab.coreRate_ == 44100 && ab.deviceRate_ == 48000, "核心 44100 / 设备 48000 状态记录");
    // 推 1 秒 Worth 的 44100 帧
    std::vector<int16_t> buf(44100 * 2, 3000);
    ab.push(buf.data(), 44100);
    CHECK(ab.stream_ != nullptr, "异率路径创建 SDL_AudioStream（v1.2.9 无重采样 → 像快进）");
    // 取 100ms（4800 帧）验证可取且无非零即垃圾
    std::vector<int16_t> out(4800 * 2, 0x7FFF);
    ab.mix((Uint8*)out.data(), 4800 * 4);
    int nonZero = 0;
    for (size_t i = 0; i < out.size(); i++) if (out[i] != 0x7FFF && out[i] != 0) nonZero++;
    CHECK(nonZero > 4000, "重采样输出有效样本（≈恒幅 3000 的重采样结果）");
    int badTails = 0;
    for (size_t i = 0; i < out.size(); i++)
      if (out[i] == 0x7FFF) badTails++;
    CHECK(badTails == 0, "全部请求样本被填充（无未初始化数据泄漏）");
    ab.stop();
  }

  // —— 3. 静音时按量消费（解除静音立即正常，无积压跳变） ——
  {
    AudioBridge ab;
    if (!ab.start(48000, 64)) return 1;
    pauseCb(ab);
    ab.setCoreFormat(48000, 2);
    std::vector<int16_t> buf(200, 1000);
    ab.push(buf.data(), 100);
    ab.setMuted(true);
    std::vector<int16_t> out(120, 0x7FFF);
    ab.mix((Uint8*)out.data(), 60 * 4);
    bool allZero = true;
    for (int i = 0; i < 120; i++) if (out[i] != 0) { allZero = false; break; }
    CHECK(allZero, "静音 mix 输出全零");
    CHECK(ab.ringSize_ == 40, "静音期间缓冲照常消费（水位与时间同步）");
    ab.stop();
  }

  SDL_Quit();
  printf(failures ? "存在失败项\n" : "全部通过\n");
  return failures ? 1 : 0;
}
