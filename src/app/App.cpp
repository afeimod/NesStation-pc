#include "app/App.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>

#include <stb_image.h>

#include "emu/EmuRunner.h"
#include "ui/Scenes.h"
#include "util/Fs.h"
#include "util/Log.h"
#include <nlohmann/json.hpp>

#include <stb_image_write.h>

namespace ns {

using nlohmann::json;

// —— Config ————————————————————————————————————————————
// —— APK 键值合法性校验（对齐 PadLayoutStore：非法值回退默认）——
static bool validVideoScale(const std::string& s) {
  return s == "stretch" || s == "4:3" || s == "3:2" || s == "8:7" || s == "16:9" ||
         s == "custom";
}
static bool validVideoFilter(const std::string& s) {
  return s == "none" || s == "scanline" || s == "crt" || s == "dot" || s == "xbr" ||
         s == "hq2x" || s == "hq4x" || s == "xbr_dot" || s == "4xbr" ||
         s == "4xbr_dot" || s == "hq4x_dot";
}

void Config::load(const std::string& file) {
  std::string text;
  if (!fsutil::readFile(file, &text)) return;
  try {
    const json j = json::parse(text, nullptr, false);
    if (j.is_discarded()) return;
    uiStyle = j.value("uiStyle", 0);
    // ★v1.2.8：新键（APK 同名字符串枚举）优先；旧版 int 键自动迁移——
    //   scalingMode: 0保持/1拉伸→stretch、2整数→stretch+integerScale=true
    //   screenEffect: 0无→none、1扫描线→scanline、2CRT→crt
    {
      std::string vs = j.value("videoScale", "");
      if (vs.empty()) {
        const int oldScaling = j.value("scalingMode", 0);
        vs = oldScaling == 1 ? "stretch" : "stretch";
        integerScale = oldScaling == 2;
      }
      videoScale = validVideoScale(vs) ? vs : "stretch";
      std::string vf = j.value("videoFilter", "");
      if (vf.empty()) {
        const int oldFx = j.value("screenEffect", 0);
        vf = oldFx == 1 ? "scanline" : (oldFx == 2 ? "crt" : "none");
      }
      videoFilter = validVideoFilter(vf) ? vf : "none";
    }
    {
      auto cl = j.find("customLayout");
      if (cl != j.end() && cl->is_array() && cl->size() == 4) {
        for (int i = 0; i < 4; i++)
          customLayout[i] =
              std::min(1.0f, std::max(0.0f, (*cl)[i].get<float>()));
      }
    }
    if (j.contains("videoScaleByPlatform") && j["videoScaleByPlatform"].is_object()) {
      for (auto it = j["videoScaleByPlatform"].begin();
           it != j["videoScaleByPlatform"].end(); ++it)
        if (validVideoScale(it.value().get<std::string>()))
          videoScaleByPlatform[it.key()] = it.value().get<std::string>();
    }
    if (j.contains("videoFilterByPlatform") && j["videoFilterByPlatform"].is_object()) {
      for (auto it = j["videoFilterByPlatform"].begin();
           it != j["videoFilterByPlatform"].end(); ++it)
        if (validVideoFilter(it.value().get<std::string>()))
          videoFilterByPlatform[it.key()] = it.value().get<std::string>();
    }
    {
      std::string sm = j.value("saveMode", "nesstation");
      saveMode = (sm == "core_builtin") ? "core_builtin" : "nesstation";
    }
    integerScale = j.value("integerScale", integerScale);
    dualLayout = j.value("dualLayout", 0);
    videoSmooth = j.value("videoSmooth", true);
    audioMuted = j.value("audioMuted", false);
    fullscreen = j.value("fullscreen", false);
    showFps = j.value("showFps", false);
    uiScalePercent = j.value("uiScalePercent", 0);   // ★v1.2.7 界面缩放
    autoSaveState = j.value("autoSaveState", true);
    fastForwardSpeed = j.value("fastForwardSpeed", 4);  // 0关/2/4/6/8/16（对齐 Android SettingsStore）
    scanRecursive = j.value("scanRecursive", true);
    homeBackground = j.value("homeBackground", "");          // ★v1.2.6 自定义主页背景
    fullscreenOnLaunch = j.value("fullscreenOnLaunch", false); // ★v1.2.6 启动游戏时全屏
    audioBufferMs = j.value("audioBufferMs", 64);              // ★v1.2.6 音频缓冲毫秒
    if (j.contains("defaultCores") && j["defaultCores"].is_object()) {
      for (auto it = j["defaultCores"].begin(); it != j["defaultCores"].end(); ++it)
        defaultCores[it.key()] = it.value().get<std::string>();
    }
    if (j.contains("emulatorPaths") && j["emulatorPaths"].is_object()) {
      for (auto it = j["emulatorPaths"].begin(); it != j["emulatorPaths"].end(); ++it)
        emulatorPaths[it.key()] = it.value().get<std::string>();
    }
    // ★v1.2.9 FSD 磁贴个性化（APK PadLayout.homeTileIcons / homeTileIconAlphas）
    if (j.contains("homeTileIcons") && j["homeTileIcons"].is_object()) {
      for (auto it = j["homeTileIcons"].begin(); it != j["homeTileIcons"].end(); ++it)
        if (it.value().is_string() && !it.value().get<std::string>().empty())
          homeTileIcons[it.key()] = it.value().get<std::string>();
    }
    if (j.contains("homeTileIconAlphas") && j["homeTileIconAlphas"].is_object()) {
      for (auto it = j["homeTileIconAlphas"].begin(); it != j["homeTileIconAlphas"].end(); ++it)
        if (it.value().is_number()) {
          const float a = it.value().get<float>();
          homeTileIconAlphas[it.key()] = a < 0.05f ? 0.05f : (a > 1.0f ? 1.0f : a);
        }
    }
    // 按键映射
    if (j.contains("padMapping") && j["padMapping"].is_object()) {
      const auto& pm = j["padMapping"];
      for (int port = 0; port < 2; port++) {
        const char* pk = port == 0 ? "p0" : "p1";
        if (!pm.contains(pk) || !pm[pk].is_array()) continue;
        const auto& arr = pm[pk];
        for (size_t i = 0; i < arr.size() && i < (size_t)RP_COUNT; i++) {
          if (!arr[i].is_array() || arr[i].size() < 2) continue;
          padMapping.binds[port][i].key = (SDL_Keycode)arr[i][0].get<int>();
          padMapping.binds[port][i].padBtn = (SDL_GameControllerButton)arr[i][1].get<int>();
        }
      }
    }
  } catch (...) {
  }
}

void Config::save(const std::string& file) const {
  json j;
  j["uiStyle"] = uiStyle;
  j["videoScale"] = videoScale;
  j["videoFilter"] = videoFilter;
  {
    json cl = json::array();
    for (int i = 0; i < 4; i++) cl.push_back(customLayout[i]);
    j["customLayout"] = cl;
  }
  j["integerScale"] = integerScale;
  j["saveMode"] = saveMode;
  j["videoScaleByPlatform"] = json::object();
  for (const auto& [k, v] : videoScaleByPlatform) j["videoScaleByPlatform"][k] = v;
  j["videoFilterByPlatform"] = json::object();
  for (const auto& [k, v] : videoFilterByPlatform) j["videoFilterByPlatform"][k] = v;
  j["dualLayout"] = dualLayout;
  j["videoSmooth"] = videoSmooth;
  j["audioMuted"] = audioMuted;
  j["fullscreen"] = fullscreen;
  j["showFps"] = showFps;
  j["uiScalePercent"] = uiScalePercent;   // ★v1.2.7 界面缩放
  j["autoSaveState"] = autoSaveState;
  j["fastForwardSpeed"] = fastForwardSpeed;
  j["scanRecursive"] = scanRecursive;
  j["homeBackground"] = homeBackground;         // ★v1.2.6
  j["fullscreenOnLaunch"] = fullscreenOnLaunch; // ★v1.2.6
  j["audioBufferMs"] = audioBufferMs;           // ★v1.2.6
  j["defaultCores"] = json::object();           // ★v1.2.6
  for (const auto& [k, v] : defaultCores) j["defaultCores"][k] = v;
  j["emulatorPaths"] = json::object();
  for (const auto& [k, v] : emulatorPaths) j["emulatorPaths"][k] = v;
  // ★v1.2.9 FSD 磁贴个性化
  j["homeTileIcons"] = json::object();
  for (const auto& [k, v] : homeTileIcons) j["homeTileIcons"][k] = v;
  j["homeTileIconAlphas"] = json::object();
  for (const auto& [k, v] : homeTileIconAlphas) j["homeTileIconAlphas"][k] = v;
  json pm;
  for (int port = 0; port < 2; port++) {
    json arr = json::array();
    for (int b = 0; b < RP_COUNT; b++) {
      arr.push_back(json::array({(int)padMapping.binds[port][b].key,
                                 (int)padMapping.binds[port][b].padBtn}));
    }
    pm[port == 0 ? "p0" : "p1"] = arr;
  }
  j["padMapping"] = pm;
  // ★replace 处理器：非法 UTF-8 字节替换为 U+FFFD（不抛异常，防退出/保存闪退）
  fsutil::writeFile(file, j.dump(2, ' ', false, json::error_handler_t::replace));
}

// —— App ——————————————————————————————————————————————
App::~App() { shutdown(); }

namespace {
// 系统字体发现（v1.2.4：不随包附带字体，全部用系统字体；TTC 集合已支持）
std::string lowerCopy(std::string s) {
  for (char& c : s) c = (char)tolower((unsigned char)c);
  return s;
}
bool hasExt(const std::string& name) {
  const std::string e = fsutil::extOf(name);
  return e == "ttf" || e == "ttc" || e == "otf";
}
// Linux：按名字给中文字体打分（分越高越优先）
int fontScore(const std::string& lowerName) {
  if (lowerName.find("notosanssc") != std::string::npos ||
      lowerName.find("notoserifsc") != std::string::npos ||
      lowerName.find("sourcehansans") != std::string::npos) return 100;
  if (lowerName.find("notosanscjk") != std::string::npos ||
      lowerName.find("notoserifcjk") != std::string::npos) return 90;
  if (lowerName.find("droidsansfallback") != std::string::npos) return 80;
  if (lowerName.find("wenquanyi") != std::string::npos ||
      lowerName.find("wqy") != std::string::npos) return 70;
  if (lowerName.find("misans") != std::string::npos ||
      lowerName.find("harmonyos") != std::string::npos ||
      lowerName.find("pingfang") != std::string::npos ||
      lowerName.find("alibaba") != std::string::npos ||
      lowerName.find("sourcehanserif") != std::string::npos) return 60;
  if (lowerName.find("cjk") != std::string::npos) return 50;
  if (lowerName.find("dejavusans") != std::string::npos ||
      lowerName.find("liberationsans") != std::string::npos ||
      lowerName.find("ubuntumono") != std::string::npos ||
      lowerName.find("ubuntu") != std::string::npos ||
      lowerName.find("freesans") != std::string::npos) return 30;
  return 10;
}
bool isBoldName(const std::string& lowerStem) {
  return lowerStem.find("bold") != std::string::npos ||
         lowerStem.find("-b") != std::string::npos ||
         lowerStem.find("_b") != std::string::npos ||
         lowerStem.find("heavy") != std::string::npos;
}
}  // namespace

bool App::initFontChain() {
  // 候选链：环境变量 → 便携 assets/fonts（用户自备）→ 系统字体
  std::vector<std::pair<std::string, std::string>> candidates;  // (regular, bold)

  // 1) NESSTATION_FONT=/path/font.ttf（自检/高级用户）
  if (const char* env = getenv("NESSTATION_FONT")) {
    candidates.emplace_back(env, "");
  }

  // 2) 运行目录 assets/fonts/：不再随包附带，但保留用户自备字体入口
  const std::string portableDir = fsutil::join(fsutil::exeDir(), "assets/fonts");
  std::vector<std::pair<int, std::string>> portable;  // (score, path)
  if (fsutil::isDir(portableDir)) {
    for (const auto& e : fsutil::listDir(portableDir)) {
      if (e.isDir || !hasExt(e.name)) continue;
      portable.emplace_back(fontScore(lowerCopy(e.name)), e.fullPath);
    }
  }
  std::sort(portable.begin(), portable.end(),
            [](const auto& a, const auto& b) {
              return a.first != b.first ? a.first > b.first : a.second < b.second;
            });
  {
    std::string reg, bold;
    for (const auto& [sc, p] : portable) {
      const std::string stem = lowerCopy(fsutil::stemOf(p));
      if (isBoldName(stem)) {
        if (bold.empty()) bold = p;
      } else if (reg.empty()) {
        reg = p;
      }
    }
    if (!reg.empty()) {
      if (bold.empty()) bold = reg;
      candidates.emplace_back(reg, bold);
    }
  }

#ifdef _WIN32
  // 3) Windows 系统字体（中文字体优先；msyh/simsun 为 TTC，已支持集合解析）
  std::vector<std::string> winDirs{"C:/Windows/Fonts"};
  if (const char* lad = getenv("LOCALAPPDATA"))
    winDirs.push_back(std::string(lad) + "/Microsoft/Windows/Fonts");
  static const char* kPairs[][2] = {
      {"msyh.ttc", "msyhbd.ttc"},    // 微软雅黑（Win7+）
      {"msyh.ttf", "msyhbd.ttf"},
      {"deng.ttf", "dengb.ttf"},     // 等线（Win10+）
      {"simhei.ttf", nullptr},       // 黑体（自带粗体观感）
      {"simsun.ttc", nullptr},       // 宋体
      {"simfang.ttf", nullptr}, {"simkai.ttf", nullptr},
      {"segoeui.ttf", "segoeuib.ttf"},  // 无中文，最后兑底（仅西文）
  };
  for (const char* const* pair : kPairs) {
    for (const std::string& dir : winDirs) {
      const std::string r = fsutil::join(dir, pair[0]);
      if (fsutil::isFile(r)) {
        std::string b = pair[1] ? fsutil::join(dir, pair[1]) : r;
        if (!fsutil::isFile(b)) b = r;
        candidates.emplace_back(r, b);
      }
    }
  }
#else
  // 3) Linux/macOS：扫描系统字体目录，按中文字体优先级选（含粗体配套）
  std::vector<std::string> roots{"/usr/share/fonts", "/usr/local/share/fonts",
                                 "/opt/homebrew/share/fonts"};
  if (const char* home = getenv("HOME")) {
    roots.push_back(std::string(home) + "/.local/share/fonts");
    roots.push_back(std::string(home) + "/.fonts");
    roots.push_back(std::string(home) + "/Library/Fonts");  // macOS
  }
  std::vector<std::pair<int, std::string>> found;  // (score, path)
  for (const std::string& root : roots) {
    if (!fsutil::isDir(root)) continue;
    for (const std::string& p : fsutil::listFilesRecursive(root)) {
      if (!hasExt(fsutil::baseOf(p))) continue;
      found.emplace_back(fontScore(lowerCopy(fsutil::baseOf(p))), p);
    }
  }
  std::sort(found.begin(), found.end(),
            [](const auto& a, const auto& b) {
              return a.first != b.first ? a.first > b.first : a.second < b.second;
            });
  {
    std::string reg, bold;
    for (const auto& [sc, p] : found) {
      const std::string stem = lowerCopy(fsutil::stemOf(p));
      // 同分才可能配套；避免拿低分字体当粗体
      if (isBoldName(stem)) {
        if (bold.empty() && (reg.empty() || sc == fontScore(lowerCopy(fsutil::baseOf(reg))))) {
          // 记住同族粗体（暂存，等 regular 定了再校验）
          if (bold.empty()) bold = p;
        }
      } else if (reg.empty()) {
        reg = p;
        if (!bold.empty() && fontScore(lowerCopy(fsutil::baseOf(bold))) != sc) bold.clear();
      }
    }
    if (!reg.empty()) {
      if (bold.empty()) bold = reg;
      candidates.emplace_back(reg, bold);
    }
  }
#endif

  // 逐个尝试：能加载且有中文字形（U+4E2D「中」）即用；全部无中文时退而求其次
  std::string warnNoCjk;
  for (const auto& [r, b] : candidates) {
    if (!font.load(r, b)) continue;
    if (font.hasCodepoint(0x4E2D)) {
      LOGI("系统字体选定：%s（粗体 %s）", r.c_str(),
           (b == r || b.empty()) ? "复用常规体" : b.c_str());
      return true;
    }
    if (warnNoCjk.empty()) warnNoCjk = r;
  }
  if (!warnNoCjk.empty()) {
    LOGW("所有字体均无中文字形，回退：%s（中文将显示为方块）", warnNoCjk.c_str());
    return font.load(warnNoCjk, warnNoCjk);
  }
  return false;
}

bool App::init() {
  // ★v1.2.8：Windows 高 DPI 根治（必须在 SDL_Init 之前设置）。
  //   旧版问题：进程不声明 DPI 感知 → Windows 把窗口虚拟化到 96 DPI，
  //   渲染器输出 = 逻辑点数，整个画面被 DWM 位图拉伸 → 高 DPI（125%/150%）
  //   下「又小又糊」；UI 缩放按 pixelH 计算也永远拿不到真实物理像素。
  //   permonitorv2 + DPI_SCALING=0：窗口/鼠标/渲染器输出全部统一为物理像素，
  //   字形 1:1 落盘不再被拉伸，自动缩放拿到真实分辨率。非 Windows 平台
  //   这两个 hint 是无害空操作（macOS 走 cocoa 缩放、Linux X11/Wayland 原生）。
  // ★v1.2.9 修复 Linux x64 构建：SDL_HINT_WINDOWS_DPI_AWARENESS / _SCALING
  //   是 SDL 2.24.0（2022-09）新增宏；ubuntu-22.04（linux-x64 job）系统
  //   SDL2 = 2.0.20 无此宏 → 编译直接 'was not declared'（本次 Build 步骤
  //   exit 2 的死因；debian:12 = 2.26.5 与 Windows FetchContent 2.32.4 均有，
  //   故仅 x64 job 挂）。版本守卫：旧 SDL 跳过这两行（非 Windows 平台本就
  //   无操作；Windows 端 SDL ≥ 2.24 恒满足，行为不变）。已用 g++ 11.4 +
  //   SDL 2.0.20 / 2.32.4 双头文件组实测验证。
#if defined(SDL_HINT_WINDOWS_DPI_AWARENESS) && defined(SDL_HINT_WINDOWS_DPI_SCALING)
  SDL_SetHint(SDL_HINT_WINDOWS_DPI_AWARENESS, "permonitorv2");
  SDL_SetHint(SDL_HINT_WINDOWS_DPI_SCALING, "0");
#endif

  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER) != 0) {
    LOGE("SDL_Init 失败：%s", SDL_GetError());
    return false;
  }
  SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");

  const Uint32 flags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI |
                       (config.fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
  // ★v1.2.7：数据目录与配置【提前到窗口创建之前】读取——
  //   旧顺序里 config.load 在窗口/渲染器之后，fullscreen 与本次新增的
  //   uiScalePercent 在启动首帧都取不到用户值（固定缩放 150% 开局仍 100%）。
  dataDir_ = fsutil::dataDir();
  fsutil::makeDirs(dataDir_);
  logInit(fsutil::join(dataDir_, "nesstation.log"));
  LOGI("NesStation PC 启动（数据目录 %s）", dataDir_.c_str());
  configFile_ = fsutil::join(dataDir_, "config.json");
  config.load(configFile_);

  // ★v1.2.7：默认窗口按桌面 75% 起（钳在 1280×720 ~ 1920×1080）。
  //   旧版固定 1280×720，在 2K/4K 桌面上起一个相对过小的小窗；
  //   配合按分辨率自动放大的 UI 缩放，首次启动即达到合适观感。
  int defW = 1280, defH = 720;
  {
    SDL_DisplayMode dm{};
    if (SDL_GetDesktopDisplayMode(0, &dm) == 0 && dm.w >= 1280 && dm.h >= 720) {
      defW = dm.w * 3 / 4;
      defH = dm.h * 3 / 4;
      if (defW < 1280) defW = 1280;
      if (defH < 720) defH = 720;
      if (defW > 1920) defW = 1920;
      if (defH > 1080) defH = 1080;
    }
  }
  // ★v1.2.8 调试/回归用：NESSTATION_TEST_WINSIZE=1920x1080 强制初始窗口尺寸
  //   （无头自检在不同分辨率下验证 UI 缩放/命中测试；正常用户不受影响）
  if (const char* tw = getenv("NESSTATION_TEST_WINSIZE")) {
    if (sscanf(tw, "%dx%d", &defW, &defH) == 2) { /* keep */ }
  }
  window_ = SDL_CreateWindow(std::string("NesStation ").append(appVersionText()).c_str(),
                             SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                             defW, defH, flags);
  if (!window_) {
    LOGE("创建窗口失败：%s", SDL_GetError());
    return false;
  }
  SDL_SetWindowMinimumSize(window_, 960, 540);
  renderer_ = SDL_CreateRenderer(
      window_, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
  if (!renderer_) {
    LOGW("加速渲染器失败，回退软件：%s", SDL_GetError());
    renderer_ = SDL_CreateRenderer(window_, -1, SDL_RENDERER_SOFTWARE);
    if (!renderer_) {
      LOGE("创建渲染器失败：%s", SDL_GetError());
      return false;
    }
  }
  updateSurface();  // ★v1.2.7 逻辑尺寸/UI 缩放/鼠标映射一次性就绪（config 已加载）

  setWindowIcon();

  // 主题 / 按键映射 / 子系统（数据目录与 config 已在窗口创建前就绪）
  Theme::setStyle(config.uiStyle == 1 ? UiStyle::Fsd : UiStyle::Neon);
  input.setMapping(config.padMapping);

  cores.init(dataDir_);
  library.load(dataDir_);
  // ★ 启动时自动重扫 ROM 目录（对齐 Android 版「启动/手动刷新时自动入库」）：
  // 后台线程枚举，主线程逐帧合并（games_ 只在主线程写，线程安全）
  library.startBackgroundRescan(config.scanRecursive);
  externals.init(dataDir_);
  for (const auto& [k, v] : config.emulatorPaths) {
    // 旧版本键名兼容：dolphin → ishiiruka（对齐 Android CoreRegistry id）
    std::string key = k;
    if (key == "dolphin") key = "ishiiruka";
    externals.setUserPath(key, v);
  }

  input.init();
  audio.start(48000, config.audioBufferMs);  // ★v1.2.6 音频缓冲毫秒
  audio.setMuted(config.audioMuted);
  video.setDualScreen(false);

  // 字体：v1.2.4 起不再随包附带字体，全部使用系统字体（TTC 集合已支持）。
  // 优先级：NESSTATION_FONT 环境变量 → 运行目录 assets/fonts/（用户自备，
  // 便携包可自行放入 .ttf/.ttc）→ 系统字体扫描（中文优先，含 CJK 字形校验）。
  if (!initFontChain()) {
    LOGE("致命：无法加载任何字体");
    return false;
  }
  return true;
}

void App::shutdown() {
  emu_.reset();
  for (auto& kv : texCache_)
    if (kv.second.tex) SDL_DestroyTexture(kv.second.tex);
  texCache_.clear();
  library.joinScanThread();  // 等待后台扫描线程退出（防退出闪退）
  if (!configFile_.empty()) {
    config.padMapping = input.mapping();
    config.save(configFile_);
  }
  input.shutdown();
  audio.stop();
  library.save();
  if (renderer_) SDL_DestroyRenderer(renderer_);
  if (window_) SDL_DestroyWindow(window_);
  renderer_ = nullptr;
  window_ = nullptr;
  SDL_Quit();
  logClose();
}

// 窗口图标：assets/icon32.png（与 Android 原版同源）
void App::setWindowIcon() {
  const std::string path = fsutil::join(fsutil::exeDir(), "assets/icon32.png");
  int w = 0, h = 0, ch = 0;
  unsigned char* data = stbi_load(path.c_str(), &w, &h, &ch, 4);
  if (!data) {
    LOGW("窗口图标缺失：%s", path.c_str());
    return;
  }
  SDL_Surface* surf =
      SDL_CreateRGBSurfaceWithFormatFrom(data, w, h, 32, w * 4, SDL_PIXELFORMAT_ABGR8888);
  if (surf) {
    SDL_SetWindowIcon(window_, surf);
    SDL_FreeSurface(surf);
  }
  stbi_image_free(data);
}

bool App::saveScreenshot(const std::string& pngPath) {
  if (!renderer_) return false;
  // ★v1.2.7：按渲染器输出尺寸（设备像素）整帧截取 —— 旧版取视口，
  //   UI 缩放后视口是逻辑单位（如 853×480），ReadPixels 会把它当设备像素
  //   用，150% 缩放下只截到左上角一小块；nullptr = 整个渲染目标。
  int pixelW = 0, pixelH = 0;
  SDL_GetRendererOutputSize(renderer_, &pixelW, &pixelH);
  if (pixelW <= 0 || pixelH <= 0) return false;
  SDL_Surface* surf =
      SDL_CreateRGBSurfaceWithFormat(0, pixelW, pixelH, 32, SDL_PIXELFORMAT_ABGR8888);
  if (!surf) return false;
  if (SDL_RenderReadPixels(renderer_, nullptr, SDL_PIXELFORMAT_ABGR8888, surf->pixels,
                           surf->pitch) != 0) {
    SDL_FreeSurface(surf);
    LOGW("截图失败：%s", SDL_GetError());
    return false;
  }
  const int ok = stbi_write_png(pngPath.c_str(), surf->w, surf->h, 4, surf->pixels,
                                surf->pitch);
  SDL_FreeSurface(surf);
  return ok != 0;
}

// —— ★v1.2.8 窗口表面 → 逻辑尺寸 / UI 缩放 / 鼠标映射 全量重算 ——
// v1.2.7 的教训（用户反馈「UI 根本没有放大 + 点击错位」）：
//   ① 自动缩放按 pixelH/720：逻辑空间恒 1280×720，窗口越大缩放越大，但
//     UI 元素占屏幕比例纹丝不动 —— 视觉相对大小完全没变；
//   ② Windows 高 DPI 不感知（v1.2.8 已在 init 前声明 permonitorv2 +
//     DPI_SCALING=0），渲染输出被 DWM 位图拉伸 —— 又小又糊。
// v1.2.8 新方案（对齐 APK 密度缩放逻辑：Compose dp × 设备密度）：
//   · 自动缩放 = pixelH / 540（设计基准 960×540）：
//     720p 窗口=133% / 1080p=200% / 4K=400% —— 1080p 下 1dp ≈ 2 物理像素，
//     与 APK 在 Android TV 1080p（xhdpi 密度 2.0）的视觉密度一致；
//     相比 v1.2.7（720 基准）整体再放大 1.33 倍，UI 相对屏幕明显变大；
//   · 字号/卡片等设计值不变，全部经渲染器统一变换放大 —— 布局逻辑零改动；
//   · 鼠标坐标用 SDL_RenderWindowToLogical 精确反演渲染器变换（含
//     letterbox/取整/DPI 一切边界），命中测试与所见像素严格一致。
void App::updateSurface() {
  if (!renderer_ || !window_) return;
  SDL_GetWindowSize(window_, &winW_, &winH_);
  int pixelW = winW_, pixelH = winH_;
  SDL_GetRendererOutputSize(renderer_, &pixelW, &pixelH);
  if (pixelW <= 0 || pixelH <= 0) {
    pixelW = winW_;
    pixelH = winH_;
  }

  float scale;
  if (config.uiScalePercent > 0) {
    scale = config.uiScalePercent / 100.0f;
    // ★防裁切：固定百分比不得把设计空间压到最低设计尺寸（960×540，
    //   与 SDL_SetWindowMinimumSize 一致）以下。
    const float maxByW = (float)pixelW / 960.0f;
    const float maxByH = (float)pixelH / 540.0f;
    const float maxScale = std::min(maxByW, maxByH);
    if (scale > maxScale) scale = maxScale;
  } else {
    // 自动：540p 基准密度缩放（0.125 步进平滑过渡，避免图集碎片化）
    scale = (float)pixelH / 540.0f;
    scale = std::floor(scale * 8.0f + 0.5f) / 8.0f;
    if (pixelH >= 540 && scale < 1.0f) scale = 1.0f;  // 可读性下限
  }
  if (scale < 0.5f) scale = 0.5f;
  if (scale > 5.0f) scale = 5.0f;

  const int logicalW = (int)((float)pixelW / scale + 0.5f);
  const int logicalH = (int)((float)pixelH / scale + 0.5f);
  if (logicalW <= 0 || logicalH <= 0) return;
  SDL_RenderSetLogicalSize(renderer_, logicalW, logicalH);
  // 取渲染器实际生效缩放（逻辑尺寸整数化带来的微差已包含在内），
  // 字形烘焙与它对齐 → 位图在设备像素上 1:1 呈现
  float rx = 1.0f, ry = 1.0f;
  SDL_RenderGetScale(renderer_, &rx, &ry);
  uiscale::set(rx > 0.0f ? rx : scale);
  draw.begin(renderer_, logicalW, logicalH);
  // 鼠标：SDL ≥ 2.0.18 已自动把事件坐标折算为逻辑坐标（InputManager 直用）；
  // 旧 SDL 的标量折算系数在此同步（逻辑单位/窗口点数）
  input.setMouseToUiScale(winW_ > 0 ? (float)logicalW / (float)winW_ : 1.0f);
  LOGI("UI 缩放：%.2f（输出 %dx%d / 逻辑 %dx%d / 窗口 %dx%d）", uiscale::get(), pixelW,
       pixelH, logicalW, logicalH, winW_, winH_);
}

void App::saveConfig() {
  if (!configFile_.empty()) config.save(configFile_);
}

void App::applyUiScale() {
  updateSurface();
  toast(std::string("界面缩放：") +
        (config.uiScalePercent > 0
             ? std::to_string(config.uiScalePercent) + "%"
             : "自动（当前 " + std::to_string((int)(uiscale::get() * 100 + 0.5f)) + "%）"));
}

void App::toggleFullscreen() {
  config.fullscreen = !config.fullscreen;
  applyFullscreen();
}

void App::applyFullscreen() {
  SDL_SetWindowFullscreen(window_, config.fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
}

std::string App::assetPath(const std::string& rel) const {
  return fsutil::join(fsutil::exeDir(), fsutil::join("assets", rel));
}

SDL_Texture* App::texture(const std::string& key) {
  auto it = texCache_.find(key);
  if (it != texCache_.end()) {
    it->second.lastUsed = frameNo_;
    return it->second.tex;
  }
  // 载入
  int w = 0, h = 0, ch = 0;
  unsigned char* data = stbi_load(key.c_str(), &w, &h, &ch, 4);
  if (!data) {
    texCache_[key] = {nullptr, frameNo_};
    return nullptr;
  }
  SDL_Texture* tex = SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_ABGR8888,
                                       SDL_TEXTUREACCESS_STATIC, w, h);
  if (tex) {
    SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
    SDL_UpdateTexture(tex, nullptr, data, w * 4);
  }
  stbi_image_free(data);
  texCache_[key] = {tex, frameNo_};
  evictTextures();
  return tex;
}

void App::invalidateTexture(const std::string& key) {
  auto it = texCache_.find(key);
  if (it != texCache_.end()) {
    if (it->second.tex) SDL_DestroyTexture(it->second.tex);
    texCache_.erase(it);
  }
}

void App::evictTextures() {
  // LRU：超过 120 张时按 lastUsed 淘汰
  if (texCache_.size() <= 120) return;
  std::vector<std::pair<uint64_t, std::string>> items;
  for (auto& kv : texCache_)
    if (kv.second.tex) items.push_back({kv.second.lastUsed, kv.first});
  std::sort(items.begin(), items.end());
  const size_t remove = texCache_.size() - 100;
  for (size_t i = 0; i < remove && i < items.size(); i++) {
    invalidateTexture(items[i].second);
  }
}

void App::pushScene(std::unique_ptr<Scene> s) {
  if (!scenes_.empty()) scenes_.back()->onExit(*this);
  s->onEnter(*this);
  scenes_.push_back(std::move(s));
}

void App::popScene() {
  if (scenes_.empty()) return;
  scenes_.back()->onExit(*this);
  scenes_.pop_back();
  if (!scenes_.empty()) scenes_.back()->onEnter(*this);
}

void App::replaceScene(std::unique_ptr<Scene> s) {
  if (!scenes_.empty()) {
    scenes_.back()->onExit(*this);
    scenes_.pop_back();
  }
  s->onEnter(*this);
  scenes_.push_back(std::move(s));
}

void App::launchGame(const GameEntry& gameIn) {
  // 统一取库内最新条目（含核心选择器写入的 preferredCore / 收藏 / 重命名）
  GameEntry game = gameIn;
  if (GameEntry* live = library.find(gameIn.id)) game = *live;

  // 平台可用核心选择 = 已安装 libretro + 已就绪外置独立核心
  //（对齐 Android EngineFactory.coreChoices：libretro 在前 / 外置在后，
  //  多于一个且未记忆选择 → 弹核心选择器）
  // ★v1.2.6：先查全局「平台默认核心」（对齐 Android defaultCores；
  //   设置 → 核心设置 → 默认核心，键 = platformShortName）
  if (game.preferredCore.empty()) {
    const auto it = config.defaultCores.find(platformShortName(game.platform));
    if (it != config.defaultCores.end() && !it->second.empty()) {
      const std::string& want = it->second;
      bool available = false;
      if (want.size() > 4 && want.compare(want.size() - 4, 4, "-ext") == 0) {
        if (const ExternalCore* c =
                externals.find(want.substr(0, want.size() - 4)))
          available = externals.available(c->id);
      } else {
        for (const auto& pc : cores.installedCoresFor(game.platform))
          if (pc.coreFile == want) available = true;
      }
      // 默认核心可用才采用；不可用（未安装/未就绪）则回落到选择器
      if (available) game.preferredCore = want;
    }
  }
  if (game.preferredCore.empty() &&
      cores.installedCoresFor(game.platform).size() +
              (size_t)externals.availableCount(game.platform) >
          1) {
    pushScene(createCorePickerScene(
        game, [this, game](const std::string&) { launchGame(game); }));
    return;
  }
  if (!emu_) emu_.reset(new EmuRunner());
  const std::string err = emu_->start(*this, game);
  if (err.empty()) {
    LOGI("游戏启动成功：%s", game.displayTitle().c_str());
  } else {
    // 核心缺失 / 启动失败 → 模态提示框（重要信息不吞进 toast）
    pushScene(createMessageScene("无法启动游戏", err));
    LOGE("游戏启动失败：%s", err.c_str());
  }
}

void App::pumpEvents() {
  input.beginFrame();
  SDL_Event ev;
  while (SDL_PollEvent(&ev)) {
    switch (ev.type) {
      case SDL_QUIT:
        running_ = false;
        break;
      case SDL_WINDOWEVENT:
        if (ev.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
          updateSurface();  // ★v1.2.7 统一入口：逻辑尺寸+缩放+鼠标映射
        }
        break;
      default:
        input.handleEvent(ev);
    }
  }
}

void App::frame(float dt) {
  frameNo_++;
  // ★v1.2.10：真实帧间隔（旧版硬编码 1/60 —— 无 VSync / 高刷环境下
  //   实际循环 120fps，模拟时间流速 ×2，FC 核心直接随 120 帧跑）
  input.endFrame(dt);
  const InputState& in = input.input();
  covers.pump(*this);

  // 启动后台扫描：枚举完成后合入游戏库（主线程写入，无锁竞争）
  if (!library.scanRunning() && library.pendingScanCount() > 0) {
    const int n = library.drainPendingScan();
    if (n > 0) {
      library.save();
      toast("启动扫描：新增 " + std::to_string(n) + " 个游戏");
    }
  }

  if (!scenes_.empty()) {
    Scene* s = scenes_.back().get();
    s->update(*this, dt, in);
    if (!scenes_.empty() && scenes_.back().get() == s) s->draw(*this);
  }

  drawToasts(dt);
  SDL_RenderPresent(renderer_);

  if (testHook_) {
    testHook_(*this, testFrame_++);
  }
}

void App::drawToasts(float dt) {
  float y = 70;
  for (auto& t : toasts_) t.life -= dt;
  toasts_.erase(std::remove_if(toasts_.begin(), toasts_.end(),
                               [](const Toast& t) { return t.life <= 0; }),
                toasts_.end());
  const Skin& sk = Theme::skin();
  for (auto& t : toasts_) {
    const float alpha = t.life > 3.0f ? 1.0f : t.life / 3.0f;
    const float w = font.textWidth(t.msg, 18) + 40;
    const Rect rc(draw.W / 2 - w / 2, y, w, 40);
    draw.roundRect(rc, 8, Color8(sk.bgPanel.r, sk.bgPanel.g, sk.bgPanel.b,
                                 (uint8_t)(230 * alpha)));
    draw.roundRectOutline(rc, 8, Color8(sk.accent.r, sk.accent.g, sk.accent.b,
                                        (uint8_t)(160 * alpha)));
    font.draw(renderer_, t.msg, rc.x + 20, rc.y + 10, 18,
              Color8(sk.textHi.r, sk.textHi.g, sk.textHi.b, (uint8_t)(255 * alpha)));
    y += 48;
  }
}

void App::drawButtonHints() {
  const Skin& sk = Theme::skin();
  const float y = draw.H - 38;
  const float size = 15;
  float x = 26;
  struct Hint {
    const char* key;
    const char* label;
    Color8 col;
  };
  const Hint hints[] = {
      {"A", "确认", sk.btnA},   {"B", "返回", sk.btnB}, {"Y", "选项", sk.btnY},
      {"X", "收藏", sk.btnX},   {"Start", "菜单", sk.text}, {"Esc", "退出", sk.textDim},
  };
  for (const auto& h : hints) {
    draw.circle(x + 10, y + 10, 10, Color8(h.col.r, h.col.g, h.col.b, 60));
    draw.circleOutline(x + 10, y + 10, 10, h.col, 1.5f);
    font.drawVCentered(renderer_, h.key, x + 3, y + 10, 12, sk.textHi, true);
    x += 24;
    font.drawVCentered(renderer_, h.label, x, y + 10, size, sk.text);
    x += font.textWidth(h.label, size) + 22;
  }
}

int App::run(int, char**) {
  if (!init()) {
    shutdown();
    return 1;
  }
  // 起始场景：主页（按当前皮肤）
  pushScene(createHomeScene());

  running_ = true;
  // ★v1.2.10：高性能计数器测量真实帧间隔（对齐 APK FramePacer 的墙钟语义；
  //   钳位 [1/240, 1/20]：拖窗口/休眠唤醒后的超大 dt 不再引发模拟追帧风暴）
  Uint64 prevPerf = SDL_GetPerformanceCounter();
  while (running_) {
    const Uint32 t0 = SDL_GetTicks();
    pumpEvents();
    const Uint64 nowPerf = SDL_GetPerformanceCounter();
    float dt = (float)((double)(nowPerf - prevPerf) / (double)SDL_GetPerformanceFrequency());
    prevPerf = nowPerf;
    if (dt < 1.0f / 240.0f) dt = 1.0f / 240.0f;
    if (dt > 1.0f / 20.0f) dt = 1.0f / 20.0f;
    frame(dt);
    // 无 VSync 设备时限制空转（vsync 生效时 RenderPresent 已阻塞到刷新率；
    // 此上限只是失控保护 —— 模拟器步进已改用真实 dt，不再依赖它作节拍器）
    const Uint32 t1 = SDL_GetTicks();
    if (t1 - t0 < 8) SDL_Delay(8 - (t1 - t0));
  }
  emu_.reset();
  return 0;
}

}  // namespace ns
