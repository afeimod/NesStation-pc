// NesStation PC — 设置（★ 1:1 移植 Android 版 NeonSettingsScreen.kt 结构）
//
//   · 左侧分类导航（220dp）：返回主页 + 外观/视频/显示/性能/输入/存储/核心设置/关于
//     （彩色图标：Palette青/Tune蓝/Speed绿/Percent金/Gamepad蓝BtnX/Dns紫/Memory红/Info白）
//   · 右侧内容：分类大标题（20sp 粗体）+ SettingsBlock（accent 小节标题 + NeonPanel
//     切角面板）+ NeonRow（无框行）+ NeonSwitch / SpinnerRow / StyleCard
//   · 滚动：滚轮 + ★鼠标/触摸拖拽（拖动列表本体即可滑动）
//   · 键盘焦点：逐行独立（修复旧版「Enter 触发所有行」的问题）
#include <algorithm>
#include <cmath>

#if defined(__has_include)
#if __has_include(<SDL_misc.h>)
#include <SDL_misc.h>
#define NS_HAVE_SDL_MISC 1
#endif
#if __has_include(<SDL_version.h>)
#include <SDL_version.h>
#endif
#endif
#if defined(SDL_MAJOR_VERSION) && SDL_VERSION_ATLEAST(2, 0, 14)
#define NS_HAVE_SDL_OPENURL 1
#endif

#include "emu/EmuRunner.h"
#include "ui/Scenes.h"
#include "ui/Widgets.h"
#include "util/Fs.h"

namespace ns {
namespace {

// 打开浏览器（对齐 Android Desktop.browse；SDL 2.0.14+ 的 SDL_OpenURL）
void openUrl(const std::string& url) {
#if defined(NS_HAVE_SDL_OPENURL)
  SDL_OpenURL(url.c_str());
#else
  // ★v1.3.3：兜底统一走 fsutil::openUrl（Windows 无窗口执行，
  //   旧版 std::system("start …") 会闪 cmd 黑窗）
  fsutil::openUrl(url);
#endif
}

// ==================== 设置场景 ====================
class SettingsScene : public Scene {
 public:
  explicit SettingsScene(int initialTab = -1) {
    if (initialTab >= 0 && initialTab < kCats) {
      catSel_ = initialTab;
      focusArea_ = 1;
    }
  }

  static constexpr int kCats = 8;  // 外观 视频 显示 性能 输入 存储 核心设置 关于

  void onEnter(App& app) override { (void)app; }

  void saveConfig(App& app) {
    app.config.save(fsutil::join(app.dataDir(), "config.json"));
  }

  void update(App& app, float dt, const InputState& in) override {
    backdropMs_ += (int64_t)(dt * 1000);  // ★v1.2.9 FSD 像素天空动画
    if (in.pressed[(int)Btn::Back]) {
      app.popScene();
      return;
    }
    // 内容区滚动：滚轮 + ★拖拽（按住上下拖动）+ 键盘上下
    {
      const float viewTop = kContentTop, viewH = (float)app.draw.H - kContentTop - 96;
      const float maxScroll = contentH_ > viewH ? contentH_ - viewH : 0;
      if (maxScroll > 0) {
        const Rect viewRc(cx_, viewTop, (float)app.draw.W - cx_ - 16, viewH);
        scroll_ += w::dragScroll(app, viewRc);
        if (in.wheel != 0 && viewRc.contains(in.mouse)) scroll_ -= (float)in.wheel * 56.0f;
        if (focusArea_ == 1) {
          if (in.pressed[(int)Btn::Up]) scroll_ -= 48;
          if (in.pressed[(int)Btn::Down]) scroll_ += 48;
        }
      }
      if (scroll_ < 0) scroll_ = 0;
      if (scroll_ > maxScroll) scroll_ = maxScroll;
    }
    // 左侧分类导航（仅 Neon 皮肤；FSD 单页无侧栏）
    if (!Theme::isNeon()) {
      focusArea_ = 1;  // FSD：焦点直接在内容行
    } else if (focusArea_ == 0) {
      if (in.pressed[(int)Btn::Down]) catSel_ = (catSel_ + 1) % kCats;
      if (in.pressed[(int)Btn::Up]) catSel_ = (catSel_ + kCats - 1) % kCats;
      if (in.pressed[(int)Btn::Right] || in.pressed[(int)Btn::Ok]) focusArea_ = 1;
    } else {
      // 内容行焦点（行数由上一帧 draw 枚举）
      if (in.pressed[(int)Btn::Up]) {
        rowSel_ = (rowSel_ + rowCnt_ - 1) % rowCnt_;
        rowScrollFollow_ = true;
      }
      if (in.pressed[(int)Btn::Down]) {
        rowSel_ = (rowSel_ + 1) % rowCnt_;
        rowScrollFollow_ = true;
      }
      if (in.pressed[(int)Btn::Left]) focusArea_ = 0;
    }
    // 切换分类时回到顶部
    if (lastCat_ != catSel_) {
      lastCat_ = catSel_;
      scroll_ = 0;
      rowSel_ = 0;
    }
  }

  // 行索引分配（draw 内逐行调用；focused = 当前行焦点）
  bool rowFocus() {
    const int idx = rowCursor_++;
    rowCnt_ = std::max(rowCnt_, idx + 1);
    return focusArea_ == 1 && rowSel_ == idx;
  }

  void draw(App& app) override {
    // ★v1.2.9 FSD 经典模式（对齐 APK SettingsScreen）：PixelBackdrop 像素天空 +
    //   白 65% 半透明卡 + 深色文字（Theme::fsdLight 浅色皮肤覆盖）；
    //   Neon 模式不变（NeonBackdrop 深空渐变）。draw 结束恢复深色皮肤，
    //   防止 toast / 后续场景误用浅色令牌。
    const bool light = !Theme::isNeon();
    if (light) Theme::setLightFsd(true);
    drawInner(app);
    if (light) Theme::setLightFsd(false);
  }

