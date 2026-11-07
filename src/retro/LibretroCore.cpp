#include "retro/LibretroCore.h"

#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <mutex>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <dlfcn.h>
#endif

#include "retro/AudioBridge.h"
#include "retro/InputManager.h"
#include "retro/VideoBridge.h"
#include "util/Fs.h"
#include "util/Log.h"

namespace ns {

namespace {
LibretroCore* g_activeCore = nullptr;  // 回调跳板（单会话）

void* nsDlopen(const char* path) {
#ifdef _WIN32
  // ★v1.3.0 宽字符：中文路径的核心 dll 也能加载（旧版 LoadLibraryA 按 ACP 解）
  return (void*)LoadLibraryW(fsutil::toWide(path).c_str());
#else
  return dlopen(path, RTLD_NOW | RTLD_LOCAL);
#endif
}
void* nsDlsym(void* h, const char* name) {
#ifdef _WIN32
  return (void*)GetProcAddress((HMODULE)h, name);
#else
  return dlsym(h, name);
#endif
}
void nsDlclose(void* h) {
#ifdef _WIN32
  FreeLibrary((HMODULE)h);
#else
  dlclose(h);
#endif
}
const char* nsDlError() {
#ifdef _WIN32
  return "LoadLibrary failed";
#else
  const char* e = dlerror();
  return e ? e : "dlopen failed";
#endif
}
}  // namespace

// —— DiskControl —————————————————————————————————————————
unsigned DiskControl::numImages() {
  if (ext_ && ext_->get_num_images) return ext_->get_num_images();
  return 0;
}
std::string DiskControl::imageLabel(unsigned idx) {
  if (ext_ && ext_->get_image_label) {
    char buf[256] = {0};
    if (ext_->get_image_label(idx, buf, sizeof(buf))) return buf;
  }
  return "磁盘 " + std::to_string(idx + 1);
}
bool DiskControl::eject() {
  if (ext_ && ext_->set_eject_state) return ext_->set_eject_state(true);
  if (legacy_ && legacy_->set_eject_state) return legacy_->set_eject_state(true);
  return false;
}
bool DiskControl::insert() {
  if (ext_ && ext_->set_eject_state) return ext_->set_eject_state(false);
  if (legacy_ && legacy_->set_eject_state) return legacy_->set_eject_state(false);
  return false;
}
bool DiskControl::select(unsigned idx) {
  if (ext_ && ext_->set_image_index) {
    ext_->set_eject_state(true);
    ext_->set_image_index(idx);
    ext_->set_eject_state(false);
    return true;
  }
  if (legacy_ && legacy_->set_image_index) {
    legacy_->set_eject_state(true);
    legacy_->set_image_index(idx);
    legacy_->set_eject_state(false);
    return true;
  }
  return false;
}

// —— LibretroCore ————————————————————————————————————————
LibretroCore::LibretroCore(AudioBridge& audio, VideoBridge& video, InputManager& input)
    : audio_(audio), video_(video), input_(input) {}

LibretroCore::~LibretroCore() { unload(); }

bool LibretroCore::load(const std::string& corePath, const std::string& systemDir,
                        const std::string& saveDir,
                        std::function<std::string(const std::string&)> optionResolver) {
  unload();
  corePath_ = corePath;
  systemDir_ = systemDir;
  saveDir_ = saveDir;
  LOGI("加载核心：%s", corePath.c_str());

  handle_ = nsDlopen(corePath.c_str());
  if (!handle_) {
    lastError_ = std::string("打开核心库失败：") + nsDlError();
    LOGE("%s", lastError_.c_str());
    return false;
  }

#define NS_LOAD_SYM(fn, sym)                                              \
  do {                                                                    \
    retro##fn##_ = (decltype(retro##fn##_))nsDlsym(handle_, #sym);        \
    if (!retro##fn##_) {                                                  \
      lastError_ = "核心缺少符号 " #sym;                                  \
      LOGE("%s", lastError_.c_str());                                     \
      unload();                                                           \
      return false;                                                       \
    }                                                                     \
  } while (0)

  NS_LOAD_SYM(Init, retro_init);
  NS_LOAD_SYM(Deinit, retro_deinit);
  NS_LOAD_SYM(ApiVersion, retro_api_version);
  NS_LOAD_SYM(GetSystemInfo, retro_get_system_info);
  NS_LOAD_SYM(GetSystemAvInfo, retro_get_system_av_info);
  NS_LOAD_SYM(SetEnvironment, retro_set_environment);
  NS_LOAD_SYM(SetVideoRefresh, retro_set_video_refresh);
  NS_LOAD_SYM(SetAudioSample, retro_set_audio_sample);
  NS_LOAD_SYM(SetAudioSampleBatch, retro_set_audio_sample_batch);
  NS_LOAD_SYM(SetInputPoll, retro_set_input_poll);
  NS_LOAD_SYM(SetInputState, retro_set_input_state);
  NS_LOAD_SYM(SetControllerPortDevice, retro_set_controller_port_device);
  {
    retroLoadGame_ = (bool (*)(const struct retro_game_info*))nsDlsym(handle_, "retro_load_game");
    if (!retroLoadGame_) {
      lastError_ = "核心缺少符号 retro_load_game";
      LOGE("%s", lastError_.c_str());
      unload();
      return false;
    }
  }
  NS_LOAD_SYM(Reset, retro_reset);
  NS_LOAD_SYM(Run, retro_run);
  NS_LOAD_SYM(SerializeSize, retro_serialize_size);
  NS_LOAD_SYM(Serialize, retro_serialize);
  NS_LOAD_SYM(Unserialize, retro_unserialize);
  NS_LOAD_SYM(UnloadGame, retro_unload_game);
  NS_LOAD_SYM(GetRegion, retro_get_region);
#undef NS_LOAD_SYM
  // 可选符号
  retroGetMemoryData_ = (void* (*)(unsigned))nsDlsym(handle_, "retro_get_memory_data");
  retroGetMemorySize_ = (size_t (*)(unsigned))nsDlsym(handle_, "retro_get_memory_size");

  // —— 会话上下文（回调需要 this；g_activeCore 在 unload 后恢复）——
  LibretroCore* prev = g_activeCore;
  g_activeCore = this;
  (void)prev;

  retroSetEnvironment_(&LibretroCore::cbEnvironment);
  retroSetVideoRefresh_(&LibretroCore::cbVideoRefresh);
  retroSetAudioSample_(&LibretroCore::cbAudioSample);
  retroSetAudioSampleBatch_(&LibretroCore::cbAudioBatch);
  retroSetInputPoll_(&LibretroCore::cbInputPoll);
  retroSetInputState_(&LibretroCore::cbInputState);

  // 选项值预载（核心声明后即可解析 UI；GET_VARIABLE 时回查）
  optionResolver_ = std::move(optionResolver);

  struct retro_system_info info{};
  retroGetSystemInfo_(&info);
  sysInfo_.libraryName = info.library_name ? info.library_name : "?";
  sysInfo_.libraryVersion = info.library_version ? info.library_version : "";
  sysInfo_.validExtensions = info.valid_extensions ? info.valid_extensions : "";
  sysInfo_.needFullpath = info.need_fullpath != 0;
  sysInfo_.blockExtract = info.block_extract != 0;
  needsFullpath_ = sysInfo_.needFullpath;
  coreName_ = sysInfo_.libraryName;
  coreVersion_ = sysInfo_.libraryVersion;
  LOGI("核心就绪：%s %s（API %u，fullpath=%d）", coreName_.c_str(),
       coreVersion_.c_str(), retroApiVersion_(), (int)needsFullpath_);

  retroInit_();
  return true;
}

void LibretroCore::unload() {
  if (!handle_) return;
  if (gameLoaded_) {
    retroUnloadGame_();
    gameLoaded_ = false;
  }
  retroDeinit_();
  nsDlclose(handle_);
  handle_ = nullptr;
  options_.clear();
  currentValues_.clear();
  inputDescriptors_.clear();
  disk_ = DiskControl();
  if (coreKeyCb_) {
    input_.setCoreKeyboardCallback(nullptr);
    coreKeyCb_ = nullptr;
  }
  avReady_ = false;
  if (g_activeCore == this) g_activeCore = nullptr;
  LOGI("核心已卸载");
}

bool LibretroCore::loadGame(const std::string& romPath) {
  if (!loaded()) {
    lastError_ = "核心未加载";
    return false;
  }
  struct retro_game_info gi{};
  gi.path = romPath.c_str();

  std::vector<uint8_t> data;
  if (!needsFullpath_) {
    FILE* f = fsutil::openFile(romPath, "rb");  // ★v1.3.0 中文路径可用
    if (f) {
      fseek(f, 0, SEEK_END);
      const long sz = ftell(f);
      fseek(f, 0, SEEK_SET);
      if (sz > 0) {
        data.resize((size_t)sz);
        if (fread(data.data(), 1, data.size(), f) != data.size()) data.clear();
      }
      fclose(f);
    }
    if (!data.empty()) {
      gi.data = data.data();
      gi.size = data.size();
    }
  }

  LOGI("载入游戏：%s（%s，%zu 字节）", romPath.c_str(),
       gi.data ? "内存" : "fullpath", (size_t)gi.size);
  if (!retroLoadGame_(&gi)) {
    lastError_ = "retro_load_game 失败（ROM 损坏 / BIOS 缺失 / 格式不支持）";
    LOGE("%s", lastError_.c_str());
    return false;
  }
  gameLoaded_ = true;

  struct retro_system_av_info av{};
  retroGetSystemAvInfo_(&av);
  applyAvInfo(&av);
  setControllerPorts();
  return true;
}

void LibretroCore::applyAvInfo(const struct retro_system_av_info* av) {
  video_.setAvInfo(av->geometry.base_width, av->geometry.base_height,
                   av->geometry.aspect_ratio, av->geometry.max_width,
                   av->geometry.max_height);
  audio_.setCoreFormat(av->timing.sample_rate, 2);
  timingFps_ = av->timing.fps > 1.0 ? av->timing.fps : 60.0;
  avReady_ = true;
  LOGI("AV：%ux%u @%.2ffps，音频 %uHz", av->geometry.base_width,
       av->geometry.base_height, av->timing.fps, av->timing.sample_rate);
}

void LibretroCore::setControllerPorts() {
  if (retroSetControllerPortDevice_) {
    retroSetControllerPortDevice_(0, RETRO_DEVICE_JOYPAD);
    retroSetControllerPortDevice_(1, RETRO_DEVICE_JOYPAD);
  }
}

void LibretroCore::runFrame() {
  if (gameLoaded_ && retroRun_) retroRun_();
}

void LibretroCore::reset() {
  if (gameLoaded_ && retroReset_) retroReset_();
}

void LibretroCore::unloadGame() {
  if (gameLoaded_ && retroUnloadGame_) retroUnloadGame_();
  gameLoaded_ = false;
}

size_t LibretroCore::serializeSize() {
  if (!gameLoaded_ || !retroSerializeSize_) return 0;
  return retroSerializeSize_();
}
bool LibretroCore::serialize(std::vector<uint8_t>* out) {
  const size_t sz = serializeSize();
  if (!sz) return false;
  out->resize(sz);
  return retroSerialize_(out->data(), sz);
}
bool LibretroCore::unserialize(const uint8_t* data, size_t size) {
  if (!gameLoaded_ || !size) return false;
  return retroUnserialize_(data, size);
}

// —— 选项 ————————————————————————————————————————————
std::string LibretroCore::optionDefaultValue(const std::string& key) const {
  for (const auto& o : options_)
    if (o.key == key && !o.values.empty())
      return o.values[(size_t)o.defaultValue < o.values.size() ? (size_t)o.defaultValue : 0];
  return "";
}

void LibretroCore::setOptionValue(const std::string& key, const std::string& value) {
  currentValues_[key] = value;
  variablesUpdated_ = true;
  LOGI("核心选项 %s = %s", key.c_str(), value.c_str());
}

std::string LibretroCore::inputDescriptor(unsigned port, unsigned id) const {
  const auto it = inputDescriptors_.find(((uint64_t)port << 32) | id);
  return it != inputDescriptors_.end() ? it->second : "";
}

unsigned LibretroCore::videoBaseW() const { return video_.frameWidth(); }
unsigned LibretroCore::videoBaseH() const { return video_.frameHeight(); }

// —— 静态回调跳板 ————————————————————————————————
void LibretroCore::cbVideoRefresh(const void* data, unsigned w, unsigned h, size_t pitch) {
  if (g_activeCore) g_activeCore->video_.frame(data, w, h, pitch);
}
void LibretroCore::cbAudioSample(int16_t l, int16_t r) {
  if (g_activeCore) {
    const int16_t s[2] = {l, r};
    g_activeCore->audio_.push(s, 1);
  }
}
size_t LibretroCore::cbAudioBatch(const int16_t* data, size_t frames) {
  if (g_activeCore && data) g_activeCore->audio_.push(data, frames);
  return frames;
}
void LibretroCore::cbInputPoll() {
  // 输入状态在 InputManager::endFrame 轮询，这里无需处理
}
int16_t LibretroCore::cbInputState(unsigned port, unsigned device, unsigned index,
                                   unsigned id) {
  if (!g_activeCore) return 0;
  // bitmask 查询展开为逐键
  if (device == RETRO_DEVICE_JOYPAD && index == RETRO_DEVICE_ID_JOYPAD_MASK) {
    int16_t mask = 0;
    for (unsigned b = 0; b < 16; b++)
      if (g_activeCore->input_.retroInputState(port, RETRO_DEVICE_JOYPAD, 0, b)) mask |= (1 << b);
    return mask;
  }
  return g_activeCore->input_.retroInputState(port, device, index, id);
}
void LibretroCore::cbLog(enum retro_log_level level, const char* fmt, ...) {
  char buf[2048];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);
  switch (level) {
    case RETRO_LOG_DEBUG: LOGD("[core] %s", buf); break;
    case RETRO_LOG_WARN: LOGW("[core] %s", buf); break;
    case RETRO_LOG_ERROR: LOGE("[core] %s", buf); break;
    default: LOGI("[core] %s", buf); break;
  }
}

