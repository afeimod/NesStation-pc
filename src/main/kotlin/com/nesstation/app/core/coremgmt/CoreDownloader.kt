package com.nesstation.app.core.coremgmt

import com.nesstation.app.core.storage.Paths
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import java.io.File
import java.net.HttpURLConnection
import java.net.URL
import java.util.zip.ZipEntry
import java.util.zip.ZipInputStream

/**
 * 核心下载器：
 *  - libretro 核心：buildbot 官方 nightly（zip 内含 .dll/.so/.dylib）
 *  - 外置核心：GitHub Releases（api.github.com → 匹配资产名）
 *
 * 全部在 IO 线程执行，带进度回调（0..100 与状态文本）。
 */
object CoreDownloader {

    data class Progress(val percent: Int, val text: String)

    private const val UA = "NesStation-PC/1.0 (libretro core manager)"

    /** 下载并安装一个 libretro 核心。 */
    suspend fun installLibretro(
        def: LibretroCoreDef,
        onProgress: (Progress) -> Unit
    ): Result<File> = withContext(Dispatchers.IO) {
        val url = def.buildbotUrl()
            ?: return@withContext Result.failure(IllegalStateException("当前平台暂无官方构建"))
        runCatching {
            onProgress(Progress(2, "连接 libretro buildbot…"))
            val zip = downloadToTemp(URL(url), "libretro-${def.id}", onProgress)
            onProgress(Progress(95, "解压核心…"))
            val target = def.installedFile() ?: error("核心文件名解析失败")
            target.parentFile?.mkdirs()
            val extracted = extractFirst(zip, def.fileName()!!)
            zip.delete()
            if (extracted == null) error("压缩包内未找到 ${def.fileName()}")
            extracted.copyTo(target, overwrite = true)
            extracted.delete()
            onProgress(Progress(100, "已安装 ${target.name}"))
            target
        }
    }

    /** 从 GitHub Releases 自动下载外置核心（返回安装目录；失败给出原因）。 */
    suspend fun installExternal(
        def: ExternalCoreDef,
        onProgress: (Progress) -> Unit
    ): Result<File> = withContext(Dispatchers.IO) {
        val api = def.githubApi
            ?: return@withContext Result.failure(UnsupportedOperationException("该核心请手动安装（官网下载后选择路径）"))
        val pattern = when (currentOs()) {
            "windows" -> def.assetPatternWin
            "linux" -> def.assetPatternLinux
            "macos" -> def.assetPatternMac
            else -> null
        } ?: return@withContext Result.failure(UnsupportedOperationException("该核心暂无 ${currentOs()} 自动下载"))
        runCatching {
            onProgress(Progress(3, "查询 GitHub 最新版本…"))
            val conn = open(URL(api))
            conn.setRequestProperty("Accept", "application/vnd.github+json")
            val body = conn.inputStream.bufferedReader().readText()
            val assets = parseAssetUrls(body)
            val asset = assets.firstOrNull { matchesPattern(it.name, pattern) }
                ?: error("最新 Release 未匹配到 ${currentOs()} 资产（可手动下载）")
            onProgress(Progress(6, "下载 ${asset.name}…"))
            val f = downloadToTemp(URL(asset.url), "external-${def.id}", onProgress)
            onProgress(Progress(92, "安装到核心目录…"))
            val dir = def.installDir().apply { mkdirs() }
            when {
                asset.name.endsWith(".zip") || asset.name.endsWith(".7z") ||
                    asset.name.endsWith(".tar.xz") || asset.name.endsWith(".tar.gz") || asset.name.endsWith(".tgz") ->
                    extractArchive(f, dir)
                else -> f.copyTo(File(dir, asset.name), overwrite = true)
            }
            f.delete()
            // AppImage 可执行化
            dir.walkTopDown().forEach {
                if (it.name.endsWith(".AppImage") || def.exeNames().contains(it.name)) {
                    runCatching { it.setExecutable(true) }
                }
            }
            onProgress(Progress(100, "已安装到 ${dir.name}/"))
            dir
        }
    }

    // ================= 工具 =================

    private fun open(url: URL): HttpURLConnection {
        val conn = url.openConnection() as HttpURLConnection
        conn.connectTimeout = 15000
        conn.readTimeout = 60000
        conn.instanceFollowRedirects = true
        conn.setRequestProperty("User-Agent", UA)
        return conn
    }