  void drawInner(App& app) {
    const Skin& sk = Theme::skin();
    Draw& d = app.draw;
    // 背景：FSD → PixelBackdrop 像素天空（APK SettingsScreen 同款）；
    //       Neon → NeonBackdrop（深空三段渐变 + 顶/底光带）
    drawBackdrop(app);

    const char* catNames[kCats] = {"外观", "视频", "显示", "性能", "输入",
                                   "存储", "核心设置", "关于"};
    // 分类彩色图标（对齐 NeonSettingsScreen cats 配色）
    const Color8 catColors[kCats] = {sk.accent, sk.blue,  sk.green, sk.gold,
                                     sk.btnX,   sk.purple, sk.red,  sk.text};
    const int catIcons[kCats] = {16, 15, 3, 21, 9, 4, 16, 17};  // 性能用百分比图标

    // —— 顶部（标题 + 返回）——
    if (w::header(app, "设置", "Settings", true)) {
      app.popScene();
      return;
    }

    // —— 双皮肤布局（对齐 APK）：
    //   Neon 3D → NeonSettingsScreen：左侧 220dp 彩色图标分类导航 + 右侧内容
    //   FSD 经典 → SettingsScreen：无侧栏，单页分节滚动（SettingsSection）
    rowCursor_ = 0;
    rowCnt_ = 0;
    if (Theme::isNeon()) {
      cx_ = 248;
      // —— 左侧分类导航（220px，BgPanel 60% 底）——
      {
        const float navW = 220;
        d.rect(Rect(0, 96, navW, (float)d.H - 96),
               Color8(sk.bgPanel.r, sk.bgPanel.g, sk.bgPanel.b, 150));
        float y = 108;
        for (int i = 0; i < kCats; i++) {
          const Rect rc(12, y, navW - 24, 40);
          const bool selected = catSel_ == i;
          const bool hov = rc.contains(app.input.input().mouse);
          const bool hot = selected || hov;
          if (hot) d.chamferRect(rc, 12, sk.bgPanelHi);
          w::icon(app, catIcons[i], Rect(rc.x + 12, rc.cy() - 8, 16, 16),
                  selected ? catColors[i] : sk.textDim);
          app.font.drawVCentered(app.sdlRenderer(), catNames[i], rc.x + 38, rc.cy(), 14,
                                 selected ? sk.textHi : sk.text, selected);
          if (selected) d.rect(Rect(rc.x, rc.y + 8, 3, rc.h - 16), catColors[i]);
          if ((hov && app.input.input().mouseReleased && !w::dragGestureActive()) ||
              (focusArea_ == 0 && catSel_ == i &&
               app.input.input().pressed[(int)Btn::Ok])) {
            catSel_ = i;
            focusArea_ = 1;
          }
          y += 44;
        }
      }

      // —— 右侧内容（当前分类，可滚动）——
      const float viewTop = kContentTop, viewH = (float)d.H - kContentTop - 96;
      const Rect viewRc(cx_, viewTop, (float)d.W - cx_ - 16, viewH);
      d.pushClip(viewRc);
      float cy = viewTop + 6 - scroll_;
      switch (catSel_) {
        case 0: drawAppearance(app, cy); break;
        case 1: drawVideo(app, cy); break;
        case 2: drawDisplay(app, cy); break;
        case 3: drawPerformance(app, cy); break;
        case 4: drawInput(app, cy); break;
        case 5: drawStorage(app, cy); break;
        case 6: drawCores(app, cy); break;
        case 7: drawAbout(app, cy); break;
      }
      contentH_ = cy + scroll_ - viewTop;
      // 键盘焦点行跟随滚动可见
      if (rowScrollFollow_ && focusArea_ == 1 && rowCnt_ > 0) {
        const float rowY = viewTop + 6 + (float)rowSel_ * kRowH - scroll_;
        if (rowY < viewTop) scroll_ -= (viewTop - rowY);
        if (rowY > viewRc.bottom() - kRowH) scroll_ += (rowY - (viewRc.bottom() - kRowH));
        if (scroll_ < 0) scroll_ = 0;
        rowScrollFollow_ = false;
      }
      d.popClip();
      // 滚动条指示（内容超出时）
      if (contentH_ > viewH) {
        // ★滚动条可直接拖拽：返回新位置回写 scroll
        scroll_ = w::scrollIndicator(app, Rect((float)d.W - 14, viewTop, 6, viewH),
                                     viewH / contentH_,
                                     scroll_ / (contentH_ - viewH)) *
                  (contentH_ - viewH);
      }
    } else {
      // —— FSD 经典：单页分节滚动（对齐 SettingsScreen.kt SettingsSection）——
      cx_ = 44;
      const float viewTop = kContentTop, viewH = (float)d.H - kContentTop - 96;
      const Rect viewRc(cx_, viewTop, (float)d.W - cx_ - 20, viewH);
      d.pushClip(viewRc);
      float cy = viewTop + 6 - scroll_;
      drawAppearance(app, cy);
      cy += 12;
      drawVideo(app, cy);
      cy += 12;
      drawDisplay(app, cy);
      cy += 12;
      drawPerformance(app, cy);
      cy += 12;
      drawInput(app, cy);
      cy += 12;
      drawStorage(app, cy);
      cy += 12;
      drawCores(app, cy);
      cy += 12;
      drawAbout(app, cy);
      contentH_ = cy + scroll_ - viewTop;
      if (rowScrollFollow_ && focusArea_ == 1 && rowCnt_ > 0) {
        const float rowY = viewTop + 6 + (float)rowSel_ * kRowH - scroll_;
        if (rowY < viewTop) scroll_ -= (viewTop - rowY);
        if (rowY > viewRc.bottom() - kRowH) scroll_ += (rowY - (viewRc.bottom() - kRowH));
        if (scroll_ < 0) scroll_ = 0;
        rowScrollFollow_ = false;
      }
      d.popClip();
      if (contentH_ > viewH) {
        // ★滚动条可直接拖拽：返回新位置回写 scroll
        scroll_ = w::scrollIndicator(app, Rect((float)d.W - 14, viewTop, 6, viewH),
                                     viewH / contentH_,
                                     scroll_ / (contentH_ - viewH)) *
                  (contentH_ - viewH);
      }
    }

    w::keyHintBar(app, {{"↑↓", "选择"}, {"A", "修改"}, {"←→", "切换值"},
                        {"B / Esc", "返回"}});
  }

  // —— 背景（★v1.2.9 对齐 APK：FSD=PixelBackdrop 像素天空 / Neon=NeonBackdrop）——
  void drawBackdrop(App& app) {
    if (!Theme::isNeon()) {
      // APK SettingsScreen.kt line252：if (!AppBackgroundState.active) PixelBackdrop()
      w::pixelSky(app, backdropMs_);
      return;
    }
    const Skin& sk = Theme::skin();
    Draw& d = app.draw;
    for (int i = 0; i < 40; i++) {
      const float t = (float)i / 40;
      d.rect(Rect(0, d.H * t, d.W, d.H / 40 + 1), sk.bg.lerp(sk.bgDeep, t));
    }
    for (int i = 0; i < 12; i++) {
      const float a = 26.0f * (1.0f - (float)i / 12);
      d.rect(Rect(0, i * 4, d.W, 5),
             Color8(sk.accent.r, sk.accent.g, sk.accent.b, (uint8_t)a));
    }
    for (int i = 0; i < 12; i++) {
      const float a = 30.0f * (1.0f - (float)i / 12);
      d.rect(Rect(0, d.H - i * 4 - 5, d.W, 5),
             Color8(sk.accentDim.r, sk.accentDim.g, sk.accentDim.b, (uint8_t)a));
    }
  }

  // 小节：标题 + 面板（返回面板内容底部 y）
  float beginBlock(App& app, float y, const char* title, float panelH) {
    const float x = cx_;
    w::blockTitle(app, x, y, title);
    y += 24;
    w::blockPanel(app, Rect(x, y, (float)app.draw.W - cx_ - 26, panelH));
    return y + 12;  // 面板内首行 y
  }

