package com.nesstation.app.core.storage

import com.nesstation.app.core.model.GameEntry
import com.nesstation.app.core.model.GameIds
import com.nesstation.app.core.model.GamePlatform
import kotlinx.serialization.builtins.ListSerializer
import kotlinx.serialization.json.Json
import java.io.File

/**
 * 游戏库（对应 Android 版 RomStore + AppDatabase；PC 版为 JSON 单文件 + 内存索引）。
 */
object RomLibrary {

    private val json = Json { ignoreUnknownKeys = true; prettyPrint = true }
    private val file: File get() = File(Paths.dataDir, "library.json")

    @Volatile
    private var games: MutableList<GameEntry> = ArrayList()
    private val listeners = ArrayList<() -> Unit>()

    init { reload() }

    fun reload() {
        games = runCatching {
            json.decodeFromString(ListSerializer(GameEntry.serializer()), file.readText()).toMutableList()
        }.getOrDefault(ArrayList())
    }

    fun persist() {
        runCatching {
            synchronized(this) {
                file.writeText(json.encodeToString(ListSerializer(GameEntry.serializer()), games.toList()))
            }
        }
    }

    fun all(): List<GameEntry> = games.toList()

    fun byId(id: String): GameEntry? = games.firstOrNull { it.id == id }

    fun byPlatform(p: GamePlatform): List<GameEntry> = games.filter { it.platformEnum == p }

    fun favorites(): List<GameEntry> = games.filter { it.favorite }

    fun recent(limit: Int = 50): List<GameEntry> =
        games.filter { it.lastPlayedAt > 0 }.sortedByDescending { it.lastPlayedAt }.take(limit)

    fun search(q: String): List<GameEntry> {
        if (q.isBlank()) return games.toList()
        val lq = q.trim().lowercase()
        return games.filter { it.title.lowercase().contains(lq) }
    }

    // ---------- 增删改 ----------

    /** 入库（按路径去重，返回最终条目）。 */
    fun add(rom: File, platform: GamePlatform, title: String = rom.nameWithoutExtension): GameEntry {
        val path = rom.absolutePath
        val existing = games.firstOrNull { it.path == path }
        if (existing != null) {
            // 平台以最新判定为准（用户可能重新导入修正）
            val updated = existing.copy(platform = platform.name, title = title, sizeBytes = rom.length())
            replace(updated)
            return updated
        }
        val entry = GameEntry(
            id = GameIds.of(path),
            title = title,
            platform = platform.name,
            path = path,
            sizeBytes = rom.length(),
            addedAt = System.currentTimeMillis(),
            coverPath = findCover(path)
        )
        games.add(entry)
        persist(); notifyChanged()
        return entry
    }

    fun remove(id: String) {
        games.removeAll { it.id == id }
        persist(); notifyChanged()
    }

    fun replace(entry: GameEntry) {
        val idx = games.indexOfFirst { it.id == entry.id }
        if (idx >= 0) games[idx] = entry else games.add(entry)
        persist(); notifyChanged()
    }

    fun toggleFavorite(id: String) {
        val g = byId(id) ?: return
        replace(g.copy(favorite = !g.favorite))
    }

    /** 游戏结束时更新游玩统计（durationMs 由 EmulatorScreen 计时）。 */
    fun recordPlay(id: String, durationMs: Long) {
        val g = byId(id) ?: return
        replace(g.withPlay(System.currentTimeMillis(), durationMs))
    }

    fun setPreferredCore(id: String, coreId: String) {
        val g = byId(id) ?: return
        replace(g.copy(preferredCore = coreId))
    }

    fun clearAll() {
        games.clear(); persist(); notifyChanged()
    }

    // ---------- 封面 ----------

    /** ROM 同目录（或 covers/ 子目录）中与 ROM 同名的图片即封面。 */
    fun findCover(romPath: String): String {
        val rom = File(romPath)
        val dir = rom.parentFile ?: return ""
        val base = rom.nameWithoutExtension
        val candidates = ArrayList<File>()
        for (ext in listOf("png", "jpg", "jpeg", "webp")) {
            candidates.add(File(dir, "$base.$ext"))
            candidates.add(File(dir, "$base-front.$ext"))
            candidates.add(File(dir, "$base.boxart.$ext"))
        }
        File(dir, "covers").takeIf { it.isDirectory }?.let { c ->
            for (ext in listOf("png", "jpg", "jpeg", "webp")) {
                candidates.add(File(c, "$base.$ext"))
            }
        }
        return candidates.firstOrNull { it.isFile }?.absolutePath ?: ""
    }

    fun refreshCovers() {
        games.replaceAll { it.copy(coverPath = findCover(it.path)) }
        persist(); notifyChanged()
    }

    /** 扫描根目录并导入新游戏（增量：已有路径不重复入库）。 */
    fun rescanRoots(onProgress: ((Int) -> Unit)? = null): Int {
        val roots = SettingsStore.load().romRoots.split(';', ';')
            .map { it.trim() }.filter { it.isNotEmpty() }
        var added = 0
        for (r in roots) {
            val dir = File(r)
            if (!dir.isDirectory) continue
            val found = com.nesstation.app.core.storage.PlatformDetector.scanDirectory(dir, onProgress = onProgress)
            for ((f, p) in found) {
                if (games.none { it.path == f.absolutePath }) {
                    games.add(
                        GameEntry(
                            id = GameIds.of(f.absolutePath),
                            title = f.nameWithoutExtension,
                            platform = p.name,
                            path = f.absolutePath,
                            sizeBytes = f.length(),
                            addedAt = System.currentTimeMillis(),
                            coverPath = findCover(f.absolutePath)
                        )
                    )
                    added++
                }
            }
        }
        if (added > 0) { persist(); notifyChanged() }
        return added
    }

    // ---------- 监听 ----------

    fun addListener(l: () -> Unit) { listeners.add(l) }

    fun removeListener(l: () -> Unit) { listeners.remove(l) }

    private fun notifyChanged() { listeners.forEach { runCatching(it) } }
}
