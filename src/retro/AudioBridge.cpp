#include "retro/AudioBridge.h"

#include <algorithm>
#include <cstring>

#include "util/Log.h"

namespace ns {

AudioBridge::~AudioBridge() { stop(); }

bool AudioBridge::start(int deviceRate, int bufferMs) {
  if (dev_) return true;
  deviceRate_ = deviceRate;

  SDL_AudioSpec want{}, have{};
  want.freq = deviceRate;
  want.format = AUDIO_S16SYS;
  want.channels = 2;
  // ★v1.2.6 音频缓冲毫秒（对齐 Android AppSettings.audioBufferMs，默认 64ms；
  //   越大越稳防断音、延迟越高）—— samples 取最近的 2 的幂（SDL 要求）
  if (bufferMs <= 0) bufferMs = 64;
  int samples = (deviceRate * bufferMs + 500) / 1000;
  int pow2 = 16;
  while (pow2 < samples) pow2 <<= 1;
  if (std::abs(pow2 - samples) > std::abs((pow2 >> 1) - samples)) pow2 >>= 1;
  if (pow2 < 16) pow2 = 16;
  if (pow2 > 4096) pow2 = 4096;
  want.samples = (Uint16)pow2;
  want.callback = &AudioBridge::audioCallback;
  want.userdata = this;

  dev_ = SDL_OpenAudioDevice(nullptr, 0, &want, &have, 0);
  if (!dev_) {
    LOGW("SDL_OpenAudioDevice 失败：%s（将静音运行）", SDL_GetError());
    return false;
  }
  ring_.assign(kMaxFrames * 2, 0);
  ringPos_ = 0;
  ringSize_ = 0;
  SDL_PauseAudioDevice(dev_, 0);
  LOGI("音频设备开启：%dHz S16 ×2 缓冲 %d 样本 ≈%dms（核心 %dHz ×%d）",
       have.freq, have.samples, (int)(have.samples * 1000LL / have.freq), coreRate_,
       coreChannels_);
  return true;
}

void AudioBridge::stop() {
  if (dev_) {
    SDL_PauseAudioDevice(dev_, 1);
    SDL_CloseAudioDevice(dev_);
    dev_ = 0;
  }
  {
    std::lock_guard<std::mutex> lock(mtx_);
    ring_.clear();
    ringSize_ = 0;
    ringPos_ = 0;
  }
}

void AudioBridge::setCoreFormat(int rate, int channels) {
  std::lock_guard<std::mutex> lock(mtx_);
  coreRate_ = rate > 0 ? rate : 48000;
  coreChannels_ = (channels == 1 || channels == 2) ? channels : 2;
  ringSize_ = 0;
}

void AudioBridge::push(const int16_t* samples, size_t frames) {
  if (!dev_ || !samples || frames == 0) return;
  std::lock_guard<std::mutex> lock(mtx_);
  // 单声道 → 立体声
  static thread_local std::vector<int16_t> expandBuf;
  const int16_t* src = samples;
  size_t srcCount = frames;
  if (coreChannels_ == 1) {
    expandBuf.resize(frames * 2);
    for (size_t i = 0; i < frames; i++) {
      expandBuf[i * 2] = samples[i];
      expandBuf[i * 2 + 1] = samples[i];
    }
    src = expandBuf.data();
    srcCount = frames * 2;
  }
  const size_t capacity = kMaxFrames * 2;
  if (ringSize_ + srcCount > capacity) {
    // 溢出：丢弃旧数据（快进时正常现象）
    const size_t overflow = ringSize_ + srcCount - capacity;
    ringPos_ = (ringPos_ + overflow) % capacity;
    ringSize_ -= overflow;
  }
  for (size_t i = 0; i < srcCount; i++) {
    const size_t pos = (ringPos_ + ringSize_) % capacity;
    ring_[pos] = src[i];
    ringSize_++;
  }
}

void AudioBridge::clear() {
  std::lock_guard<std::mutex> lock(mtx_);
  ringSize_ = 0;
}

float AudioBridge::bufferFill() const {
  std::lock_guard<std::mutex> lock(mtx_);
  return (float)ringSize_ / (float)(kMaxFrames * 2);
}

void SDLCALL AudioBridge::audioCallback(void* userdata, Uint8* stream, int len) {
  static_cast<AudioBridge*>(userdata)->mix(stream, len);
}

void AudioBridge::mix(Uint8* stream, int lenBytes) {
  std::memset(stream, 0, (size_t)lenBytes);
  if (muted_) {
    // 静音但仍消费缓冲
    std::lock_guard<std::mutex> lock(mtx_);
    const size_t framesToDrop = (size_t)lenBytes / 4;
    const size_t drop = std::min(framesToDrop, ringSize_);
    ringPos_ = (ringPos_ + drop) % (kMaxFrames * 2);
    ringSize_ -= drop;
    return;
  }

  int16_t* out = reinterpret_cast<int16_t*>(stream);
  const size_t wantFrames = (size_t)lenBytes / 4;  // 立体声帧
  size_t got = 0;

  std::lock_guard<std::mutex> lock(mtx_);
  // 简单线性重采样（核心率 → 设备率）
  if (coreRate_ == deviceRate_) {
    const size_t take = std::min(wantFrames, ringSize_);
    for (size_t i = 0; i < take; i++) {
      out[i * 2] = ring_[ringPos_];
      out[i * 2 + 1] = ring_[(ringPos_ + 1) % (kMaxFrames * 2)];
      ringPos_ = (ringPos_ + 2) % (kMaxFrames * 2);
      ringSize_ -= 2;
    }
    // got = take
  } else {
    // 逐帧按比率取
    double pos = 0.0;
    const double step = (double)coreRate_ / (double)deviceRate_;
    while (got < wantFrames && ringSize_ >= 2) {
      const int16_t l = ring_[ringPos_];
      const int16_t r = ring_[(ringPos_ + 1) % (kMaxFrames * 2)];
      out[got * 2] = l;
      out[got * 2 + 1] = r;
      got++;
      const size_t advance = (size_t)(pos + step) - (size_t)pos;
      pos += step;
      const size_t adv = std::min(advance * 2, ringSize_);
      ringPos_ = (ringPos_ + adv) % (kMaxFrames * 2);
      ringSize_ -= adv;
    }
  }
}

}  // namespace ns