// —— environment 实现 ————————————————————————————————
bool LibretroCore::cbEnvironment(unsigned cmd, void* data) {
  if (g_activeCore) return g_activeCore->environment(cmd, data);
  return false;
}

void LibretroCore::parseOptionsV2(const struct retro_core_options_v2* opts) {
  options_.clear();
  if (!opts) return;
  // 分类表
  std::map<std::string, std::string> catDesc;
  if (opts->categories) {
    for (const struct retro_core_option_v2_category* c = opts->categories;
         c && c->key; c++) {
      catDesc[c->key] = c->desc ? c->desc : c->key;
    }
  }
  for (const struct retro_core_option_v2_definition* d = opts->definitions; d && d->key;
       d++) {
    CoreOption o;
    o.key = d->key;
    o.desc = (d->category_key && *d->category_key && d->desc_categorized)
                 ? d->desc_categorized
                 : (d->desc ? d->desc : d->key);
    o.info = d->info ? d->info : "";
    if (d->category_key && *d->category_key) {
      auto it = catDesc.find(d->category_key);
      o.categoryKey = it != catDesc.end() ? it->second : d->category_key;
    }
    for (const struct retro_core_option_value* v = d->values; v && v->value; v++) {
      o.values.push_back(v->value);
      o.labels.push_back(v->label ? v->label : v->value);
    }
    o.defaultValue = 0;
    if (d->default_value) {
      for (size_t i = 0; i < o.values.size(); i++)
        if (o.values[i] == d->default_value) {
          o.defaultValue = (int)i;
          break;
        }
    }
    o.visible = true;
    if (!o.values.empty()) options_.push_back(std::move(o));
  }
}

