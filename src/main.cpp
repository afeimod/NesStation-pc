// NesStation PC — 入口
//
// 环境变量 NESSTATION_SELFTEST=<脚本> 启用无头自检：
//   SDL dummy 视频/音频驱动渲染若干场景并逐帧截图（CI / 视觉回归用）。
//   脚本格式（逗号分隔）: home:60,fsd:60,library:30,settings:30,about:20
//   截图输出目录: NESSTATION_SHOT_DIR（默认 /tmp/nesstation-shots）
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include <SDL.h>

#include "app/App.h"
#include "emu/EmuRunner.h"
#include "ui/Scenes.h"
#include "ui/Theme.h"
#include "util/Fs.h"
#include "util/Log.h"

namespace {

// 解析 "home:60,fsd:60,..." → [(step, frames)]
std::vector<std::pair<std::string, int>> parseScript(const char* s) {
  std::vector<std::pair<std::string, int>> out;
  std::string text(s ? s : "");
  size_t pos = 0;
  while (pos <= text.size()) {
    const size_t comma = text.find(',', pos);
    const std::string seg = text.substr(
        pos, comma == std::string::npos ? std::string::npos : comma - pos);
    const size_t colon = seg.rfind(':');
    if (colon != std::string::npos) {
      out.push_back({seg.substr(0, colon), std::atoi(seg.c_str() + colon + 1)});
    }
    if (comma == std::string::npos) break;
    pos = comma + 1;
  }
  return out;
}

// 自检状态（shared_ptr 保证在 run() 期间存活）
struct SelftestState {
  std::vector<std::pair<std::string, int>> steps;
  std::string shotDir;
  int stepIdx = 0;
  int framesLeft = 0;
  int saved = 0;
};

void applyStep(ns::App& app, const std::string& name) {
  if (name == "fsd") {
    ns::Theme::setStyle(ns::UiStyle::Fsd);
    app.replaceScene(ns::createHomeScene());
  } else if (name == "neon" || name == "home") {
    ns::Theme::setStyle(ns::UiStyle::Neon);
    app.replaceScene(ns::createHomeScene());
  } else if (name == "library") {
    app.pushScene(ns::createLibraryScene(-1));
  } else if (name == "settings") {
    app.pushScene(ns::createSettingsScene());
  } else if (name == "perf") {
    app.pushScene(ns::createSettingsScene(6));  // 性能 tab（快进倍速）
  } else if (name == "cores") {
    app.pushScene(ns::createSettingsScene(1));  // 核心 tab（应用内下载）
  } else if (name == "ext") {
    // 独立模拟器 tab（外置核心管理：自动下载 / 手动选择 / 打开官网 / 移除）
    app.pushScene(ns::createSettingsScene(4));
  } else if (name == "about") {
    app.pushScene(ns::createAboutScene());
  } else if (name == "browser") {
    // 文件浏览器（存储书签 / 权限提示新 UI）
    ns::BrowserRequest req;
    req.title = "选择 ROM 目录";
    req.mode = 0;
    req.onPick = [](const std::string&) {};
    app.pushScene(ns::createFileBrowser(req));
  } else if (name == "picker") {
    // 核心选择器（首个多核心已安装平台的游戏）
    const auto games = app.library.games();
    const ns::GameEntry* pick = nullptr;
    for (const auto& g : games)
      if (app.cores.installedCoresFor(g.platform).size() > 1) { pick = &g; break; }
    if (pick)
      app.pushScene(ns::createCorePickerScene(*pick, [](const std::string&) {}));
  } else if (name == "pickerext") {
    // 双形态平台选择器（进程内 libretro + 已就绪外置独立核心并存）
    const auto games = app.library.games();
    const ns::GameEntry* pick = nullptr;
    for (const auto& g : games) {
      if (app.cores.installedCoresFor(g.platform).size() > 0 &&
          app.externals.availableCount(g.platform) > 0) {
        pick = &g;
        break;
      }
    }
    if (pick)
      app.pushScene(ns::createCorePickerScene(*pick, [](const std::string&) {}));
  } else if (name == "ctxmenu") {
    // 右键上下文菜单组件（Android「长按卡片」等价交互）
    std::vector<ns::ContextMenuItem> items;
    items.push_back({1, "启动游戏", "Enter", false, [] {}});
    items.push_back({2, "加入收藏", "F", false, [] {}});
    items.push_back({0, "重命名（显示名）", "", false, [] {}});
    items.push_back({16, "重新选择核心", "已记忆", false, [] {}});
    items.push_back({0, "从库中移除", "", true, [] {}});
    ns::pushContextMenu(app, ns::Vec2(640, 300), "超级马力欧兄弟",
                        "NES · Nintendo Entertainment System · ★ 已收藏",
                        std::move(items));
  }
}

}  // namespace

int main(int argc, char** argv) {
  // 无头自检（必须在 SDL_Init 前设置）
  if (const char* script = SDL_getenv("NESSTATION_SELFTEST")) {
    SDL_setenv("SDL_VIDEODRIVER", "dummy", 1);
    SDL_setenv("SDL_AUDIODRIVER", "dummy", 1);
    auto st = std::make_shared<SelftestState>();
    st->steps = parseScript(script);
    st->shotDir = SDL_getenv("NESSTATION_SHOT_DIR")
                      ? SDL_getenv("NESSTATION_SHOT_DIR")
                      : "/tmp/nesstation-shots";
    fsutil::makeDirs(st->shotDir);  // 不存在则建（失败由保存时报错）
    if (!st->steps.empty()) st->framesLeft = st->steps[0].second;

    ns::App app;
    app.setTestHook([st](ns::App& a, Uint32) {
      if (st->framesLeft > 0) {
        st->framesLeft--;
        if (st->framesLeft == 0) {
          const std::string name = st->steps[(size_t)st->stepIdx].first;
          const std::string path =
              st->shotDir + "/shot_" + std::to_string(st->stepIdx) + "_" + name +
              ".png";
          if (a.saveScreenshot(path)) st->saved++;
          SDL_Log("SELFTEST shot: %s", path.c_str());
          st->stepIdx++;
          if (st->stepIdx < (int)st->steps.size()) {
            const auto& next = st->steps[(size_t)st->stepIdx];
            st->framesLeft = next.second;
            applyStep(a, next.first);
          } else {
            a.requestExit();
          }
        }
      }
    });
    const int rc = app.run(argc, argv);
    SDL_Log("SELFTEST DONE rc=%d shots=%d", rc, st->saved);
    return rc;
  }

  ns::App app;
  return app.run(argc, argv);
}