    private fun downloadToTemp(url: URL, prefix: String, onProgress: (Progress) -> Unit): File {
        val conn = open(url)
        if (conn.responseCode !in 200..299) error("下载失败 HTTP ${conn.responseCode}")
        val total = conn.contentLengthLong
        val tmp = File.createTempFile(prefix, ".download")
        conn.inputStream.use { ins ->
            tmp.outputStream().use { outs ->
                val buf = ByteArray(64 * 1024)
                var read = 0L
                while (true) {
                    val n = ins.read(buf)
                    if (n < 0) break
                    outs.write(buf, 0, n)
                    read += n
                    if (total > 0) {
                        val pct = (6 + (read * 86 / total)).toInt().coerceIn(6, 92)
                        onProgress(Progress(pct, "下载中 ${read / 1024 / 1024}MB / ${total / 1024 / 1024}MB"))
                    } else {
                        onProgress(Progress(-1, "下载中 ${read / 1024 / 1024}MB"))
                    }
                }
            }
        }
        return tmp
    }

    private fun extractFirst(zip: File, wantedName: String): File? {
        ZipInputStream(zip.inputStream().buffered()).use { zis ->
            while (true) {
                val e: ZipEntry = zis.nextEntry ?: break
                if (e.name == wantedName || e.name.endsWith("/$wantedName")) {
                    val out = File(zip.parentFile, wantedName)
                    out.outputStream().use { zis.copyTo(it) }
                    return out
                }
            }
        }
        return null
    }

    /** zip 直接解压；tar.* 走系统 tar；7z 走系统 7z（缺失则提示手动解压。 */
    private fun extractArchive(archive: File, dir: File) {
        when {
            archive.name.endsWith(".zip") -> {
                java.util.zip.ZipInputStream(archive.inputStream().buffered()).use { zis ->
                    var e: java.util.zip.ZipEntry? = zis.nextEntry
                    while (e != null) {
                        val out = File(dir, e.name.replace('\\', '/'))
                        if (e.isDirectory) out.mkdirs()
                        else {
                            out.parentFile?.mkdirs()
                            out.outputStream().use { zis.copyTo(it) }
                        }
                        e = zis.nextEntry
                    }
                }
            }
            archive.name.endsWith(".tar.xz") || archive.name.endsWith(".tar.gz") || archive.name.endsWith(".tgz") -> {
                ProcessBuilder("tar", "xf", archive.absolutePath, "-C", dir.absolutePath)
                    .start().waitFor()
            }
            archive.name.endsWith(".7z") -> {
                val sevenz = listOf("/usr/bin/7z", "/usr/bin/7za", "7z", "7za")
                    .firstOrNull { File(it).isFile || runCatching { ProcessBuilder("which", it).start().waitFor() == 0 }.getOrDefault(false) }
                    ?: error("需要 7-Zip 解包：请安装 7z 后重试，或手动解压到 ${dir.absolutePath}")
                ProcessBuilder(sevenz, "x", "-y", "-o${dir.absolutePath}", archive.absolutePath)
                    .start().waitFor()
            }
            archive.name.endsWith(".dmg") -> {
                // macOS dmg：提示手动挂载（少见路径，不自动处理）
                archive.copyTo(File(dir, archive.name), overwrite = true)
            }
            else -> archive.copyTo(File(dir, archive.name), overwrite = true)
        }
    }

    private data class Asset(val name: String, val url: String)

    /** 简易 JSON 资产解析（避免引重型 JSON 依赖的浏览器 API 依赖问题）。 */
    private fun parseAssetUrls(body: String): List<Asset> {
        val out = ArrayList<Asset>()
        val regex = Regex("\"browser_download_url\"\\s*:\\s*\"([^\"]+)\"")
        for (m in regex.findAll(body)) {
            val url = m.groupValues[1]
            val name = url.substringAfterLast('/')
            out.add(Asset(name, url))
        }
        return out
    }

    /** 简单通配：| 分隔多个模式，* 任意。 */
    private fun matchesPattern(name: String, pattern: String): Boolean =
        pattern.split('|').any { p -> regexOf(p).matches(name) }

    private fun regexOf(glob: String): Regex =
        Regex(Regex.escape(glob).replace("\\*", ".*"))
}
