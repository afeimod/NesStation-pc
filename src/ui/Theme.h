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
  // ★v1.2.9 FSD 浅色页覆盖（对齐 APK SettingsScreen：PixelBackdrop 像素天空 +
  //   白 65% 半透明卡片 + 深色文字）：仅 FSD 皮肤的设置页等浅色场景开启；
  //   skin() 在 开 && FSD 时返回 fsdLight()，其余不变。
  static void setLightFsd(bool on) { lightFsd_ = on; }
  static bool lightFsd() { return lightFsd_; }
  static const Skin& skin() {
    if (style_ == UiStyle::Fsd && lightFsd_) return fsdLight();
    return style_ == UiStyle::Neon ? neon() : fsd();
  }

  static const Skin& neon();
  static const Skin& fsd();
  static const Skin& fsdLight();

 private:
  static UiStyle style_;
  static bool lightFsd_;
};

// 按皮肤画面板（切角/圆角自动选择）
void skinPanel(class Draw& d, const Rect& rc, bool highlighted = false, float glow = 0);

}  // namespace ns
