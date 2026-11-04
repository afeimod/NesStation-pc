#include "retro/InputManager.h"

#include <cstring>

#include <libretro.h>

#include "util/Log.h"

namespace ns {

const char* retroPadName(int btn) {
  switch (btn) {
    case RP_B: return "B";
    case RP_A: return "A";
    case RP_Y: return "Y";
    case RP_X: return "X";
    case RP_L: return "L";
    case RP_R: return "R";
    case RP_L2: return "L2";
    case RP_R2: return "R2";
    case RP_L3: return "L3";
    case RP_R3: return "R3";
    case RP_START: return "Start";
    case RP_SELECT: return "Select";
    case RP_UP: return "上";
    case RP_DOWN: return "下";
    case RP_LEFT: return "左";
    case RP_RIGHT: return "右";
  }
  return "?";
}

std::string sdlKeyName(SDL_Keycode key) {
  switch (key) {
    case SDLK_UP: return "↑";
    case SDLK_DOWN: return "↓";
    case SDLK_LEFT: return "←";
    case SDLK_RIGHT: return "→";
    case SDLK_RETURN: return "Enter";
    case SDLK_KP_ENTER: return "小键盘Enter";
    case SDLK_ESCAPE: return "Esc";
    case SDLK_LSHIFT: return "左Shift";
    case SDLK_RSHIFT: return "右Shift";
    case SDLK_LCTRL: return "左Ctrl";
    case SDLK_SPACE: return "空格";
    case SDLK_TAB: return "Tab";
    case SDLK_BACKSPACE: return "退格";
    default: break;
  }
  if (key >= 32 && key < 127) {
    std::string s;
    s.push_back((char)toupper((int)key));
    return s;
  }
  if (key >= SDLK_F1 && key <= SDLK_F12) {
    return "F" + std::to_string(key - SDLK_F1 + 1);
  }
  return "键" + std::to_string((int)key);
}

std::string sdlPadButtonName(SDL_GameControllerButton b) {
  switch (b) {
    case SDL_CONTROLLER_BUTTON_A: return "手柄A";
    case SDL_CONTROLLER_BUTTON_B: return "手柄B";
    case SDL_CONTROLLER_BUTTON_X: return "手柄X";
    case SDL_CONTROLLER_BUTTON_Y: return "手柄Y";
    case SDL_CONTROLLER_BUTTON_BACK: return "手柄Back";
    case SDL_CONTROLLER_BUTTON_GUIDE: return "手柄Guide";
    case SDL_CONTROLLER_BUTTON_START: return "手柄Start";
    case SDL_CONTROLLER_BUTTON_LEFTSTICK: return "左摇杆按下";
    case SDL_CONTROLLER_BUTTON_RIGHTSTICK: return "右摇杆按下";
    case SDL_CONTROLLER_BUTTON_LEFTSHOULDER: return "手柄L";
    case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER: return "手柄R";
    case SDL_CONTROLLER_BUTTON_DPAD_UP: return "手柄↑";
    case SDL_CONTROLLER_BUTTON_DPAD_DOWN: return "手柄↓";
    case SDL_CONTROLLER_BUTTON_DPAD_LEFT: return "手柄←";
    case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: return "手柄→";
    default: return "手柄键" + std::to_string((int)b);
  }
}

PadMapping PadMapping::defaults() {
  PadMapping m;
  // 键码 + 物理扫描码双存：扫描码与布局无关（非美式键盘 / 部分 arm64
  // 设备 evdev/Wine 环境下 keysym 翻译失败时仍能正确识别）
  auto K = [&](int btn, SDL_Keycode k, SDL_Scancode sc) {
    m.binds[0][btn].key = k;
    m.binds[0][btn].scan = sc;
  };
  K(RP_UP, SDLK_UP, SDL_SCANCODE_UP);
  K(RP_DOWN, SDLK_DOWN, SDL_SCANCODE_DOWN);
  K(RP_LEFT, SDLK_LEFT, SDL_SCANCODE_LEFT);
  K(RP_RIGHT, SDLK_RIGHT, SDL_SCANCODE_RIGHT);
  K(RP_A, SDLK_x, SDL_SCANCODE_X);
  K(RP_B, SDLK_z, SDL_SCANCODE_Z);
  K(RP_X, SDLK_s, SDL_SCANCODE_S);
  K(RP_Y, SDLK_a, SDL_SCANCODE_A);
  K(RP_L, SDLK_q, SDL_SCANCODE_Q);
  K(RP_R, SDLK_w, SDL_SCANCODE_W);
  K(RP_L2, SDLK_d, SDL_SCANCODE_D);
  K(RP_R2, SDLK_f, SDL_SCANCODE_F);
  K(RP_START, SDLK_RETURN, SDL_SCANCODE_RETURN);
  K(RP_SELECT, SDLK_LSHIFT, SDL_SCANCODE_LSHIFT);

  auto P = [&](int btn, SDL_GameControllerButton b) { m.binds[0][btn].padBtn = b; };
  P(RP_UP, SDL_CONTROLLER_BUTTON_DPAD_UP);
  P(RP_DOWN, SDL_CONTROLLER_BUTTON_DPAD_DOWN);
  P(RP_LEFT, SDL_CONTROLLER_BUTTON_DPAD_LEFT);
  P(RP_RIGHT, SDL_CONTROLLER_BUTTON_DPAD_RIGHT);
  P(RP_A, SDL_CONTROLLER_BUTTON_A);
  P(RP_B, SDL_CONTROLLER_BUTTON_B);
  P(RP_X, SDL_CONTROLLER_BUTTON_X);
  P(RP_Y, SDL_CONTROLLER_BUTTON_Y);
  P(RP_L, SDL_CONTROLLER_BUTTON_LEFTSHOULDER);
  P(RP_R, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER);
  P(RP_L2, SDL_CONTROLLER_BUTTON_LEFTSTICK);
  P(RP_R2, SDL_CONTROLLER_BUTTON_RIGHTSTICK);
  P(RP_START, SDL_CONTROLLER_BUTTON_START);
  P(RP_SELECT, SDL_CONTROLLER_BUTTON_BACK);

  for (int b = 0; b < RP_COUNT; b++) m.binds[1][b].padBtn = m.binds[0][b].padBtn;
  return m;
}

void InputManager::init() {
  SDL_GameControllerEventState(SDL_ENABLE);
  openGamepads();
}

void InputManager::openGamepads() {
  for (int i = 0; i < SDL_NumJoysticks(); i++) {
    if (!SDL_IsGameController(i)) continue;
    int slot = -1;
    for (int s = 0; s < 2; s++)
      if (!pads_[s]) { slot = s; break; }
    if (slot < 0) break;
    SDL_GameController* gc = SDL_GameControllerOpen(i);
    if (gc) {
      pads_[slot] = gc;
      LOGI("手柄 %d 已连接：%s", slot, SDL_GameControllerName(gc));
    }
  }
}

void InputManager::shutdown() {
  for (auto& p : pads_) {
    if (p) { SDL_GameControllerClose(p); p = nullptr; }
  }
}

int InputManager::gamepadCount() const {
  int n = 0;
  for (auto p : pads_)
    if (p) n++;
  return n;
}

std::string InputManager::gamepadName(int idx) const {
  if (idx < 0 || idx >= 2 || !pads_[idx]) return "";
  const char* n = SDL_GameControllerName(pads_[idx]);
  return n ? n : "Gamepad";
}

void InputManager::setUiBtn(Btn b, bool down) {
  if (down) {
    if (!rawHeld_[(int)b]) {
      rawHeld_[(int)b] = true;
      ui_.pressed[(int)b] = true;
      repeatDelay_[(int)b] = 0.45f;
      repeatTimer_[(int)b] = 0.0f;
    }
  } else {
    rawHeld_[(int)b] = false;
    ui_.released[(int)b] = true;
  }
}

void InputManager::beginFrame() {
  // 新的一帧：清除上一帧的沿与累积
  for (int i = 0; i < (int)Btn::Count; i++) {
    ui_.pressed[i] = false;
    ui_.released[i] = false;
  }
  mousePressedAccum_ = false;
  mouseReleasedAccum_ = false;
  mouseRightPressedAccum_ = false;
  wheelAccum_ = 0;
}

void InputManager::endFrame(float dt) {
  for (int i = 0; i < (int)Btn::Count; i++) {
    ui_.held[i] = rawHeld_[i];
    // 方向类长按自动重复（键盘/手柄导航体验）
    if (rawHeld_[i]) {
      const bool repeatable =
          i == (int)Btn::Up || i == (int)Btn::Down || i == (int)Btn::Left ||
          i == (int)Btn::Right || i == (int)Btn::PageUp || i == (int)Btn::PageDown;
      if (repeatable) {
        if (repeatDelay_[i] > 0) {
          repeatDelay_[i] -= dt;
        } else {
          repeatTimer_[i] += dt;
          if (repeatTimer_[i] >= 0.12f) {
            repeatTimer_[i] = 0;
            ui_.pressed[i] = true;
          }
        }
      }
    }
  }
  ui_.mousePressed = mousePressedAccum_;
  ui_.mouseReleased = mouseReleasedAccum_;
  ui_.mouseHeld = mouseHeldNow_;
  ui_.mouseRightPressed = mouseRightPressedAccum_;
  ui_.wheel = wheelAccum_;
  pollRetroState();
}

void InputManager::setMapping(const PadMapping& m) {
  mapping_ = m;
  // 配置只持久化键码；加载后补齐物理扫描码（当前布局下同名键的位置）
  for (int p = 0; p < 2; p++) {
    for (int b = 0; b < RP_COUNT; b++) {
      KeyBind& kb = mapping_.binds[p][b];
      if (kb.key != SDLK_UNKNOWN && kb.scan == SDL_SCANCODE_UNKNOWN)
        kb.scan = SDL_GetScancodeFromKey(kb.key);
    }
  }
}

void InputManager::resetMapping() { mapping_ = PadMapping::defaults(); }

// —— 键盘事件匹配：键码或扫描码命中均算（部分 arm64 设备/非美式布局下
// keysym 翻译失败，但 scancode 始终有效；反之亦然）——
static inline bool keyIs(const SDL_Keysym& k, SDL_Keycode sym, SDL_Scancode sc) {
  return k.sym == sym || k.scancode == sc;
}

void InputManager::pollRetroState() {
  const Uint8* kbd = SDL_GetKeyboardState(nullptr);
  for (int port = 0; port < 2; port++) {
    for (int btn = 0; btn < RP_COUNT; btn++) {
      bool v = false;
      const KeyBind& kb = mapping_.binds[port][btn];
      if (port == 0) {
        // 优先物理扫描码（布局无关）；无扫描码时回退键码转换
        if (kb.scan != SDL_SCANCODE_UNKNOWN) {
          if (kbd[kb.scan]) v = true;
        } else if (kb.key != SDLK_UNKNOWN) {
          const SDL_Scancode sc = SDL_GetScancodeFromKey(kb.key);
          if (sc != SDL_SCANCODE_UNKNOWN && kbd[sc]) v = true;
        }
      }
      if (kb.padBtn != SDL_CONTROLLER_BUTTON_INVALID && pads_[port]) {
        if (SDL_GameControllerGetButton(pads_[port], kb.padBtn)) v = true;
      }
      retroHeld_[port][btn] = v;
    }
    if (pads_[port]) {
      retroAxes_[port][0] = SDL_GameControllerGetAxis(pads_[port], SDL_CONTROLLER_AXIS_LEFTX);
      retroAxes_[port][1] = SDL_GameControllerGetAxis(pads_[port], SDL_CONTROLLER_AXIS_LEFTY);
      retroAxes_[port][2] = SDL_GameControllerGetAxis(pads_[port], SDL_CONTROLLER_AXIS_RIGHTX);
      retroAxes_[port][3] = SDL_GameControllerGetAxis(pads_[port], SDL_CONTROLLER_AXIS_RIGHTY);
    } else {
      for (auto& a : retroAxes_[port]) a = 0;
    }
  }
}

void InputManager::handleEvent(const SDL_Event& ev) {
  switch (ev.type) {
    case SDL_KEYDOWN: {
      if (ev.key.repeat) break;
      const SDL_Keysym& k = ev.key.keysym;

      if (isCapturing() && k.sym != SDLK_ESCAPE && k.scancode != SDL_SCANCODE_ESCAPE) {
        // 同时存键码与扫描码：扫描码保证非美式布局/arm64 设备上稳定
        mapping_.binds[capturePort_][captureBtn_].key = k.sym;
        mapping_.binds[capturePort_][captureBtn_].scan = k.scancode;
        mapping_.binds[capturePort_][captureBtn_].padBtn = SDL_CONTROLLER_BUTTON_INVALID;
        cancelCapture();
        break;
      }

      // 文本输入模式：退格删字符，其余交给 UI
      if (textEditing_ &&
          (k.sym == SDLK_BACKSPACE || k.scancode == SDL_SCANCODE_BACKSPACE) &&
          !textBuf_.empty()) {
        // 删除最后一个 UTF-8 字符：从末尾回溯到非延续字节
        size_t s = textBuf_.size() - 1;
        while (s > 0 && ((uint8_t)textBuf_[s] & 0xC0) == 0x80) s--;
        textBuf_.erase(s);
        break;
      }

      feedCoreKey(true, k);

      // UI 导航：键码或扫描码任一命中（修复 arm64 设备 keysym 翻译失败
      // 导致方向键无法控制菜单的问题）
      if (keyIs(k, SDLK_UP, SDL_SCANCODE_UP)) setUiBtn(Btn::Up, true);
      else if (keyIs(k, SDLK_DOWN, SDL_SCANCODE_DOWN)) setUiBtn(Btn::Down, true);
      else if (keyIs(k, SDLK_LEFT, SDL_SCANCODE_LEFT)) setUiBtn(Btn::Left, true);
      else if (keyIs(k, SDLK_RIGHT, SDL_SCANCODE_RIGHT)) setUiBtn(Btn::Right, true);
      else if (k.sym == SDLK_RETURN || k.sym == SDLK_KP_ENTER ||
               k.scancode == SDL_SCANCODE_RETURN || k.scancode == SDL_SCANCODE_KP_ENTER)
        setUiBtn(Btn::Ok, true);
      else if (keyIs(k, SDLK_ESCAPE, SDL_SCANCODE_ESCAPE)) setUiBtn(Btn::Back, true);
      else if (keyIs(k, SDLK_F1, SDL_SCANCODE_F1)) setUiBtn(Btn::Menu, true);
      else if (keyIs(k, SDLK_e, SDL_SCANCODE_E)) setUiBtn(Btn::Option, true);
      else if (keyIs(k, SDLK_f, SDL_SCANCODE_F)) setUiBtn(Btn::Fav, true);
      else if (keyIs(k, SDLK_PAGEUP, SDL_SCANCODE_PAGEUP)) setUiBtn(Btn::PageUp, true);
      else if (keyIs(k, SDLK_PAGEDOWN, SDL_SCANCODE_PAGEDOWN)) setUiBtn(Btn::PageDown, true);
      break;
    }
    case SDL_KEYUP: {
      const SDL_Keysym& k = ev.key.keysym;
      feedCoreKey(false, k);
      if (keyIs(k, SDLK_UP, SDL_SCANCODE_UP)) setUiBtn(Btn::Up, false);
      else if (keyIs(k, SDLK_DOWN, SDL_SCANCODE_DOWN)) setUiBtn(Btn::Down, false);
      else if (keyIs(k, SDLK_LEFT, SDL_SCANCODE_LEFT)) setUiBtn(Btn::Left, false);
      else if (keyIs(k, SDLK_RIGHT, SDL_SCANCODE_RIGHT)) setUiBtn(Btn::Right, false);
      else if (k.sym == SDLK_RETURN || k.sym == SDLK_KP_ENTER ||
               k.scancode == SDL_SCANCODE_RETURN || k.scancode == SDL_SCANCODE_KP_ENTER)
        setUiBtn(Btn::Ok, false);
      else if (keyIs(k, SDLK_ESCAPE, SDL_SCANCODE_ESCAPE)) setUiBtn(Btn::Back, false);
      else if (keyIs(k, SDLK_F1, SDL_SCANCODE_F1)) setUiBtn(Btn::Menu, false);
      else if (keyIs(k, SDLK_e, SDL_SCANCODE_E)) setUiBtn(Btn::Option, false);
      else if (keyIs(k, SDLK_f, SDL_SCANCODE_F)) setUiBtn(Btn::Fav, false);
      else if (keyIs(k, SDLK_PAGEUP, SDL_SCANCODE_PAGEUP)) setUiBtn(Btn::PageUp, false);
      else if (keyIs(k, SDLK_PAGEDOWN, SDL_SCANCODE_PAGEDOWN)) setUiBtn(Btn::PageDown, false);
      break;
    }
    case SDL_TEXTINPUT:
      if (textEditing_) textBuf_ += ev.text.text;
      break;
    case SDL_MOUSEMOTION:
      ui_.mouse = Vec2((float)ev.motion.x, (float)ev.motion.y);
      break;
    case SDL_MOUSEBUTTONDOWN:
      ui_.mouse = Vec2((float)ev.button.x, (float)ev.button.y);
      if (ev.button.button == SDL_BUTTON_LEFT) {
        if (!mouseHeldNow_) {
          mouseHeldNow_ = true;
          mousePressedAccum_ = true;
        }
        // 注意：鼠标点击不再映射为 Btn::Ok —— 否则点窗口任意位置都会
        // 触发“确认当前焦点项”。鼠标交互统一由各控件矩形命中测试处理
        //（w::activated / 场景内 rc.contains）。
      }
      if (ev.button.button == SDL_BUTTON_RIGHT) {
        if (!mouseRightHeld_) {
          mouseRightHeld_ = true;
          mouseRightPressedAccum_ = true;
        }
      }
      break;
    case SDL_MOUSEBUTTONUP:
      ui_.mouse = Vec2((float)ev.button.x, (float)ev.button.y);
      if (ev.button.button == SDL_BUTTON_LEFT) {
        mouseHeldNow_ = false;
        mouseReleasedAccum_ = true;
      }
      if (ev.button.button == SDL_BUTTON_RIGHT) {
        mouseRightHeld_ = false;
      }
      break;
    case SDL_MOUSEWHEEL:
      wheelAccum_ += ev.wheel.y;
      break;
    case SDL_CONTROLLERDEVICEADDED:
      openGamepads();
      break;
    case SDL_CONTROLLERDEVICEREMOVED:
      for (auto& p : pads_) {
        if (p && SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(p)) == ev.cdevice.which) {
          SDL_GameControllerClose(p);
          p = nullptr;
        }
      }
      break;
    case SDL_CONTROLLERBUTTONDOWN: {
      const SDL_GameControllerButton b = (SDL_GameControllerButton)ev.cbutton.button;
      if (isCapturing() && b != SDL_CONTROLLER_BUTTON_GUIDE) {
        mapping_.binds[capturePort_][captureBtn_].key = SDLK_UNKNOWN;
        mapping_.binds[capturePort_][captureBtn_].padBtn = b;
        cancelCapture();
        break;
      }
      switch (b) {
        case SDL_CONTROLLER_BUTTON_DPAD_UP: setUiBtn(Btn::Up, true); break;
        case SDL_CONTROLLER_BUTTON_DPAD_DOWN: setUiBtn(Btn::Down, true); break;
        case SDL_CONTROLLER_BUTTON_DPAD_LEFT: setUiBtn(Btn::Left, true); break;
        case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: setUiBtn(Btn::Right, true); break;
        case SDL_CONTROLLER_BUTTON_A: setUiBtn(Btn::Ok, true); break;
        case SDL_CONTROLLER_BUTTON_B: setUiBtn(Btn::Back, true); break;
        case SDL_CONTROLLER_BUTTON_START: setUiBtn(Btn::Menu, true); break;
        case SDL_CONTROLLER_BUTTON_Y: setUiBtn(Btn::Option, true); break;
        case SDL_CONTROLLER_BUTTON_X: setUiBtn(Btn::Fav, true); break;
        case SDL_CONTROLLER_BUTTON_LEFTSHOULDER: setUiBtn(Btn::PageUp, true); break;
        case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER: setUiBtn(Btn::PageDown, true); break;
        default: break;
      }
      break;
    }
    case SDL_CONTROLLERBUTTONUP: {
      const SDL_GameControllerButton b = (SDL_GameControllerButton)ev.cbutton.button;
      switch (b) {
        case SDL_CONTROLLER_BUTTON_DPAD_UP: setUiBtn(Btn::Up, false); break;
        case SDL_CONTROLLER_BUTTON_DPAD_DOWN: setUiBtn(Btn::Down, false); break;
        case SDL_CONTROLLER_BUTTON_DPAD_LEFT: setUiBtn(Btn::Left, false); break;
        case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: setUiBtn(Btn::Right, false); break;
        case SDL_CONTROLLER_BUTTON_A: setUiBtn(Btn::Ok, false); break;
        case SDL_CONTROLLER_BUTTON_B: setUiBtn(Btn::Back, false); break;
        case SDL_CONTROLLER_BUTTON_START: setUiBtn(Btn::Menu, false); break;
        case SDL_CONTROLLER_BUTTON_Y: setUiBtn(Btn::Option, false); break;
        case SDL_CONTROLLER_BUTTON_X: setUiBtn(Btn::Fav, false); break;
        case SDL_CONTROLLER_BUTTON_LEFTSHOULDER: setUiBtn(Btn::PageUp, false); break;
        case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER: setUiBtn(Btn::PageDown, false); break;
        default: break;
      }
      break;
    }
    default:
      break;
  }
}

