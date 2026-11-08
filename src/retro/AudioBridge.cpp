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
  // 音频缓冲毫秒（对齐 Android AppSettings.audioBufferMs，默认 64ms）
  //   samples 取最近的 2 的幂（SDL 回调粒度，非总缓冲）
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
  // ★v1.3.0：设备实际开启的采样率可能不同（驱动协商）——必须以 have 为准，
  //   旧版固定用请求值，设备率不符时重采样比率全错（音调偏差/噪声）
  deviceRate_ = have.freq > 0 ? have.freq : deviceRate;
  ring_.assign(kMaxFrames * 2, 0);
  ringPos_ = 0;
  ringSize_ = 0;
  coreRate_ = deviceRate_;  // 会话内 setCoreFormat 覆盖
  SDL_PauseAudioDevice(dev_, 0);
  LOGI("音频设备开启：%dHz S16 ×2 回调 %d 样本（核心 %dHz ×%d → 设备 %dHz）",
       have.freq, have.samples, coreRate_, coreChannels_, deviceRate_);
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
    if (stream_) {
      SDL_FreeAudioStream(stream_);
      stream_ = nullptr;
    }
    ring_.clear();
    ringSize_ = 0;
    ringPos_ = 0;
  }
}

void AudioBridge::setCoreFormat(int rate, int channels) {
  std::lock_guard<std::mutex> lock(mtx_);
  coreRate_ = rate > 0 ? rate : 48000;
  coreChannels_ = (channels == 1 || channels == 2) ? channels : 2;
  ringSize_ = 0;  // 换核心：旧缓冲作废（防新旧采样率数据混播）
  // 等率直拷路径；异率需要 SDL_AudioStream —— 延迟创建（push/mix 共用锁，
  // Put/Get 两个入口都被 mtx_ 串行化，满足 SDL_AudioStream 非线程安全约束）
}

void AudioBridge::push(const int16_t* samples, size_t frames) {
  if (!dev_ || !samples || frames == 0) return;
  std::lock_guard<std::mutex> lock(mtx_);

  // 单声道 → 立体声展开
  static thread_local std::vector<int16_t> expandBuf;
  const int16_t* src = samples;
  if (coreChannels_ == 1) {
    expandBuf.resize(frames * 2);
    for (size_t i = 0; i < frames; i++) {
      expandBuf[i * 2] = samples[i];
      expandBuf[i * 2 + 1] = samples[i];
    }
    src = expandBuf.data();
  }

  if (coreRate_ == deviceRate_) {
    // —— 等率：写入环形缓冲（单位 = 帧，杜绝旧版帧/样本混用下溢）——
    const size_t capacity = kMaxFrames;
    // ★v1.3.2 溢出修复：旧版 overflow = ringSize_ + frames - capacity，
    //   缓冲为空（ringSize_=0）时推入超容量数据（快进连跑多帧）会
    //   ringSize_ -= overflow → size_t 下溢成 SIZE_MAX，缓冲状态全错
    //   （爆音/杂音的又一来源）。现分两段处理：
    //     1) 单次写入本身超容量 → 只保留最新 capacity 帧（丢新数据头部）；
    //     2) 常规溢出 → 丢最旧（overflow ≤ ringSize_，减法安全）。
    if (frames >= capacity) {
      src += (frames - capacity) * 2;
      frames = capacity;
    }
    if (ringSize_ + frames > capacity) {
      const size_t overflow = ringSize_ + frames - capacity;
      ringPos_ = (ringPos_ + overflow) % capacity;
      ringSize_ -= overflow;
    }
    for (size_t i = 0; i < frames; i++) {
      const size_t pos = (ringPos_ + ringSize_) % capacity;
      ring_[pos * 2] = src[i * 2];
      ring_[pos * 2 + 1] = src[i * 2 + 1];
      ringSize_++;
    }
    return;
  }

  // —— 异率：SDL_AudioStream（官方线性重采样，44100→48000 等场景）——
  if (!stream_ || streamSrcRate_ != coreRate_ || streamSrcChans_ != coreChannels_) {
    if (stream_) SDL_FreeAudioStream(stream_);
    stream_ = SDL_NewAudioStream(AUDIO_S16SYS, coreChannels_, coreRate_,
                                 AUDIO_S16SYS, 2, deviceRate_);
    streamSrcRate_ = coreRate_;
    streamSrcChans_ = coreChannels_;
    if (!stream_) {
      LOGW("SDL_NewAudioStream(%d→%d) 失败：%s", coreRate_, deviceRate_,
           SDL_GetError());
      return;
    }
    LOGI("音频重采样开启：%dHz ×%d → %dHz ×2", coreRate_, coreChannels_,
         deviceRate_);
  }
  // ★v1.3.2 堆积清空时机修复：旧版在 Put 之后检查 available，
  //   核心单次大块提交（≥0.5s 音频，如补帧/换 core 首批）会立即被
  //   SDL_AudioStreamClear 清光 → 整段无声。改为 Put 之前检查
  //   「上一轮残留」：快进持续堆积仍会被清（延迟不发散），
  //   而单次大块提交完整保留。
  if ((size_t)SDL_AudioStreamAvailable(stream_) >
      kStreamCapFrames * 2 * (size_t)sizeof(int16_t)) {
    SDL_AudioStreamClear(stream_);
  }
  const int bytes = (int)(frames * coreChannels_ * sizeof(int16_t));
  if (SDL_AudioStreamPut(stream_, src, bytes) != 0) return;
}

