package com.nesstation.app.core.coremgmt

import com.nesstation.app.core.model.GamePlatform
import com.nesstation.app.core.storage.Paths
import com.nesstation.app.core.storage.SettingsStore
import java.io.File

/**
 * ★ PC 核心注册表 ★
 *
 * 全部核心替换为 PC 端移植（GitHub 均可找到），分两类：
 *  - [LibretroCoreDef]：进程内 libretro 核心，libretro buildbot 官方 PC 构建，
 *    统一由 LibretroHost（JNA）加载，游戏内体验与 Android 版完全一致。
 *  - [ExternalCoreDef]：外置 PC 独立核心，NesStation 统一检测 / 下载 / 启动，
 *    使用其原生 GPU 全速渲染（PC 平台的标准做法）。
 *
 * ★ 已移除（无 PC 版）：NDS「激烈 DraStic」（Android 专有商业模拟器）、
 *   3DS「Citra MMJ」（Android 专有社区分支）。★
 */

data class LibretroCoreDef(
    val id: String,                 // "fceumm"
    val name: String,               // "FCEUmm"
    val platform: GamePlatform,
    val sourceRepo: String,         // 源码仓库（README/核心管理页展示）
    val biosNote: String = "",      // BIOS 说明
    val experimental: Boolean = false
) {
    /** buildbot 文件名（按操作系统/架构）。 */
    fun fileName(os: String = currentOs(), arch: String = currentArch()): String? {
        val core = when (os) {
            "windows" -> "$id.dll"
            "linux" -> "${id}_libretro.so"
            "macos" -> "$id.dylib"
            else -> return null
        }
        return core
    }

    fun buildbotUrl(os: String = currentOs(), arch: String = currentArch()): String? {
        val base = when (os) {
            "windows" -> "https://buildbot.libretro.com/nightly/windows/x86_64/latest"
            "linux" ->
                if (arch == "aarch64")
                    "https://buildbot.libretro.com/nightly/linux/arm64/latest"
                else
                    "https://buildbot.libretro.com/nightly/linux/x86_64/latest"
            "macos" ->
                if (arch == "aarch64")
                    "https://buildbot.libretro.com/nightly/apple/osx/arm64/latest"
                else
                    "https://buildbot.libretro.com/nightly/apple/osx/x86_64/latest"
            else -> null
        } ?: return null
        val name = fileName(os, arch) ?: return null
        return "$base/$name.zip"
    }

    fun installedFile(os: String = currentOs()): File? {
        val name = fileName(os) ?: return null
        return File(Paths.libretroCoresDir, name)
    }

    val installed: Boolean get() = installedFile()?.isFile == true
}