void LibretroCore::parseOptionsV1(const struct retro_core_option_definition* defs) {
  options_.clear();
  if (!defs) return;
  for (const struct retro_core_option_definition* d = defs; d && d->key; d++) {
    CoreOption o;
    o.key = d->key;
    o.desc = d->desc ? d->desc : d->key;
    o.info = d->info ? d->info : "";
    for (const struct retro_core_option_value* v = d->values; v && v->value; v++) {
      o.values.push_back(v->value);
      o.labels.push_back(v->label ? v->label : v->value);
    }
    o.defaultValue = 0;
    if (d->default_value) {
      for (size_t i = 0; i < o.values.size(); i++)
        if (o.values[i] == d->default_value) {
          o.defaultValue = (int)i;
          break;
        }
    }
    o.visible = true;
    if (!o.values.empty()) options_.push_back(std::move(o));
  }
}

void LibretroCore::parseVariablesLegacy(const struct retro_variable* vars) {
  options_.clear();
  if (!vars) return;
  for (const struct retro_variable* v = vars; v && v->key; v++) {
    if (!v->value) continue;
    // v1 旧格式："描述; 选项1|选项2|..."
    std::string s = v->value;
    const size_t semi = s.find(';');
    if (semi == std::string::npos) continue;
    CoreOption o;
    o.key = v->key;
    o.desc = s.substr(0, semi);
    while (!o.desc.empty() && o.desc.back() == ' ') o.desc.pop_back();
    std::string rest = s.substr(semi + 1);
    std::string cur;
    for (size_t i = 0; i <= rest.size(); i++) {
      if (i == rest.size() || rest[i] == '|') {
        while (!cur.empty() && (cur.front() == ' ')) cur.erase(cur.begin());
        while (!cur.empty() && (cur.back() == ' ')) cur.pop_back();
        if (!cur.empty()) {
          o.values.push_back(cur);
          o.labels.push_back(cur);
        }
        cur.clear();
      } else {
        cur.push_back(rest[i]);
      }
    }
    o.defaultValue = 0;
    o.visible = true;
    if (!o.values.empty()) options_.push_back(std::move(o));
  }
}

