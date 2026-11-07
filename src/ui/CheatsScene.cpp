// NesStation PC — 金手指场景（★v1.3.1，游戏内菜单「金手指」入口）
// 对齐 APK CheatsPanel 的信息架构：文件列表 → 作弊码开关列表；
// 数据源 RetroArch .cht（放 <数据目录>/cheats/，或场景内「导入」复制进去）。
// 开关即时生效（写入集经 LibretroCore::setCheats 逐帧应用）。
#include <algorithm>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "app/App.h"
#include "emu/Cheats.h"
#include "emu/EmuRunner.h"
#include "ui/Scenes.h"
#include "ui/Widgets.h"
#include "util/Fs.h"
#include "util/Log.h"

namespace ns {
namespace {

class CheatsScene : public Scene {
 public:
  void onEnter(App& app) override {
    rescan(app);
    enabled_ = cheats::loadEnabled(app.dataDir());
    applyToCore(app);
  }

  void rescan(App& app) {
    files_ = cheats::scanFiles(app.dataDir());
    if (fileIdx_ >= (int)files_.size()) fileIdx_ = 0;
  }

  // 把启用集合刷进运行中的核心（跨文件汇总；未运行则仅存状态）
  void applyToCore(App& app) {
    EmuRunner* emu = app.activeEmu();
    if (emu && emu->running() && emu->isLibretro() && emu->core())
      emu->core()->setCheats(cheats::activeWrites(files_, enabled_));
  }

  void toggle(App& app, const cheats::File& f, size_t idx) {
    const std::string key = cheats::enabledKey(f, idx);
    auto it = enabled_.find(key);
    const bool now = !(it != enabled_.end() && it->second);
    enabled_[key] = now;
    cheats::saveEnabled(app.dataDir(), enabled_);
    applyToCore(app);
    app.toast(now ? "已开启：" + f.entries[idx].desc
                  : "已关闭：" + f.entries[idx].desc);
  }

  void pushImport(App* app) {
    BrowserRequest req;
    req.title = "导入 .cht 金手指文件";
    req.mode = 1;  // 选文件
    req.onPick = [app](const std::string& src) {
      const std::string dest =
          fsutil::join(cheats::dir(app->dataDir()), fsutil::baseOf(src));
      std::string data;
      if (!fsutil::readFile(src, &data)) {
        app->toast("读取文件失败");
        return;
      }
      if (!fsutil::writeFile(dest, data)) {
        app->toast("复制到金手指目录失败");
        return;
      }
      app->toast("已导入：" + fsutil::baseOf(src));
      if (CheatsScene* s = app->findScene<CheatsScene>()) {
        s->rescan(*app);
        s->enabled_ = cheats::loadEnabled(app->dataDir());
        s->applyToCore(*app);
      }
    };
    browsePath(*app, req);
  }

  void update(App& app, float dt, const InputState& in) override {
    (void)dt;
    if (in.pressed[(int)Btn::Back]) {
      if (fileIdx_ >= 0) {
        fileIdx_ = -1;  // 作弊码视图 → 返回文件列表
        sel_ = 0;
        scroll_ = 0;
        return;
      }
      app.popScene();
      return;
    }
    const int N = rowCount();
    if (N > 0) {
      if (in.pressed[(int)Btn::Up]) { sel_ = (sel_ + N - 1) % N; followSel_ = true; }
      if (in.pressed[(int)Btn::Down]) { sel_ = (sel_ + 1) % N; followSel_ = true; }
    } else {
      sel_ = 0;
    }
  }

  int rowCount() const {
    if (fileIdx_ < 0) return (int)files_.size();  // 文件列表
    return (int)files_[(size_t)fileIdx_].entries.size();
  }