data class ExternalCoreDef(
    val id: String,                 // "pcsx2"
    val name: String,               // "PCSX2"
    val platform: GamePlatform,
    val sourceRepo: String,         // GitHub 源码/发布页
    val downloadPage: String,       // 官方下载页
    /** GitHub Releases 自动下载（api:repo → 匹配资产名，含 {os} 占位）。 */
    val githubApi: String? = null,
    val assetPatternWin: String? = null,
    val assetPatternLinux: String? = null,
    val assetPatternMac: String? = null,
    /** Windows exe 候选名（自动检测安装）。 */
    val exeNamesWin: List<String> = emptyList(),
    val exeNamesLinux: List<String> = emptyList(),
    val exeNamesMac: List<String> = emptyList(),
    /** 启动参数模板，{rom} 为游戏路径占位。 */
    val argTemplate: String = "{rom}",
    /** 常见安装目录（自动检测，%为用户主目录）。 */
    val commonDirs: List<String> = emptyList(),
    val notes: String = ""
) {
    fun exeNames(os: String = currentOs()): List<String> = when (os) {
        "windows" -> exeNamesWin
        "linux" -> exeNamesLinux
        "macos" -> exeNamesMac
        else -> emptyList()
    }

    /** 核心安装目录（cores/external/<id>）。 */
    fun installDir(): File = File(Paths.externalCoresDir, id)

    /** 解析可执行文件：设置覆盖 → 安装目录 → 常见安装位置。 */
    fun resolveExecutable(): File? {
        // 1) 用户显式设置
        SettingsStore.load().externalCorePaths[id]?.let { p ->
            val f = File(p)
            if (f.isFile) return f
        }
        // 2) NesStation 管理的安装目录
        for (name in exeNames()) {
            val f = File(installDir(), name)
            if (f.isFile) return f
            // 有些发行包带一层子目录
            installDir().listFiles()?.filter { it.isDirectory }?.forEach { sub ->
                val f2 = File(sub, name)
                if (f2.isFile) return f2
            }
        }
        // 3) 常见安装位置
        val home = System.getProperty("user.home")
        for (dir in commonDirs) {
            val d = File(dir.replace("%HOME%", home).replace("~", home))
            if (!d.isDirectory) continue
            for (name in exeNames()) {
                val f = File(d, name)
                if (f.isFile) return f
                // macOS .app 包内
                if (name.endsWith(".app")) {
                    val inner = File(d, "$name/Contents/MacOS/${name.removeSuffix(".app")}")
                    if (inner.isFile) return inner
                }
                d.listFiles()?.filter { it.isDirectory }?.forEach { sub ->
                    val f2 = File(sub, name)
                    if (f2.isFile) return f2
                    if (name.endsWith(".app")) {
                        val inner2 = File(sub, "$name/Contents/MacOS/${name.removeSuffix(".app")}")
                        if (inner2.isFile) return inner2
                    }
                }
            }
        }
        return null
    }

    val installed: Boolean get() = resolveExecutable() != null
}

fun currentOs(): String {
    val os = System.getProperty("os.name").lowercase()
    return when {
        os.contains("win") -> "windows"
        os.contains("mac") || os.contains("darwin") -> "macos"
        else -> "linux"
    }
}

fun currentArch(): String =
    if (System.getProperty("os.arch").lowercase().contains("aarch64") ||
        System.getProperty("os.arch").lowercase().contains("arm")) "aarch64" else "x86_64"

object CoreRegistry {

    // ============================================================
    // 进程内 libretro 核心（buildbot 官方 PC 构建）
    // ============================================================

    val LIBRETRO: List<LibretroCoreDef> = listOf(
        LibretroCoreDef(
            id = "fceumm", name = "FCEUmm", platform = GamePlatform.NES,
            sourceRepo = "https://github.com/libretro/libretro-fceumm",
            biosNote = "FDS 磁碟游戏需要 disksys.rom（放 system/fceumm/）"
        ),
        LibretroCoreDef(
            id = "snes9x", name = "snes9x", platform = GamePlatform.SFC,
            sourceRepo = "https://github.com/libretro/snes9x"
        ),
        LibretroCoreDef(
            id = "mgba", name = "mGBA", platform = GamePlatform.GB,
            sourceRepo = "https://github.com/libretro/mgba"
        ),
        LibretroCoreDef(
            id = "mgba", name = "mGBA", platform = GamePlatform.GBA,
            sourceRepo = "https://github.com/libretro/mgba"
        ),
        LibretroCoreDef(
            id = "genesis_plus_gx", name = "Genesis Plus GX", platform = GamePlatform.MD,
            sourceRepo = "https://github.com/libretro/Genesis-Plus-GX",
            biosNote = "Mega-CD 需要 bios_CD_U/E/J.bin（放 system/genesis_plus_gx/）"
        ),
        LibretroCoreDef(
            id = "geargrafx", name = "Geargrafx", platform = GamePlatform.PCE,
            sourceRepo = "https://github.com/libretro/beetle-geargrafx",
            biosNote = "PCE-CD 需要 syscard1/2/3.pce（放 system/geargrafx/）"
        ),
        LibretroCoreDef(
            id = "dosbox_pure", name = "DOSBox-Pure", platform = GamePlatform.DOS,
            sourceRepo = "https://github.com/Schellingb/dosbox-pure"
        ),
        LibretroCoreDef(
            id = "fbneo", name = "FBNeo", platform = GamePlatform.ARCADE,
            sourceRepo = "https://github.com/libretro/FBNeo",
            biosNote = "NeoGeo 需要 neogeo.zip；CPS3 需要 qsound.zip（放 system/fbneo/）"
        ),
        LibretroCoreDef(
            id = "melonds", name = "melonDS", platform = GamePlatform.NDS,
            sourceRepo = "https://github.com/libretro/melonDS",
            biosNote = "内置 FreeBIOS；DSi 游戏需要 bios7/bios9/firmware.bin"
        ),
        LibretroCoreDef(
            id = "pcsx_rearmed", name = "PCSX-ReARMed", platform = GamePlatform.PSX,
            sourceRepo = "https://github.com/libretro/pcsx_rearmed",
            biosNote = "推荐放 SCPH1001.BIN 等正版 BIOS（system/pcsx_rearmed/）"
        ),
        // ★ 实验性：PS2 进程内核心（软件渲染，速度受限；推荐外置 PCSX2）
        LibretroCoreDef(
            id = "pcsx2", name = "PCSX2 (libretro)", platform = GamePlatform.PS2,
            sourceRepo = "https://github.com/PCSX2/pcsx2",
            biosNote = "需要正版 PS2 BIOS（system/pcsx2/bios/）",
            experimental = true
        ),
        // ★ 实验性：PSP 进程内核心（软件渲染，速度受限；推荐外置 PPSSPP）
        LibretroCoreDef(
            id = "ppsspp", name = "PPSSPP (libretro)", platform = GamePlatform.PSP,
            sourceRepo = "https://github.com/hrydgard/ppsspp",
            experimental = true
        )
    )

