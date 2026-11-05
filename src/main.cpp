// NesStation PC — 入口
//
// 环境变量 NESSTATION_SELFTEST=<脚本> 启用无头自检：
//   SDL dummy 视频/音频驱动渲染若干场景并逐帧截图（CI / 视觉回归用）。
//   脚本格式（逗号分隔）: home:60,fsd:60,library:30,settings:30,about:20
//   截图输出目录: NESSTATION_SHOT_DIR（默认 /tmp/nesstation-shots）
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <map>
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
  bool started = false;

  // —— 合成输入（拖拽/点击模拟）——
  // synthFrame >= 0 时启用：帧号 → 待推送事件（在帧末推送，下一帧被泵入）
  int synthFrame = -1;
  std::map<int, std::vector<SDL_Event>> synth;
};

// 合成鼠标事件工具
SDL_Event synthMotion(int x, int y, int dx, int dy) {
  SDL_Event e;
  SDL_zero(e);
  e.type = SDL_MOUSEMOTION;
  e.motion.windowID = 1;
  e.motion.which = 0;
  e.motion.x = x;
  e.motion.y = y;
  e.motion.xrel = dx;
  e.motion.yrel = dy;
  return e;
}
SDL_Event synthButton(bool down, int x, int y) {
  SDL_Event e;
  SDL_zero(e);
  e.type = down ? SDL_MOUSEBUTTONDOWN : SDL_MOUSEBUTTONUP;
  e.button.windowID = 1;
  e.button.button = SDL_BUTTON_LEFT;
  e.button.state = down ? SDL_PRESSED : SDL_RELEASED;
  e.button.clicks = 1;
  e.button.x = x;
  e.button.y = y;
  return e;
}

