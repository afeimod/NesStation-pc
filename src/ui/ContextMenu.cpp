// NesStation PC — 通用右键上下文菜单（Android 版「长按卡片」的 PC 等价交互）
//
// 对应 Android 版语义：长按游戏卡 / 列表项 → 弹出快捷操作菜单。
// PC 版统一改为：鼠标右键（或手柄 Option / 键盘 E）呼出。
// 菜单出现位置 = 呼出点（越界自动翻转），支持：
//   键盘 ↑↓ 导航 / Enter 执行 / Esc 关闭；
//   鼠标点击执行 / 点击菜单外关闭；
//   皮肤化（Neon 切角面板 / FSD 圆角面板）。
#include <algorithm>
#include <cmath>

#include "ui/Scenes.h"
#include "ui/Widgets.h"

namespace ns {
namespace {

class ContextMenuScene : public Scene {
 public:
  ContextMenuScene(const Vec2& at, std::string title, std::string subtitle,
                   std::vector<ContextMenuItem> items)
      : at_(at), title_(std::move(title)), subtitle_(std::move(subtitle)),
        items_(std::move(items)) {
    // 立即计算布局（draw 依赖）
    const float w = 286;
    float h = 18;  // 顶留白
    if (!title_.empty()) {
      h += 30;
      if (!subtitle_.empty()) h += 20;
      h += 10;  // 分隔区
    }
    for (size_t i = 0; i < items_.size(); i++) h += 46;
    h += 12;
    panel_.w = w;
    panel_.h = h;
    // 右下展开；越界翻转（对齐桌面右键菜单习惯）
    // ★v1.2.10：旧版 1280×720 硬编码 —— 大逻辑画布（固定缩放/大窗）下
    //   右下角右键不翻转导致菜单出屏。构造时无 App 引用，翻转延迟到首帧
    //   layout(App&) 里按真实 draw.W/H 计算（首帧前 panel_ 仍用保守估位）。
    float x = at_.x + 6, y = at_.y + 6;
    if (x < 8) x = 8;
    if (y < 8) y = 8;
    panel_.x = x;
    panel_.y = y;
    laidOut_ = false;
  }

  void update(App& app, float dt, const InputState& in) override {
    (void)dt;
    // ★首帧按真实画布尺寸重算翻转（右下角右键不再出屏）
    if (!laidOut_) {
      laidOut_ = true;
      float x = at_.x + 6, y = at_.y + 6;
      if (x + panel_.w > (float)app.draw.W - 16) x = at_.x - panel_.w - 6;
      if (y + panel_.h > (float)app.draw.H - 56) y = at_.y - panel_.h - 6;
      if (x < 8) x = 8;
      if (y < 8) y = 8;
      panel_.x = x;
      panel_.y = y;
    }
    // 关闭：Esc / B / Menu / 右键菜单外点击
    const bool outsideClick =
        (in.mousePressed || in.mouseRightPressed) && !panel_.contains(in.mouse);
    if (in.pressed[(int)Btn::Back] || in.pressed[(int)Btn::Menu] || outsideClick) {
      app.popScene();
      return;
    }
    const int N = (int)items_.size();
    if (N == 0) {
      app.popScene();
      return;
    }
    if (in.pressed[(int)Btn::Up]) sel_ = (sel_ + N - 1) % N;
    if (in.pressed[(int)Btn::Down]) sel_ = (sel_ + 1) % N;
    if (in.pressed[(int)Btn::Ok] && !hoverAny_) trigger(app, sel_);
    // 鼠标 hover 期间同步 sel_（键盘焦点与鼠标统一）
    if (hoverIdx_ >= 0 && hoverIdx_ != sel_ && !in.pressed[(int)Btn::Up] &&
        !in.pressed[(int)Btn::Down]) {
      sel_ = hoverIdx_;
    }
    if (hoverIdx_ >= 0 && in.mousePressed) trigger(app, hoverIdx_);
  }

  void trigger(App& app, int idx) {
    if (idx < 0 || idx >= (int)items_.size()) return;
    auto act = std::move(items_[(size_t)idx].action);
    app.popScene();
    if (act) act();  // 回调里可能再 push 场景（重命名 / 核心选择器等）
  }

  void draw(App& app) override {
    const Skin& sk = Theme::skin();
    Draw& d = app.draw;
    // 点击拦截层（视觉轻遮罩，能看出底下的卡片）
    d.rect(Rect(0, 0, app.draw.W, app.draw.H), Color8(0, 0, 0, 66));

    skinPanel(d, panel_, true, 10);
    d.roundRectOutline(panel_, sk.panelRadius, sk.line, 1.2f);

    const InputState& in = app.input.input();
    hoverAny_ = false;
    hoverIdx_ = -1;
    float y = panel_.y + 16;
    if (!title_.empty()) {
      app.font.draw(app.sdlRenderer(), app.font.ellipsize(title_, 17, (int)panel_.w - 36, true),
                    panel_.x + 18, y, 17, sk.textHi, true);
      y += 26;
      if (!subtitle_.empty()) {
        app.font.draw(app.sdlRenderer(),
                      app.font.ellipsize(subtitle_, 12, (int)panel_.w - 36),
                      panel_.x + 18, y, 12, sk.textDim);
        y += 20;
      }
      y += 4;
      d.line(panel_.x + 14, y, panel_.right() - 14, y, sk.line, 1);
      y += 10;
    }
    for (size_t i = 0; i < items_.size(); i++) {
      const Rect rc(panel_.x + 10, y, panel_.w - 20, 40);
      const ContextMenuItem& it = items_[i];
      const bool focused = sel_ == (int)i;
      const bool hov = rc.contains(in.mouse);
      if (hov) {
        hoverIdx_ = (int)i;
        hoverAny_ = true;
        d.glowRect(rc, 6, sk.accent.withA(70));
      }
      // 底色条（焦点 / hover）
      if (focused && !hov) d.glowRect(rc, 6, sk.accent.withA(46));
      if (focused || hov) d.roundRect(rc, 6, sk.accent.withA((uint8_t)(hov ? 42 : 26)));
      // 图标
      float tx = rc.x + 10;
      if (it.iconId > 0) {
        w::icon(app, it.iconId, Rect(tx, rc.y + 9, 22, 22),
                it.danger ? sk.red : sk.accent);
        tx += 30;
      }
      // 文本
      const Color8 col = it.danger ? (focused || hov ? Color8(0xFF, 0x88, 0x78) : sk.red)
                                   : (focused || hov ? sk.textHi : sk.text);
      app.font.draw(app.sdlRenderer(), it.label, tx, rc.y + 11, 16, col,
                    focused || hov);
      // 右侧提示小字
      if (!it.hint.empty()) {
        app.font.draw(app.sdlRenderer(), it.hint, rc.right() - 12 -
                          app.font.textWidth(it.hint, 12),
                      rc.y + 14, 12, sk.textDim);
      }
      y += 46;
    }
  }

 private:
  Vec2 at_;
  std::string title_, subtitle_;
  std::vector<ContextMenuItem> items_;
  Rect panel_;
  int sel_ = 0;
  int hoverIdx_ = -1;
  bool hoverAny_ = false;
  bool laidOut_ = false;  // ★v1.2.10 首帧按真实画布尺寸重算翻转位置
};

}  // namespace

void pushContextMenu(App& app, const Vec2& at, const std::string& title,
                     const std::string& subtitle,
                     std::vector<ContextMenuItem> items) {
  app.pushScene(std::unique_ptr<Scene>(
      new ContextMenuScene(at, title, subtitle, std::move(items))));
}

}  // namespace ns
