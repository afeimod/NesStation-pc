// NesStation PC — 音频桥：libretro 核心音频 → SDL 音频设备
// ★v1.3.0 全面重写（修复「所有核心声音乱七八糟 / 像快进」）：
//   旧版两个致命 bug ——
//   ① mix() 里 wantFrames（帧）与 ringSize_（样本数）单位混用：
//     take = min(wantFrames, ringSize_) 后每帧扣 2 个样本，
//     缓冲不足时 ringSize_ (size_t) 直接下溢成天文数字 → 回调从陈旧/
//     未写入区读数据 → 持续输出碎片噪声（听感即「乱七八糟、像快进」）；
//   ② 核心率 ≠ 设备率（44100→48000 极常见）时的手写重采样步进逻辑
//     完全错误（advance 计算错位 → 反复读同两个样本 → 蜂鸣）。
//   新实现：环形缓冲一律以【立体声帧】为单位；等率直拷；异率用
//   SDL_AudioStream（官方重采样器）；欠载补静音（绝不输出垃圾）；
//   溢出（快进）超阈值清空防延迟发散。
#pragma once
#include <cstdint>
#include <cstddef>
#include <mutex>
#include <vector>

#include <SDL.h>

namespace ns {

class AudioBridge {
 public:
  ~AudioBridge();

  // ★v1.2.6 bufferMs：音频缓冲毫秒（对齐 Android audioBufferMs，
  //   默认 64；32/64/128/256 由设置页选择，越大越稳、延迟越高）
  bool start(int deviceRate = 48000, int bufferMs = 64);
  void stop();
  bool running() const { return dev_ != 0; }

  // 核心输出格式（retro_get_system_av_info 后调用；可运行中变更）
  void setCoreFormat(int rate, int channels);

  // 核心音频数据入口（核心线程 = 主线程）
  void push(const int16_t* samples, size_t frames);

  void clear();
  void setFastForward(bool ff) { fastForward_ = ff; }
  void setMuted(bool m) { muted_ = m; }
  bool muted() const { return muted_; }

  // 缓冲健康度（0..1，1=满）— UI 音频延迟指示
  float bufferFill() const;

 private:
  static void SDLCALL audioCallback(void* userdata, Uint8* stream, int len);
  void mix(Uint8* stream, int lenBytes);

  SDL_AudioDeviceID dev_ = 0;
  mutable std::mutex mtx_;
  // 等率环形缓冲：立体声帧交错（L,R,L,R…），容量单位 = 帧
  std::vector<int16_t> ring_;
  size_t ringPos_ = 0;    // 读位置（帧）
  size_t ringSize_ = 0;   // 有效帧数
  // 异率重采样（SDL_AudioStream：核心率/声道 → 设备率立体声）
  SDL_AudioStream* stream_ = nullptr;
  int streamSrcRate_ = 0;    // stream_ 建立时的源采样率
  int streamSrcChans_ = 0;   // stream_ 建立时的源声道数
  int coreRate_ = 48000;
  int coreChannels_ = 2;
  int deviceRate_ = 48000;
  bool fastForward_ = false;
  bool muted_ = false;
  static constexpr size_t kMaxFrames = 48000;      // 等率环形缓冲 1s @48k
  static constexpr size_t kStreamCapFrames = 24000; // 异率流缓冲上限 0.5s
};

}  // namespace ns
