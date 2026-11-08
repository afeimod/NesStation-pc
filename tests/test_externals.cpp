// ExternalEmulators 逻辑冒烟测试（注册表 / 探测 / 参数模板 / 通配 / JSON 解析）
// 编译（仓库根目录）：
//   g++ -std=c++17 -O1 -I src -I third_party $(sdl2-config --cflags) \
//       tests/test_externals.cpp src/util/Fs.cpp src/util/Log.cpp \
//       $(sdl2-config --libs) -ldl -lpthread -o /tmp/test_ext && /tmp/test_ext
// 注：直接 include .cpp 以访问匿名命名空间内的 globMatch / parseAssetUrls
#include <cassert>
#include <cstdio>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

#include "emu/ExternalEmulators.h"
#include "util/Fs.h"

// —— 内部函数单测（匿名命名空间，include 后可见）——
namespace ns {
namespace {
// globMatch / parseAssetUrls 在 ExternalEmulators.cpp 的匿名命名空间里，
// 通过下面的 include 引入（放在同一线性翻译单元中）
}
}  // namespace ns
#include "emu/ExternalEmulators.cpp"

using namespace ns;

// ★v1.3.2 链接桩：ExternalEmulators.cpp 的后台启动验证线程引用
//   App::postNotice（实现在 App.cpp，测试不链接整个 App）——这里提供
//   空实现（v1.3.1 前 ExternalEmulators 无此引用，旧 CI 命令无需桩）
namespace ns {
void App::postNotice(const std::string& msg) { (void)msg; }
}  // namespace ns

// 从匿名命名空间暴露出来的符号（同一翻译单元可直接引用）
namespace ns {
namespace test_hooks {
bool glob(const std::string& n, const std::string& p) { return globMatch(n, p); }
std::vector<std::string> assets(const std::string& body) { return parseAssetUrls(body); }
}  // namespace test_hooks
}  // namespace ns

using ns::test_hooks::glob;
using ns::test_hooks::assets;

