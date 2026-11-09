#pragma once
// ============================================================================
// NesStation PC — 随包核心支撑文件播种（BIOS / 密钥 / 系统档案）
//
// 对齐 Android 版 NesApp 的 ensureFdsBios / ensureFbNeoBios /
// ensureGenesisBios / ensurePceBios / ensureDcBios / ensurePs2Bios /
// ensureAzaharKeys + AzaharSystemData / PspSettingsStore 系列：
// 把随包 assets/system/ 下的核心支撑文件在首次启动时安装到数据目录，
// 目标已存在且非空 → 保留（用户自行导入的文件永远优先）。
//
// 随包载荷（assets/system/，与 APK assets 同源）与安装目标：
//   disksys.rom                    → <system>/disksys.rom      （FCEUmm FDS）
//   fbneo/*.zip                    → <system>/fbneo/           （FBNeo 街机 BIOS：
//                                                                  neogeo/pgm/neocdz）
//   genesis/bios_CD_*.{zip,bin}    → <system>/                 （Mega-CD BIOS；
//                                                                  .bin 已预解压，
//                                                                  genplus 直接可用）
//   pce/*.pce                      → <system>/                 （PCE-CD System Card）
//   dc/*（BIOS + VMU）             → <system>/dc/ + <saves>/   （Flycast；VMU 模板
//                                                                  放存档目录）
//   pcsx2/bios/*                   → <system>/pcsx2/bios/      （LRPS2/PCSX2）
//   azahar/（sysdata+nand）        → <saves>/Azahar/           （Azahar libretro
//                                                                  用户目录 = 存档
//                                                                  目录/Azahar，见
//                                                                  azahar core_settings.cpp
//                                                                  ParseStorageOptions）
//   PPSSPP/flash0/font/*.pgf       → <system>/PPSSPP/flash0/   （PPSSPP 原版字体）
//
// 载荷来源根（按优先级）：
//   1. NESSTATION_SYSTEM_ASSETS 环境变量（测试/自定义包）
//   2. exe 同目录 assets/system/（官方 zip / tar.gz / deb /opt/nesstation 布局）
// ============================================================================
#include <string>

namespace ns::sysassets {

// 播种入口（App 启动时调用一次；幂等，全为存在性检查 + 缺失补拷）。
// systemDir = CoreManager 的系统目录；savesDir = CoreManager 的存档目录。
void seedSystemAssets(const std::string& systemDir, const std::string& savesDir);

}  // namespace ns::sysassets