  void draw(App& app) override {
    const Skin& sk = Theme::skin();
    Draw& d = app.draw;
    for (int i = 0; i < 40; i++) {
      const float t = (float)i / 40;
      d.rect(Rect(0, d.H * t, d.W, d.H / 40 + 1), sk.bg.lerp(sk.bgDeep, t));
    }
    EmuRunner* emu = app.activeEmu();
    const std::string gameTitle =
        emu && emu->running() ? emu->game().displayTitle() : std::string("未运行游戏");
    if (fileIdx_ < 0) {
      if (w::header(app, "金手指 · 选择文件", gameTitle + " · cheats/ 目录", true)) {
        app.popScene();
        return;
      }
    } else {
      const cheats::File& f = files_[(size_t)fileIdx_];
      char sub[256];
      snprintf(sub, sizeof(sub), "%s · %zu 条", gameTitle.c_str(), f.entries.size());
      if (w::header(app, std::string("金手指 · ") + f.name, sub, true)) {
        fileIdx_ = -1;
        sel_ = 0;
        scroll_ = 0;
        return;
      }
    }
    // 右上：导入 / 全开 / 全关（作弊码视图）
    {
      const Rect impRc(d.W - 476, 16, 150, 36);
      if (w::button(app, impRc, "导入 .cht", false, 13, true)) pushImport(&app);
      if (fileIdx_ >= 0) {
        const Rect allRc(d.W - 318, 16, 150, 36);
        const Rect noneRc(d.W - 160, 16, 142, 36);
        if (w::button(app, allRc, "全部开启", false, 13)) {
          const cheats::File& f = files_[(size_t)fileIdx_];
          for (size_t i = 0; i < f.entries.size(); i++)
            if (f.entries[i].supported())
              enabled_[cheats::enabledKey(f, i)] = true;
          cheats::saveEnabled(app.dataDir(), enabled_);
          applyToCore(app);
          app.toast("已全部开启（不支持的码自动跳过）");
        }
        if (w::button(app, noneRc, "全部关闭", false, 13)) {
          const cheats::File& f = files_[(size_t)fileIdx_];
          for (size_t i = 0; i < f.entries.size(); i++)
            enabled_[cheats::enabledKey(f, i)] = false;
          cheats::saveEnabled(app.dataDir(), enabled_);
          applyToCore(app);
          app.toast("已全部关闭");
        }
      }
    }

    const Rect listRc(24, 74, d.W - 48, d.H - 74 - 64);
    scroll_ += w::dragScroll(app, listRc);
    if (listRc.contains(app.input.input().mouse))
      scroll_ = std::max(0.0f, scroll_ - (float)app.input.input().wheel * 60.0f);
    if (followSel_) {
      followSel_ = false;
      const float rowY = 78 + (float)sel_ * 60 - scroll_;
      if (rowY < listRc.y) scroll_ -= listRc.y - rowY;
      if (rowY > listRc.bottom() - 60) scroll_ += rowY - (listRc.bottom() - 60);
    }
    d.pushClip(listRc);
    float y = 78 - scroll_;
    if (fileIdx_ < 0) {
      // —— 文件列表 ——
      for (size_t i = 0; i < files_.size(); i++) {
        const cheats::File& f = files_[i];
        const Rect rc(24, y, listRc.w, 54);
        int on = 0;
        for (size_t k = 0; k < f.entries.size(); k++) {
          auto it = enabled_.find(cheats::enabledKey(f, k));
          if (it != enabled_.end() && it->second) on++;
        }
        char value[32];
        snprintf(value, sizeof(value), "%zu 条 · 开 %d", f.entries.size(), on);
        if (w::listRow(app, rc, f.name, value, sel_ == (int)i,
                       fsutil::pathDisplay(f.path))) {
          fileIdx_ = (int)i;
          sel_ = 0;
          scroll_ = 0;
        }
        y += 60;
      }
      if (files_.empty()) {
        app.font.draw(app.sdlRenderer(),
                      "没有金手指文件——把 .cht 放入数据目录 cheats/，或右上「导入 .cht」",
                      40, 120, 15, sk.textDim);
        app.font.draw(app.sdlRenderer(),
                      "（RetroArch 格式：cheat0_desc / cheat0_code = \"地址:值\"）",
                      40, 148, 13, sk.textDim);
      }
    } else {
      // —— 作弊码开关列表 ——
      const cheats::File& f = files_[(size_t)fileIdx_];
      for (size_t i = 0; i < f.entries.size(); i++) {
        const cheats::Entry& e = f.entries[i];
        const Rect rc(24, y, listRc.w, 54);
        auto it = enabled_.find(cheats::enabledKey(f, i));
        bool on = it != enabled_.end() && it->second;
        if (e.supported()) {
          if (w::toggleRow(app, rc,
                           e.desc.size() > 64 ? e.desc.substr(0, 64) + "…" : e.desc, on,
                           sel_ == (int)i,
                           e.raw.substr(0, 56))) {
            toggle(app, f, i);
          }
        } else {
          // 不支持的格式（GameGenie 原码 / ROM 域 / 带比较）：灰显说明
          app.font.drawVCentered(app.sdlRenderer(),
                                 e.desc + "（该码格式暂不支持内存直写）", rc.x + 14,
                                 rc.cy(), 14, sk.textDim);
          const std::string raw = e.raw.substr(0, 56);
          app.font.drawVCentered(
              app.sdlRenderer(), raw,
              rc.right() - 14 - app.font.textWidth(raw, 12), rc.cy(), 12, sk.textDim);
        }
        y += 60;
      }
    }
    d.popClip();
    {
      const int N = rowCount();
      const float total = 78 + (float)N * 60;
      const float maxScroll = std::max(0.0f, total - 78 - listRc.h);
      if (scroll_ > maxScroll) scroll_ = maxScroll;
      if (scroll_ < 0) scroll_ = 0;
      if (total > listRc.h + 78) {
        scroll_ = w::scrollIndicator(app, Rect(d.W - 12, listRc.y, 6, listRc.h),
                                     listRc.h / (total - 78),
                                     scroll_ / (total - 78 - listRc.h)) *
                  (total - 78 - listRc.h);
      }
    }
    w::keyHintBar(app, {{"A / Enter", "开关"}, {"B / Esc", "返回"}, {"←→", "也可切换"}});
  }

 private:
  std::vector<cheats::File> files_;
  std::map<std::string, bool> enabled_;
  int fileIdx_ = -1;  // -1 = 文件列表；≥0 = 该文件的作弊码视图
  int sel_ = 0;
  float scroll_ = 0;
  bool followSel_ = false;
};

}  // namespace（匿名）
}  // namespace ns

std::unique_ptr<ns::Scene> ns::createCheatsScene() {
  return std::unique_ptr<Scene>(new CheatsScene());
}
