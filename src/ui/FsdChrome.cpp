// NesStation PC — FSD 风格共享组件实现（★v1.2.9，1:1 对齐 APK FsdScaffold.kt）
#include "ui/FsdChrome.h"
#include "ui/Widgets.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <ctime>

#ifdef _WIN32
#define NOMINMAX
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/statvfs.h>
#include <sys/sysinfo.h>
#include <unistd.h>
#endif

namespace ns {
namespace fsdchrome {

std::string fmtGB(double bytes) {
  const double gb = bytes / (1024.0 * 1024.0 * 1024.0);
  char buf[24];
  if (gb >= 10) snprintf(buf, sizeof(buf), "%.0fGB", gb);
  else snprintf(buf, sizeof(buf), "%.1fGB", gb);
  return buf;
}

#ifdef _WIN32
static bool readCpu(float* load, uint64_t* prevTotal, uint64_t* prevIdle) {
  FILETIME idle, kernel, user;
  if (!GetSystemTimes(&idle, &kernel, &user)) return false;
  auto toU64 = [](const FILETIME& t) {
    return ((uint64_t)t.dwHighDateTime << 32) | t.dwLowDateTime;
  };
  const uint64_t idleU = toU64(idle), totalU = toU64(kernel) + toU64(user);
  const uint64_t dT = totalU - *prevTotal, dI = idleU - *prevIdle;
  *prevTotal = totalU;
  *prevIdle = idleU;
  if (dT > 0) *load = (float)((double)(dT - dI) / (double)dT);
  return true;
}
#else
static bool readCpu(float* load, uint64_t* prevTotal, uint64_t* prevIdle) {
  FILE* f = fopen("/proc/stat", "r");
  if (!f) return false;
  char line[512];
  const char* got = fgets(line, sizeof(line), f);
  fclose(f);
  if (!got) return false;
  uint64_t vals[10] = {};
  int n = 0;
  char* p = line + 3;
  while (*p && n < 10) {
    while (*p == ' ') p++;
    if (!*p) break;
    vals[n++] = (uint64_t)strtoull(p, &p, 10);
  }
  if (n < 5) return false;
  uint64_t total = 0;
  for (int i = 0; i < n; i++) total += vals[i];
  const uint64_t idle = vals[3] + (n > 4 ? vals[4] : 0);
  const uint64_t dT = total - *prevTotal, dI = idle - *prevIdle;
  *prevTotal = total;
  *prevIdle = idle;
  if (dT > 0) *load = (float)((double)(dT - dI) / (double)dT);
  return true;
}
#endif

SysStats sampleStats(const std::string& dir, uint64_t* prevTotal, uint64_t* prevIdle) {
  SysStats s;
  readCpu(&s.cpuLoad, prevTotal, prevIdle);
#ifdef _WIN32
  MEMORYSTATUSEX ms;
  memset(&ms, 0, sizeof(ms));
  ms.dwLength = sizeof(ms);
  if (GlobalMemoryStatusEx(&ms)) {
    s.memFrac = (float)ms.dwMemoryLoad / 100.0f;
    s.memText = fmtGB((double)(ms.ullTotalPhys - ms.ullAvailPhys)) + "/" +
                fmtGB((double)ms.ullTotalPhys);
  }
  ULARGE_INTEGER freeAvail, total, freeNum;
  memset(&freeAvail, 0, sizeof(freeAvail));
  memset(&total, 0, sizeof(total));
  memset(&freeNum, 0, sizeof(freeNum));
  if (GetDiskFreeSpaceExA(dir.c_str(), &freeAvail, &total, &freeNum) &&
      total.QuadPart > 0) {
    s.diskFrac = (float)(1.0 - (double)freeAvail.QuadPart / (double)total.QuadPart);
    s.diskText = fmtGB((double)(total.QuadPart - freeAvail.QuadPart)) + "/" +
                 fmtGB((double)total.QuadPart);
  }
#else
  struct sysinfo si;
  memset(&si, 0, sizeof(si));
  if (sysinfo(&si) == 0 && si.totalram > 0) {
    const double totalB = (double)si.totalram * (double)si.mem_unit;
    const double availB = (double)(si.freeram + si.bufferram) * (double)si.mem_unit;
    s.memFrac = (float)((totalB - availB) / totalB);
    s.memText = fmtGB(totalB - availB) + "/" + fmtGB(totalB);
  }
  struct statvfs vfs;
  memset(&vfs, 0, sizeof(vfs));
  if (statvfs(dir.c_str(), &vfs) == 0 && vfs.f_blocks > 0) {
    const double totalB = (double)vfs.f_blocks * (double)vfs.f_frsize;
    const double availB = (double)vfs.f_bavail * (double)vfs.f_frsize;
    s.diskFrac = (float)((totalB - availB) / totalB);
    s.diskText = fmtGB(totalB - availB) + "/" + fmtGB(totalB);
  }
#endif
  if (s.cpuLoad < 0) s.cpuLoad = 0;
  if (s.cpuLoad > 1) s.cpuLoad = 1;
  return s;
}

std::string localIp() {
#ifdef _WIN32
  WSADATA wsa;
  if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) return "";
#endif
  std::string ip;
  const int fd = (int)socket(AF_INET, SOCK_DGRAM, 0);
  if (fd >= 0) {
    sockaddr_in target;
    memset(&target, 0, sizeof(target));
    target.sin_family = AF_INET;
    target.sin_port = htons(80);
    target.sin_addr.s_addr = htonl(0x08080808);  // 8.8.8.8（仅路由探测，不发包）
    if (connect(fd, (sockaddr*)&target, sizeof(target)) == 0) {
      sockaddr_in local;
      memset(&local, 0, sizeof(local));
      socklen_t len = sizeof(local);
      if (getsockname(fd, (sockaddr*)&local, &len) == 0) {
        char buf[INET_ADDRSTRLEN];
        buf[0] = 0;
        if (inet_ntop(AF_INET, &local.sin_addr, buf, sizeof(buf))) ip = buf;
      }
    }
#ifdef _WIN32
    closesocket(fd);
#else
    close(fd);
#endif
  }
#ifdef _WIN32
  WSACleanup();
#endif
  return ip;
}

std::string clockHHMM() {
  const time_t now = time(nullptr);
  const struct tm* t = localtime(&now);
  char buf[16];
  if (t) strftime(buf, sizeof(buf), "%H:%M", t);
  else buf[0] = 0;
  return buf;
}

std::string dateText() {
  const time_t now = time(nullptr);
  const struct tm* t = localtime(&now);
  if (!t) return "";
  static const char* wd[] = {"周日", "周一", "周二", "周三", "周四", "周五", "周六"};
  char buf[32];
  snprintf(buf, sizeof(buf), "%04d-%02d-%02d %s", t->tm_year + 1900, t->tm_mon + 1,
           t->tm_mday, wd[t->tm_wday % 7]);
  return buf;
}

const char* platformBadgeLabel(GamePlatform p) {
  switch (p) {
    case GamePlatform::NES: return "FC";
    case GamePlatform::SFC: return "SFC";
    case GamePlatform::GB: return "GB";
    case GamePlatform::GBA: return "GBA";
    case GamePlatform::DOS: return "DOS";
    case GamePlatform::ARCADE: return "ARC";
    case GamePlatform::MD: return "MD";
    case GamePlatform::PCE: return "PCE";
    case GamePlatform::NDS: return "NDS";
    case GamePlatform::PSX: return "PSX";
    case GamePlatform::PS2: return "PS2";
    case GamePlatform::JAVA: return "Java";
    case GamePlatform::DC: return "DC";
    case GamePlatform::N3DS: return "3DS";
    case GamePlatform::NGCWII: return "NWC";
    case GamePlatform::PSP: return "PSP";
    case GamePlatform::WIIU: return "WiiU";
    case GamePlatform::NSWITCH: return "NS";
  }
  return "";
}

int platformTileIcon(GamePlatform p) {
  switch (p) {
    case GamePlatform::NES: return 9;    // Gamepad（FC 十字手柄）
    case GamePlatform::SFC: return 24;   // VideogameAsset
    case GamePlatform::GB: return 25;    // Smartphone（竖向掌机）
    case GamePlatform::GBA: return 26;   // MobileFriendly（横向掌机）
    case GamePlatform::MD: return 27;    // Computer（16-bit 主机）
    case GamePlatform::PCE: return 28;   // Radio（PC-E 小白机）
    case GamePlatform::PSX: return 29;   // Album（CD 光盘）
    case GamePlatform::PS2: return 16;   // Memory（Emotion Engine 芯片）
    case GamePlatform::NDS: return 30;   // MenuBook（翻盖双屏）
    case GamePlatform::ARCADE: return 31;  // LocalPlay（街机厅票券）
    case GamePlatform::DOS: return 15;   // Terminal
    case GamePlatform::JAVA: return 32;  // LocalCafe（Java 咖啡）
    case GamePlatform::DC: return 33;    // SportsEsports
    case GamePlatform::N3DS: return 34;  // TabletAndroid（双屏掌机）
    case GamePlatform::NGCWII: return 35;  // Tv
    case GamePlatform::PSP: return 36;   // SmartDisplay（宽屏掌机）
    case GamePlatform::WIIU: return 37;  // Devices（TV+GamePad）
    case GamePlatform::NSWITCH: return 24;  // VideogameAsset
  }
  return 9;
}

const GamePlatform kHomePlatformOrder[16] = {
    GamePlatform::NES,  GamePlatform::SFC,    GamePlatform::GB,
    GamePlatform::GBA,  GamePlatform::MD,     GamePlatform::PCE,
    GamePlatform::PSX,  GamePlatform::PS2,    GamePlatform::NDS,
    GamePlatform::N3DS, GamePlatform::NGCWII, GamePlatform::DC,
    GamePlatform::ARCADE, GamePlatform::DOS,  GamePlatform::JAVA,
    GamePlatform::PSP,
};

}  // namespace fsdchrome