  // ============ 0 外观 ============
  void drawAppearance(App& app, float& y) {
    const Skin& sk = Theme::skin();
    app.font.draw(app.sdlRenderer(), "外观", cx_, y, 24, sk.textHi, true);
    y += 42;
    // 主界面风格（两套主题切换，对齐 AppearancePanel StyleCard）
    {
      const float cw = ((float)app.draw.W - cx_ - 26 - 14) / 2;
      const float ph = 92;
      w::blockTitle(app, cx_, y, "主界面风格（两套主题即时切换）");
      y += 24;
      w::blockPanel(app, Rect(cx_, y, (float)app.draw.W - cx_ - 26, ph));
      const bool f = rowFocus();
      if (w::styleCard(app, Rect(cx_ + 12, y + 12, cw - 12, ph - 24),
                       "Neon 3D 新UI", "深空底 + 电光青 + 3D 封面流（默认）",
                       Theme::isNeon(), sk.accent)) {
        setStyle(app, 0);
      }
      if (w::styleCard(app, Rect(cx_ + cw + 14, y + 12, cw - 12, ph - 24),
                       "FSD 经典桌面", "Xbox 360 磁贴流 + 深蓝壁纸（v1.2.9 重制）",
                       !Theme::isNeon(), Color8::hex(0x3E64AF))) {
        setStyle(app, 1);
      }
      (void)f;
      y += ph + 16;
    }
    // 界面
    {
      const float ph = 38 * 2 + 12;
      y = beginBlock(app, y, "界面", ph);
      const Rect rc(cx_ + 12, y, (float)app.draw.W - cx_ - 50, 38);
      const bool f = rowFocus();
      w::blockRow(app, rc, "全屏模式", "快捷键 F11", f);  // 行体（点击由开关处理）
      if (w::switchNeon(app, rc, app.config.fullscreen, f)) {
        app.applyFullscreen();
        saveConfig(app);
      }
      y += 38;
      // ★v1.2.8 整体界面缩放（字体/标题/菜单/卡片统一放大，高 DPI 不发糊）
      //   自动 = 密度缩放：720p 133% / 1080p 200% / 4K 400%（对齐 APK
      //   在 Android TV 1080p 的 xhdpi 视觉密度）
      {
        const Rect rc2(cx_ + 12, y, (float)app.draw.W - cx_ - 50, 38);
        std::vector<std::string> scales = {"自动", "100%", "125%", "150%", "175%",
                                           "200%", "250%", "300%"};
        const int vals[] = {0, 100, 125, 150, 175, 200, 250, 300};
        int sel = 0;
        for (int i = 0; i < 8; i++)
          if (app.config.uiScalePercent == vals[i]) {
            sel = i;
            break;
          }
        const bool f2 = rowFocus();
        w::blockRow(app, rc2, "界面缩放",
                    "字体/标题/菜单/卡片整体放大；自动 = 720p 133%、1080p 200%、4K 400%", f2);
        if (w::spinnerChip(app, rc2, scales, sel, f2)) {
          app.config.uiScalePercent = vals[sel];
          app.applyUiScale();
          saveConfig(app);
        }
        y += 38 + 12 + 14;
      }
    }
    // ★v1.2.6 主页背景（对齐 Android AppSettings.homeBackgroundUri：
    //   自定义背景图两套风格通用；空 = 默认背景）
    {
      const bool hasBg = !app.config.homeBackground.empty();
      const float ph = hasBg ? 38 * 2 + 12 : 62;
      y = beginBlock(app, y, "主页背景", ph);
      const float rw = (float)app.draw.W - cx_ - 50;
      {
        const Rect rc(cx_ + 12, y, rw, 38);
        const bool f = rowFocus();
        w::blockRow(app, rc, "自定义主页背景…",
                    "选择 png / jpg / webp 图片（Neon 与 FSD 两套风格通用）", f);
        if (f && (app.input.input().pressed[(int)Btn::Ok] ||
                  (rc.contains(app.input.input().mouse) &&
                   app.input.input().mousePressed))) {
          ns::BrowserRequest req;
          req.title = "选择主页背景图片";
          req.mode = 3;  // 图片选择
          req.startDir = fsutil::homeDir();
          req.onPick = [&app](const std::string& picked) {
            // 复制到数据目录持久化（原文件被删/移动后背景仍可用）
            std::string data;
            if (!fsutil::readFile(picked, &data) || data.empty()) {
              app.pushScene(createMessageScene("无法设置背景", "图片读取失败"));
              return;
            }
            const std::string ext = fsutil::extOf(picked);
            fsutil::makeDirs(app.dataDir());
            const std::string dest = fsutil::join(
                app.dataDir(), "home_background." + (ext == "jpeg" ? "jpg" : ext));
            if (!fsutil::writeFile(dest, data)) {
              app.pushScene(createMessageScene("无法设置背景", "背景保存失败"));
              return;
            }
            app.config.homeBackground = dest;
            app.config.save(fsutil::join(app.dataDir(), "config.json"));
            app.invalidateTexture(dest);
            app.toast("已设置自定义主页背景");
          };
          browsePath(app, req);
        }
        y += 38;
      }
      if (hasBg) {
        const Rect rc(cx_ + 12, y, rw, 38);
        const bool f = rowFocus();
        w::blockRow(app, rc, "恢复默认背景",
                    "当前：" + app.config.homeBackground, f);
        if (f && (app.input.input().pressed[(int)Btn::Ok] ||
                  (rc.contains(app.input.input().mouse) &&
                   app.input.input().mousePressed))) {
          app.config.homeBackground.clear();
          saveConfig(app);
          app.toast("已恢复默认背景");
        }
        y += 38 + 12 + 14;
      } else {
        y += 12 + 14;
      }
    }
  }

  void setStyle(App& app, int idx) {
    Theme::setStyle(idx == 0 ? UiStyle::Neon : UiStyle::Fsd);
    app.config.uiStyle = idx;
    saveConfig(app);
    app.toast(idx == 0 ? "已切换到：Neon 3D 游戏站" : "已切换到：FSD 经典桌面");
  }

