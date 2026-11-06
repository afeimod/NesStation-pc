// NesStation PC — 输入管理：键盘 / SDL 手柄 / 鼠标 → UI 动作 + libretro 输入
#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include <SDL.h>

#include "ui/Ui.h"

namespace ns {

// libretro 手柄键位（与 RETRO_DEVICE_ID_JOYPAD_* 一致）
enum RetroPad : int {
  RP_B = 0, RP_A = 1, RP_Y = 2, RP_X = 3,
  RP_L = 4, RP_R = 5, RP_L2 = 6, RP_R2 = 7,
  RP_L3 = 8, RP_R3 = 9,
  RP_START = 10, RP_SELECT = 11,
  RP_UP = 12, RP_DOWN = 13, RP_LEFT = 14, RP_RIGHT = 15,
  RP_COUNT = 16
};

extern const char* retroPadName(int btn);  // "B" "A" "L2" "Start" ...

// 单个键位绑定（键盘或手柄）
struct KeyBind {
  SDL_Keycode key = SDLK_UNKNOWN;                                    // 键盘键（布局相关，持久化用）
  SDL_Scancode scan = SDL_SCANCODE_UNKNOWN;                           // 物理扫描码（布局无关）
  SDL_GameControllerButton padBtn = SDL_CONTROLLER_BUTTON_INVALID;   // 手柄键
  bool valid() const {
    return key != SDLK_UNKNOWN || scan != SDL_SCANCODE_UNKNOWN ||
           padBtn != SDL_CONTROLLER_BUTTON_INVALID;
  }
};

// 完整映射：两套端口 × 全部键位
struct PadMapping {
  KeyBind binds[2][RP_COUNT];
  // 默认键盘映射（原项目 README 控制表）：方向键=十字，X=A，Z=B，S=X，
  // A=Y，Q=L，W=R，D=L2，F=R2，Enter=Start，Shift=Select
  static PadMapping defaults();
};

class InputManager {
 public:
  void init();
  void shutdown();

  // 帧序：beginFrame() → 事件泵 → endFrame(dt) → UI 读取 input()
  void beginFrame();
  void endFrame(float dt);

  const InputState& input() const { return ui_; }

  // SDL 事件
  void handleEvent(const SDL_Event& ev);

  // —— libretro 输入（核心回调，主线程）——
  int16_t retroInputState(unsigned port, unsigned device, unsigned index, unsigned id);

  // —— 核心键盘直通（DOSBox 等）——
  void setCoreKeyboardCallback(void (*cb)(bool down, unsigned keycode, uint32_t character,
                                          uint16_t key_modifiers));

  // —— 按键重映射捕获 ——
  void captureBind(int port, int retroBtn);
  bool isCapturing() const { return capturePort_ >= 0; }
  int capturePort() const { return capturePort_; }
  int captureBtn() const { return captureBtn_; }
  void cancelCapture() { capturePort_ = -1; }

  const PadMapping& mapping() const { return mapping_; }
  void setMapping(const PadMapping& m);
  void resetMapping();

  int gamepadCount() const;
  std::string gamepadName(int idx) const;

  // —— 文本输入（重命名弹窗）——
  const std::string& textBuffer() const { return textBuf_; }
  std::string& textBufferMut() { return textBuf_; }
  void textClear() { textBuf_.clear(); }
  void textInputStart();
  void textInputStop();

  // ★v1.2.7 鼠标坐标 → UI 设计单位 的折算系数（窗口点数 → 逻辑坐标）。
  //   渲染器逻辑尺寸不再等于窗口点数（高 DPI / UI 缩放），
  //   所有 UI 命中测试在设计单位下进行，事件坐标需同比例折算。
  void setMouseToUiScale(float s) { mouseToUi_ = s > 0.0f ? s : 1.0f; }

 private:
  void openGamepads();
  void setUiBtn(Btn b, bool down);
  void feedCoreKey(bool down, const SDL_Keysym& sym);
  void pollRetroState();

  InputState ui_;
  bool rawHeld_[(int)Btn::Count] = {};

  // 帧内累积
  bool mousePressedAccum_ = false;
  bool mouseReleasedAccum_ = false;
  bool mouseHeldNow_ = false;
  bool mouseRightHeld_ = false;
  bool mouseRightPressedAccum_ = false;
  int wheelAccum_ = 0;
  float mouseDxAccum_ = 0, mouseDyAccum_ = 0;
  float mouseToUi_ = 1.0f;  // ★v1.2.7 窗口点数 → 设计单位

  // 长按重复
  float repeatTimer_[(int)Btn::Count] = {};
  float repeatDelay_[(int)Btn::Count] = {};

  // libretro 原始状态
  bool retroHeld_[2][RP_COUNT] = {};
  int16_t retroAxes_[2][4] = {};  // [port][Lx,Ly,Rx,Ry]

  PadMapping mapping_ = PadMapping::defaults();
  SDL_GameController* pads_[2] = {nullptr, nullptr};
  void (*coreKeyCb_)(bool, unsigned, uint32_t, uint16_t) = nullptr;

  int capturePort_ = -1;
  int captureBtn_ = -1;

  std::string textBuf_;
  bool textEditing_ = false;
};

// 键名（中文界面）："←" "Enter" "A" 等
std::string sdlKeyName(SDL_Keycode key);
std::string sdlPadButtonName(SDL_GameControllerButton b);

}  // namespace ns
