// NesStation PC — 音频桥：libretro 核心音频 → SDL 音频设备
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

  bool start(int deviceRate = 48000);
  void stop();
  bool running() const { return dev_ != 0; }

  // 核心输出格式（retro_get_system_av_info 后调用）
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
  SDL_AudioStream* stream_ = nullptr;
  mutable std::mutex mtx_;
  std::vector<int16_t> ring_;
  size_t ringPos_ = 0;   // 读位置
  size_t ringSize_ = 0;  // 有效帧数
  int coreRate_ = 48000;
  int coreChannels_ = 2;
  int deviceRate_ = 48000;
  bool fastForward_ = false;
  bool muted_ = false;
  static constexpr size_t kMaxFrames = 48000;  // 1s @48k stereo frames
};

}  // namespace ns