  // ============ 1 视频 ============
  // ★v1.2.8：对齐 APK SettingsScreen「视频」区 —— 画面缩放 6 档 + 视频滤镜
  //   11 档（文案与枚举值 1:1，SettingsScreen.kt:280-297）；设置页写入全局键
  //   （游戏内菜单写平台键，APK pk() 语义）。
  void drawVideo(App& app, float& y) {
    const Skin& sk = Theme::skin();
    app.font.draw(app.sdlRenderer(), "视频", cx_, y, 24, sk.textHi, true);
    y += 42;
    const float rw = (float)app.draw.W - cx_ - 50;
    // —— APK「画面缩放」（videoScale：stretch/4:3/3:2/8:7/16:9/custom）——
    {
      static const std::vector<std::string> opts = {
          "全屏拉伸(默认)", "4:3", "3:2 (GBA 原生)", "8:7 (NES 像素比)", "16:9",
          "自定义(拖动四角)"};
      static const std::vector<std::string> keys = {"stretch", "4:3", "3:2", "8:7",
                                                    "16:9", "custom"};
      const float ph = 38 + 12 + 14;
      y = beginBlock(app, y, "游戏画面", ph);
      int idx = 0;
      for (size_t i = 0; i < keys.size(); i++)
        if (keys[i] == app.config.videoScale) idx = (int)i;
      const bool f = rowFocus();
      w::blockRow(app, Rect(cx_ + 12, y, rw, 38), "画面缩放",
                  "全屏拉伸 / 固定比例 / 自定义（游戏内拖四角）", f);
      if (w::spinnerChip(app, Rect(cx_ + 12, y, rw, 38), opts, idx, f)) {
        app.config.videoScale = keys[idx];
        if (keys[idx] == "custom")
          app.toast("已选自定义：进入游戏 → 菜单 → 画面设置拖动四角");
        saveConfig(app);
      }
      y += 38 + 12 + 14;
    }
    // —— APK「视频滤镜」（videoFilter：none…hq4x_dot 11 档）——
    {
      static const std::vector<std::string> opts = {
          "关闭",       "扫描线",   "CRT",     "点阵",      "XBR",
          "HQ2X",     "HQ4X",     "XBR+点阵", "4XBR",      "4XBR+点阵",
          "HQ4X+点阵"};
      static const std::vector<std::string> keys = {
          "none",    "scanline", "crt",     "dot",     "xbr",
          "hq2x",    "hq4x",     "xbr_dot", "4xbr",    "4xbr_dot",
          "hq4x_dot"};
      const float ph = 38 + 12 + 14;
      y = beginBlock(app, y, "视频滤镜", ph);
      int idx = 0;
      for (size_t i = 0; i < keys.size(); i++)
        if (keys[i] == app.config.videoFilter) idx = (int)i;
      const bool f = rowFocus();
      w::blockRow(app, Rect(cx_ + 12, y, rw, 38), "视频滤镜",
                  "叠加类（扫描线/CRT/点阵）+ 放大类（XBR/HQ 系，对齐 APK）", f);
      if (w::spinnerChip(app, Rect(cx_ + 12, y, rw, 38), opts, idx, f)) {
        app.config.videoFilter = keys[idx];
        saveConfig(app);
      }
      y += 38 + 12 + 14;
    }
    {
      const float ph = 38 * 3 + 12;
      y = beginBlock(app, y, "画面选项", ph);
      {
        const Rect rc(cx_ + 12, y, rw, 38);
        std::vector<std::string> dual = {"双屏", "仅上屏", "仅下屏"};
        const bool f = rowFocus();
        w::blockRow(app, rc, "双屏布局（NDS / 3DS）", "上下双屏或单屏显示", f);
        if (w::spinnerChip(app, rc, dual, app.config.dualLayout, f)) saveConfig(app);
        y += 38;
      }
      {
        const Rect rc(cx_ + 12, y, rw, 38);
        const bool f = rowFocus();
        w::blockRow(app, rc, "平滑缩放", "使用线性采样（像素风游戏建议关闭）", f);
        if (w::switchNeon(app, rc, app.config.videoSmooth, f)) saveConfig(app);
        y += 38;
      }
      {
        const Rect rc(cx_ + 12, y, rw, 38);
        const bool f = rowFocus();
        w::blockRow(app, rc, "整数缩放", "按源分辨率取整倍数（像素完美，优先于画面比例）", f);
        if (w::switchNeon(app, rc, app.config.integerScale, f)) saveConfig(app);
        y += 38 + 12 + 14;
      }
    }
  }

  // ============ 2 显示 ============
  // ★v1.2.8：APK「显示」区只放横竖屏（Android 专属）——PC 对应保留
  //   全屏相关 + 快捷键说明；「显示帧数」按 APK 归属移入「性能」区。
  void drawDisplay(App& app, float& y) {
    const Skin& sk = Theme::skin();
    app.font.draw(app.sdlRenderer(), "显示", cx_, y, 24, sk.textHi, true);
    y += 42;
    // ★v1.2.6 启动游戏时自动全屏（对齐 Android AppSettings.fullscreenOnLaunch；
    //   退出游戏自动恢复原窗口状态；游戏中 F11 仍可手动切换）
    {
      const float ph = 62;
      y = beginBlock(app, y, "启动游戏时全屏", ph);
      const Rect rc(cx_ + 12, y, (float)app.draw.W - cx_ - 50, 38);
      const bool f = rowFocus();
      w::blockRow(app, rc, "进入游戏自动全屏", "退出游戏后恢复原窗口大小", f);
      if (w::switchNeon(app, rc, app.config.fullscreenOnLaunch, f)) saveConfig(app);
      y += 38 + 12 + 14;
    }
    {
      // 只读快捷键（对齐 DisplayPanel「游戏内菜单」）
      const float ph = 38 * 5 + 12;
      y = beginBlock(app, y, "游戏内菜单与快捷键", ph);
      const float rw = (float)app.draw.W - cx_ - 50;
      const char* rows[][2] = {
          {"菜单呼出键", "Esc / F1 / 右键"},
          {"快进", "Tab 按住 · F6 换挡（0→2→4→6→8→16）"},
          {"快存 / 快读", "F5 / F8（菜单内共 12 槽）"},
          {"暂停", "F7"},
          {"截图", "F12（存至数据目录 shots/）"},
      };
      for (auto& r : rows) {
        const Rect rc(cx_ + 12, y, rw, 38);
        w::blockRow(app, rc, r[0], "", rowFocus());
        app.font.drawVCentered(app.sdlRenderer(), r[1], rc.right() - 320, rc.cy(), 13,
                               sk.accent);
        y += 38;
      }
      y += 12 + 14;
    }
  }

