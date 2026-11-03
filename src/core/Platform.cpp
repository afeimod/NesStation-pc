#include "core/Platform.h"

#include <algorithm>
#include <cctype>
#include <unordered_map>

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

}  // namespace

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
      return {CoreKind::Libretro, "fceumm_libretro", nullptr, "FCEUmm",
              "FDS 游戏需要 disksys.rom（放入 system 目录）"};
    case GamePlatform::SFC:
      return {CoreKind::Libretro, "snes9x_libretro", nullptr, "snes9x", "无需 BIOS"};
    case GamePlatform::GB:
      return {CoreKind::Libretro, "mgba_libretro", nullptr, "mGBA", "无需 BIOS"};
    case GamePlatform::GBA:
      return {CoreKind::Libretro, "mgba_libretro", nullptr, "mGBA", "无需 BIOS"};
    case GamePlatform::DOS:
      return {CoreKind::Libretro, "dosbox_pure_libretro", nullptr, "DOSBox-Pure",
              "无需 BIOS"};
    case GamePlatform::ARCADE:
      return {CoreKind::Libretro, "fbneo_libretro", nullptr, "FBNeo",
              "NeoGeo → neogeo.zip，PGM → pgm.zip（放 system/fbneo/）"};
    case GamePlatform::MD:
      return {CoreKind::Libretro, "genesis_plus_gx_libretro", nullptr,
              "Genesis-Plus-GX", "Mega-CD 需要 bios_CD_E/J/U.zip"};
    case GamePlatform::PCE:
      return {CoreKind::Libretro, "geargrafx_libretro", nullptr, "Geargrafx",
              "PCE-CD 需要 syscard3.pce（System Card）"};
    case GamePlatform::NDS:
      return {CoreKind::Libretro, "melonds_libretro", nullptr, "melonDS",
              "无需 BIOS（内置 FreeBIOS）"};
    case GamePlatform::PSX:
      return {CoreKind::Libretro, "pcsx_rearmed_libretro", nullptr, "PCSX-ReARMed",
              "无需 BIOS（HLE）；部分游戏推荐 BIOS"};
    case GamePlatform::PS2:
      return {CoreKind::Libretro, "pcsx2_libretro", nullptr, "PCSX2",
              "无需 BIOS（内置）；推荐放 system/pcsx2/bios"};
    case GamePlatform::JAVA:
      return {CoreKind::Libretro, "squirreljme_libretro", nullptr, "SquirrelJME",
              "无需 BIOS"};
    case GamePlatform::DC:
      return {CoreKind::Libretro, "flycast_libretro", nullptr, "Flycast",
              "dc_boot.bin + dc_flash.bin（NAOMI 无需）"};
    case GamePlatform::N3DS:
      return {CoreKind::Libretro, "azahar_libretro", nullptr, "Azahar",
              "加密游戏需 aes_keys.txt；部分游戏需系统档案"};
    case GamePlatform::NGCWII:
      return {CoreKind::Standalone, nullptr, "dolphin", "Dolphin",
              "大部分游戏免 BIOS（DSP LLE 需 DSP ROM）"};
    case GamePlatform::WIIU:
      return {CoreKind::Standalone, nullptr, "cemu", "Cemu",
              "免 BIOS（WUD 解密需 otp.bin）"};
    case GamePlatform::PSP:
      return {CoreKind::Libretro, "ppsspp_libretro", nullptr, "PPSSPP", "无需 BIOS"};
    case GamePlatform::NSWITCH:
      return {CoreKind::Standalone, nullptr, "eden", "Eden",
              "加密游戏需 prod.keys；固件 zip 在模拟器内安装"};
  }
  return {CoreKind::Libretro, "fceumm_libretro", nullptr, "FCEUmm", ""};
}

