// NesStation PC — 立即模式控件库（绘制 + 交互同帧，主题自动切换）
// 控件视觉令牌与 Android 版对齐：
//   Neon → neonChamfer 切角语言（NeonUi.kt / NeonChrome.kt）
//   FSD  → 圆角 + 蓝 #2E86E0 / 黄 #F8E71C（FsdCoverFlow.kt）
#pragma once
#include <string>
#include <vector>

#include "app/App.h"

namespace ns {
namespace w {

// 激活判定：键盘/手柄焦点确认 或 鼠标点击（矩形命中）
bool activated(App& app, const Rect& rc, bool focused);
bool hovered(App& app, const Rect& rc);

// —— 控件 ——
bool button(App& app, const Rect& rc, const std::string& label, bool focused,
            float fontSize = 17, bool primary = false);
// Android NeonPrimaryButton：切角横向渐变(accentDim→accent)主按钮，深色文字
bool primaryButton(App& app, const Rect& rc, const std::string& label, int iconId = 0);
// 列表行（标题 + 右侧值 + 副标题），返回激活
bool listRow(App& app, const Rect& rc, const std::string& title,
             const std::string& value, bool focused, const std::string& subtitle = "");
bool toggleRow(App& app, const Rect& rc, const std::string& title, bool& value,
               bool focused, const std::string& subtitle = "");
// 左右循环选项（手柄左右键 / 点击箭头）
bool cycleRow(App& app, const Rect& rc, const std::string& title,
              const std::vector<std::string>& options, int& index, bool focused,
              const std::string& subtitle = "");
// 滑条（0..1）
bool sliderRow(App& app, const Rect& rc, const std::string& title, float& value01,
               bool focused, const std::string& subtitle = "");

// —— 结构元素 ——
void panel(App& app, const Rect& rc, bool highlighted = false, float glow = 0);
// 页头（canBack=true 时左侧带「‹ 返回」按钮；返回 true = 请求返回）
bool header(App& app, const std::string& title, const std::string& subtitle = "",
            bool canBack = false);
void sectionLabel(App& app, float x, float y, const std::string& text);

// 底部按键提示条（Android NeonKeyHint 样式：键帽 chip + 动作文字）
struct KeyHint {
  std::string key;
  std::string label;
  Color8 color;  // 键帽文字色；alpha=0 时用皮肤弱文字色
  KeyHint(const std::string& k, const std::string& l, Color8 c = Color8(0, 0, 0, 0))
      : key(k), label(l), color(c) {}
};
void keyHintBar(App& app, const std::vector<KeyHint>& hints);

// 底部滚动条
void scrollIndicator(App& app, const Rect& track, float visible01, float offset01);

// 平台徽章（Android PlatformBadge：平台色 22% 底 + 平台色文字，圆角 4）
// 返回徽章宽度
float platformBadge(App& app, float x, float cy, GamePlatform p);

// —— 图标（矢量绘制，无位图依赖；对应 Material Icons 圆角系）——
// iconId: 0=无 1=play 2=star 3=gear 4=folder 5=power 6=save 7=load 8=reset
// 9=gamepad 10=info 11=search 12=grid 13=heart 14=history 15=terminal
// 16=chip(memory) 17=help 18=logout 19=arrowLeft 20=refresh
void icon(App& app, int id, const Rect& rc, Color8 c);
void iconPlay(App& app, const Rect& rc, Color8 c);
void iconStar(App& app, const Rect& rc, Color8 c, bool filled);
void iconGear(App& app, const Rect& rc, Color8 c);
void iconBackArrow(App& app, const Rect& rc, Color8 c);
void iconFolder(App& app, const Rect& rc, Color8 c);
void iconPower(App& app, const Rect& rc, Color8 c);
void iconSave(App& app, const Rect& rc, Color8 c);
void iconLoad(App& app, const Rect& rc, Color8 c);
void iconReset(App& app, const Rect& rc, Color8 c);
void iconGamepad(App& app, const Rect& rc, Color8 c);
void iconInfo(App& app, const Rect& rc, Color8 c);
void iconSearch(App& app, const Rect& rc, Color8 c);
void iconGrid(App& app, const Rect& rc, Color8 c);        // 游戏库
void iconHeart(App& app, const Rect& rc, Color8 c);       // 收藏
void iconHistory(App& app, const Rect& rc, Color8 c);     // 最近
void iconTerminal(App& app, const Rect& rc, Color8 c);    // 核心管理(Neon)
void iconChip(App& app, const Rect& rc, Color8 c);        // 核心管理(FSD)
void iconHelp(App& app, const Rect& rc, Color8 c);        // 关于
void iconLogout(App& app, const Rect& rc, Color8 c);      // 退出
void iconArrowLeft(App& app, const Rect& rc, Color8 c);   // 返回
void iconRefresh(App& app, const Rect& rc, Color8 c);     // 刷新

// 封面卡片（面板 + 封面/占位 + 收藏角标）
void coverCard(App& app, const Rect& rc, const GameEntry& g, bool selected);

// 平台占位封面（程序化像素风：平台色 + 平台名 + 游戏名）
void platformPlaceholder(App& app, const Rect& rc, GamePlatform p, const std::string& title);
// 取游戏封面纹理（本地文件 → 占位返回 nullptr）
SDL_Texture* coverTexture(App& app, const GameEntry& g);

}  // namespace w
}  // namespace ns
