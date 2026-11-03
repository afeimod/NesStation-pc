package com.nesstation.app.core.storage

import java.io.File

/**
 * PC 版数据目录布局（对应 Android 版 filesDir/getExternalFilesDir 的角色）。
 *
 * 规则：
 *  - 便携模式：exe 同目录存在 `portable.marker` 时，全部数据落在 `./nesstation-data`，
 *    适合 U 盘 / 绿色解压版。
 *  - 标准模式：`~/.nesstation`（Windows 同样在用户主目录，干净且无需注册表）。
 */
object Paths {

    /** 便携模式标记文件名。 */
    const val PORTABLE_MARKER = "portable.marker"

    val isPortable: Boolean by lazy { File(currentAppDir, PORTABLE_MARKER).isFile }

    val currentAppDir: File by lazy {
        // Compose Desktop/jpackage 下取 jar/或 app-image 所在目录
        val cp = System.getProperty("java.class.path") ?: ""
        val first = cp.split(File.pathSeparator).firstOrNull() ?: ""
        val f = File(first).absoluteFile
        if (f.isFile) f.parentFile ?: File(".") else if (f.isDirectory) f else File(".")
    }

    val dataDir: File by lazy {
        val base = if (isPortable) {
            File(currentAppDir, "nesstation-data")
        } else {
            File(System.getProperty("user.home"), ".nesstation")
        }
        base.mkdirs()
        base
    }

    val coresDir: File get() = dir("cores")
    val libretroCoresDir: File get() = dir("cores/libretro")
    val externalCoresDir: File get() = dir("cores/external")
    val systemDir: File get() = dir("system")
    val savesDir: File get() = dir("saves")
    val screenshotsDir: File get() = dir("screenshots")
    val logsDir: File get() = dir("logs")
    val cacheDir: File get() = dir("cache")

    /** 某平台核心的 system 目录（BIOS 等）。 */
    fun platformSystemDir(platformId: String): File = dir("system/$platformId")

    /** 某核心的专属 system 目录。 */
    fun coreSystemDir(coreId: String): File = dir("system/cores/$coreId")

    private fun dir(rel: String): File = File(dataDir, rel).apply { mkdirs() }
}