void applyStep(ns::App& app, const std::string& name, SelftestState& st) {
  st.synthFrame = -1;
  st.synth.clear();
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
    app.pushScene(ns::createSettingsScene(3));  // 性能 tab（快进倍速）
  } else if (name == "cores") {
    app.pushScene(ns::createSettingsScene(6));  // 核心设置 tab（应用内下载）
  } else if (name == "ext") {
    // 核心设置 tab 的外置核心区块（自动下载 / 手动选择 / 打开官网 / 移除）
    app.pushScene(ns::createSettingsScene(6));
  } else if (name == "about") {
    app.pushScene(ns::createAboutScene());
  } else if (name == "browser") {
    // 文件浏览器（★v1.2.2 默认纯净视图：当前目录直接置顶，无书签堆叠）
    ns::BrowserRequest req;
    req.title = "选择 ROM 目录";
    req.mode = 0;
    req.onPick = [](const std::string&) {};
    app.pushScene(ns::createFileBrowser(req));
  } else if (name == "browserquick") {
    // ★v1.2.2 快速位置视图：点击右上「▣ 快速位置」按钮（W-456 中心 ≈ 924,53）
    //   → 截图应显示书签收藏夹（主目录/桌面/下载/文档/…）+ 顶部「← 返回当前目录」
    ns::BrowserRequest req;
    req.title = "选择 ROM 目录";
    req.mode = 0;
    req.onPick = [](const std::string&) {};
    app.pushScene(ns::createFileBrowser(req));
    st.synthFrame = 0;
    st.synth[3].push_back(synthMotion(924, 53, 0, 0));
    st.synth[4].push_back(synthButton(true, 924, 53));
    st.synth[6].push_back(synthButton(false, 924, 53));
  } else if (name == "browserdrag") {
    // ★拖拽滚动验证：进入浏览器（40 个子目录保证列表溢出）→ 按住列表中部
    //   上划 400px → 截图应看到列表已滚动（旧版焦点行跟随每帧把 scroll 拉回
    //   选中行，截图停在顶部 = 回归）
    const std::string many = "/tmp/nesstation-selftest-roms/many";
    fsutil::makeDirs(many);
    for (int i = 0; i < 40; i++)
      fsutil::makeDirs(many + "/游戏目录_" + std::to_string(i + 1));
    ns::BrowserRequest req;
    req.title = "选择 ROM 目录";
    req.mode = 0;
    req.startDir = many;
    req.onPick = [](const std::string&) {};
    app.pushScene(ns::createFileBrowser(req));
    st.synthFrame = 0;
    st.synth[3].push_back(synthMotion(400, 500, 0, 0));
    st.synth[4].push_back(synthButton(true, 400, 500));
    for (int k = 1; k <= 16; k++)
      st.synth[4 + k].push_back(synthMotion(400, 500 - k * 25, 0, -25));
    st.synth[21].push_back(synthButton(false, 400, 100));
  } else if (name == "browserbar") {
    // ★滚动条拖拽验证：按住右侧滚动条滑块下拽 → 截图应看到列表滚动且滑块高亮
    const std::string many = "/tmp/nesstation-selftest-roms/many";
    fsutil::makeDirs(many);
    for (int i = 0; i < 40; i++)
      fsutil::makeDirs(many + "/游戏目录_" + std::to_string(i + 1));
    ns::BrowserRequest req;
    req.title = "选择 ROM 目录";
    req.mode = 0;
    req.startDir = many;
    req.onPick = [](const std::string&) {};
    app.pushScene(ns::createFileBrowser(req));
    st.synthFrame = 0;
    st.synth[3].push_back(synthMotion(1271, 160, 0, 0));
    st.synth[4].push_back(synthButton(true, 1271, 160));
    for (int k = 1; k <= 12; k++)
      st.synth[4 + k].push_back(synthMotion(1271, 160 + k * 30, 0, 30));
    st.synth[17].push_back(synthButton(false, 1271, 520));
  } else if (name == "browsertap") {
    // ★「选择此目录」点击验证：弹栈+回调路径（use-after-free 回归测试；
    //   截图应回到主页并弹出 toast，进程不崩溃）
    ns::BrowserRequest req;
    req.title = "选择 ROM 目录";
    req.mode = 0;
    req.onPick = [](const std::string&) {};
    app.pushScene(ns::createFileBrowser(req));
    st.synthFrame = 0;
    st.synth[3].push_back(synthMotion(1140, 53, 0, 0));
    st.synth[4].push_back(synthButton(true, 1140, 53));
    st.synth[6].push_back(synthButton(false, 1140, 53));
  } else if (name == "scan") {
    // ★扫描验证：生成临时 ROM（含 NES 魔数/各平台扩展名）→ 扫描入库 →
    //   库页截图应显示新增游戏；随后的 home 步骤应显示 N GAMES
    const std::string dir = "/tmp/nesstation-selftest-roms";
    fsutil::makeDirs(dir);
    fsutil::makeDirs(fsutil::join(dir, "sub"));  // 验证递归扫描
    {
      std::ofstream f(fsutil::join(dir, "超级马力欧.nes"), std::ios::binary);
      f << "NES\x1A" << std::string(64, '\0');
    }
    {
      std::ofstream f(fsutil::join(dir, "恶魔城.nes"), std::ios::binary);
      f << "NES\x1A" << std::string(64, '\0');
    }
    {
      std::ofstream f(fsutil::join(dir, "sub", "超级马里奥世界.sfc"), std::ios::binary);
      f << std::string(512, '\xAB');
    }
    {
      std::ofstream f(fsutil::join(dir, "sub", "俄罗斯方块.gb"), std::ios::binary);
      f << std::string(512, '\xCD');
    }
    app.library.addRomDir(dir);
    const int n = app.library.scanDirectory(dir, ns::GamePlatform::NES, false, true);
    app.library.save();
    SDL_Log("SELFTEST scan added=%d total=%d", n, (int)app.library.games().size());
    app.pushScene(ns::createLibraryScene(-1));
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
      // 首帧：应用第一步（否则 scan 等需要主动初始化的首步不生效）
      if (!st->started) {
        st->started = true;
        if (!st->steps.empty()) applyStep(a, st->steps[(size_t)st->stepIdx].first, *st);
      }
      // 合成输入：帧末推送，下一帧被 SDL_PollEvent 泵入
      if (st->synthFrame >= 0) {
        st->synthFrame++;
        auto it = st->synth.find(st->synthFrame);
        if (it != st->synth.end())
          for (auto& e : it->second) SDL_PushEvent(&e);
      }
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
            applyStep(a, next.first, *st);
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
