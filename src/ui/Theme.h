// NesStation PC — 主题系统：Neon 3D 游戏站 / FSD 经典 双皮肤即时切换
//
// 令牌完整移植自 Android 版：
//   · Neon（NeonUi.kt 设计令牌）：深空黑蓝底 + 电光青 + 警示红 + 切角边框
//   · FSD（FsdHomeScreen / FsdScaffold）：Xbox360 Freestyle Dash 深蓝渐变 +
//     蓝/黄对角磁贴 + 圆角
#pragma once
#include <string>

#include "ui/Ui.h"

namespace ns {

enum class UiStyle : int { Neon = 0, Fsd = 1 };

struct Skin {
  // 底色
  Color8 bg, bgDeep, bgPanel, bgPanelHi, line;
  // 主色
  Color8 accent, accentDim, red, blue, gold, green, purple;
  // 文字
  Color8 textHi, text, textDim;
  // 手柄键色（A/B/X/Y）
  Color8 btnA, btnB, btnX, btnY;
  // 形状语言：Neon = 切角，FSD = 圆角
  float panelRadius = 10;
  bool chamfer = false;
  const char* name = "";
};

// 当前皮肤（全局单例，App 启动时从配置载入）
class Theme {
 public:
  static void setStyle(UiStyle s);
  static UiStyle style() { return style_; }
  static bool isNeon() { return style_ == UiStyle::Neon; }
  static const Skin& skin() { return style_ == UiStyle::Neon ? neon() : fsd(); }

  static const Skin& neon();
  static const Skin& fsd();

 private:
  static UiStyle style_;
};

// 按皮肤画面板（切角/圆角自动选择）
void skinPanel(class Draw& d, const Rect& rc, bool highlighted = false, float glow = 0);

}  // namespace ns