  // ============ 3 性能 ============
  // ★v1.2.8：「显示帧数」按 APK 归属放在本区（SettingsScreen 性能区 =
  //   高质量缩放 + 显示帧数；高质量缩放为 Android 缓冲策略，PC 对应项
  //   为视频区的「平滑缩放」，此处不重复）。
  void drawPerformance(App& app, float& y) {
    const Skin& sk = Theme::skin();
    app.font.draw(app.sdlRenderer(), "性能", cx_, y, 24, sk.textHi, true);
    y += 42;
    {
      const float ph = 62;
      y = beginBlock(app, y, "显示帧数", ph);
      const Rect rc(cx_ + 12, y, (float)app.draw.W - cx_ - 50, 38);
      const bool f = rowFocus();
      w::blockRow(app, rc, "显示帧数", "游戏画面角落显示实时 FPS（APK 性能区同款）", f);
      if (w::switchNeon(app, rc, app.config.showFps, f)) saveConfig(app);
      y += 38 + 12 + 14;
    }
    {
      const float ph = 100;
      y = beginBlock(app, y, "快进", ph);
      const Rect rc(cx_ + 12, y, (float)app.draw.W - cx_ - 50, 38);
      static const int vals[] = {0, 2, 4, 6, 8, 16};
      std::vector<std::string> opts = {"关", "2×", "4×", "6×", "8×", "16×"};
      int idx = 2;
      for (int i = 0; i < 6; i++)
        if (vals[i] == app.config.fastForwardSpeed) {
          idx = i;
          break;
        }
      const bool f = rowFocus();
      w::blockRow(app, rc, "默认快进倍速", "游戏内按住 Tab 快进；F6 循环档位", f);
      if (w::spinnerChip(app, rc, opts, idx, f)) {
        app.config.fastForwardSpeed = vals[idx];
        saveConfig(app);
      }
      y += 38 + 12 + 14;
    }
    {
      const float ph = 62;
      y = beginBlock(app, y, "音频", ph);
      const Rect rc(cx_ + 12, y, (float)app.draw.W - cx_ - 50, 38);
      const bool f = rowFocus();
      w::blockRow(app, rc, "静音", "全局关闭模拟音频输出", f);
      if (w::switchNeon(app, rc, app.config.audioMuted, f)) {
        app.audio.setMuted(app.config.audioMuted);
        saveConfig(app);
      }
      y += 38 + 12 + 14;
    }
    // ★v1.2.6 音频缓冲（对齐 Android AppSettings.audioBufferMs：
    //   越大越稳（防断音）、延迟越高；下次进入游戏生效）
    {
      const float ph = 38 * 2 + 12;
      y = beginBlock(app, y, "音频缓冲", ph);
      const Rect rc(cx_ + 12, y, (float)app.draw.W - cx_ - 50, 38);
      static const int vals[] = {32, 64, 128, 256};
      std::vector<std::string> opts = {"32ms（低延迟）", "64ms（默认）",
                                       "128ms（稳定）", "256ms（最稳）"};
      int idx = 1;
      for (int i = 0; i < 4; i++)
        if (vals[i] == app.config.audioBufferMs) {
          idx = i;
          break;
        }
      const bool f = rowFocus();
      w::blockRow(app, rc, "音频缓冲大小",
                  "断音/爆音时调大；越小延迟越低（下次启动游戏生效）", f);
      if (w::spinnerChip(app, rc, opts, idx, f)) {
        app.config.audioBufferMs = vals[idx];
        saveConfig(app);
      }
      y += 38 + 12 + 14;
    }
    // ★v1.3.5 软件渲染（PC 排障项）：部分显卡驱动/Wine 转译层会打乱纹理
    //   通道序（游戏画面偏色/发紫）。启动时的色彩探针异常会自动切软件；
    //   此开关供手动指定（重启应用生效）。
    {
      const float ph = 62;
      y = beginBlock(app, y, "软件渲染", ph);
      const Rect rc(cx_ + 12, y, (float)app.draw.W - cx_ - 50, 38);
      const bool f = rowFocus();
      w::blockRow(app, rc, "软件渲染（排障）",
                  "画面偏色/发紫时开启；不用显卡、CPU 渲染（重启生效）", f);
      if (w::switchNeon(app, rc, app.config.renderSoftware, f)) saveConfig(app);
      y += 38 + 12 + 14;
    }
  }

  // ============ 4 输入 ============
  void drawInput(App& app, float& y) {
    const Skin& sk = Theme::skin();
    app.font.draw(app.sdlRenderer(), "输入", cx_, y, 24, sk.textHi, true);
    y += 42;
    const float rw = (float)app.draw.W - cx_ - 50;
    // 键位映射（两个端口）
    for (int port = 0; port < 2; port++) {
      char portTitle[48];
      snprintf(portTitle, sizeof(portTitle), "键盘 / 手柄 · 端口 %d（%s）", port + 1,
               app.input.gamepadName(port).c_str());
      const int rows = (RP_COUNT + 3) / 4;
      const float ph = 26 + rows * 46 + 12;
      y = beginBlock(app, y, portTitle, ph);
      app.font.draw(app.sdlRenderer(),
                    "点击键位后按新键 / 手柄键绑定（Esc 取消）", cx_ + 12, y - 4, 12,
                    sk.textDim);
      y += 12;
      const float cw = (rw - 8) / 4;
      for (int b = 0; b < RP_COUNT; b++) {
        const float bx = cx_ + 12 + (b % 4) * (cw + 2);
        const float by = y + (b / 4) * 46;
        const Rect rc(bx, by, cw - 4, 42);
        const KeyBind& kb = app.input.mapping().binds[port][b];
        std::string key;
        if (app.input.isCapturing() && app.input.capturePort() == port &&
            app.input.captureBtn() == b) {
          key = "按新键…";
        } else if (kb.key != SDLK_UNKNOWN) {
          key = sdlKeyName(kb.key);
        } else if (kb.padBtn != SDL_CONTROLLER_BUTTON_INVALID) {
          key = sdlPadButtonName(kb.padBtn);
        } else {
          key = "未绑定";
        }
        if (w::listRow(app, rc, retroPadName(b), key, rowFocus())) {
          app.input.captureBind(port, b);
          app.toast("按下要绑定的新键（Esc 取消）");
        }
      }
      y += rows * 46 + 12 + 14;
    }
    {
      const float ph = 62;
      y = beginBlock(app, y, "默认键位说明", ph);
      const Rect rc(cx_ + 12, y, rw, 38);
      w::blockRow(app, rc, "恢复默认映射",
                  "方向键=十字键 · Z=B X=A A=Y S=X · Q=L W=R · Enter=Start Shift=Select",
                  rowFocus());
      const Rect btn(rc.right() - 150, rc.y + 5, 140, 28);
      if (w::button(app, btn, "恢复默认", false, 13, false)) {
        app.input.resetMapping();
        app.config.padMapping = app.input.mapping();
        saveConfig(app);
        app.toast("已恢复默认按键映射");
      }
      y += 38 + 12 + 14;
    }
  }

