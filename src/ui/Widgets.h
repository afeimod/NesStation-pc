// NesStation PC — 立即模式控件库（绘制 + 交互同帧，主题自动切换）
#pragma once
#include <string>
#include <vector>

#include "app/App.h"

namespace ns {
namespace w {

// 激活判定：键盘/手柄焦点确认 或 鼠标点击
bool activated(App& app, const Rect& rc, bool focused);
bool hovered(App& app, const Rect& rc);

// —— 控件 ——
bool button(App& app, const Rect& rc, const std::string& label, bool focused,
            float fontSize = 17, bool primary = false);
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
void header(App& app, const std::string& title, const std::string& subtitle = "");
void sectionLabel(App& app, float x, float y, const std::string& text);
void hintBar(App& app, const std::vector<std::pair<std::string, std::string>>& hints);
// 底部滚动条
void scrollIndicator(App& app, const Rect& track, float visible01, float offset01);

// —— 图标（矢量绘制，无位图依赖）——
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

// 封面卡片（面板 + 封面/占位 + 收藏角标）
void coverCard(App& app, const Rect& rc, const GameEntry& g, bool selected);

// 平台占位封面（程序化像素风：平台色 + 平台名 + 游戏名）
void platformPlaceholder(App& app, const Rect& rc, GamePlatform p, const std::string& title);
// 取游戏封面纹理（本地文件 → 占位返回 nullptr）
SDL_Texture* coverTexture(App& app, const GameEntry& g);

}  // namespace w
}  // namespace ns
