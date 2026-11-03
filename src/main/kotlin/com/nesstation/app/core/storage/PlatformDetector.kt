package com.nesstation.app.core.storage

import com.nesstation.app.core.model.GamePlatform
import java.io.File
import java.util.zip.ZipFile

/**
 * 文件 → 平台检测（与 Android 版 PlatformDetector 同源的判定表）。
 *
 * PC 版的差异：
 *  - 扫描目录改为递归遍历（无 SAF 权限限制）；
 *  - zip/7z：zip 可逐条目探头判定平台；7z/gz 不探头，按平台页 hint 处理。
 */
object PlatformDetector {

    val CD_IMAGE_EXTENSIONS = setOf("cue", "img", "iso", "ccd", "sub", "bin", "chd")
    val MDF_MDS_NRG_EXTENSIONS = setOf("mdf", "mds", "nrg")
    val ARCHIVE_EXTENSIONS = setOf("zip", "7z", "gz")

    /** 街机 ROM 在 zip 内常见的扩展名（NeoGeo / CPS1/2/3 / PGM）。 */
    val ARCADE_ROM_EXTENSIONS = setOf(
        // NeoGeo
        "p1", "p2", "sp1", "sp2", "p3", "p4", "s1", "s2", "m1", "m2",
        "v1", "v2", "v3", "v4", "c1", "c2", "c3", "c4", "c5", "c6", "c7", "c8",
        "lo", "sm1", "sfix",
        // CPS1 / CPS2 / CPS3
        "prg", "gfx", "snd", "qsf", "q1", "q2", "q3", "q4", "q5",
        // PGM
        "b1", "b2", "t1", "t2", "u1", "u2", "v10", "v20", "v30", "v40"
    )

    /** Switch（Eden）可直接启动的裸格式。 */
    val SWITCH_DIRECT_EXTENSIONS = setOf("xci", "nsp", "nca", "nro")

    /** 自动扫描白名单（专属扩展名，日常文件不撞名）。 */
    val AUTO_TRUSTED_EXTENSIONS: Map<String, GamePlatform> = mapOf(
        // NES / Famicom
        "nes" to GamePlatform.NES, "fds" to GamePlatform.NES,
        "unf" to GamePlatform.NES, "unif" to GamePlatform.NES,
        "nez" to GamePlatform.NES, "unh" to GamePlatform.NES,
        // SNES / SFC
        "smc" to GamePlatform.SFC, "sfc" to GamePlatform.SFC,
        "swc" to GamePlatform.SFC, "fig" to GamePlatform.SFC,
        // GB / GBC
        "gb" to GamePlatform.GB, "sgb" to GamePlatform.GB, "gbc" to GamePlatform.GB,
        // GBA
        "gba" to GamePlatform.GBA,
        // DOSBox-Pure 专属打包格式
        "dosz" to GamePlatform.DOS,
        // SEGA MD / Genesis
        "smd" to GamePlatform.MD, "gen" to GamePlatform.MD, "68k" to GamePlatform.MD,
        "md" to GamePlatform.MD,
        // SMS / GG / SG（Genesis-Plus-GX 同核心支持）
        "sms" to GamePlatform.MD, "gg" to GamePlatform.MD, "sg" to GamePlatform.MD,
        // PC-Engine / SuperGrafx
        "pce" to GamePlatform.PCE, "sgx" to GamePlatform.PCE,
        // Nintendo DS
        "nds" to GamePlatform.NDS, "srl" to GamePlatform.NDS,
        // Nintendo 3DS（cia 为安装包，不在自动扫描白名单）
        "3ds" to GamePlatform.N3DS, "cci" to GamePlatform.N3DS, "cxi" to GamePlatform.N3DS,
        // GameCube / Wii
        "rvz" to GamePlatform.NGCWII, "gcm" to GamePlatform.NGCWII,
        "gcz" to GamePlatform.NGCWII, "wad" to GamePlatform.NGCWII,
        "dol" to GamePlatform.NGCWII, "nkit" to GamePlatform.NGCWII,
        "wbfs" to GamePlatform.NGCWII, "ciso" to GamePlatform.NGCWII,
        // Wii U
        "wud" to GamePlatform.WIIU, "wux" to GamePlatform.WIIU,
        "wua" to GamePlatform.WIIU, "wuhb" to GamePlatform.WIIU, "rpx" to GamePlatform.WIIU,
        // Switch
        "xci" to GamePlatform.NSWITCH, "nsp" to GamePlatform.NSWITCH,
        "nca" to GamePlatform.NSWITCH, "nro" to GamePlatform.NSWITCH,
        // Dreamcast / NAOMI
        "gdi" to GamePlatform.DC, "cdi" to GamePlatform.DC,
        // PSP
        "cso" to GamePlatform.PSP,
        // J2ME
        "jar" to GamePlatform.JAVA, "jad" to GamePlatform.JAVA,
        // PS1 / PS2 常见格式（cue/bin/chd/iso 走 hint 消歧，这里只放专属）
        "pbp" to GamePlatform.PSX, "mdf" to GamePlatform.PS2
    )

