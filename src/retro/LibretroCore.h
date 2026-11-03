// NesStation PC — libretro 宿主：核心加载 / 环境回调 / 游戏运行 / 快速存档
#pragma once
#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <vector>

#include <libretro.h>

namespace ns {

class AudioBridge;
class VideoBridge;
class InputManager;

// 规范化后的核心选项（v1/v2 统一）
struct CoreOption {
  std::string key;
  std::string desc;        // 显示名（优先 categorized）
  std::string info;        // 说明
  std::string categoryKey; // 分类 key（可空）
  std::vector<std::string> values;
  std::vector<std::string> labels;
  int defaultValue = 0;
  bool visible = true;
};

// 磁盘控制（FDS / PCE-CD / Mega-CD 换碟）
class DiskControl {
 public:
  void set(const struct retro_disk_control_ext_callback* cb) { ext_ = cb; }
  void setLegacy(const struct retro_disk_control_callback* cb) { legacy_ = cb; }
  bool available() const { return ext_ != nullptr || legacy_ != nullptr; }
  unsigned numImages();
  std::string imageLabel(unsigned idx);
  bool eject();
  bool insert();
  bool select(unsigned idx);  // 组合：弹出→选盘→插入

 private:
  const struct retro_disk_control_ext_callback* ext_ = nullptr;
  const struct retro_disk_control_callback* legacy_ = nullptr;
};

class LibretroCore {
 public:
  LibretroCore(AudioBridge& audio, VideoBridge& video, InputManager& input);
  ~LibretroCore();

  // —— 加载 / 卸载 ——
  // optionResolver: key → 当前值（CoreManager 持久层）；返回空表示用默认
  bool load(const std::string& corePath, const std::string& systemDir,
            const std::string& saveDir,
            std::function<std::string(const std::string&)> optionResolver);
  void unload();

  bool loaded() const { return handle_ != nullptr; }
  bool gameLoaded() const { return gameLoaded_; }
  const std::string& lastError() const { return lastError_; }
  const std::string& coreName() const { return coreName_; }
  const std::string& coreVersion() const { return coreVersion_; }
  bool needsFullpath() const { return needsFullpath_; }
  bool supportsNoGame() const { return noGame_; }

  // —— 游戏生命周期 ——
  bool loadGame(const std::string& romPath);  // 按核心需求走 fullpath 或内存加载
  void runFrame();
  void reset();
  void unloadGame();

  // 双摇杆端口设备声明
  void setControllerPorts();

  // —— 快速存档 ——
  size_t serializeSize();
  bool serialize(std::vector<uint8_t>* out);
  bool unserialize(const uint8_t* data, size_t size);

  // —— 核心选项 ——
  const std::vector<CoreOption>& options() const { return options_; }
  // 应用新值（更新内部值表并通知核心变量已变更）
  void setOptionValue(const std::string& key, const std::string& value);
  // 取核心声明的默认值（供 UI「恢复默认」）
  std::string optionDefaultValue(const std::string& key) const;

  // —— 输入描述符（核心提供的按键名，设置页显示用）——
  std::string inputDescriptor(unsigned port, unsigned id) const;

  // —— 磁盘控制 ——
  DiskControl& diskControl() { return disk_; }

  // 快进状态（GET_FASTFORWARDING / GET_AUDIO_VIDEO_ENABLE 响应）
  void setFastForward(bool ff) { fastForward_ = ff; }

  // AV 信息就绪（loadGame 后）
  bool avReady() const { return avReady_; }
  unsigned videoBaseW() const;
  unsigned videoBaseH() const;
  double timingFps() const { return timingFps_; }

  struct SystemInfo {
    std::string libraryName, libraryVersion, validExtensions;
    bool needFullpath = false, blockExtract = false;
  };
  const SystemInfo& systemInfo() const { return sysInfo_; }

 private:
  // retro_* 函数表
#define NS_RETRO_FUNC(name, decl) decl
  void (*retroInit_)() = nullptr;
  void (*retroDeinit_)() = nullptr;
  unsigned (*retroApiVersion_)() = nullptr;
  void (*retroGetSystemInfo_)(struct retro_system_info*) = nullptr;
  void (*retroGetSystemAvInfo_)(struct retro_system_av_info*) = nullptr;
  void (*retroSetEnvironment_)(retro_environment_t) = nullptr;
  void (*retroSetVideoRefresh_)(retro_video_refresh_t) = nullptr;
  void (*retroSetAudioSample_)(retro_audio_sample_t) = nullptr;
  void (*retroSetAudioSampleBatch_)(retro_audio_sample_batch_t) = nullptr;
  void (*retroSetInputPoll_)(retro_input_poll_t) = nullptr;
  void (*retroSetInputState_)(retro_input_state_t) = nullptr;
  void (*retroSetControllerPortDevice_)(unsigned, unsigned) = nullptr;
  bool (*retroLoadGame_)(const struct retro_game_info*) = nullptr;
  void (*retroReset_)() = nullptr;
  void (*retroRun_)() = nullptr;
  size_t (*retroSerializeSize_)() = nullptr;
  bool (*retroSerialize_)(void*, size_t) = nullptr;
  bool (*retroUnserialize_)(const void*, size_t) = nullptr;
  void (*retroUnloadGame_)() = nullptr;
  unsigned (*retroGetRegion_)() = nullptr;
  void* (*retroGetMemoryData_)(unsigned) = nullptr;
  size_t (*retroGetMemorySize_)(unsigned) = nullptr;
#undef NS_RETRO_FUNC

  // 静态回调 → 实例
  static void cbVideoRefresh(const void* data, unsigned w, unsigned h, size_t pitch);
  static void cbAudioSample(int16_t left, int16_t right);
  static size_t cbAudioBatch(const int16_t* data, size_t frames);
  static void cbInputPoll();
  static int16_t cbInputState(unsigned port, unsigned device, unsigned index, unsigned id);
  static bool cbEnvironment(unsigned cmd, void* data);
  static void cbLog(enum retro_log_level level, const char* fmt, ...);

  bool environment(unsigned cmd, void* data);
  void parseOptionsV2(const struct retro_core_options_v2* opts);
  void parseOptionsV1(const struct retro_core_option_definition* defs);
  void parseVariablesLegacy(const struct retro_variable* vars);
  void fillOptionDefaults();
  void applyAvInfo(const struct retro_system_av_info* av);

  AudioBridge& audio_;
  VideoBridge& video_;
  InputManager& input_;

  void* handle_ = nullptr;
  std::string corePath_;
  std::string systemDir_;
  std::string saveDir_;
  std::string lastError_;
  std::string coreName_, coreVersion_;

  SystemInfo sysInfo_;
  bool needsFullpath_ = false;
  bool noGame_ = false;
  bool gameLoaded_ = false;
  bool avReady_ = false;
  bool fastForward_ = false;
  bool variablesUpdated_ = false;
  double timingFps_ = 60.0;

  std::vector<CoreOption> options_;
  std::map<std::string, std::string> currentValues_;  // key → value（本会话）
  std::map<uint64_t, std::string> inputDescriptors_;  // (port<<32|id) → desc
  std::function<std::string(const std::string&)> optionResolver_;

  DiskControl disk_;
  retro_keyboard_event_t coreKeyCb_ = nullptr;
};

}  // namespace ns