int main() {
  // 0) 通配匹配（对齐 Android matchesPattern：| 分隔，* 任意）
  assert(glob("pcsx2-v2.3.0-windows-x64-qt.7z",
              "pcsx2-v*-windows-x64-qt.7z|pcsx2-v*-windows-x64.7z|pcsx2-*win*x64*.zip"));
  assert(glob("cemu_2.6_win64.zip", "cemu-*-windows-x64.zip|cemu_*_win64.zip"));
  assert(glob("cemu-2.6-ubuntu-24.04-x86_64.AppImage",
              "cemu-*-ubuntu-24.04-x86_64.AppImage|cemu-*.AppImage"));
  assert(glob("PPSSPPWindowsSetup.exe", "PPSSPPWindows*.zip|PPSSPPWindowsSetup.exe"));
  assert(glob("azahar-2503-linux-x86_64.tar.xz",
              "azahar-*-linux-x86_64.tar.xz|azahar-*linux*.tar.*"));
  assert(!glob("cemu-macos-x86_64.dmg", "cemu-*-windows-x64.zip|cemu_*_win64.zip"));
  assert(!glob("pcsx2-v2.3.0-macos-qt.dmg",
               "pcsx2-v*-windows-x64-qt.7z|pcsx2-v*-windows-x64.7z"));
  printf("  ✓ 资产通配匹配（多模式 | 与 * 任意段）\n");

  // 0b) GitHub API JSON 资产解析
  {
    const std::string body =
        R"({"tag_name":"v2.6","assets":[{"name":"a.zip","browser_download_url":"https://github.com/cemu-project/Cemu/releases/download/v2.6/cemu-2.6-windows-x64.zip"},{"name":"b.AppImage","browser_download_url":"https://github.com/cemu-project/Cemu/releases/download/v2.6/cemu-2.6-ubuntu-24.04-x86_64.AppImage"}]})";
    const auto urls = assets(body);
    assert(urls.size() == 2);
    assert(urls[0].find("cemu-2.6-windows-x64.zip") != std::string::npos);
    assert(urls[1].find("AppImage") != std::string::npos);
    printf("  ✓ browser_download_url 解析（%zu 个资产）\n", urls.size());
  }
  const std::string dataDir = "/tmp/exttest-data";
  fsutil::removeFile(dataDir);
  fsutil::makeDirs(dataDir);

  ExternalEmulators ex;
  ex.init(dataDir);

  // 1) 注册表：8 个外置核心，中文名与平台正确（照搬 APK）
  assert(ex.all().size() == 8);
  struct Expect { const char* id; const char* name; GamePlatform p; const char* arg; };
  const Expect expects[] = {
      {"pcsx2", "PCSX2", GamePlatform::PS2, "--fullscreen \"ROM\""},
      {"ppsspp", "PPSSPP", GamePlatform::PSP, "--fullscreen \"ROM\""},
      {"flycast", "Flycast", GamePlatform::DC, "\"ROM\""},
      {"azahar", "Azahar", GamePlatform::N3DS, "-g \"ROM\""},
      {"ishiiruka", "Ishiiruka（Dolphin 系）", GamePlatform::NGCWII, "-e \"ROM\""},
      {"cemu", "Cemu", GamePlatform::WIIU, "-g \"ROM\" -f"},
      {"eden", "Eden", GamePlatform::NSWITCH, "-g \"ROM\""},
      {"freej2me", "FreeJ2ME", GamePlatform::JAVA, "\"ROM\""},
  };
  for (const auto& e : expects) {
    const ExternalCore* c = ex.find(e.id);
    assert(c && c->name == e.name);
    assert(c->platform == e.p);
    assert(c->buildArgs("ROM") == e.arg);
    printf("  ✓ %-10s %-24s args: %s\n", e.id, e.name, e.arg);
  }

  // 2) 说明全部为中文（含 CJK）
  for (const auto& c : ex.all()) {
    bool hasCJK = false;
    for (unsigned char ch : c.notes)
      if (ch & 0x80) { hasCJK = true; break; }
    assert(hasCJK && "notes should be Chinese");
  }
  printf("  ✓ 全部核心说明为中文\n");

  // 3) 探测：安装目录 <dataDir>/external/azahar/azahar-qt
  fsutil::makeDirs(dataDir + "/external/azahar");
  { FILE* f = fopen((dataDir + "/external/azahar/azahar-qt").c_str(), "w"); fputs("x", f); fclose(f); }
  ex.reDetect("azahar");
  assert(ex.available("azahar"));
  assert(ex.available(GamePlatform::N3DS));
  assert(!ex.available("pcsx2"));
  printf("  ✓ 安装目录探测：azahar → %s\n", ex.find("azahar")->foundPath.c_str());

  // 4) 探测：用户路径覆盖
  fsutil::makeDirs("/tmp/exttest-bin");
  { FILE* f = fopen("/tmp/exttest-bin/cemu-bin", "w"); fputs("x", f); fclose(f); }
  ex.setUserPath("cemu", "/tmp/exttest-bin/cemu-bin");
  assert(ex.available("cemu"));
  printf("  ✓ 用户路径探测：cemu → %s\n", ex.find("cemu")->foundPath.c_str());

  // 5) 平台计数 / 启动链
  assert(ex.availableCount(GamePlatform::N3DS) == 1);
  assert(ex.forPlatform(GamePlatform::PS2).size() == 1);
  assert(ex.forPlatform(GamePlatform::NGCWII)[0]->id == "ishiiruka");
  printf("  ✓ 平台映射：NGCWII → ishiiruka（对齐 APK）\n");

  // 6) 旧键 dolphin 不存在（由 App.cpp 做映射）；ishiiruka 生效
  assert(ex.find("dolphin") == nullptr);
  assert(ex.find("ishiiruka") != nullptr);
  printf("  ✓ 键名 = ishiiruka（旧 dolphin 由 App 启动时映射）\n");

  // 7b) 超级构建产物目录：exe 同目录 emulators/dolphin/dolphin-emu
  //     ★路径无关：exeDir() 取测试程序实际所在目录（CI 编译到 ./smoke/ 下同样成立；
  //     旧版硬编码 /tmp/emulators —— 二进制不在 /tmp 时探测必失败，CI 134 崩溃根因）
  {
    const std::string emuRoot =
        fsutil::join(fsutil::exeDir(), "emulators");
    const std::string dolDir = fsutil::join(emuRoot, "dolphin");
    const std::string fakeBin = fsutil::join(dolDir, "dolphin-emu");
    fsutil::makeDirs(dolDir);
    { FILE* f = fopen(fakeBin.c_str(), "w"); fputs("x", f); fclose(f); }
    ex.reDetect("ishiiruka");
    assert(ex.available("ishiiruka"));
    assert(ex.available(GamePlatform::NGCWII));
    printf("  ✓ 超级构建目录探测：ishiiruka → %s\n",
           ex.find("ishiiruka")->foundPath.c_str());
    fsutil::removeFile(fakeBin);
    ex.reDetect("ishiiruka");
    assert(!ex.available("ishiiruka"));
  }

  // 7c) ★v1.3.0 递归探测（深度 ≤4）：CI 超级构建真实布局
  //     emulators/dolphin/bin/dolphin-emu（cmake --install 布局）——
  //     旧版只下探一层探测不到 → 「三大核心无法调用」的根因（v1.2.9 用户实测）
  {
    const std::string emuRoot = fsutil::join(fsutil::exeDir(), "emulators");
    const std::string dolBin = fsutil::join(emuRoot, "dolphin/bin");
    const std::string fakeBin = fsutil::join(dolBin, "dolphin-emu");
    fsutil::makeDirs(dolBin);
    { FILE* f = fopen(fakeBin.c_str(), "w"); fputs("x", f); fclose(f); }
    ex.reDetect("ishiiruka");
    assert(ex.available("ishiiruka"));
    printf("  ✓ 递归探测（CI 布局 dolphin/bin/）：ishiiruka → %s\n",
           ex.find("ishiiruka")->foundPath.c_str());
    fsutil::removeFile(fakeBin);
    // share/ 等干扰目录存在时仍优先命中 bin/
    fsutil::makeDirs(fsutil::join(emuRoot, "dolphin/share"));
    fsutil::makeDirs(dolBin);
    { FILE* f = fopen(fakeBin.c_str(), "w"); fputs("x", f); fclose(f); }
    ex.reDetect("ishiiruka");
    assert(ex.available("ishiiruka"));
    printf("  ✓ share/ 干扰目录下仍命中：ishiiruka → %s\n",
           ex.find("ishiiruka")->foundPath.c_str());
    fsutil::removeFile(fakeBin);
    ex.reDetect("ishiiruka");
    assert(!ex.available("ishiiruka"));
  }

  // 7d) eden 布局：emulators/eden/eden（build/eden/bin/eden 拷到根层）
  {
    const std::string emuRoot = fsutil::join(fsutil::exeDir(), "emulators");
    fsutil::makeDirs(fsutil::join(emuRoot, "eden"));
    const std::string fakeBin = fsutil::join(emuRoot, "eden/eden");
    { FILE* f = fopen(fakeBin.c_str(), "w"); fputs("x", f); fclose(f); }
    ex.reDetect("eden");
    assert(ex.available("eden"));
    assert(ex.available(GamePlatform::NSWITCH));
    printf("  ✓ eden 根层直命中：eden → %s\n", ex.find("eden")->foundPath.c_str());
    fsutil::removeFile(fakeBin);
    ex.reDetect("eden");
    assert(!ex.available("eden"));
  }

  // 8) ★v1.3.2 端到端启动：模拟 emulators/dolphin/bin/dolphin-emu（真实
  //    可执行脚本），launchPlatform 必须真正拉起进程并传 ROM 参数 ——
  //    「点了运行没反应」类问题在此层回归（spawn 路径 + 参数拼接 + 探测链）
  {
    const std::string emuRoot = fsutil::join(fsutil::exeDir(), "emulators");
    const std::string dolBin = fsutil::join(emuRoot, "dolphin/bin");
    fsutil::makeDirs(dolBin);
    const std::string script = fsutil::join(dolBin, "dolphin-emu");
    const std::string marker = "/tmp/nesstation_spawn_test.marker";
    fsutil::removeFile(marker);
    {
      FILE* f = fopen(script.c_str(), "w");
      fprintf(f,
              "#!/bin/sh\n"
              "echo \"$@\" > '%s'\n"
              "sleep 2\n",  // 睡 2s：模拟器持续运行（不会被判定秒退）
              marker.c_str());
      fclose(f);
      chmod(script.c_str(), 0755);
    }
    ex.reDetect("ishiiruka");
    assert(ex.available("ishiiruka"));
    std::string err;
    const bool ok = ex.launchPlatform(GamePlatform::NGCWII,
                                       "/tmp/fake_game.iso", &err);
    assert(ok && "launchPlatform 应成功拉起模拟器");
    // 等脚本落盘标记文件（spawn 异步）
    bool markerSeen = false;
    for (int i = 0; i < 40 && !markerSeen; i++) {
      struct stat st;
      markerSeen = stat(marker.c_str(), &st) == 0 && st.st_size > 0;
      if (!markerSeen) usleep(100 * 1000);
    }
    assert(markerSeen && "模拟器进程应已执行并写出参数标记");
    {
      FILE* f = fopen(marker.c_str(), "rb");
      char buf[512] = {0};
      if (f) { fread(buf, 1, sizeof(buf) - 1, f); fclose(f); }
      const std::string args = buf;
      printf("  ✓ 端到端启动：dolphin-emu 参数 = %s\n", args.c_str());
      assert(args.find("-e") != std::string::npos && "应传 -e 启动参数");
      assert(args.find("fake_game.iso") != std::string::npos && "应传 ROM 路径");
    }
    fsutil::removeFile(script);
    fsutil::removeFile(marker);
    ex.reDetect("ishiiruka");
    assert(!ex.available("ishiiruka"));
  }

  // 7) 状态文本中文
  const std::string st = ex.statusText(GamePlatform::NGCWII);
  assert(st.find("未找到") != std::string::npos || st.find("：") != std::string::npos);
  printf("  ✓ 状态文本：%s\n", st.c_str());

  printf("全部通过\n");
  return 0;
}