  // ============ 5 存储 ============
  // ★v1.2.8：补全 APK「存档方式」（globalSaveMode：统一存档目录 /
  //   ROM 同目录同名 .srm/.sav，SettingsScreen.kt:411-425）；切换后需重进游戏。
  void drawStorage(App& app, float& y) {
    const Skin& sk = Theme::skin();
    Draw& d = app.draw;
    app.font.draw(app.sdlRenderer(), "存储", cx_, y, 24, sk.textHi, true);
    y += 42;
    const float rw = (float)app.draw.W - cx_ - 50;
    {
      const float ph = 38 + 12 + 14 + 34;
      y = beginBlock(app, y, "存档方式", ph);
      static const std::vector<std::string> opts = {"统一存档目录 (推荐)",
                                                    "ROM 同目录同名 (.srm/.sav)"};
      static const std::vector<std::string> keys = {"nesstation", "core_builtin"};
      int idx = app.config.saveMode == "core_builtin" ? 1 : 0;
      const bool f = rowFocus();
      w::blockRow(app, Rect(cx_ + 12, y, rw, 38), "存档方式",
                  "所有核心通用的 SRAM/电可擦写存档落盘位置（切换后需重进游戏）", f);
      if (w::spinnerChip(app, Rect(cx_ + 12, y, rw, 38), opts, idx, f)) {
        app.config.saveMode = keys[idx];
        saveConfig(app);
      }
      y += 38 + 10;
      app.font.draw(app.sdlRenderer(),
                    "「统一存档目录」把存档集中在应用数据目录 saves/；「ROM 同目录」"
                    "直接读写 ROM 旁的同名存档（与官方模拟器/RetroArch 习惯一致，便于交换存档）。",
                    cx_ + 12, y, 11, sk.textDim);
      y += 12 + 4 + 14;
    }
    // ROM 扫描目录
    {
      const float ph = 62 + (float)std::max<size_t>(app.library.romDirs().size(), 1) * 46 + 46;
      y = beginBlock(app, y, "ROM 扫描目录", ph);
      if (app.library.romDirs().empty()) {
        app.font.draw(app.sdlRenderer(), "（还没有 ROM 目录 —— 点击下方添加）",
                      cx_ + 12, y + 8, 13, sk.textDim);
        y += 46;
      }
      for (const auto& dir : app.library.romDirs()) {
        const Rect rc(cx_ + 12, y, rw, 42);
        const bool f = rowFocus();
        w::blockRow(app, rc, fsutil::pathDisplay(dir), "", f);
        // 右侧：重新扫描 + 移除
        const Rect rm(rc.right() - 84, rc.y + 7, 76, 28);
        if (w::button(app, rm, "✗ 移除", false, 12, false)) {
          app.library.removeRomDir(dir);
          app.library.save();
        }
        const Rect scan(rc.right() - 178, rc.y + 7, 88, 28);
        if (w::button(app, scan, "↻ 重新扫描", false, 12, false)) {
          // ★v1.2.3：同步 → 后台（大目录同步扫卡界面；完成时主循环自动 toast）
          if (app.library.scanRunning()) {
            app.toast("正在扫描中，请稍候…");
          } else {
            app.library.startBackgroundRescan(app.config.scanRecursive);
            app.toast("正在后台重新扫描…");
          }
        }
        y += 46;
      }
      {
        const Rect rc(cx_ + 12, y, rw, 42);
        const bool f = rowFocus();
        if (w::blockRow(app, rc, "添加 ROM 目录", "启动时自动扫描这些目录（含子目录）",
                        f)) {
          BrowserRequest req;
          req.title = "选择 ROM 目录";
          req.mode = 0;
          req.startDir = app.library.romDirs().empty()
                             ? fsutil::homeDir()
                             : app.library.romDirs().back();
          req.onPick = [&app](const std::string& dir) {
            // ★v1.2.3：后台扫描（同步扫大目录卡界面）+ 非 UTF-8 目录名拦截
            if (!app.library.addRomDir(dir)) {
              app.toast("该目录已在扫描列表中（或目录名含不支持的字符）");
              return;
            }
            app.library.save();
            if (app.library.scanRunning()) {
              app.toast("已添加目录，当前扫描完成后自动纳入下次扫描");
            } else {
              app.library.startBackgroundRescan(app.config.scanRecursive);
              app.toast("已添加目录，正在后台扫描…");
            }
          };
          browsePath(app, req);
        }
        const Rect btn(rc.right() - 100, rc.y + 7, 92, 28);
        if (w::button(app, btn, "＋ 添加", false, 13, true)) {
          BrowserRequest req;
          req.title = "选择 ROM 目录";
          req.mode = 0;
          req.startDir = app.library.romDirs().empty()
                             ? fsutil::homeDir()
                             : app.library.romDirs().back();
          req.onPick = [&app](const std::string& dir) {
            if (!app.library.addRomDir(dir)) {
              app.toast("该目录已在扫描列表中（或目录名含不支持的字符）");
              return;
            }
            app.library.save();
            if (app.library.scanRunning()) {
              app.toast("已添加目录，当前扫描完成后自动纳入下次扫描");
            } else {
              app.library.startBackgroundRescan(app.config.scanRecursive);
              app.toast("已添加目录，正在后台扫描…");
            }
          };
          browsePath(app, req);
        }
        y += 46;
      }
      // 递归开关
      {
        const Rect rc(cx_ + 12, y, rw, 38);
        const bool f = rowFocus();
        w::blockRow(app, rc, "扫描子目录（递归）", "同时扫描 ROM 目录下的子文件夹", f);
        if (w::switchNeon(app, rc, app.config.scanRecursive, f)) saveConfig(app);
        y += 38 + 12 + 14;
      }
    }
    // 数据目录
    {
      const float ph = 38 * 3 + 12 + 20;
      y = beginBlock(app, y, "数据目录", ph);
      const char* rows[3][2] = {
          {"存档 / 快速存档 / 核心选项 / 封面缓存", ""},
          {"日志", ""},
          {"便携模式", "在程序目录放置 portable.flag 即切换为绿色便携模式"},
      };
      const Rect rc0(cx_ + 12, y, rw, 38);
      w::blockRow(app, rc0, rows[0][0],
                  "位置：" + app.dataDir(), rowFocus());
      y += 38;
      const Rect rc1(cx_ + 12, y, rw, 38);
      w::blockRow(app, rc1, rows[1][0], fsutil::join(app.dataDir(), "nesstation.log"),
                  rowFocus());
      y += 38;
      const Rect rc2(cx_ + 12, y, rw, 38);
      w::blockRow(app, rc2, rows[2][0], rows[2][1], rowFocus());
      y += 38 + 12 + 14;
    }
    // BIOS / 系统文件
    {
      static const struct {
        const char* name;
        const char* note;
      } biosList[] = {
          {"disksys.rom", "NES FDS 磁碟 BIOS（放 system/）"},
          {"neogeo.zip / pgm.zip", "FBNeo 街机 BIOS（放 system/fbneo/）"},
          {"bios_CD_E/J/U.zip", "Mega-CD BIOS（放 system/）"},
          {"syscard1/2/3.pce", "PCE-CD System Card（放 system/）"},
          {"dc_boot.bin / dc_flash.bin", "Dreamcast BIOS（放 system/dc/）"},
          {"psx BIOS（scph*.bin）", "PS1 可选 BIOS（放 system/pcsx_rearmed/）"},
          {"aes_keys.txt", "3DS 加密密钥（放 system/azahar/）"},
          {"prod.keys / 固件", "Switch（在 Eden 模拟器内配置）"},
      };
      const float ph = 26 + sizeof(biosList) / sizeof(biosList[0]) * 44 + 12;
      y = beginBlock(app, y, "BIOS / 系统文件（可选）", ph);
      app.font.draw(app.sdlRenderer(), "系统目录：" + app.cores.systemDir(),
                    cx_ + 12, y - 4, 12, sk.textDim);
      y += 12;
      for (const auto& b : biosList) {
        const Rect rc(cx_ + 12, y, rw, 40);
        w::blockRow(app, rc, b.name, b.note, rowFocus());
        y += 44;
      }
      y += 12 + 14;
      app.font.draw(app.sdlRenderer(),
                    "⚠ BIOS 文件含受版权保护的代码，请仅使用合法获取的文件，不得分发。",
                    cx_, y, 12, sk.red);
      y += 20;
      (void)d;
    }
  }

