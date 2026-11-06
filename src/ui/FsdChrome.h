// NesStation PC — FSD 风格共享组件（★v1.2.9）
//
// 1:1 对齐 APK FsdScaffold.kt / FsdCoverFlow.kt / FsdHomeScreen.platformIcon：
//   · fsdchrome::SysStats  — 真实系统状态（CPU /proc/stat 或 GetSystemTimes、
//     内存 / 存储容量、局械网 IP；对齐 FsdSysInfo，不做假数据）
//   · w::fsdTopBar         — 顶部状态条（CPU/内存/存储 chips + NES STATION + 时钟）
//   · w::fsdBottomBar      — 底部状态条（网络图标 + IP + 状态 + 日期时间）
//   · w::fsdButtonHints    — 手柄彩色圆钮提示（A/B/X/Y）
//   · w::fsdTitleBanner    — 金属灰标题横幅（平台 · 游戏名）
//   · w::fsdCounter        — 「N of M」计数
//   · w::fsdToolButton     — 工具按钮（图标 + 小字标签）
//   · w::fsdBreadcrumb     — 面包屑（游戏库 ▪ NES）
// 供 FSD 主页（HomeScenes.cpp）与 FSD 游戏库（LibraryScene.cpp）共用。
#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "app/App.h"

namespace ns {

namespace fsdchrome {

struct SysStats {
  float cpuLoad = 0;  // 0..1
  float memFrac = 0;
  std::string memText;
  float diskFrac = 0;
  std::string diskText;
};

// 采样一次系统状态（CPU 用差分，无阻塞 sleep；prev* 由调用方持有跨帧状态）
SysStats sampleStats(const std::string& dir, uint64_t* prevTotal, uint64_t* prevIdle);

// 局域网 IPv4（默认路由出口；取不到 = 空串 → 显示「未联网」）
std::string localIp();

std::string clockHHMM();  // "HH:MM"
std::string dateText();   // "2026-11-06 周五"

// 平台 → 徽标短名（APK FsdPlatformBadge 同表：FC/SFC/GB/.../NS）
const char* platformBadgeLabel(GamePlatform p);

// 平台 → 主页磁贴图标 id（APK FsdHomeScreen.platformIcon 同表）
int platformTileIcon(GamePlatform p);

// APK 主页 platformOrder（16 平台；WiiU/Switch 不进主页磁贴，与 APK 一致）
extern const GamePlatform kHomePlatformOrder[16];

}  // namespace fsdchrome

namespace w {

// 手柄按键提示项（letter 圆钮字母 / label 动作 / color 键色 0xRRGGBB）
struct FsdHint {
  const char* letter;
  const char* label;
  uint32_t color;
};

void fsdTopBar(App& app, const fsdchrome::SysStats& s);
float fsdTopBarH(App& app);   // 顶栏高度（dp 换算后）
void fsdBottomBar(App& app, const std::string& ip, const std::string& status);
float fsdBottomBarH(App& app);
// 居中一行圆钮提示（cy 为圆心 y）
void fsdButtonHints(App& app, float cy, const std::vector<FsdHint>& hints);
// 金属灰标题横幅（在 rc 内绘制，文字超宽自动省略）
void fsdTitleBanner(App& app, const Rect& rc, const std::string& text);
// 「N of M」计数徽章
void fsdCounter(App& app, const Rect& rc, int cur, int total);
// 工具按钮（图标 + 小字），返回本次激活
bool fsdToolButton(App& app, const Rect& rc, int iconId, const std::string& label);
// 面包屑（seg1 ▪ seg2，seg2 高亮大字）
void fsdBreadcrumb(App& app, float x, float cy, const std::string& seg1,
                   const std::string& seg2);

}  // namespace w
}  // namespace ns
