package com.nesstation.app.core.model

import kotlinx.serialization.Serializable
import java.io.File

/**
 * 游戏库条目（对应 Android 版 RomStore 的 Game 实体，PC 版用 JSON 持久化）。
 */
@Serializable
data class GameEntry(
    /** 稳定 ID：路径哈希（同一文件移动路径后重新扫描会生成新 ID，与原版行为一致）。 */
    val id: String,
    val title: String,
    val platform: String,           // GamePlatform.name
    val path: String,               // ROM 绝对路径
    val sizeBytes: Long = 0L,
    val addedAt: Long = 0L,
    val lastPlayedAt: Long = 0L,
    val playCount: Int = 0,
    val playDurationMs: Long = 0L,
    val favorite: Boolean = false,
    val coverPath: String = "",     // 封面图片路径（自动在 ROM 同目录找 png/jpg）
    /** 双核心平台（PS2/PSP）记住的核心选择；空 = 每次询问。 */
    val preferredCore: String = "",
    /** 多张光盘（cue/chd 多文件）时的备注名。 */
    val note: String = ""
) {
    val platformEnum: GamePlatform get() = GamePlatform.fromString(platform)
    val romFile: File get() = File(path)

    fun withPlay(now: Long, durationMs: Long): GameEntry = copy(
        lastPlayedAt = now,
        playCount = playCount + 1,
        playDurationMs = playDurationMs + durationMs
    )
}

object GameIds {
    /** 由路径生成稳定 ID（与 Android 版 id 生成策略相同的语义）。 */
    fun of(path: String): String =
        java.security.MessageDigest.getInstance("MD5")
            .digest(path.toByteArray(Charsets.UTF_8))
            .joinToString("") { "%02x".format(it) }
}
