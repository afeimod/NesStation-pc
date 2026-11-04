#include "core/Platform.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <unordered_map>
#include <vector>

#include "util/Fs.h"

namespace ns {
namespace {

std::string toLower(std::string s) {
  std::transform(s.begin(), s.end(), s.begin(),
                 [](unsigned char c) { return (char)std::tolower(c); });
  return s;
}

// 去掉 - _ / . 后的小写归一化（与原项目一致）
std::string normalizeAlias(const std::string& value) {
  std::string v = toLower(value);
  std::string out;
  out.reserve(v.size());
  for (char c : v) {
    if (c == '-' || c == '_' || c == '/' || c == '.') continue;
    out.push_back(c);
  }
  return out;
}

// —— ROM 内容嗅探（扫描入库用）——

// 读取文件头部 n 字节
static bool readHead(const std::string& path, size_t n, std::vector<uint8_t>* out) {
  FILE* f = fopen(path.c_str(), "rb");
  if (!f) return false;
  out->resize(n);
  const size_t got = fread(out->data(), 1, n, f);
  fclose(f);
  out->resize(got);
  return got > 0;
}

static bool startsWithAt(const std::vector<uint8_t>& d, size_t off, const char* needle) {
  const size_t len = strlen(needle);
  if (off + len > d.size()) return false;
  return memcmp(d.data() + off, needle, len) == 0;
}

// zip：解析 End of Central Directory → Central Directory 文件名（不解压）
// 返回内部条目的小写扩展名列表（最多 96 条）
static std::vector<std::string> zipInnerExts(const std::string& path) {
  std::vector<std::string> exts;
  const uint64_t fsize = fsutil::fileSize(path);
  if (fsize == 0 || fsize > (uint64_t)1 << 31) return exts;
  const size_t tailLen = (size_t)(fsize < 66000 ? fsize : 66000);
  std::vector<uint8_t> tail;
  FILE* f = fopen(path.c_str(), "rb");
  if (!f) return exts;
  tail.resize(tailLen);
  if (fseek(f, (long)(fsize - tailLen), SEEK_SET) != 0) {
    fclose(f);
    return exts;
  }
  const size_t got = fread(tail.data(), 1, tailLen, f);
  fclose(f);
  tail.resize(got);
  if (tail.size() < 22) return exts;
  // 从尾部找 EOCD（PK\x05\x06）
  size_t eocd = std::string::npos;
  for (size_t i = tail.size() - 22;; i--) {
    if (memcmp(tail.data() + i, "PK\x05\x06", 4) == 0) {
      eocd = i;
      break;
    }
    if (i == 0) break;
  }
  if (eocd == std::string::npos) return exts;
  const uint16_t count = (uint16_t)(tail[eocd + 10] | (tail[eocd + 11] << 8));
  const uint32_t cdOff = (uint32_t)(tail[eocd + 16] | (tail[eocd + 17] << 8) |
                                    ((uint32_t)tail[eocd + 18] << 16) |
                                    ((uint32_t)tail[eocd + 19] << 24));
  if (count == 0 || count > 4096 || cdOff >= fsize) return exts;
  std::vector<uint8_t> cd;
  cd.resize((size_t)std::min<uint64_t>(1 << 18, fsize - cdOff));
  f = fopen(path.c_str(), "rb");
  if (!f) return exts;
  if (fseek(f, (long)cdOff, SEEK_SET) != 0) {
    fclose(f);
    return exts;
  }
  const size_t cdGot = fread(cd.data(), 1, cd.size(), f);
  fclose(f);
  cd.resize(cdGot);
  size_t pos = 0;
  int seen = 0;
  while (pos + 46 <= cd.size() && seen < count && seen < 96) {
    if (memcmp(cd.data() + pos, "PK\x01\x02", 4) != 0) break;
    const uint16_t nameLen = (uint16_t)(cd[pos + 28] | (cd[pos + 29] << 8));
    if (pos + 46 + nameLen > cd.size()) break;
    std::string name((const char*)cd.data() + pos + 46, nameLen);
    const size_t dot = name.find_last_of('.');
    if (dot != std::string::npos && dot + 1 < name.size()) {
      std::string ext = toLower(name.substr(dot + 1));
      if (ext.size() <= 8) exts.push_back(ext);
    } else {
      exts.push_back("");  // 无扩展名（FBNeo 街机包常见）
    }
    pos += 46 + nameLen + (uint16_t)(cd[pos + 30] | (cd[pos + 31] << 8)) /* extra */ +
           (uint16_t)(cd[pos + 32] | (cd[pos + 33] << 8)) /* comment */;
    seen++;
  }
  return exts;
}

// 光盘镜像嗅探：ISO9660 PVD / SEGA 引导区 / GC·Wii 魔数 / PSP 魔数
static GamePlatform sniffDisc(const std::string& path) {
  std::vector<uint8_t> head;
  if (!readHead(path, 0x8018, &head)) return (GamePlatform)-1;
  // GC / Wii 镜像魔数（文件开头）
  if (head.size() >= 4) {
    if (memcmp(head.data(), "\xC2\x33\x9F\x3D", 4) == 0) return GamePlatform::NGCWII;
    if (memcmp(head.data(), "\x5D\x1C\x9E\xA3", 4) == 0) return GamePlatform::NGCWII;
    if (memcmp(head.data(), "~PSP", 4) == 0) return GamePlatform::PSP;
    if (memcmp(head.data(), "\x00PSP", 4) == 0) return GamePlatform::PSP;
    if (memcmp(head.data(), "NES\x1A", 4) == 0) return GamePlatform::NES;
  }
  // SEGA 引导区（原始 2352 扇区镜像 / IP.BIN）
  if (startsWithAt(head, 0x10, "SEGADISCSYSTEM")) return GamePlatform::MD;
  if (startsWithAt(head, 0x10, "SEGA SEGAKATANA")) return GamePlatform::DC;
  // ISO9660 PVD：扇区 16（0x8000）处 "\x01CD001"
  // ★C7744 修复："\x01CD001" 的十六进制转义贪婪消费 CD001（均合法十六进制
  //   字符）→ MSVC 报 escape sequence out of range（GCC 仅截断告警被 -w
  //   吞掉）。拆分字符串字面量，转义解析在引号处截止，两编译器语义一致。
  if (startsWithAt(head, 0x8000, "\x01" "CD001")) {
    // 系统标识符在 0x8008（32 字节）：PS1/PS2 为 "PLAYSTATION"
    if (startsWithAt(head, 0x8008, "PLAYSTATION")) {
      // PS1 CD < 700MB，PS2 DVD 通常 > 700MB
      const uint64_t sz = fsutil::fileSize(path);
      return sz > (uint64_t)700 * 1024 * 1024 ? GamePlatform::PS2 : GamePlatform::PSX;
    }
    return (GamePlatform)-1;  // 普通 ISO9660 无法区分 → 调用方回退
  }
  return (GamePlatform)-1;
}

}  // namespace

GamePlatform sniffRomPlatform(const std::string& romPath, GamePlatform fallback) {
  const std::string ext = fsutil::extOf(romPath);
  if (ext == "cue") {
    // 读 .cue 文本，找首个 FILE 行指向的镜像再嗅探
    std::vector<uint8_t> d;
    if (readHead(romPath, 4096, &d)) {
      std::string text((const char*)d.data(), d.size());
      const size_t filePos = text.find("FILE");
      if (filePos != std::string::npos) {
        const size_t q1 = text.find('"', filePos);
        const size_t q2 = q1 == std::string::npos ? std::string::npos : text.find('"', q1 + 1);
        if (q1 != std::string::npos && q2 != std::string::npos && q2 > q1 + 1) {
          const std::string target =
              fsutil::join(fsutil::dirOf(romPath), text.substr(q1 + 1, q2 - q1 - 1));
          const GamePlatform p = sniffDisc(target);
          if (p != (GamePlatform)-1) return p;
        }
      }
    }
    // .cue 本身无法判断时：默认 PS1（最常见）
    return GamePlatform::PSX;
  }
  if (ext == "bin" || ext == "iso" || ext == "img" || ext == "mdf") {
    const GamePlatform p = sniffDisc(romPath);
    return p != (GamePlatform)-1 ? p : fallback;
  }
  if (ext == "zip") {
    const std::vector<std::string> inner = zipInnerExts(romPath);
    if (inner.empty()) return fallback;
    // 1) 内部文件扩展名直接命中平台
    for (const std::string& e : inner) {
      if (e.empty()) continue;
      const GamePlatform p = platformFromExtension(e);
      // 排除歧义扩展名自身（zip 里的 zip / iso）与 DOS 业务后缀误报
      if (e == "zip" || e == "bin" || e == "iso" || e == "cue" || e == "img" ||
          e == "exe" || e == "com" || e == "bat" || e == "conf")
        continue;
      // platformFromExtension 兑底返回 NES：需要额外验证确实是 NES 系扩展
      static const char* nesExt[] = {"nes", "unf", "unif", "fds", "nez", "unh"};
      bool isNes = false;
      for (const char* x : nesExt)
        if (e == x) isNes = true;
      if (p == GamePlatform::NES && !isNes) continue;
      return p;
    }
    // 2) FBNeo / MAME 街机包特征：内部文件无扩展名或数字/部件扩展名（m1/v1/c1/sfix/p1…）
    bool arcadeish = false, hasNamed = false;
    for (const std::string& e : inner) {
      if (e.empty()) {
        arcadeish = true;
        continue;
      }
      static const char* arcadeExts[] = {"m1", "v1", "c1", "c2", "p1", "sp1", "sfix",
                                         "rom", "bin", "ic", "m", "v", "c"};
      bool isArc = false;
      for (const char* x : arcadeExts)
        if (e == x) isArc = true;
      bool numeric = !e.empty() && e.find_first_not_of("0123456789u") == std::string::npos;
      if (isArc || numeric)
        arcadeish = true;
      else
        hasNamed = true;
    }
    if (arcadeish && !hasNamed) return GamePlatform::ARCADE;
    return fallback;
  }
  return fallback;
}

// 平台占位封面渐变色对（与 Android 版 PlaceholderCover.platformColors 完全一致）
void platformGradient(GamePlatform p, uint32_t* top, uint32_t* bottom) {
  struct G {
    uint32_t a, b;
  };
  G c;
  switch (p) {
    case GamePlatform::NES: c = {0xE74C3C, 0x922B21}; break;
    case GamePlatform::SFC: c = {0x7D3C98, 0x4A235A}; break;
    case GamePlatform::GB: c = {0x7FB069, 0x4F622F}; break;
    case GamePlatform::GBA: c = {0x5D6D7E, 0x34495E}; break;
    case GamePlatform::MD: c = {0x3E64AF, 0x253C7A}; break;
    case GamePlatform::PCE: c = {0xD4AC0D, 0x9A7D0A}; break;
    case GamePlatform::PSX: c = {0x95A5A6, 0x5D6D7E}; break;
    case GamePlatform::PS2: c = {0x5499C7, 0x2E6DA4}; break;
    case GamePlatform::NDS: c = {0xAF7AC5, 0x6C3483}; break;
    case GamePlatform::N3DS: c = {0xE67E22, 0x935116}; break;
    case GamePlatform::NGCWII: c = {0x76D7C4, 0x48C9B0}; break;
    case GamePlatform::WIIU: c = {0x85C1E9, 0x5499C7}; break;
    case GamePlatform::DC: c = {0xF1948A, 0xCB4335}; break;
    case GamePlatform::ARCADE: c = {0xF1C40F, 0xB7950B}; break;
    case GamePlatform::DOS: c = {0xB2BABB, 0x7F8C8D}; break;
    case GamePlatform::JAVA: c = {0x7E5109, 0x4A3B0C}; break;
    case GamePlatform::PSP: c = {0x9B59B6, 0x6C3483}; break;
    case GamePlatform::NSWITCH: c = {0xE74C3C, 0x27AE60}; break;
    default: c = {0x5D6D7E, 0x34495E}; break;
  }
  if (top) *top = c.a;
  if (bottom) *bottom = c.b;
}

const PlatformList& allPlatforms() {
  static const PlatformList kList = {
      GamePlatform::NES,     GamePlatform::SFC,     GamePlatform::GB,
      GamePlatform::GBA,     GamePlatform::DOS,     GamePlatform::ARCADE,
      GamePlatform::MD,      GamePlatform::PCE,     GamePlatform::NDS,
      GamePlatform::PSX,     GamePlatform::PS2,     GamePlatform::JAVA,
      GamePlatform::DC,      GamePlatform::N3DS,    GamePlatform::NGCWII,
      GamePlatform::WIIU,    GamePlatform::PSP,     GamePlatform::NSWITCH,
  };
  return kList;
}

const char* platformShortName(GamePlatform p) {
  switch (p) {
    case GamePlatform::NES: return "NES";
    case GamePlatform::SFC: return "SFC";
    case GamePlatform::GB: return "GB/GBC";
    case GamePlatform::GBA: return "GBA";
    case GamePlatform::DOS: return "DOS";
    case GamePlatform::ARCADE: return "Arcade";
    case GamePlatform::MD: return "MD/SEGA";
    case GamePlatform::PCE: return "PCE/TG16";
    case GamePlatform::NDS: return "NDS";
    case GamePlatform::PSX: return "PSX";
    case GamePlatform::PS2: return "PS2";
    case GamePlatform::JAVA: return "Java";
    case GamePlatform::DC: return "DC";
    case GamePlatform::N3DS: return "3DS";
    case GamePlatform::NGCWII: return "NGC/Wii";
    case GamePlatform::WIIU: return "Wii U";
    case GamePlatform::PSP: return "PSP";
    case GamePlatform::NSWITCH: return "Switch";
  }
  return "?";
}

const char* platformFullName(GamePlatform p) {
  switch (p) {
    case GamePlatform::NES: return "Nintendo Entertainment System";
    case GamePlatform::SFC: return "Super Famicom / SNES";
    case GamePlatform::GB: return "Game Boy / Game Boy Color";
    case GamePlatform::GBA: return "Game Boy Advance";
    case GamePlatform::MD: return "SEGA Mega Drive / Genesis";
    case GamePlatform::PCE: return "PC Engine / TurboGrafx-16";
    case GamePlatform::PSX: return "Sony PlayStation";
    case GamePlatform::PS2: return "PlayStation 2";
    case GamePlatform::NDS: return "Nintendo DS / DSi";
    case GamePlatform::N3DS: return "Nintendo 3DS";
    case GamePlatform::NGCWII: return "GameCube / Wii";
    case GamePlatform::DC: return "SEGA Dreamcast";
    case GamePlatform::ARCADE: return "Arcade / FBNeo";
    case GamePlatform::DOS: return "MS-DOS";
    case GamePlatform::JAVA: return "Java Micro Edition";
    case GamePlatform::PSP: return "Sony PlayStation Portable";
    case GamePlatform::WIIU: return "Nintendo Wii U";
    case GamePlatform::NSWITCH: return "Nintendo Switch";
  }
  return "?";
}

const char* platformEra(GamePlatform p) {
  switch (p) {
    case GamePlatform::NES: return "1983 · 8-BIT";
    case GamePlatform::SFC: return "1990 · 16-BIT";
    case GamePlatform::GB: return "1989 · HANDHELD";
    case GamePlatform::GBA: return "2001 · 32-BIT";
    case GamePlatform::MD: return "1988 · 16-BIT";
    case GamePlatform::PCE: return "1987 · 8/16-BIT";
    case GamePlatform::PSX: return "1994 · 32-BIT CD";
    case GamePlatform::PS2: return "2000 · 128-BIT";
    case GamePlatform::NDS: return "2004 · DUAL SCREEN";
    case GamePlatform::N3DS: return "2011 · GLASSLESS 3D";
    case GamePlatform::NGCWII: return "2001 / 2006 · OPTICAL";
    case GamePlatform::DC: return "1998 · 128-BIT";
    case GamePlatform::ARCADE: return "1978 · COIN-OP";
    case GamePlatform::DOS: return "1981 · 16-BIT X86";
    case GamePlatform::JAVA: return "2001 · MIDP 2.0";
    case GamePlatform::PSP: return "2004 · PORTABLE 128-BIT";
    case GamePlatform::WIIU: return "2012 · HD + GAMEPAD";
    case GamePlatform::NSWITCH: return "2017 · HYBRID";
  }
  return "";
}

// 平台强调色 —— 沿用原项目 FSD 磁贴与 Neon 徽章的配色语言
uint32_t platformAccent(GamePlatform p) {
  switch (p) {
    case GamePlatform::NES: return 0xE74C3C;   // NES 红
    case GamePlatform::SFC: return 0x8E44AD;   // 紫
    case GamePlatform::GB: return 0x7BB36A;    // 草绿
    case GamePlatform::GBA: return 0x2F7BD9;   // 紫蓝
    case GamePlatform::MD: return 0x0D4FA8;    // SEGA 蓝
    case GamePlatform::PCE: return 0xDE8A00;   // PCE 橙
    case GamePlatform::PSX: return 0x9FB6D4;   // PS 灰蓝
    case GamePlatform::PS2: return 0x2E86E0;   // PS2 蓝
    case GamePlatform::NDS: return 0x6DBE28;   // NDS 绿
    case GamePlatform::N3DS: return 0xE23B3B;  // 3DS 红
    case GamePlatform::NGCWII: return 0x7ED321;  // NGC 紫? 用亮绿区分 Wii
    case GamePlatform::DC: return 0xF2C200;    // DC 橙黄
    case GamePlatform::ARCADE: return 0xF8E71C; // 街机黄
    case GamePlatform::DOS: return 0x9AA5B1;   // DOS 灰
    case GamePlatform::JAVA: return 0xE76F00;  // Java 橙
    case GamePlatform::PSP: return 0x00E5FF;   // PSP 青
    case GamePlatform::WIIU: return 0x00B8D9;  // Wii U 青
    case GamePlatform::NSWITCH: return 0xFF2D3F; // Switch 红
  }
  return 0xCCCCCC;
}

PlatformCore platformCore(GamePlatform p) {
  switch (p) {
    case GamePlatform::NES:
      return {CoreKind::Libretro, "fceumm_libretro", nullptr, "FCEUmm · FC/NES 模拟",
              "FDS 游戏需要 disksys.rom（放入 system 目录）"};
    case GamePlatform::SFC:
      return {CoreKind::Libretro, "snes9x_libretro", nullptr, "Snes9x · 超级任天堂模拟",
              "无需 BIOS"};
    case GamePlatform::GB:
      return {CoreKind::Libretro, "mgba_libretro", nullptr, "mGBA · GB/GBC 模拟", "无需 BIOS"};
    case GamePlatform::GBA:
      return {CoreKind::Libretro, "mgba_libretro", nullptr, "mGBA · GBA 模拟", "无需 BIOS"};
    case GamePlatform::DOS:
      return {CoreKind::Libretro, "dosbox_pure_libretro", nullptr, "DOSBox-Pure · DOS 模拟",
              "无需 BIOS"};
    case GamePlatform::ARCADE:
      return {CoreKind::Libretro, "fbneo_libretro", nullptr, "FBNeo · 街机模拟",
              "NeoGeo → neogeo.zip，PGM → pgm.zip（放 system/fbneo/）"};
    case GamePlatform::MD:
      return {CoreKind::Libretro, "genesis_plus_gx_libretro", nullptr,
              "Genesis Plus GX · MD/世嘉模拟", "Mega-CD 需要 bios_CD_E/J/U.zip"};
    case GamePlatform::PCE:
      return {CoreKind::Libretro, "geargrafx_libretro", nullptr, "Geargrafx · PCE 模拟",
              "PCE-CD 需要 syscard3.pce（System Card）"};
    case GamePlatform::NDS:
      return {CoreKind::Libretro, "melonds_libretro", nullptr, "melonDS · NDS 模拟",
              "无需 BIOS（内置 FreeBIOS）"};
    case GamePlatform::PSX:
      return {CoreKind::Libretro, "pcsx_rearmed_libretro", nullptr, "PCSX-ReARMed · PS1 模拟",
              "无需 BIOS（HLE）；部分游戏推荐 BIOS"};
    case GamePlatform::PS2:
      // 实验性进程内核心（软件渲染）；推荐外置 PCSX2（对齐 Android 注册表标记）
      return {CoreKind::Libretro, "pcsx2_libretro", nullptr, "PCSX2 · PS2 模拟",
              "需要正版 PS2 BIOS（system/pcsx2/bios/）", true};
    case GamePlatform::JAVA:
      return {CoreKind::Libretro, "squirreljme_libretro", nullptr, "SquirrelJME · Java ME 模拟",
              "无需 BIOS"};
    case GamePlatform::DC:
      return {CoreKind::Libretro, "flycast_libretro", nullptr, "Flycast · DC/NAOMI 模拟",
              "dc_boot.bin + dc_flash.bin（NAOMI 无需）"};
    case GamePlatform::N3DS:
      return {CoreKind::Libretro, "azahar_libretro", nullptr, "Azahar · 3DS 模拟",
              "加密游戏需 aes_keys.txt；部分游戏需系统档案"};
    case GamePlatform::NGCWII:
      return {CoreKind::Standalone, nullptr, "ishiiruka", "Ishiiruka · NGC/Wii 模拟（Dolphin 系）",
              "GC/Wii（Dolphin 优化分支，PC 版原生构建；标准 Dolphin 同参数兼容）"};
    case GamePlatform::WIIU:
      return {CoreKind::Standalone, nullptr, "cemu", "Cemu · Wii U 模拟",
              "Wii U（官方开源 PC 版，Vulkan 全速）。WUD 解密需 otp.bin"};
    case GamePlatform::PSP:
      return {CoreKind::Libretro, "ppsspp_libretro", nullptr, "PPSSPP · PSP 模拟",
              "实验性进程内核心；推荐外置 PPSSPP（原生 GPU 全速）", true};
    case GamePlatform::NSWITCH:
      return {CoreKind::Standalone, nullptr, "eden", "Eden · Switch 模拟",
              "Switch（yuzu 系社区延续版）。加密游戏需 prod.keys（核心设置内导入）"};
  }
  return {CoreKind::Libretro, "fceumm_libretro", nullptr, "FCEUmm · FC/NES 模拟", ""};
}

// —— 核心候选链 ————————————————————————————————————————————
// 候选均经 libretro buildbot 实测（linux/x86_64、linux/aarch64、
// windows/x86_64 三个平台），缺失平台由链上后续核心自动补位：
//   azahar 在 linux/aarch64 无预编译包 → Citra 顶替
std::vector<PlatformCore> platformCoreChain(GamePlatform p) {
  switch (p) {
    case GamePlatform::NES:
      return {
          {CoreKind::Libretro, "fceumm_libretro", nullptr, "FCEUmm · FC/NES 模拟",
           "FDS 游戏需要 disksys.rom（放入 system 目录）"},
          {CoreKind::Libretro, "nestopia_libretro", nullptr, "Nestopia · FC/NES 高精度模拟",
           "FDS 游戏需要 disksys.rom"},
          {CoreKind::Libretro, "quicknes_libretro", nullptr, "QuickNES · FC/NES 快速模拟",
           "无需 BIOS（精度较低）"},
      };
    case GamePlatform::SFC:
      return {
          {CoreKind::Libretro, "snes9x_libretro", nullptr, "Snes9x · 超级任天堂模拟", "无需 BIOS"},
          {CoreKind::Libretro, "bsnes_libretro", nullptr, "bsnes · 超任高精度模拟", "无需 BIOS"},
      };
    case GamePlatform::GB:
      return {
          {CoreKind::Libretro, "mgba_libretro", nullptr, "mGBA · GB/GBC 模拟", "无需 BIOS"},
          {CoreKind::Libretro, "gambatte_libretro", nullptr, "Gambatte · GB/GBC 高精度模拟",
           "无需 BIOS"},
      };
    case GamePlatform::GBA:
      return {
          {CoreKind::Libretro, "mgba_libretro", nullptr, "mGBA · GBA 模拟", "无需 BIOS"},
          {CoreKind::Libretro, "vba_next_libretro", nullptr, "VBA-M · GBA 模拟", "无需 BIOS"},
          {CoreKind::Libretro, "gpsp_libretro", nullptr, "gpSP · GBA 快速模拟",
           "GBA BIOS 可选（gba_bios.bin）"},
      };
    case GamePlatform::DOS:
      return {
          {CoreKind::Libretro, "dosbox_pure_libretro", nullptr, "DOSBox-Pure · DOS 模拟",
           "无需 BIOS"},
          {CoreKind::Libretro, "dosbox_svn_libretro", nullptr, "DOSBox-SVN · DOS 模拟",
           "无需 BIOS"},
      };
    case GamePlatform::ARCADE:
      return {
          {CoreKind::Libretro, "fbneo_libretro", nullptr, "FBNeo · 街机模拟",
           "NeoGeo → neogeo.zip，PGM → pgm.zip（放 system/fbneo/）"},
          {CoreKind::Libretro, "mame2003_plus_libretro", nullptr, "MAME 2003-Plus · 街机模拟",
           "NeoGeo → neogeo.zip"},
      };
    case GamePlatform::MD:
      return {
          {CoreKind::Libretro, "genesis_plus_gx_libretro", nullptr,
           "Genesis Plus GX · MD/世嘉模拟", "Mega-CD 需要 bios_CD_E/J/U.zip"},
          {CoreKind::Libretro, "picodrive_libretro", nullptr, "PicoDrive · MD/32X 快速模拟",
           "32X 支持；Mega-CD 需要 BIOS"},
      };
    case GamePlatform::PCE:
      return {
          {CoreKind::Libretro, "geargrafx_libretro", nullptr, "Geargrafx · PCE 模拟",
           "PCE-CD 需要 syscard3.pce（System Card）"},
          {CoreKind::Libretro, "mednafen_pce_fast_libretro", nullptr,
           "Mednafen PCE Fast · PCE 快速模拟", "PCE-CD 需要 syscard3.pce"},
      };
    case GamePlatform::NDS:
      return {
          {CoreKind::Libretro, "melonds_libretro", nullptr, "melonDS · NDS 模拟",
           "无需 BIOS（内置 FreeBIOS）"},
          {CoreKind::Libretro, "desmume_libretro", nullptr, "DeSmuME · NDS 模拟",
           "无需 BIOS"},
      };
    case GamePlatform::PSX:
      return {
          {CoreKind::Libretro, "pcsx_rearmed_libretro", nullptr, "PCSX-ReARMed · PS1 模拟",
           "无需 BIOS（HLE）；部分游戏推荐 BIOS"},
          {CoreKind::Libretro, "swanstation_libretro", nullptr, "SwanStation · PS1 高精度模拟",
           "无需 BIOS（HLE）；要求更高精度时使用"},
      };
    case GamePlatform::PS2:
      return {
          {CoreKind::Libretro, "pcsx2_libretro", nullptr, "PCSX2 · PS2 模拟",
           "需要正版 PS2 BIOS（system/pcsx2/bios/）", true},
      };
    case GamePlatform::JAVA:
      return {
          {CoreKind::Libretro, "squirreljme_libretro", nullptr, "SquirrelJME · Java ME 模拟",
           "无需 BIOS"},
      };
    case GamePlatform::DC:
      return {
          {CoreKind::Libretro, "flycast_libretro", nullptr, "Flycast · DC/NAOMI 模拟",
           "dc_boot.bin + dc_flash.bin（NAOMI 无需）"},
      };
    case GamePlatform::N3DS:
      return {
          {CoreKind::Libretro, "azahar_libretro", nullptr, "Azahar · 3DS 模拟",
           "加密游戏需 aes_keys.txt；部分游戏需系统档案"},
          {CoreKind::Libretro, "citra_libretro", nullptr, "Citra · 3DS 模拟",
           "加密游戏需 aes_keys.txt；部分游戏需系统档案"},
      };
    case GamePlatform::NGCWII:
      return {{CoreKind::Standalone, nullptr, "ishiiruka", "Ishiiruka · NGC/Wii 模拟（Dolphin 系）",
               "GC/Wii（Dolphin 优化分支，PC 版原生构建；标准 Dolphin 同参数兼容）"}};
    case GamePlatform::WIIU:
      return {{CoreKind::Standalone, nullptr, "cemu", "Cemu · Wii U 模拟",
               "Wii U（官方开源 PC 版，Vulkan 全速）。WUD 解密需 otp.bin"}};
    case GamePlatform::PSP:
      return {
          {CoreKind::Libretro, "ppsspp_libretro", nullptr, "PPSSPP · PSP 模拟",
           "实验性进程内核心；推荐外置 PPSSPP（原生 GPU 全速）", true},
      };
    case GamePlatform::NSWITCH:
      return {{CoreKind::Standalone, nullptr, "eden", "Eden · Switch 模拟",
               "Switch（yuzu 系社区延续版）。加密游戏需 prod.keys（核心设置内导入）"}};
  }
  return {};
}

GamePlatform platformFromExtension(const std::string& extRaw) {
  const std::string ext = toLower(extRaw);
  if (ext == "nes" || ext == "unf" || ext == "unif" || ext == "fds" || ext == "nez" ||
      ext == "unh")
    return GamePlatform::NES;
  if (ext == "smc" || ext == "sfc" || ext == "swc" || ext == "fig" || ext == "bs")
    return GamePlatform::SFC;
  if (ext == "gb" || ext == "sgb" || ext == "gbc") return GamePlatform::GB;
  if (ext == "gba") return GamePlatform::GBA;
  // DOSBox-Pure 启动器与镜像
  if (ext == "bat" || ext == "exe" || ext == "com" || ext == "dosz" || ext == "conf" ||
      ext == "iso" || ext == "cue" || ext == "img" || ext == "ima" || ext == "vhd" ||
      ext == "hd")
    return GamePlatform::DOS;
  if (ext == "7z") return GamePlatform::ARCADE;
  if (ext == "md" || ext == "smd" || ext == "gen" || ext == "sms" || ext == "gg" ||
      ext == "sg" || ext == "68k" || ext == "chd")
    return GamePlatform::MD;
  if (ext == "pce" || ext == "sgx" || ext == "hes") return GamePlatform::PCE;
  if (ext == "nds" || ext == "app" || ext == "ids" || ext == "srl" || ext == "dsi")
    return GamePlatform::NDS;
  if (ext == "pbp" || ext == "m3u" || ext == "ecm" || ext == "mds" || ext == "mdf")
    return GamePlatform::PSX;
  if (ext == "cso" || ext == "isz" || ext == "elf" || ext == "zso") return GamePlatform::PS2;
  if (ext == "gdi" || ext == "cdi" || ext == "lst") return GamePlatform::DC;
  if (ext == "3ds" || ext == "cci" || ext == "cxi" || ext == "cia")
    return GamePlatform::N3DS;
  if (ext == "gcm" || ext == "rvz" || ext == "gcz" || ext == "ciso" || ext == "nkit" ||
      ext == "wbfs" || ext == "wad" || ext == "dol")
    return GamePlatform::NGCWII;
  if (ext == "wud" || ext == "wux" || ext == "wua" || ext == "wuhb" || ext == "rpx")
    return GamePlatform::WIIU;
  if (ext == "xci" || ext == "nsp" || ext == "nca" || ext == "nro")
    return GamePlatform::NSWITCH;
  if (ext == "jar" || ext == "jad") return GamePlatform::JAVA;
  // 与原项目一致：.zip / .bin 故意不在此映射
  // 兜底：按原 GamePlatform.valueOf 语义无法表达，返回 NES 由调用方用平台页消歧
  return GamePlatform::NES;
}

GamePlatform platformFromString(const std::string& value) {
  const std::string v = normalizeAlias(value);
  if (v == "gbc" || v == "gameboycolor") return GamePlatform::GB;
  if (v == "nes" || v == "fc" || v == "famicom" || v == "fceumm" || v == "fceux" ||
      v == "nestopia")
    return GamePlatform::NES;
  if (v == "sfc" || v == "snes" || v == "supernintendo" || v == "snes9x")
    return GamePlatform::SFC;
  if (v == "gb" || v == "gameboy") return GamePlatform::GB;
  if (v == "gba" || v == "gameboyadvance") return GamePlatform::GBA;
  if (v == "dos" || v == "dosbox" || v == "dosboxpure") return GamePlatform::DOS;
  if (v == "arcade" || v == "fbneo" || v == "fbneocore" || v == "mame" || v == "cps1" ||
      v == "cps2" || v == "cps3" || v == "neogeo" || v == "pgm" || v == "finalburn")
    return GamePlatform::ARCADE;
  if (v == "md" || v == "genesis" || v == "megadrive" || v == "segagenesis" ||
      v == "megacd" || v == "segacd" || v == "genplus" || v == "genplusgx" ||
      v == "genesisplusgx")
    return GamePlatform::MD;
  if (v == "pce" || v == "pcengine" || v == "turbografx" || v == "tg16" ||
      v == "tg16cd" || v == "geargrafx" || v == "supergrafx")
    return GamePlatform::PCE;
  if (v == "nds" || v == "ds" || v == "nintendo" || v == "nintendods" || v == "melonds")
    return GamePlatform::NDS;
  if (v == "psx" || v == "ps1" || v == "playstation" || v == "playstation1" ||
      v == "sony" || v == "pcsx" || v == "pcsxrearmed" || v == "pcsxr")
    return GamePlatform::PSX;
  if (v == "ps2" || v == "playstation2" || v == "play" || v == "psx2" || v == "pcsx2")
    return GamePlatform::PS2;
  if (v == "dc" || v == "dreamcast" || v == "naomi" || v == "atomiswave" ||
      v == "flycast")
    return GamePlatform::DC;
  if (v == "3ds" || v == "n3ds" || v == "citra" || v == "azahar" || v == "azaharplus")
    return GamePlatform::N3DS;
  if (v == "ngcwii" || v == "ngc" || v == "wii" || v == "gamecube" || v == "gc" ||
      v == "dolphin" || v == "ishiruka" || v == "ishiiruka")
    return GamePlatform::NGCWII;
  if (v == "wiiu" || v == "wiiued" || v == "cemu" || v == "cemuu")
    return GamePlatform::WIIU;
  if (v == "psp" || v == "ppsspp" || v == "playstationportable" || v == "psportable")
    return GamePlatform::PSP;
  if (v == "switch" || v == "nswitch" || v == "ns" || v == "eden" || v == "yuzu")
    return GamePlatform::NSWITCH;
  if (v == "java" || v == "j2me" || v == "midlet" || v == "squirreljme")
    return GamePlatform::JAVA;
  return GamePlatform::NES;
}

std::vector<std::string> platformExtensions(GamePlatform p) {
  switch (p) {
    case GamePlatform::NES: return {"nes", "unf", "unif", "fds", "nez", "unh"};
    case GamePlatform::SFC: return {"smc", "sfc", "swc", "fig", "bs"};
    case GamePlatform::GB: return {"gb", "sgb", "gbc"};
    case GamePlatform::GBA: return {"gba"};
    case GamePlatform::DOS:
      return {"bat", "exe", "com", "dosz", "conf", "iso", "cue", "img", "ima", "vhd", "hd"};
    case GamePlatform::ARCADE: return {"zip", "7z"};
    case GamePlatform::MD:
      return {"md", "smd", "gen", "sms", "gg", "sg", "68k", "chd", "cue"};
    case GamePlatform::PCE: return {"pce", "sgx", "hes", "cue", "chd"};
    case GamePlatform::NDS: return {"nds", "ids", "srl", "dsi"};
    case GamePlatform::PSX: return {"cue", "bin", "pbp", "chd", "m3u", "ecm", "mds", "mdf"};
    case GamePlatform::PS2: return {"iso", "cso", "zso", "isz", "elf"};
    case GamePlatform::JAVA: return {"jar", "jad"};
    case GamePlatform::DC: return {"gdi", "cdi", "chd", "cue", "lst", "iso", "zip"};
    case GamePlatform::N3DS: return {"3ds", "cci", "cxi", "cia"};
    case GamePlatform::NGCWII:
      return {"gcm", "rvz", "wbfs", "gcz", "ciso", "nkit", "wad", "dol", "elf", "iso"};
    case GamePlatform::WIIU: return {"wud", "wux", "wua", "wuhb", "rpx", "iso", "elf"};
    case GamePlatform::PSP: return {"iso", "cso", "pbp", "chd", "elf"};
    case GamePlatform::NSWITCH: return {"xci", "nsp", "nca", "nro"};
  }
  return {};
}

const char* platformThumbnailSystem(GamePlatform p) {
  switch (p) {
    case GamePlatform::NES: return "Nintendo - Nintendo Entertainment System";
    case GamePlatform::SFC: return "Nintendo - Super Nintendo Entertainment System";
    case GamePlatform::GB: return "Nintendo - Game Boy";
    case GamePlatform::GBA: return "Nintendo - Game Boy Advance";
    case GamePlatform::MD: return "Sega - Mega Drive - Genesis";
    case GamePlatform::PCE: return "NEC - PC Engine - TurboGrafx 16";
    case GamePlatform::PSX: return "Sony - PlayStation";
    case GamePlatform::PS2: return "Sony - PlayStation 2";
    case GamePlatform::NDS: return "Nintendo - Nintendo DS";
    case GamePlatform::N3DS: return "Nintendo - Nintendo 3DS";
    case GamePlatform::NGCWII: return "Nintendo - GameCube";
    case GamePlatform::DC: return "Sega - Dreamcast";
    case GamePlatform::ARCADE: return "FBNeo - Arcade Games";
    case GamePlatform::PSP: return "Sony - PlayStation Portable";
    case GamePlatform::WIIU: return "Nintendo - Wii U";
    case GamePlatform::NSWITCH: return "Nintendo - Nintendo Switch";
    case GamePlatform::DOS: return nullptr;
    case GamePlatform::JAVA: return nullptr;
  }
  return nullptr;
}

bool platformIsDualScreen(GamePlatform p) {
  return p == GamePlatform::NDS || p == GamePlatform::N3DS;
}

uint32_t platformBadgeColor(GamePlatform p) { return platformAccent(p); }

}  // namespace ns
