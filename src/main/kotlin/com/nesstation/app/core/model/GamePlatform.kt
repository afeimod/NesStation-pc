package com.nesstation.app.core.model

/**
 * 游戏平台类型（PC 版）。
 *
 * 与 Android 版 NesStation 一一对应；核心全部替换为 PC 端移植：
 *  - 进程内 libretro 核心（JNA 加载 fceumm_libretro.dll/.so/.dylib 等）
 *  - 外置 PC 独立核心（Azahar / Ishiiruka / Cemu / Eden / PCSX2 / PPSSPP / Flycast / FreeJ2ME，
 *    由 NesStation 统一管理与启动，各自使用原生 PC 端全部 GPU 能力）
 *
 * ★ Android 版移除的核心（无 PC 版）：
 *  - NDS「激烈 / DraStic」—— Android 专有商业模拟器，无电脑版；PC 版 NDS 统一用 melonDS。
 *  - 3DS「Citra MMJ」—— Android 专有社区分支，无电脑版；PC 版 3DS 统一用 Azahar 官方版。
 */
enum class GamePlatform(val displayName: String) {
    NES("NES"),          // FCEUmm（libretro 进程内）
    SFC("SFC"),          // snes9x（libretro 进程内）
    GB("GB/GBC"),        // mGBA（libretro 进程内）
    GBA("GBA"),          // mGBA（libretro 进程内）
    DOS("DOS"),          // DOSBox-Pure（libretro 进程内）
    ARCADE("Arcade"),    // FBNeo（libretro 进程内）
    MD("MD/SEGA"),       // Genesis-Plus-GX（libretro 进程内）
    PCE("PCE/TG16"),     // Geargrafx（libretro 进程内）
    NDS("NDS"),          // melonDS（libretro 进程内）
    PSX("PSX"),          // PCSX-ReARMed（libretro 进程内）
    PS2("PS2"),          // PCSX2 独立核心（推荐）/ pcsx2_libretro（实验性）
    JAVA("Java"),        // FreeJ2ME（外置，复用 NesStation 自带 JRE）
    DC("DC"),            // Flycast 独立核心（原生 GPU 渲染）
    N3DS("3DS"),         // Azahar 独立核心
    NGCWII("NGC/Wii"),   // Ishiiruka / Dolphin 独立核心
    WIIU("Wii U"),       // Cemu 独立核心
    PSP("PSP"),          // PPSSPP 独立核心（推荐）/ ppsspp_libretro（实验性）
    NSWITCH("Switch");   // Eden 独立核心

    companion object {
        /**
         * 从字符串解析平台标识（大小写不敏感 + 常见别名，
         * 与 Android 版 GamePlatform.fromString 行为一致）。
         */
        fun fromString(value: String?): GamePlatform {
            if (value.isNullOrBlank()) return NES
            val v = value.trim().lowercase()
                .replace("-", "").replace("_", "").replace("/", "").replace(".", "")
            return when (v) {
                "gbc", "gameboycolor" -> GB
                "nes", "fc", "famicom", "fceumm", "fceux", "nestopia" -> NES
                "sfc", "snes", "supernintendo", "snes9x" -> SFC
                "gb", "gameboy" -> GB
                "gba", "gameboyadvance" -> GBA
                "dos", "dosbox", "dosboxpure" -> DOS
                "arcade", "fbneo", "fbneocore", "mame", "cps1", "cps2", "cps3",
                "neogeo", "pgm", "finalburn" -> ARCADE
                "md", "genesis", "megadrive", "segagenesis", "megacd", "segacd",
                "genplus", "genplusgx", "genesisplusgx" -> MD
                "pce", "pcengine", "turbografx", "tg16", "tg16cd", "geargrafx",
                "supergrafx" -> PCE
                "nds", "ds", "nintendo", "nintendods", "melonds" -> NDS
                "psx", "ps1", "playstation", "playstation1", "pcsxrearmed" -> PSX
                "ps2", "playstation2", "pcsx2", "pcee2" -> PS2
                "java", "j2me", "midlet", "freej2me" -> JAVA
                "dc", "dreamcast", "flycast", "naomi" -> DC
                "n3ds", "3ds", "nintendo3ds", "azahar", "citra" -> N3DS
                "ngcwii", "gcn", "ngc", "wii", "gamecube", "dolphin", "ishiiruka" -> NGCWII
                "wiiu", "cemu" -> WIIU
                "psp", "ppsspp" -> PSP
                "nswitch", "switch", "eden", "yuzu" -> NSWITCH
                else -> NES
            }
        }
    }
}

