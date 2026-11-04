#include "app/App.h"

#include <cmath>
#include <cstdio>

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
void Config::load(const std::string& file) {
  std::string text;
  if (!fsutil::readFile(file, &text)) return;
  try {
    const json j = json::parse(text, nullptr, false);
    if (j.is_discarded()) return;
    uiStyle = j.value("uiStyle", 0);
    scalingMode = j.value("scalingMode", 0);
    screenEffect = j.value("screenEffect", 0);
    dualLayout = j.value("dualLayout", 0);
    videoSmooth = j.value("videoSmooth", true);
    audioMuted = j.value("audioMuted", false);
    fullscreen = j.value("fullscreen", false);
    showFps = j.value("showFps", false);
    autoSaveState = j.value("autoSaveState", true);
    fastForwardSpeed = j.value("fastForwardSpeed", 4);  // 0关/2/4/6/8/16（对齐 Android SettingsStore）
    scanRecursive = j.value("scanRecursive", true);
    if (j.contains("emulatorPaths") && j["emulatorPaths"].is_object()) {
      for (auto it = j["emulatorPaths"].begin(); it != j["emulatorPaths"].end(); ++it)
        emulatorPaths[it.key()] = it.value().get<std::string>();
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
  j["scalingMode"] = scalingMode;
  j["screenEffect"] = screenEffect;
  j["dualLayout"] = dualLayout;
  j["videoSmooth"] = videoSmooth;
  j["audioMuted"] = audioMuted;
  j["fullscreen"] = fullscreen;
  j["showFps"] = showFps;
  j["autoSaveState"] = autoSaveState;
  j["fastForwardSpeed"] = fastForwardSpeed;
  j["scanRecursive"] = scanRecursive;
  j["emulatorPaths"] = json::object();
  for (const auto& [k, v] : emulatorPaths) j["emulatorPaths"][k] = v;
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
  fsutil::writeFile(file, j.dump(2));
}

// —— App ——————————————————————————————————————————————
App::~App() { shutdown(); }

bool App::init() {
  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER) != 0) {
    LOGE("SDL_Init 失败：%s", SDL_GetError());
    return false;
  }
  SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");

  const Uint32 flags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI |
                       (config.fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
  window_ = SDL_CreateWindow(std::string("NesStation ").append(appVersionText()).c_str(),
                             SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                             winW_, winH_, flags);
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
  SDL_GetWindowSize(window_, &winW_, &winH_);
  SDL_RenderSetLogicalSize(renderer_, winW_, winH_);

  draw.begin(renderer_, winW_, winH_);
  setWindowIcon();

  // 数据目录与子系统
  dataDir_ = fsutil::dataDir();
  fsutil::makeDirs(dataDir_);
  logInit(fsutil::join(dataDir_, "nesstation.log"));
  LOGI("NesStation PC 启动（数据目录 %s）", dataDir_.c_str());

  configFile_ = fsutil::join(dataDir_, "config.json");
  config.load(configFile_);
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
  audio.start(48000);
  audio.setMuted(config.audioMuted);
  video.setDualScreen(false);

  // 字体：assets/fonts → 运行目录回退 → 系统字体回退
  std::string fontDir;
  for (const char* cand : {"assets/fonts"}) {
    const std::string p = fsutil::join(fsutil::exeDir(), cand);
    if (fsutil::isDir(p)) {
      fontDir = p;
      break;
    }
  }
  std::string regular = fsutil::join(fontDir, "NotoSansSC-Regular.ttf");
  std::string bold = fsutil::join(fontDir, "NotoSansSC-Bold.ttf");
  if (!fsutil::isFile(regular)) {
#ifdef _WIN32
    regular = "C:/Windows/Fonts/msyh.ttc";
    bold = "C:/Windows/Fonts/msyhbd.ttc";
#else
    for (const char* p : {"/usr/share/fonts/truetype/chinese/NotoSansSC-Regular.ttf",
                          "/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc",
                          "/usr/share/fonts/truetype/noto/NotoSansCJK-Regular.ttc"}) {
      if (fsutil::isFile(p)) {
        regular = p;
        bold = p;
        break;
      }
    }
#endif
  }
  if (!font.load(regular, bold)) {
    LOGE("致命：无法加载任何字体（%s）", regular.c_str());
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
  SDL_Rect vp{};
  SDL_RenderGetViewport(renderer_, &vp);
  SDL_Surface* surf =
      SDL_CreateRGBSurfaceWithFormat(0, vp.w, vp.h, 32, SDL_PIXELFORMAT_ABGR8888);
  if (!surf) return false;
  if (SDL_RenderReadPixels(renderer_, &vp, SDL_PIXELFORMAT_ABGR8888, surf->pixels,
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
          SDL_GetWindowSize(window_, &winW_, &winH_);
          SDL_RenderSetLogicalSize(renderer_, winW_, winH_);
          draw.begin(renderer_, winW_, winH_);
        }
        break;
      default:
        input.handleEvent(ev);
    }
  }
}

void App::frame() {
  frameNo_++;
  input.endFrame(1.0f / 60.0f);
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
    s->update(*this, 1.0f / 60.0f, in);
    if (!scenes_.empty() && scenes_.back().get() == s) s->draw(*this);
  }

  drawToasts(1.0f / 60.0f);
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
  while (running_) {
    const Uint32 frameStart = SDL_GetTicks();
    pumpEvents();
    frame();
    // 上限约 120fps（无 VSync 设备时限制空转）
    const Uint32 frameEnd = SDL_GetTicks();
    if (frameEnd - frameStart < 8) SDL_Delay(8 - (frameEnd - frameStart));
  }
  emu_.reset();
  return 0;
}

}  // namespace ns