// —— 核心候选链 ————————————————————————————————————————————
// 候选均经 libretro buildbot 实测（linux/x86_64、linux/aarch64、
// windows/x86_64 三个平台），缺失平台由链上后续核心自动补位：
//   azahar 在 linux/aarch64 无预编译包 → Citra 顶替
std::vector<PlatformCore> platformCoreChain(GamePlatform p) {
  switch (p) {
    case GamePlatform::NES:
      return {
          {CoreKind::Libretro, "fceumm_libretro", nullptr, "FCEUmm",
           "FDS 游戏需要 disksys.rom（放入 system 目录）"},
          {CoreKind::Libretro, "nestopia_libretro", nullptr, "Nestopia",
           "FDS 游戏需要 disksys.rom"},
          {CoreKind::Libretro, "quicknes_libretro", nullptr, "QuickNES",
           "无需 BIOS（精度较低）"},
      };
    case GamePlatform::SFC:
      return {
          {CoreKind::Libretro, "snes9x_libretro", nullptr, "snes9x", "无需 BIOS"},
          {CoreKind::Libretro, "bsnes_libretro", nullptr, "bsnes", "无需 BIOS"},
      };
    case GamePlatform::GB:
      return {
          {CoreKind::Libretro, "mgba_libretro", nullptr, "mGBA", "无需 BIOS"},
          {CoreKind::Libretro, "gambatte_libretro", nullptr, "Gambatte",
           "无需 BIOS"},
      };
    case GamePlatform::GBA:
      return {
          {CoreKind::Libretro, "mgba_libretro", nullptr, "mGBA", "无需 BIOS"},
          {CoreKind::Libretro, "vba_next_libretro", nullptr, "VBA-M", "无需 BIOS"},
          {CoreKind::Libretro, "gpsp_libretro", nullptr, "gpSP",
           "GBA BIOS 可选（gba_bios.bin）"},
      };
    case GamePlatform::DOS:
      return {
          {CoreKind::Libretro, "dosbox_pure_libretro", nullptr, "DOSBox-Pure",
           "无需 BIOS"},
          {CoreKind::Libretro, "dosbox_svn_libretro", nullptr, "DOSBox-SVN",
           "无需 BIOS"},
      };
    case GamePlatform::ARCADE:
      return {
          {CoreKind::Libretro, "fbneo_libretro", nullptr, "FBNeo",
           "NeoGeo → neogeo.zip，PGM → pgm.zip（放 system/fbneo/）"},
          {CoreKind::Libretro, "mame2003_plus_libretro", nullptr, "MAME 2003-Plus",
           "NeoGeo → neogeo.zip"},
      };
    case GamePlatform::MD:
      return {
          {CoreKind::Libretro, "genesis_plus_gx_libretro", nullptr,
           "Genesis-Plus-GX", "Mega-CD 需要 bios_CD_E/J/U.zip"},
          {CoreKind::Libretro, "picodrive_libretro", nullptr, "PicoDrive",
           "32X 支持；Mega-CD 需要 BIOS"},
      };
    case GamePlatform::PCE:
      return {
          {CoreKind::Libretro, "geargrafx_libretro", nullptr, "Geargrafx",
           "PCE-CD 需要 syscard3.pce（System Card）"},
          {CoreKind::Libretro, "mednafen_pce_fast_libretro", nullptr,
           "Mednafen PCE Fast", "PCE-CD 需要 syscard3.pce"},
      };
    case GamePlatform::NDS:
      return {
          {CoreKind::Libretro, "melonds_libretro", nullptr, "melonDS",
           "无需 BIOS（内置 FreeBIOS）"},
          {CoreKind::Libretro, "desmume_libretro", nullptr, "DeSmuME",
           "无需 BIOS"},
      };
    case GamePlatform::PSX:
      return {
          {CoreKind::Libretro, "pcsx_rearmed_libretro", nullptr, "PCSX-ReARMed",
           "无需 BIOS（HLE）；部分游戏推荐 BIOS"},
          {CoreKind::Libretro, "swanstation_libretro", nullptr, "SwanStation",
           "无需 BIOS（HLE）；要求更高精度时使用"},
      };
    case GamePlatform::PS2:
      return {
          {CoreKind::Libretro, "pcsx2_libretro", nullptr, "PCSX2",
           "无需 BIOS（内置）；推荐放 system/pcsx2/bios"},
      };
    case GamePlatform::JAVA:
      return {
          {CoreKind::Libretro, "squirreljme_libretro", nullptr, "SquirrelJME",
           "无需 BIOS"},
      };
    case GamePlatform::DC:
      return {
          {CoreKind::Libretro, "flycast_libretro", nullptr, "Flycast",
           "dc_boot.bin + dc_flash.bin（NAOMI 无需）"},
      };
    case GamePlatform::N3DS:
      return {
          {CoreKind::Libretro, "azahar_libretro", nullptr, "Azahar",
           "加密游戏需 aes_keys.txt；部分游戏需系统档案"},
          {CoreKind::Libretro, "citra_libretro", nullptr, "Citra",
           "加密游戏需 aes_keys.txt；部分游戏需系统档案"},
      };
    case GamePlatform::NGCWII:
      return {{CoreKind::Standalone, nullptr, "dolphin", "Dolphin",
               "大部分游戏免 BIOS（DSP LLE 需 DSP ROM）"}};
    case GamePlatform::WIIU:
      return {{CoreKind::Standalone, nullptr, "cemu", "Cemu",
               "免 BIOS（WUD 解密需 otp.bin）"}};
    case GamePlatform::PSP:
      return {
          {CoreKind::Libretro, "ppsspp_libretro", nullptr, "PPSSPP", "无需 BIOS"},
      };
    case GamePlatform::NSWITCH:
      return {{CoreKind::Standalone, nullptr, "eden", "Eden",
               "加密游戏需 prod.keys；固件 zip 在模拟器内安装"}};
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