    // ============================================================
    // 外置 PC 独立核心（GitHub 发布的 PC 版）
    // ============================================================

    val EXTERNAL: List<ExternalCoreDef> = listOf(
        ExternalCoreDef(
            id = "pcsx2", name = "PCSX2", platform = GamePlatform.PS2,
            sourceRepo = "https://github.com/PCSX2/pcsx2",
            downloadPage = "https://pcsx2.net/downloads",
            githubApi = "https://api.github.com/repos/PCSX2/pcsx2/releases/latest",
            assetPatternWin = "pcsx2-v*-windows-x64-qt.7z|pcsx2-v*-windows-x64.7z|pcsx2-*win*x64*.zip",
            assetPatternLinux = "pcsx2-v*-linux-x64-qt.AppImage|pcsx2-*linux*x64*",
            assetPatternMac = "pcsx2-v*-macos-qt.dmg|pcsx2-*macos*",
            exeNamesWin = listOf("pcsx2-qtx64.exe", "pcsx2-qx64.exe", "pcsx2.exe"),
            exeNamesLinux = listOf("pcsx2-qt", "pcsx2", "PCSX2"),
            exeNamesMac = listOf("PCSX2.app"),
            argTemplate = "--fullscreen {rom}",
            commonDirs = listOf(
                "%HOME%/AppData/Local/Programs/PCSX2", "%HOME%/scoop/apps/pcsx2",
                "C:/Program Files/PCSX2", "/usr/bin", "/usr/local/bin", "%HOME%/Applications",
                "/Applications"
            ),
            notes = "PS2 推荐：原生 GPU 全速渲染。PS2 BIOS 放核心自身 bios 目录。"
        ),
        ExternalCoreDef(
            id = "ppsspp", name = "PPSSPP", platform = GamePlatform.PSP,
            sourceRepo = "https://github.com/hrydgard/ppsspp",
            downloadPage = "https://www.ppsspp.org/download",
            githubApi = "https://api.github.com/repos/hrydgard/ppsspp/releases/latest",
            assetPatternWin = "PPSSPPWindows*.zip|PPSSPPWindowsSetup.exe",
            assetPatternLinux = "ppsspp-*.AppImage|ppsspp*.deb",
            assetPatternMac = "PPSSPPSDL*.dmg|PPSSPP*.dmg",
            exeNamesWin = listOf("PPSSPPWindows64.exe", "PPSSPPWindows.exe"),
            exeNamesLinux = listOf("PPSSPPSDL", "ppsspp", "ppsspp-qt"),
            exeNamesMac = listOf("PPSSPP.app"),
            argTemplate = "--fullscreen {rom}",
            commonDirs = listOf(
                "C:/Program Files/PPSSPP", "%HOME%/AppData/Local/Programs/PPSSPP",
                "/usr/bin", "/usr/local/bin", "/Applications", "%HOME%/Applications"
            ),
            notes = "PSP 推荐：原生 GPU 全速渲染，自带加速即时存档。"
        ),
        ExternalCoreDef(
            id = "flycast", name = "Flycast", platform = GamePlatform.DC,
            sourceRepo = "https://github.com/flycast/flycast",
            downloadPage = "https://flycast.github.io",
            githubApi = "https://api.github.com/repos/flycast/flycast/releases/latest",
            assetPatternWin = "flycast-win64*.zip|flycast-win32*.zip",
            assetPatternLinux = "flycast-linux*.AppImage|flycast-*.AppImage",
            assetPatternMac = "flycast-macos*.dmg|flycast*.dmg",
            exeNamesWin = listOf("flycast.exe"),
            exeNamesLinux = listOf("flycast", "flycast.elf"),
            exeNamesMac = listOf("Flycast.app"),
            argTemplate = "{rom}",
            commonDirs = listOf(
                "C:/Program Files/Flycast", "%HOME%/AppData/Local/Programs/flycast",
                "/usr/bin", "/usr/local/bin", "/Applications", "%HOME%/Applications"
            ),
            notes = "DC/NAOMI/AtomisWave。光盘游戏需要 dc_boot.bin + dc_flash.bin（放核心数据目录）。"
        ),
        ExternalCoreDef(
            id = "azahar", name = "Azahar", platform = GamePlatform.N3DS,
            sourceRepo = "https://github.com/azahar-emu/azahar",
            downloadPage = "https://azahar-emu.org/pages/download.html",
            githubApi = "https://api.github.com/repos/azahar-emu/azahar/releases/latest",
            assetPatternWin = "azahar-*-windows-x86_64.7z|azahar-*-windows-msvc.7z|azahar-*win*.zip",
            assetPatternLinux = "azahar-*-linux-x86_64.tar.xz|azahar-*linux*.tar.*",
            assetPatternMac = "azahar-*-macos-universal.dmg|azahar-*macos*.dmg",
            exeNamesWin = listOf("azahar-qt.exe", "azahar.exe"),
            exeNamesLinux = listOf("azahar", "azahar-qt"),
            exeNamesMac = listOf("Azahar.app"),
            argTemplate = "-g {rom}",
            commonDirs = listOf(
                "%HOME%/AppData/Local/azahar", "C:/Program Files/Azahar",
                "/usr/bin", "/usr/local/bin", "/Applications", "%HOME%/Applications"
            ),
            notes = "3DS（Citra 系官方延续版）。加密游戏需 aes_keys.txt；部分游戏需系统档案。"
        ),
        ExternalCoreDef(
            id = "ishiiruka", name = "Ishiiruka（Dolphin 系）", platform = GamePlatform.NGCWII,
            sourceRepo = "https://github.com/Tinob/Ishiiruka",
            downloadPage = "https://github.com/Tinob/Ishiiruka/releases",
            exeNamesWin = listOf("Ishiiruka.exe", "Dolphin.exe", "Dolphin-x64.exe"),
            exeNamesLinux = listOf("dolphin-emu", "ishiiruka"),
            exeNamesMac = listOf("Dolphin.app", "Ishiiruka.app"),
            argTemplate = "-e {rom}",
            commonDirs = listOf(
                "C:/Program Files/Dolphin-Ishiiruka", "C:/Program Files/Dolphin",
                "%HOME%/AppData/Local/Programs/Dolphin",
                "/usr/bin", "/usr/local/bin", "/Applications", "%HOME%/Applications"
            ),
            notes = "GC/Wii（Dolphin 优化分支，PC 版原生构建；标准 Dolphin 同参数兼容）。"
        ),
        ExternalCoreDef(
            id = "cemu", name = "Cemu", platform = GamePlatform.WIIU,
            sourceRepo = "https://github.com/cemu-project/Cemu",
            downloadPage = "https://cemu.info",
            githubApi = "https://api.github.com/repos/cemu-project/Cemu/releases/latest",
            assetPatternWin = "cemu-*-windows-x64.zip|cemu_*_win64.zip",
            assetPatternLinux = "cemu-*-ubuntu-24.04-x86_64.AppImage|cemu-*.AppImage",
            assetPatternMac = "cemu-*-macos-x86_64.dmg|cemu-*macos*",
            exeNamesWin = listOf("Cemu.exe"),
            exeNamesLinux = listOf("Cemu", "cemu"),
            exeNamesMac = listOf("Cemu.app"),
            argTemplate = "-g {rom} -f",
            commonDirs = listOf(
                "C:/Program Files/Cemu", "%HOME%/AppData/Local/Programs/Cemu",
                "/opt/cemu", "%HOME%/Applications", "/Applications"
            ),
            notes = "Wii U（官方开源 PC 版，Vulkan 全速）。WUD 解密需 otp.bin。"
        ),
        ExternalCoreDef(
            id = "eden", name = "Eden", platform = GamePlatform.NSWITCH,
            sourceRepo = "https://github.com/eden-emulator/mirror",
            downloadPage = "https://eden-emu.dev",
            exeNamesWin = listOf("eden.exe", "Eden.exe"),
            exeNamesLinux = listOf("eden", "Eden"),
            exeNamesMac = listOf("Eden.app"),
            argTemplate = "{rom}",
            commonDirs = listOf(
                "%HOME%/AppData/Local/eden", "C:/Program Files/Eden",
                "/usr/bin", "/usr/local/bin", "/opt/eden", "/Applications", "%HOME%/Applications"
            ),
            notes = "Switch（yuzu 系社区延续版）。加密游戏需 prod.keys（核心设置内导入）。"
        ),
        ExternalCoreDef(
            id = "freej2me", name = "FreeJ2ME", platform = GamePlatform.JAVA,
            sourceRepo = "https://github.com/hex007/FreeJ2ME",
            downloadPage = "https://github.com/hex007/FreeJ2ME/releases",
            exeNamesWin = listOf("FreeJ2ME.jar"),
            exeNamesLinux = listOf("FreeJ2ME.jar"),
            exeNamesMac = listOf("FreeJ2ME.jar"),
            argTemplate = "{rom}",
            commonDirs = emptyList(),
            notes = "J2ME（Java ME）PC 端开源模拟器，直接用 NesStation 自带的 Java 运行时启动。"
        )
    )

    // ============================================================
    // 平台 → 核心组合
    // ============================================================

    fun libretroFor(platform: GamePlatform): List<LibretroCoreDef> =
        LIBRETRO.filter { it.platform == platform }

    fun externalFor(platform: GamePlatform): List<ExternalCoreDef> =
        EXTERNAL.filter { it.platform == platform }

    /** 平台默认首选核心 id（设置里可改；双核心平台启动时也会弹选择器）。 */
    fun defaultCoreId(platform: GamePlatform): String {
        SettingsStore.load().defaultCores[platform.name]?.let { return it }
        return when (platform) {
            GamePlatform.PS2 -> "pcsx2-ext"
            GamePlatform.PSP -> "ppsspp-ext"
            else -> libretroFor(platform).firstOrNull()?.id
                ?: externalFor(platform).firstOrNull()?.let { it.id + "-ext" }
                ?: "none"
        }
    }

    fun findLibretro(id: String): LibretroCoreDef? = LIBRETRO.firstOrNull { it.id == id }
    fun findExternal(id: String): ExternalCoreDef? = EXTERNAL.firstOrNull { it.id == id }
}