void LibretroCore::fillOptionDefaults() {
  for (const auto& o : options_) {
    if (currentValues_.find(o.key) == currentValues_.end()) {
      const size_t di = (size_t)o.defaultValue < o.values.size() ? (size_t)o.defaultValue : 0;
      // 先查持久层
      std::string stored;
      if (optionResolver_) stored = optionResolver_(o.key);
      if (!stored.empty()) {
        bool ok = false;
        for (const auto& v : o.values)
          if (v == stored) { ok = true; break; }
        if (ok) {
          currentValues_[o.key] = stored;
          continue;
        }
      }
      currentValues_[o.key] = o.values[di];
    }
  }
}

bool LibretroCore::environment(unsigned cmd, void* data) {
  switch (cmd) {
    case RETRO_ENVIRONMENT_GET_OVERSCAN:
      *(bool*)data = false;
      return true;
    case RETRO_ENVIRONMENT_GET_CAN_DUPE:
      *(bool*)data = true;
      return true;
    case RETRO_ENVIRONMENT_SET_PIXEL_FORMAT: {
      const enum retro_pixel_format fmt = *(enum retro_pixel_format*)data;
      if ((int)fmt > (int)RETRO_PIXEL_FORMAT_XRGB8888) return false;
      video_.setPixelFormat(fmt);
      return true;
    }
    case RETRO_ENVIRONMENT_SET_SYSTEM_AV_INFO: {
      applyAvInfo((const struct retro_system_av_info*)data);
      return true;
    }
    case RETRO_ENVIRONMENT_SET_GEOMETRY: {
      const struct retro_game_geometry* g = (const struct retro_game_geometry*)data;
      video_.setGeometry(g->base_width, g->base_height, g->aspect_ratio);
      return true;
    }
    case RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY:
      *(const char**)data = systemDir_.c_str();
      return true;
    case RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY:
      *(const char**)data = saveDir_.c_str();
      return true;
    case RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME:
      noGame_ = *(const bool*)data;
      return true;
    case RETRO_ENVIRONMENT_GET_LOG_INTERFACE: {
      struct retro_log_callback* cb = (struct retro_log_callback*)data;
      cb->log = &LibretroCore::cbLog;
      return true;
    }
    case RETRO_ENVIRONMENT_GET_PERF_INTERFACE:
      return false;
    case RETRO_ENVIRONMENT_SET_PERFORMANCE_LEVEL:
      return true;
    case RETRO_ENVIRONMENT_SET_CONTROLLER_INFO:
      return true;
    case RETRO_ENVIRONMENT_SET_MEMORY_MAPS:
      return true;
    case RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS: {
      const struct retro_input_descriptor* d = (const struct retro_input_descriptor*)data;
      for (; d && d->port != 0xFF && d->description; d++) {
        if (d->device == RETRO_DEVICE_JOYPAD) {
          inputDescriptors_[((uint64_t)d->port << 32) | d->id] = d->description;
        }
      }
      return true;
    }
    case RETRO_ENVIRONMENT_SET_KEYBOARD_CALLBACK: {
      const struct retro_keyboard_callback* kb =
          (const struct retro_keyboard_callback*)data;
      coreKeyCb_ = kb->callback;
      input_.setCoreKeyboardCallback(coreKeyCb_);
      return true;
    }
    case RETRO_ENVIRONMENT_SET_DISK_CONTROL_INTERFACE:
      disk_.setLegacy((const struct retro_disk_control_callback*)data);
      return true;
    case RETRO_ENVIRONMENT_SET_DISK_CONTROL_EXT_INTERFACE:
      disk_.set((const struct retro_disk_control_ext_callback*)data);
      return true;
    case RETRO_ENVIRONMENT_GET_VARIABLE: {
      struct retro_variable* rv = (struct retro_variable*)data;
      if (!rv || !rv->key) return false;
      auto it = currentValues_.find(rv->key);
      if (it != currentValues_.end()) {
        rv->value = it->second.c_str();
        return true;
      }
      // 尚未 fillOptionDefaults（核心直接查询）：现查
      if (optionResolver_) {
        const std::string stored = optionResolver_(rv->key);
        if (!stored.empty()) {
          currentValues_[rv->key] = stored;
          rv->value = currentValues_[rv->key].c_str();
          return true;
        }
      }
      for (const auto& o : options_) {
        if (o.key == rv->key && !o.values.empty()) {
          const size_t di =
              (size_t)o.defaultValue < o.values.size() ? (size_t)o.defaultValue : 0;
          currentValues_[rv->key] = o.values[di];
          rv->value = currentValues_[rv->key].c_str();
          return true;
        }
      }
      rv->value = nullptr;
      return false;
    }
    case RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE: {
      *(bool*)data = variablesUpdated_;
      variablesUpdated_ = false;
      return true;
    }
    case RETRO_ENVIRONMENT_SET_VARIABLES:
      parseVariablesLegacy((const struct retro_variable*)data);
      fillOptionDefaults();
      return true;
    case RETRO_ENVIRONMENT_GET_CORE_OPTIONS_VERSION:
      *(unsigned*)data = 2;
      return true;
    case RETRO_ENVIRONMENT_SET_CORE_OPTIONS:
      parseOptionsV1((const struct retro_core_option_definition*)data);
      fillOptionDefaults();
      return true;
    case RETRO_ENVIRONMENT_SET_CORE_OPTIONS_V2:
      parseOptionsV2((const struct retro_core_options_v2*)data);
      fillOptionDefaults();
      return true;
    case RETRO_ENVIRONMENT_SET_CORE_OPTIONS_V2_INTL: {
      const struct retro_core_options_v2_intl* intl =
          (const struct retro_core_options_v2_intl*)data;
      parseOptionsV2(intl ? intl->us : nullptr);
      fillOptionDefaults();
      return true;
    }
    case RETRO_ENVIRONMENT_SET_CORE_OPTIONS_INTL: {
      const struct retro_core_options_intl* intl =
          (const struct retro_core_options_intl*)data;
      parseOptionsV1(intl && intl->us ? intl->us : nullptr);
      fillOptionDefaults();
      return true;
    }
    case RETRO_ENVIRONMENT_SET_CORE_OPTIONS_DISPLAY: {
      const struct retro_core_option_display* d =
          (const struct retro_core_option_display*)data;
      for (auto& o : options_)
        if (o.key == d->key) o.visible = d->visible;
      return true;
    }
    case RETRO_ENVIRONMENT_GET_INPUT_BITMASKS:
      return true;
    case RETRO_ENVIRONMENT_GET_INPUT_MAX_USERS:
      *(unsigned*)data = 2;
      return true;
    case RETRO_ENVIRONMENT_GET_LANGUAGE:
      *(unsigned*)data = RETRO_LANGUAGE_CHINESE_SIMPLIFIED;
      return true;
    case RETRO_ENVIRONMENT_GET_FASTFORWARDING:
      *(bool*)data = fastForward_;
      return true;
    case RETRO_ENVIRONMENT_GET_AUDIO_VIDEO_ENABLE: {
      int v = 0x1 | 0x2;  // video | audio
      if (fastForward_) v |= 0x1000;  // 快进提示（核心可自行降载）
      *(int*)data = v;
      return true;
    }
    case RETRO_ENVIRONMENT_SET_MINIMUM_AUDIO_LATENCY:
      return true;
    case RETRO_ENVIRONMENT_SET_AUDIO_BUFFER_STATUS_CALLBACK:
      return false;
    case RETRO_ENVIRONMENT_SET_FRAME_TIME_CALLBACK:
      return false;
    case RETRO_ENVIRONMENT_SET_SUBSYSTEM_INFO:
      return true;
    case RETRO_ENVIRONMENT_GET_VFS_INTERFACE:
      return false;  // 核心回退 stdio
    case RETRO_ENVIRONMENT_GET_LED_INTERFACE:
      return false;
    case RETRO_ENVIRONMENT_GET_MESSAGE_INTERFACE_VERSION:
      return false;
    case RETRO_ENVIRONMENT_GET_HW_RENDER_INTERFACE:
      return false;
    case RETRO_ENVIRONMENT_SET_HW_RENDER:
      return false;  // 核心回退软件渲染
    case RETRO_ENVIRONMENT_GET_PREFERRED_HW_RENDER:
      return false;
    case RETRO_ENVIRONMENT_GET_CURRENT_SOFTWARE_FRAMEBUFFER:
      return false;
    default:
      // 其余（消息 / 网络等）一律未支持
      return false;
  }
}

}  // namespace ns