    /** 递归扫描目录，返回识别出的游戏文件（含 zip 内部探头）。 */
    fun scanDirectory(
        dir: File,
        hintPlatform: GamePlatform? = null,
        maxDepth: Int = 4,
        onProgress: ((Int) -> Unit)? = null
    ): List<Pair<File, GamePlatform>> {
        val out = ArrayList<Pair<File, GamePlatform>>()
        val queue = ArrayDeque<Pair<File, Int>>()
        queue.add(dir to 0)
        var scanned = 0
        while (queue.isNotEmpty()) {
            val (d, depth) = queue.removeFirst()
            val files = d.listFiles() ?: continue
            for (f in files.sorted()) {
                if (f.isDirectory) {
                    if (depth < maxDepth && !f.name.startsWith(".")) queue.add(f to depth + 1)
                } else {
                    scanned++
                    if (scanned % 50 == 0) onProgress?.invoke(scanned)
                    detect(f, hintPlatform)?.let { out.add(f to it) }
                }
            }
        }
        return out
    }

    /** 单文件检测：白名单直判 → 平台页 hint → zip 探头 → null。 */
    fun detect(file: File, hintPlatform: GamePlatform? = null): GamePlatform? {
        val ext = file.extension.lowercase()
        // Switch 不支持压缩包直启：hint 为 Switch 时只收直连格式
        if (hintPlatform == GamePlatform.NSWITCH) {
            return if (ext in SWITCH_DIRECT_EXTENSIONS) GamePlatform.NSWITCH else null
        }
        // 白名单直判（hint 优先：平台页导入优先信 hint）
        if (hintPlatform != null) {
            if (acceptsExtension(hintPlatform, ext) && ext !in ARCHIVE_EXTENSIONS) return hintPlatform
            if (ext in ARCHIVE_EXTENSIONS) return hintPlatform.takeIf { it != GamePlatform.NSWITCH }
            return null
        }
        AUTO_TRUSTED_EXTENSIONS[ext]?.let { return it }
        if (ext == "zip") return detectZip(file)
        return null
    }

    /** 平台是否接受该扩展名（平台页导入用，含 CD 镜像与 MDF/NRG）。 */
    fun acceptsExtension(p: GamePlatform, ext: String): Boolean = when (p) {
        GamePlatform.NES -> ext in setOf("nes", "fds", "unf", "unif", "nez", "unh", "zip", "7z")
        GamePlatform.SFC -> ext in setOf("smc", "sfc", "swc", "fig", "zip", "7z")
        GamePlatform.GB -> ext in setOf("gb", "gbc", "sgb", "zip", "7z")
        GamePlatform.GBA -> ext in setOf("gba", "zip", "7z")
        GamePlatform.DOS -> ext in setOf("dosz", "zip", "conf", "exe", "bat", "iso", "cue")
        GamePlatform.ARCADE -> ext in setOf("zip", "7z")
        GamePlatform.MD -> ext in setOf(
            "md", "smd", "gen", "68k", "sms", "gg", "sg", "bin", "cue", "chd", "zip", "7z"
        )
        GamePlatform.PCE -> ext in setOf("pce", "sgx", "cue", "chd", "hes", "zip", "7z")
        GamePlatform.NDS -> ext in setOf("nds", "srl", "zip", "7z")
        GamePlatform.PSX -> ext in CD_IMAGE_EXTENSIONS + setOf("pbp", "mdf", "mds", "nrg", "img", "exe")
        GamePlatform.PS2 -> ext in (CD_IMAGE_EXTENSIONS + MDF_MDS_NRG_EXTENSIONS + setOf("gz"))
        GamePlatform.JAVA -> ext in setOf("jar", "jad")
        GamePlatform.DC -> ext in (CD_IMAGE_EXTENSIONS + setOf("gdi", "cdi", "lst"))
        GamePlatform.N3DS -> ext in setOf("3ds", "cci", "cxi", "cxi", "app", "cia")
        GamePlatform.NGCWII -> ext in setOf(
            "rvz", "gcm", "iso", "gcz", "wbfs", "ciso", "nkit", "wad", "dol", "elf"
        )
        GamePlatform.WIIU -> ext in setOf("wud", "wux", "wua", "wuhb", "rpx", "iso", "elf")
        GamePlatform.PSP -> ext in setOf("cso", "iso", "chd", "pbp", "elf", "bin")
        GamePlatform.NSWITCH -> ext in SWITCH_DIRECT_EXTENSIONS
    }

    /** zip 内部探头：街机扩展名 → ARCADE；否则白名单；最后 CD 镜像 → null（需 hint）。 */
    private fun detectZip(file: File): GamePlatform? = runCatching {
        ZipFile(file).use { zip ->
            val entries = zip.entries().toList()
            val entryExts = entries.mapNotNull {
                val n = it.name.substringAfterLast('/')
                if (n.contains('.')) n.substringAfterLast('.').lowercase() else null
            }
            if (entryExts.any { it in ARCADE_ROM_EXTENSIONS }) return@runCatching GamePlatform.ARCADE
            entryExts.firstNotNullOfOrNull { AUTO_TRUSTED_EXTENSIONS[it] }?.let { return@runCatching it }
            if (entryExts.any { it in CD_IMAGE_EXTENSIONS }) return@runCatching null
            if (entryExts.any { it == "jar" || it == "jad" }) return@runCatching GamePlatform.JAVA
            null
        }
    }.getOrNull()
}