void InputManager::feedCoreKey(bool down, const SDL_Keysym& sym) {
  if (!coreKeyCb_) return;
  unsigned rk = RETROK_UNKNOWN;
  const SDL_Keycode k = sym.sym;
  if (k >= 32 && k < 127) {
    rk = (unsigned)k;
  } else {
    switch (k) {
      case SDLK_RETURN: rk = RETROK_RETURN; break;
      case SDLK_ESCAPE: rk = RETROK_ESCAPE; break;
      case SDLK_BACKSPACE: rk = RETROK_BACKSPACE; break;
      case SDLK_TAB: rk = RETROK_TAB; break;
      case SDLK_SPACE: rk = RETROK_SPACE; break;
      case SDLK_DELETE: rk = RETROK_DELETE; break;
      case SDLK_INSERT: rk = RETROK_INSERT; break;
      case SDLK_HOME: rk = RETROK_HOME; break;
      case SDLK_END: rk = RETROK_END; break;
      case SDLK_PAGEUP: rk = RETROK_PAGEUP; break;
      case SDLK_PAGEDOWN: rk = RETROK_PAGEDOWN; break;
      case SDLK_UP: rk = RETROK_UP; break;
      case SDLK_DOWN: rk = RETROK_DOWN; break;
      case SDLK_LEFT: rk = RETROK_LEFT; break;
      case SDLK_RIGHT: rk = RETROK_RIGHT; break;
      case SDLK_LSHIFT: case SDLK_RSHIFT: rk = RETROK_LSHIFT; break;
      case SDLK_LCTRL: case SDLK_RCTRL: rk = RETROK_LCTRL; break;
      case SDLK_LALT: case SDLK_RALT: rk = RETROK_LALT; break;
      default: break;
    }
  }
  uint16_t mods = 0;
  if (sym.mod & KMOD_SHIFT) mods |= RETROKMOD_SHIFT;
  if (sym.mod & KMOD_CTRL) mods |= RETROKMOD_CTRL;
  if (sym.mod & KMOD_ALT) mods |= RETROKMOD_ALT;
  if (sym.mod & KMOD_GUI) mods |= RETROKMOD_META;
  coreKeyCb_(down, rk, (uint32_t)(down ? sym.sym : 0), mods);
}

