// NesStation PC — 平台域模型（完整移植自 Android 版 GameEntry.kt，
// 并按 PC 核心矩阵替换全部核心；DraStic（激烈）与 Citra MMJ 无 PC 版，已移除）
#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace ns {

enum class GamePlatform : int {
  NES,     // NES / FC          — FCEUmm（libretro PC）
  SFC,     // SNES / SFC        — snes9x（libretro PC）
  GB,      // GB / GBC          — mGBA（libretro PC）
  GBA,     // GBA               — mGBA（libretro PC）
  DOS,     // MS-DOS            — DOSBox-Pure（libretro PC）
  ARCADE,  // 街机              — FBNeo（libretro PC）
  MD,      // MD/SMS/GG/SG/CD   — Genesis-Plus-GX（libretro PC）
  PCE,     // PCE / TG16        — Geargrafx（libretro PC）
  NDS,     // NDS / DSi         — melonDS（libretro PC）
  PSX,     // PlayStation 1     — PCSX-ReARMed（libretro PC）
  PS2,     // PlayStation 2     — PCSX2（libretro PC）
  JAVA,    // Java ME           — SquirrelJME（libretro PC）
  DC,      // Dreamcast/NAOMI   — Flycast（libretro PC）
  N3DS,    // Nintendo 3DS      — Azahar（官方 libretro PC 核心）
  NGCWII,  // GameCube / Wii    — Dolphin（源码集成，独立模拟器形态）
  WIIU,    // Wii U             — Cemu（源码集成，独立模拟器形态）
  PSP,     // PSP               — PPSSPP（libretro PC）
  NSWITCH, // Switch            — Eden（源码集成，独立模拟器形态）
};

using PlatformList = std::vector<GamePlatform>;

// 有序全平台（与原项目 GamePlatform 枚举顺序一致）
const PlatformList& allPlatforms();

const char* platformShortName(GamePlatform p);   // "NES" "GB/GBC" "NGC/Wii" ...
const char* platformFullName(GamePlatform p);    // "Nintendo Entertainment System"
const char* platformEra(GamePlatform p);         // "1983 · 8-BIT"
uint32_t platformAccent(GamePlatform p);         // 0xRRGGBB 主题色（沿用原配色语言）

// —— PC 核心矩阵 ——————————————————————————————————————————————
// kind = Libretro（进程内 .dll/.so）| Standalone（源码集成的独立模拟器）
enum class CoreKind { Libretro, Standalone };

struct PlatformCore {
  CoreKind kind;
  const char* coreFile;     // libretro: "fceumm_libretro"（运行时按平台拼扩展名）
  const char* emulatorKey;  // Standalone: "dolphin" | "cemu" | "eden"
  const char* coreTitle;    // "FCEUmm"
  const char* biosNote;     // BIOS 提示（照搬原 README 表）
};

PlatformCore platformCore(GamePlatform p);

// 平台核心候选链（按优先级）：首选在前，替代核心在后。
// 例如 3DS：Azahar → Citra（部分架构 buildbot 不提供 azahar 预编译包）。
// 运行时取链上第一个已安装的核心；链为空表示该平台无 libretro 核心
// （Standalone 平台）。
std::vector<PlatformCore> platformCoreChain(GamePlatform p);

// ROM 扩展名 → 平台（完整移植原 fromExtension；.zip/.bin 故意不映射，
// 与原逻辑一致：由导入时的平台页提示消歧）
GamePlatform platformFromExtension(const std::string& ext);

// 字符串 → 平台（原 fromString 的别名表全量移植）
GamePlatform platformFromString(const std::string& value);

// 平台接受的扩展名集合（导入页提示用）
std::vector<std::string> platformExtensions(GamePlatform p);

// libretro 官方缩略图库（thumbnails.libretro.com）的系统目录名，
// 供封面自动获取；无缩略图的平台返回 nullptr
const char* platformThumbnailSystem(GamePlatform p);

// NDS / 3DS 双屏平台
bool platformIsDualScreen(GamePlatform p);

// 平台标题色（FSD/Neon 徽章）
uint32_t platformBadgeColor(GamePlatform p);

}  // namespace ns
