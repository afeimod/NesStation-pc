// NesStation PC — 游戏运行器：进程内 libretro 会话 + 模拟器场景（游戏内菜单）
#pragma once
#include <memory>
#include <string>

#include "app/App.h"
#include "retro/LibretroCore.h"

namespace ns {

class EmuRunner {
 public:
  ~EmuRunner();

  // 启动游戏：libretro 平台装载核心并推入 EmulatorScene；
  // Standalone 平台直接外部启动模拟器。
  // 返回空串 = 成功；否则为错误提示（调用方 toast）
  std::string start(App& app, const GameEntry& game);
  void stop(App& app, bool saveAuto);

  bool running() const { return running_; }
  const GameEntry& game() const { return game_; }
  LibretroCore* core() { return core_.get(); }
  bool isLibretro() const { return isLibretro_; }

  // —— 会话操作（EmulatorScene 转发）——
  bool saveStateSlot(App& app, int slot);
  bool loadStateSlot(App& app, int slot);
  std::vector<int> slots(App& app) const;
  void resetCore();
  void setFastForward(bool ff);
  bool fastForward() const { return fastForward_; }
  // 核心标称帧率（av_info.timing.fps，实时 —— SET_SYSTEM_AV_INFO 后会更新）
  double coreFps() const { return core_ ? core_->timingFps() : coreFps_; }

  // ★v1.2.11 目标帧率（对齐 APK FramePacer 逐核心目标）：
  //   NES/SFC/GB/GBA/MD/PCE/NDS/ARCADE/DOS → 硬编码 60（APK pace(t0, 60)，
  //     不用 av_info 的 60.0988 —— FC 锁 60fps 的根源）；
  //   PSX/DC/PSP（及 PC 进程内的 PS2/3DS/JAVA）→ round(av_info.fps)
  //     值位 [30,600]（APK：NTSC 59.83→60，PAL 50→50）。
  double targetFps() const;

  void runFrame();           // 一帧（含快进倍率）
  void runFrameOnce();       // 严格一帧

  int64_t elapsedSec() const;

 private:
  void teardown(App& app);

  GameEntry game_;
  bool running_ = false;
  bool isLibretro_ = false;
  bool fastForward_ = false;
  int ffCount_ = 0;
  std::unique_ptr<LibretroCore> core_;
  int64_t startTickMs_ = 0;
  std::string coreKey_;  // 选项持久层键
  double coreFps_ = 60.0;
};

}  // namespace ns
