#include "emu/EmuRunner.h"

#include <chrono>

#include "ui/Scenes.h"
#include "util/Fs.h"
#include "util/Log.h"

namespace ns {

EmuRunner::~EmuRunner() = default;

std::string EmuRunner::start(App& app, const GameEntry& game) {
  game_ = game;
  const PlatformCore pc = platformCore(game.platform);

  // —— Standalone 平台：外部启动 ——
  if (pc.kind == CoreKind::Standalone) {
    std::string err;
    if (!app.externals.launch(game.platform, game.romPath, &err)) {
      return std::string("启动失败：") + err;
    }
    app.library.recordPlay(game.id, 0);
    app.library.save();
    app.toast(std::string("已启动 ") + pc.coreTitle + "：" + game.displayTitle());
    return "";
  }

  // —— libretro 平台：进程内会话 ——
  // 候选链解析：首选核心缺失时自动落到替代核心（如 3DS：Azahar → Citra）
  const PlatformCore resolved = app.cores.resolveCore(game.platform);
  const std::string corePath = app.cores.corePathFor(game.platform);
  if (!fsutil::isFile(corePath)) {
    return app.cores.availabilityHint(game.platform);
  }
  if (resolved.kind == CoreKind::Libretro && resolved.coreFile != pc.coreFile) {
    app.toast(std::string("首选核心 ") + pc.coreTitle + " 缺失，已改用 " +
              resolved.coreTitle);
  }

  teardown(app);
  core_.reset(new LibretroCore(app.audio, app.video, app.input));
  // 核心键 = 实际解析到的核心（可能是替代核心，如 3DS 的 Citra）
  coreKey_ = fsutil::stemOf(fsutil::baseOf(corePath));

  App* appPtr = &app;
  auto resolver = [appPtr, this](const std::string& optKey) -> std::string {
    return appPtr->cores.resolveOption(coreKey_, optKey);
  };

  if (!core_->load(corePath, app.cores.systemDir(), app.cores.savesDir(), resolver)) {
    const std::string err = core_->lastError();
    core_.reset();
    return std::string("核心装载失败：") + err;
  }

  app.video.setDualScreen(platformIsDualScreen(game.platform));
  if (!core_->loadGame(game.romPath)) {
    const std::string err = core_->lastError();
    teardown(app);
    return std::string("游戏载入失败：") + err;
  }

  running_ = true;
  isLibretro_ = true;
  coreFps_ = core_->timingFps();
  startTickMs_ = (int64_t)SDL_GetTicks();
  app.audio.clear();
  app.pushScene(createEmulatorScene());
  app.library.recordPlay(game.id, 0);
  return "";
}

void EmuRunner::teardown(App& app) {
  if (core_) {
    core_->unload();
    core_.reset();
  }
  running_ = false;
  isLibretro_ = false;
  app.video.setDualScreen(false);
}

void EmuRunner::stop(App& app, bool saveAuto) {
  if (!running_) return;
  if (saveAuto && isLibretro_ && core_ && core_->gameLoaded()) {
    saveStateSlot(app, 0);
    app.toast("已自动存档（槽位 A）");
  }
  // 游玩时长
  const int64_t secs = elapsedSec();
  app.library.recordPlay(game_.id, secs);
  app.library.save();
  teardown(app);
}

bool EmuRunner::saveStateSlot(App& app, int slot) {
  if (!core_) return false;
  if (!app.cores.saveState(*core_, game_.id, slot)) return false;
  app.library.save();
  return true;
}

bool EmuRunner::loadStateSlot(App& app, int slot) {
  if (!core_) return false;
  return app.cores.loadState(*core_, game_.id, slot);
}

std::vector<int> EmuRunner::slots(App& app) const {
  return app.cores.existingSlots(game_.id);
}

void EmuRunner::resetCore() {
  if (core_) core_->reset();
}

void EmuRunner::setFastForward(bool ff) {
  fastForward_ = ff;
  if (core_) core_->setFastForward(ff);
  // 快进时清缓冲防啸叫
}

void EmuRunner::runFrameOnce() {
  if (core_) core_->runFrame();
}

void EmuRunner::runFrame() {
  if (!core_) return;
  core_->runFrame();
  if (fastForward_) {
    // 2 倍速：额外跑一帧（音频会溢出丢弃，正常）
    core_->runFrame();
  }
}

int64_t EmuRunner::elapsedSec() const {
  if (startTickMs_ == 0) return 0;
  return ((int64_t)SDL_GetTicks() - startTickMs_) / 1000;
}

}  // namespace ns