/** 平台 → 全称（游戏库「全部」视图大标题 / 设置页等处使用）。 */
fun platformFullName(p: GamePlatform): String = when (p) {
    GamePlatform.NES -> "Nintendo Entertainment System"
    GamePlatform.SFC -> "Super Famicom / SNES"
    GamePlatform.GB -> "Game Boy / Game Boy Color"
    GamePlatform.GBA -> "Game Boy Advance"
    GamePlatform.MD -> "SEGA Mega Drive / Genesis"
    GamePlatform.PCE -> "PC Engine / TurboGrafx-16"
    GamePlatform.PSX -> "Sony PlayStation"
    GamePlatform.PS2 -> "PlayStation 2"
    GamePlatform.NDS -> "Nintendo DS / DSi"
    GamePlatform.N3DS -> "Nintendo 3DS"
    GamePlatform.NGCWII -> "GameCube / Wii"
    GamePlatform.DC -> "SEGA Dreamcast"
    GamePlatform.ARCADE -> "Arcade / FBNeo"
    GamePlatform.DOS -> "MS-DOS"
    GamePlatform.JAVA -> "Java Micro Edition"
    GamePlatform.PSP -> "Sony PlayStation Portable"
    GamePlatform.WIIU -> "Nintendo Wii U"
    GamePlatform.NSWITCH -> "Nintendo Switch"
}

/** 平台 → 建档年代副标题。 */
fun platformEra(p: GamePlatform): String = when (p) {
    GamePlatform.NES -> "1983 · 8-BIT"
    GamePlatform.SFC -> "1990 · 16-BIT"
    GamePlatform.GB -> "1989 · HANDHELD"
    GamePlatform.GBA -> "2001 · 32-BIT"
    GamePlatform.MD -> "1988 · 16-BIT"
    GamePlatform.PCE -> "1987 · 8/16-BIT"
    GamePlatform.PSX -> "1994 · 32-BIT CD"
    GamePlatform.PS2 -> "2000 · 128-BIT"
    GamePlatform.NDS -> "2004 · DUAL SCREEN"
    GamePlatform.N3DS -> "2011 · GLASSLESS 3D"
    GamePlatform.NGCWII -> "2001 / 2006 · OPTICAL"
    GamePlatform.DC -> "1998 · 128-BIT"
    GamePlatform.ARCADE -> "1978 · COIN-OP"
    GamePlatform.DOS -> "1981 · X86"
    GamePlatform.JAVA -> "2000 · J2ME"
    GamePlatform.PSP -> "2004 · UMD"
    GamePlatform.WIIU -> "2012 · GAMEPAD"
    GamePlatform.NSWITCH -> "2017 · HYBRID"
}

/** 格式化游戏时长。 */
fun formatPlayTime(ms: Long): String = when {
    ms <= 0L -> "—"
    ms < 60 * 60 * 1000L -> "${ms / 60_000L}分钟"
    else -> String.format("%.1f小时", ms / 3_600_000.0)
}

/** 格式化「最近游玩」相对时间。 */
fun formatLastPlayed(ts: Long, now: Long = System.currentTimeMillis()): String = when {
    ts <= 0L -> "未游玩"
    now - ts < 60_000L -> "刚刚"
    now - ts < 3_600_000L -> "${(now - ts) / 60_000L}分钟前"
    now - ts < 86_400_000L -> "${(now - ts) / 3_600_000L}小时前"
    now - ts < 30L * 86_400_000L -> "${(now - ts) / 86_400_000L}天前"
    else -> "很久以前"
}