int16_t InputManager::retroInputState(unsigned port, unsigned device, unsigned index,
                                      unsigned id) {
  if (device == RETRO_DEVICE_JOYPAD || device == RETRO_DEVICE_NONE) {
    if (port >= 2 || id >= RP_COUNT) return 0;
    return retroHeld_[port][id] ? 1 : 0;
  }
  if (device == RETRO_DEVICE_ANALOG) {
    if (port >= 2 || index > 1 || id > 1) return 0;
    if (pads_[port]) return retroAxes_[port][index * 2 + id];
    // 无手柄：十字键 → 左摇杆（键盘玩家可用方向键玩模拟摇杆游戏）
    if (index == RETRO_DEVICE_INDEX_ANALOG_LEFT) {
      if (id == RETRO_DEVICE_ID_ANALOG_X) {
        int16_t v = 0;
        if (retroHeld_[port][RP_RIGHT]) v = 0x7fff;
        else if (retroHeld_[port][RP_LEFT]) v = (int16_t)-0x8000;
        return v;
      }
      if (id == RETRO_DEVICE_ID_ANALOG_Y) {
        int16_t v = 0;
        if (retroHeld_[port][RP_DOWN]) v = 0x7fff;
        else if (retroHeld_[port][RP_UP]) v = (int16_t)-0x8000;
        return v;
      }
    }
    return 0;
  }
  return 0;
}

void InputManager::setCoreKeyboardCallback(
    void (*cb)(bool, unsigned, uint32_t, uint16_t)) {
  coreKeyCb_ = cb;
}

void InputManager::captureBind(int port, int retroBtn) {
  capturePort_ = port;
  captureBtn_ = retroBtn;
}

void InputManager::textInputStart() {
  textEditing_ = true;
  SDL_StartTextInput();
}
void InputManager::textInputStop() {
  textEditing_ = false;
  SDL_StopTextInput();
}

}  // namespace ns