  // ============ 6 核心设置 ============
  void drawCores(App& app, float& y) {
    const Skin& sk = Theme::skin();
    Draw& d = app.draw;
    app.font.draw(app.sdlRenderer(), "核心设置", cx_, y, 24, sk.textHi, true);
    y += 42;
    const float rw = (float)app.draw.W - cx_ - 50;

    // 下载进度（后台线程）
    const auto dl = app.cores.coreDownloadProgress();
    float progH = 0;
    if (dl.running || dl.finished) progH = 34;
    {
      const float ph = 62 + progH;
      y = beginBlock(app, y, "核心管理（libretro buildbot 官方 PC 构建）", ph);
      if (dl.running) {
        const Rect bar(cx_ + 12, y, rw - 24, 8);
        d.rect(bar, sk.bgDeep);
        const float t = dl.total > 0 ? (float)dl.done / dl.total : 0;
        d.rect(Rect(bar.x, bar.y, std::max(8.0f, bar.w * t), bar.h), sk.accent);
        char buf[96];
        snprintf(buf, sizeof(buf), "正在下载核心 %d/%d：%s", dl.done + 1, dl.total,
                 dl.label.c_str());
        app.font.draw(app.sdlRenderer(), buf, cx_ + 12, y + 12, 12, sk.accent);
      } else if (dl.finished) {
        app.font.draw(app.sdlRenderer(),
                      dl.hadFailure ? "核心下载完成（部分平台本架构无预编译包）"
                                    : "核心下载完成 ✓",
                      cx_ + 12, y + 4, 12,
                      dl.hadFailure ? sk.textDim : sk.green);
      }
      const Rect rc(cx_ + 12, y + progH, rw, 38);
      const bool f = rowFocus();
      w::blockRow(app, rc, "一键下载全部缺失核心",
                  "进程内 libretro 核心；核心位于 " + app.cores.coreDir(), f);
      const bool downloading = app.cores.coreDownloadRunning();
      const Rect btn(rc.right() - 190, rc.y + 5, 180, 28);
      if (w::button(app, btn, downloading ? "下载中…" : "⇩ 一键下载", false, 13, true)) {
        if (downloading) {
          app.toast("正在下载，请稍候");
        } else {
          app.cores.clearDownloadFinishFlag();
          app.cores.startCoreDownload({});
          app.toast("开始下载全部缺失核心");
        }
      }
      y += 38 + 12 + 14;
    }

    // 进程内核心（18 平台）
    {
      const float ph = 26 + (float)allPlatforms().size() * 46 + 12;
      y = beginBlock(app, y, "进程内核心（18 平台）", ph);
      y += 12;
      for (GamePlatform p : allPlatforms()) {
        const PlatformCore pc = platformCore(p);
        const Rect rc(cx_ + 12, y, rw, 42);
        std::string status;
        if (pc.kind == CoreKind::Libretro) {
          if (app.cores.libretroCoreAvailable(p)) {
            const PlatformCore inUse = app.cores.resolveCore(p);
            status = std::string(inUse.coreFile) == std::string(pc.coreFile)
                         ? "✓ 已安装"
                         : std::string("✓ 用替代 ") + inUse.coreTitle;
            if (app.externals.availableCount(p) > 0) status += " · 独立核心已就绪";
          } else if (app.externals.availableCount(p) > 0) {
            status = "✓ 独立核心已就绪";
          } else {
            status = app.cores.coreDownloadRunning() ? "… 下载中" : "✗ 缺失 · 点击下载";
          }
        } else {
          const bool ready = app.externals.availableCount(p) > 0;
          const auto exts = app.externals.forPlatform(p);
          if (ready) {
            status = "✓ 已就绪（独立核心）";
          } else if (!exts.empty() && !exts[0]->githubApi.empty()) {
            status = app.externals.downloadRunning() ? "… 下载中" : "✗ 未安装 · 点击查看";
          } else {
            status = "✗ 未安装 · 点击查看";
          }
        }
        std::string title = std::string(platformShortName(p)) + " · " + pc.coreTitle;
        if (pc.experimental) title += "（实验性）";
        const bool f = rowFocus();
        if (w::blockRow(app, rc, title, pc.biosNote, f)) {
          if (pc.kind == CoreKind::Libretro) {
            if (!app.cores.libretroCoreAvailable(p) &&
                app.externals.availableCount(p) == 0) {
              if (app.cores.coreDownloadRunning()) {
                app.toast("正在下载，请稍候");
              } else {
                app.cores.clearDownloadFinishFlag();
                app.cores.startCoreDownload({p});
                app.toast(std::string("开始下载 ") + platformShortName(p) + " 核心");
              }
            } else {
              app.pushScene(createCoreOptionsScene(p));
            }
          } else {
            // ★v1.3.2：外置核心平台一律进入核心设置面板（旧版已就绪时仅 toast
            //   一闪而过、未就绪时直接触发下载 —— 「核心设置进不去」的根因；
            //   下载/官网/手动选择收进面板内的独立按钮，语义清晰）
            app.pushScene(createCoreOptionsScene(p));
          }
        }
        // 状态色（UTF-8 前缀比较，勿用 status[0] 字节比较）
      const Color8 stCol = status.rfind("✓", 0) == 0
                               ? sk.green
                               : (status.rfind("…", 0) == 0 ? sk.accent : sk.red);
      app.font.drawVCentered(app.sdlRenderer(), status,
                               rc.right() - app.font.textWidth(status, 12) - 14, rc.cy(),
                               12, stCol);
        y += 46;
      }
      y += 12 + 14;
    }

    // ★v1.2.6 默认核心（对齐 Android defaultCores：多核心平台的启动默认，
    //   避免每次启动都弹核心选择器；单核心平台不列出）
    {
      struct DefCoreRow {
        GamePlatform platform;
        std::vector<std::string> labels;  // 0 = 每次询问
        std::vector<std::string> values;  // 0 = ""（询问）
      };
      std::vector<DefCoreRow> rows;
      for (GamePlatform p : allPlatforms()) {
        std::vector<std::string> labels, values;
        labels.push_back("每次询问");
        values.push_back("");
        for (const auto& pc : app.cores.installedCoresFor(p)) {
          labels.push_back(std::string(pc.coreTitle) + "（进程内）");
          values.push_back(pc.coreFile);
        }
        for (const auto* c : app.externals.forPlatform(p)) {
          if (!app.externals.available(c->id)) continue;
          labels.push_back(c->name + "（独立核心）");
          values.push_back(c->id + "-ext");
        }
        if (labels.size() > 2)  // 至少两个可选核心才需要默认
          rows.push_back({p, labels, values});
      }
      if (!rows.empty()) {
        const float ph = 26 + (float)rows.size() * 46 + 12;
        y = beginBlock(app, y, "默认核心（多核心平台启动时直接采用）", ph);
        y += 12;
        for (const auto& r : rows) {
          const Rect rc(cx_ + 12, y, rw, 42);
          const std::string key = platformShortName(r.platform);
          const std::string& cur = app.config.defaultCores.count(key)
                                       ? app.config.defaultCores[key]
                                       : std::string();
          int idx = 0;
          for (size_t i = 0; i < r.values.size(); i++)
            if (r.values[i] == cur) idx = (int)i;
          const bool f = rowFocus();
          w::blockRow(app, rc, std::string("默认 ") + key + " 核心",
                      "启动游戏时的默认核心（未记忆选择的游戏）", f);
          if (w::spinnerChip(app, rc, r.labels, idx, f)) {
            app.config.defaultCores[key] = r.values[(size_t)idx];
            saveConfig(app);
          }
          y += 46;
        }
        y += 12 + 14;
      }
    }

    // 外置核心（独立模拟器）
    {
      const auto pr = app.externals.progress();
      float progH = (pr.running || pr.finished) ? 26 : 0;
      const float ph = 26 + progH + (float)app.externals.all().size() * 96 + 12;
      y = beginBlock(app, y, "外置核心（独立模拟器 · 源码内置构建 / 自动检测 / 自动下载）",
                     ph);
      y += 12;
      if (pr.running || pr.finished) {
        app.font.draw(app.sdlRenderer(), pr.label, cx_ + 12, y, 12,
                      pr.ok ? sk.green : sk.gold);
        y += 22;
        if (pr.finished) {
          app.externals.clearFinishFlag();
        }
      }
      for (const auto& e : app.externals.all()) {
        const bool ready = !e.foundPath.empty();
        const Rect rc(cx_ + 12, y, rw, 88);
        const bool f = rowFocus();
        const bool hov = rc.contains(app.input.input().mouse);
        if (hov || f) {
          d.chamferRect(rc, 10, sk.bgPanelHi);
          d.chamferRectOutline(rc, 10, f ? sk.accent : sk.line, 1.2f);
        }
        app.font.draw(app.sdlRenderer(), e.name, rc.x + 14, rc.y + 8, 15, sk.textHi, true);
        {
          const std::string plat = platformShortName(e.platform);
          const float pw = app.font.textWidth(plat, 11) + 14;
          const Rect brc(rc.x + 14 + app.font.textWidth(e.name, 15) + 10, rc.y + 10, pw, 19);
          d.roundRect(brc, 4, Color8(sk.accent.r, sk.accent.g, sk.accent.b, 40));
          app.font.draw(app.sdlRenderer(), plat,
                        brc.x + (brc.w - app.font.textWidth(plat, 11)) / 2, brc.y + 3, 11,
                        sk.accent);
        }
        app.font.draw(app.sdlRenderer(), e.notes, rc.x + 14, rc.y + 30, 12, sk.textDim);
        if (ready) {
          app.font.draw(app.sdlRenderer(), "→ " + e.foundPath, rc.x + 14, rc.y + 48, 11,
                        sk.green);
        } else if (!e.userPath.empty()) {
          app.font.draw(app.sdlRenderer(), "⚠ 配置的路径无效：" + e.userPath, rc.x + 14,
                        rc.y + 48, 11, sk.red);
        }
        // 右侧按钮（对齐 ExternalCoreRow：自动下载 / 官网 / 手动选择 / 移除）
        const float bw = 92, bh = 28;
        float bx = rc.right() - bw - 12;
        if (ready) {
          const Rect rm(bx, rc.y + 30, bw, bh);
          if (w::button(app, rm, "移除", false, 12, false)) {
            std::string id = e.id;
            app.externals.clearUserPath(id);
            app.config.emulatorPaths.erase(id);
            app.config.save(fsutil::join(app.dataDir(), "config.json"));
            app.toast("已移除 " + e.name);
          }
        } else if (!e.githubApi.empty()) {
          const Rect dl(bx, rc.y + 30, bw, bh);
          if (w::button(app, dl,
                        app.externals.downloadRunning() ? "下载中…" : "自动下载", false,
                        12, true)) {
            if (app.externals.downloadRunning()) {
              app.toast("正在下载，请稍候");
            } else {
              app.externals.startDownload(e.id);
              app.toast("开始自动下载 " + e.name);
            }
          }
        } else {
          const Rect wp(bx, rc.y + 30, bw, bh);
          if (w::button(app, wp, "打开官网", false, 12, false)) {
            openUrl(e.downloadPage);
            app.toast("已打开 " + e.name + " 下载页");
          }
        }
        bx -= bw + 8;
        {
          const Rect pick(bx, rc.y + 30, bw, bh);
          if (w::button(app, pick, "手动选择", false, 12, false)) {
            BrowserRequest req;
            req.title = "选择 " + e.name + " 可执行文件（exe / AppImage / jar）";
            req.mode = 1;
            req.startDir = fsutil::exeDir();
            std::string id = e.id;
            req.onPick = [&app, id](const std::string& exe) {
              app.externals.setUserPath(id, exe);
              app.config.emulatorPaths[id] = exe;
              app.config.save(fsutil::join(app.dataDir(), "config.json"));
              app.toast("已配置 " + id);
            };
            browsePath(app, req);
          }
        }
        y += 96;
      }
      y += 12 + 14;
      app.font.draw(app.sdlRenderer(),
                    "★ 源码级集成：仓库 emulators/ 目录已内置 Dolphin / Cemu / Eden 全部源码，"
                    "运行 scripts/build_emulators.sh(.ps1) 本地构建后放入 emulators/<名称>/ 即自动识别，无需联网。",
                    cx_, y, 12, sk.textDim);
    }
  }