void AudioBridge::clear() {
  std::lock_guard<std::mutex> lock(mtx_);
  ringSize_ = 0;
  if (stream_) SDL_AudioStreamClear(stream_);
}

float AudioBridge::bufferFill() const {
  std::lock_guard<std::mutex> lock(mtx_);
  if (coreRate_ == deviceRate_)
    return (float)ringSize_ / (float)kMaxFrames;
  const int avail = stream_ ? SDL_AudioStreamAvailable(stream_) : 0;
  return (float)avail / (float)(kMaxFrames * 2 * (int)sizeof(int16_t));
}

void SDLCALL AudioBridge::audioCallback(void* userdata, Uint8* stream, int len) {
  static_cast<AudioBridge*>(userdata)->mix(stream, len);
}

void AudioBridge::mix(Uint8* stream, int lenBytes) {
  // 静音：仍按量消费缓冲（保持缓冲水位与时间同步，解除静音立即正常）
  if (muted_) {
    std::memset(stream, 0, (size_t)lenBytes);
    std::lock_guard<std::mutex> lock(mtx_);
    const size_t framesToDrop = (size_t)lenBytes / 4;
    if (coreRate_ == deviceRate_) {
      const size_t drop = std::min(framesToDrop, ringSize_);
      ringPos_ = (ringPos_ + drop) % kMaxFrames;
      ringSize_ -= drop;
    } else if (stream_) {
      Uint8 sink[4096];
      size_t left = (size_t)lenBytes;
      while (left > 0) {
        const int chunk = (int)std::min<size_t>(left, sizeof(sink));
        const int got = SDL_AudioStreamGet(stream_, sink, chunk);
        if (got <= 0) break;
        left -= (size_t)got;
      }
    }
    return;
  }

  int16_t* out = reinterpret_cast<int16_t*>(stream);
  const size_t wantFrames = (size_t)lenBytes / 4;  // 立体声帧
  size_t got = 0;

  std::lock_guard<std::mutex> lock(mtx_);
  if (coreRate_ == deviceRate_) {
    // —— 等率直拷：不足部分补静音（欠载是短暂无声，绝不是垃圾噪声）——
    const size_t take = std::min(wantFrames, ringSize_);
    for (size_t i = 0; i < take; i++) {
      out[i * 2] = ring_[ringPos_ * 2];
      out[i * 2 + 1] = ring_[ringPos_ * 2 + 1];
      ringPos_ = (ringPos_ + 1) % kMaxFrames;
    }
    ringSize_ -= take;
    got = take;
  } else if (stream_) {
    // —— 异率：从重采样流取（SDL 保证输出格式 = 设备格式）——
    int total = SDL_AudioStreamGet(stream_, stream, lenBytes);
    if (total > 0) got = (size_t)total / 4;
  }
  if (got < wantFrames) {
    // 欠载：剩余帧补零（核心线程暂时供给不足的正常表现）
    std::memset(out + got * 2, 0, (wantFrames - got) * 4);
  }
}

}  // namespace ns