namespace w {

// dp（与主页 NeonHome::dpOf 同式：★v1.2.10 改 540/960 基准 —— 对齐逻辑画布
// 与 APK 横屏密度；旧 720/1280 基准在 540 基准画布上恒乘 0.75，FSD 文字整体偏小）
static float dpOf(const App& app) {
  const float dH = (float)app.draw.H / 540.0f;
  const float dW = (float)app.draw.W / 960.0f;
  return std::max(0.7f, std::min(dH, dW));
}

float fsdTopBarH(App& app) { return 40 * dpOf(app); }
float fsdBottomBarH(App& app) { return 27 * dpOf(app); }

void fsdTopBar(App& app, const fsdchrome::SysStats& s) {
  Draw& d = app.draw;
  const float D = dpOf(app);
  const float barH = fsdTopBarH(app);
  d.rect(Rect(0, 0, (float)d.W, barH), Color8::hex(0x061225, 0xCC));
  const Color8 textDim = Color8::hex(0x9FB6D4), textHi = Color8::hex(0xE8F1FF);
  float x = 14 * D;
  auto chip = [&](const char* label, const std::string& value, float frac) {
    const float lw = app.font.textWidth(label, 10 * D, true);
    const float vw = app.font.textWidth(value, 10 * D, true);
    app.font.draw(app.sdlRenderer(), label, x, barH * 0.12f, 10 * D, textDim, true);
    app.font.draw(app.sdlRenderer(), value, x + lw + 5 * D, barH * 0.12f, 10 * D, textHi,
                  true);
    const float barW = 86 * D;
    const Rect bg(x, barH * 0.56f, barW, 5 * D);
    d.roundRect(bg, 2.5f * D, Color8(255, 255, 255, 36));
    const Color8 col = frac >= 0.9f ? Color8::hex(0xE53935)
                          : frac >= 0.7f ? Color8::hex(0xF8E71C)
                                         : Color8::hex(0x7ED321);
    const float fw = bg.w * std::max(0.02f, std::min(1.0f, frac));
    d.roundRect(Rect(bg.x, bg.y, fw, bg.h), 2.5f * D, col);
    x += std::max(barW, lw + 5 * D + vw) + 18 * D;
  };
  char cpuV[8];
  snprintf(cpuV, sizeof(cpuV), "%d%%", (int)(s.cpuLoad * 100 + 0.5f));
  chip("CPU", cpuV, s.cpuLoad);
  chip("内存", s.memText, s.memFrac);
  chip("存储", s.diskText, s.diskFrac);
  // 右侧：NES STATION + 时钟
  const std::string clock = fsdchrome::clockHHMM();
  const float cw = app.font.textWidth(clock, 13 * D, true);
  app.font.draw(app.sdlRenderer(), clock, (float)d.W - 14 * D - cw, barH * 0.28f, 13 * D,
                textHi, true);
  app.font.draw(
      app.sdlRenderer(), "NES STATION",
      (float)d.W - 14 * D - cw - 10 * D - app.font.textWidth("NES STATION", 10 * D, true),
      barH * 0.36f, 10 * D, textDim, true);
}

void fsdBottomBar(App& app, const std::string& ip, const std::string& status) {
  Draw& d = app.draw;
  const float D = dpOf(app);
  const float barH = fsdBottomBarH(app);
  const float y = (float)d.H - barH;
  d.rect(Rect(0, y, (float)d.W, barH), Color8::hex(0x061225, 0xCC));
  const Color8 textDim = Color8::hex(0x9FB6D4), textHi = Color8::hex(0xE8F1FF);
  float x = 14 * D;
  iconNetwork(app, Rect(x, y + barH * 0.22f, 14 * D, 14 * D), textDim);
  x += 18 * D;
  const std::string ipTxt = ip.empty() ? "未联网" : ip;
  app.font.drawVCentered(app.sdlRenderer(), ipTxt, x, y + barH * 0.5f, 11 * D, textHi,
                         true);
  x += app.font.textWidth(ipTxt, 11 * D, true) + 16 * D;
  app.font.drawVCentered(app.sdlRenderer(), "状态: " + status, x, y + barH * 0.5f, 11 * D,
                         textDim);
  const std::string tm = fsdchrome::clockHHMM();
  const std::string dt = fsdchrome::dateText();
  const float tw = app.font.textWidth(tm, 12 * D, true);
  app.font.drawVCentered(app.sdlRenderer(), tm, (float)d.W - 14 * D - tw,
                         y + barH * 0.5f, 12 * D, textHi, true);
  app.font.drawVCentered(
      app.sdlRenderer(), dt,
      (float)d.W - 14 * D - tw - 12 * D - app.font.textWidth(dt, 11 * D),
      y + barH * 0.5f, 11 * D, textDim);
}

void fsdButtonHints(App& app, float cy, const std::vector<FsdHint>& hints) {
  Draw& d = app.draw;
  const float D = dpOf(app);
  float totalW = 0;
  for (const auto& h : hints)
    totalW += 20 * D + 5 * D + app.font.textWidth(h.label, 11 * D) + 14 * D;
  totalW -= 14 * D;
  // ★v1.2.10：加 BarBg 半透明底条（APK 底/顶栏同款 80% 黑蓝）——
  //   无底条时提示直接浮在磁贴上，视觉混杂
  const float barH = 32 * D;
  {
    const Rect bar((float)d.W * 0.5f - totalW / 2 - 14 * D, cy - barH * 0.5f,
                   totalW + 28 * D, barH);
    d.roundRect(bar, 6 * D, Color8::hex(0x061225, 0xB3));
  }
  float x = (float)d.W * 0.5f - totalW / 2;
  for (const auto& h : hints) {
    const float r = 10 * D;
    // 径向渐变圆（外圈 55% 内圈实色）
    d.circle(x + r, cy, r, Color8::hex(h.color).withA(140));
    d.circle(x + r, cy, r * 0.8f, Color8::hex(h.color));
    app.font.drawVCentered(
        app.sdlRenderer(), h.letter,
        x + r - app.font.textWidth(h.letter, 10 * D, true) / 2, cy, 10 * D,
        Color8(255, 255, 255), true);
    x += 20 * D + 5 * D;
    app.font.drawVCentered(app.sdlRenderer(), h.label, x, cy, 11 * D,
                           Color8::hex(0xE8F1FF));
    x += app.font.textWidth(h.label, 11 * D) + 14 * D;
  }
}

void fsdTitleBanner(App& app, const Rect& rc, const std::string& text) {
  Draw& d = app.draw;
  // 金属灰半透明垂直渐变（B9C2CC 85% → 6E7A88 85%）
  const Color8 top = Color8::hex(0xB9C2CC, 217), bot = Color8::hex(0x6E7A88, 217);
  const int bands = 14;
  for (int i = 0; i < bands; i++) {
    const float t = (float)i / bands;
    d.rect(Rect(rc.x, rc.y + rc.h * t, rc.w, rc.h / bands + 1), top.lerp(bot, t));
  }
  d.roundRectOutline(rc, 6, Color8(255, 255, 255, 60), 1);
  const float fs = std::min(17.0f, rc.h * 0.5f);
  const std::string txt = app.font.ellipsize(text, fs, (int)(rc.w - 22 * 2));
  app.font.drawVCentered(app.sdlRenderer(), txt, rc.x + 22, rc.cy(), fs,
                         Color8::hex(0x121A24), true);
}

void fsdCounter(App& app, const Rect& rc, int cur, int total) {
  Draw& d = app.draw;
  d.roundRect(rc, 4, Color8(0, 0, 0, 115));
  const std::string txt = std::to_string(cur) + " of " + std::to_string(total);
  const float fs = std::min(14.0f, rc.h * 0.55f);
  app.font.drawVCentered(
      app.sdlRenderer(), txt, rc.cx() - app.font.textWidth(txt, fs, true) / 2, rc.cy(),
      fs, Color8::hex(0xDCE8F5), true);
}

bool fsdToolButton(App& app, const Rect& rc, int iconId, const std::string& label) {
  Draw& d = app.draw;
  const float D = dpOf(app);
  const bool hov = rc.contains(app.input.input().mouse);
  if (hov) d.roundRect(rc, 8 * D, Color8(255, 255, 255, 26));
  const float is = 18 * D;
  icon(app, iconId, Rect(rc.cx() - is / 2, rc.y + 3 * D, is, is),
       hov ? Color8(255, 255, 255) : Color8(255, 255, 255, 224));
  const float fs = 9 * D;
  app.font.draw(app.sdlRenderer(), app.font.ellipsize(label, fs, (int)rc.w),
                rc.cx() - app.font.textWidth(app.font.ellipsize(label, fs, (int)rc.w), fs) / 2,
                rc.y + rc.h - 13 * D, fs, Color8::hex(0x9FB6D4));
  return activated(app, rc, false);
}

void fsdBreadcrumb(App& app, float x, float cy, const std::string& seg1,
                   const std::string& seg2) {
  const float D = dpOf(app);
  app.font.drawVCentered(app.sdlRenderer(), seg1, x, cy, 17 * D, Color8::hex(0x9FB6D4));
  float cx = x + app.font.textWidth(seg1, 17 * D) + 10 * D;
  app.font.drawVCentered(app.sdlRenderer(), "▪", cx, cy, 16 * D,
                         Color8(0x9F, 0xB6, 0xD4, 178));
  cx += app.font.textWidth("▪", 16 * D) + 10 * D;
  app.font.drawVCentered(app.sdlRenderer(), seg2, cx, cy, 22 * D, Color8(255, 255, 255),
                         true);
}

}  // namespace w
}  // namespace ns