  // ============ 7 关于 ============
  void drawAbout(App& app, float& y) {
    const Skin& sk = Theme::skin();
    app.font.draw(app.sdlRenderer(), "关于", cx_, y, 24, sk.textHi, true);
    y += 42;
    {
      const float ph = 38 * 3 + 12 + 14;
      y = beginBlock(app, y, "NesStation for PC", ph);
      const float rw = (float)app.draw.W - cx_ - 50;
      {
        const Rect rc(cx_ + 12, y, rw, 38);
        w::blockRow(app, rc, "版本",
                    std::string(appVersionText()).append(" · 多平台复古游戏站（Windows / Linux）").c_str(),
                    rowFocus());
        const Rect btn(rc.right() - 100, rc.y + 5, 92, 28);
        if (w::button(app, btn, "详情", false, 13, false)) app.pushScene(createAboutScene());
        y += 38;
      }
      {
        const Rect rc(cx_ + 12, y, rw, 38);
        w::blockRow(app, rc, "源码级集成模拟器", "Dolphin（NGC/Wii）· Cemu（Wii U）· Eden（Switch）",
                    rowFocus());
        y += 38;
      }
      {
        const Rect rc(cx_ + 12, y, rw, 38);
        w::blockRow(app, rc, "许可证", "应用代码 MIT · 各核心遵循其上游许可证（assets/legal）",
                    rowFocus());
        y += 38 + 12 + 14;
      }
    }
  }

 private:
  static constexpr float kContentTop = 108;
  static constexpr float kRowH = 46;        // 焦点行跟随估算步长
  float cx_ = 248;                          // 内容区左缘（Neon=220 导航+28；FSD=44 单页）
  int64_t backdropMs_ = 0;                  // ★v1.2.9 FSD 像素天空动画时钟
  int catSel_ = 0;
  int focusArea_ = 0;      // 0 = 分类导航 1 = 内容行
  int lastCat_ = -1;
  int rowSel_ = 0;         // 内容行焦点索引
  int rowCursor_ = 0;      // 本帧行枚举游标
  int rowCnt_ = 0;         // 上一帧行数
  bool rowScrollFollow_ = false;
  float scroll_ = 0;
  float contentH_ = 0;
};

}  // namespace

std::unique_ptr<Scene> createSettingsScene(int initialTab) {
  return std::unique_ptr<Scene>(new SettingsScene(initialTab));
}

}  // namespace ns
