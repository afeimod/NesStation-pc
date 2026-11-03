package com.nesstation.app.core.storage

import kotlinx.serialization.Serializable
import kotlinx.serialization.json.Json
import java.io.File

/**
 * 全局设置（对应 Android 版 PadLayoutStore 中与界面/显示/存储相关的字段子集，
 * PC 版精简为一个 JSON 文件，字段名与语义保持一致）。
 */
@Serializable
data class AppSettings(
    // ===== 外观 =====
    /** 主界面风格："neon"（Neon 3D 新UI，默认）| "fsd"（FSD 经典桌面）。 */
    var homeUiStyle: String = "neon",
    /** FSD 经典界面的深浅主题（NesTheme 亮 / 暗）。 */
    var darkTheme: Boolean = true,
    /** 自定义背景图路径（两套风格通用；空 = 默认背景）。 */
    var homeBackgroundUri: String = "",

    // ===== 视频/显示 =====
    /** 画面滤镜：0 无（像素完美） 1 双线性 2 CRT 扫描线（前端实现）。 */
    var videoFilter: Int = 0,
    /** 保持宽高比。 */
    var keepAspect: Boolean = true,
    /** 整数缩放（像素完美）。 */
    var integerScaling: Boolean = false,
    /** 全屏启动游戏。 */
    var fullscreenOnLaunch: Boolean = false,
    /** 显示 FPS HUD。 */
    var showFps: Boolean = true,

    // ===== 性能 =====
    /** 默认快进倍速（0 = 关闭；2/4/6/8/16）。 */
    var fastForwardSpeed: Int = 4,
    /** 音频缓冲毫秒（越大越稳、延迟越高）。 */
    var audioBufferMs: Int = 64,

    // ===== 输入 =====
    /** 菜单呼出键（Compose Key 字符串）。 */
    var menuKey: String = "Escape",
    /** 快进按住键。 */
    var fastForwardKey: String = "Tab",

    // ===== 存储 =====
    /** 游戏库扫描根目录（多目录用分号分隔）。 */
    var romRoots: String = "",
    /** 外置核心路径覆盖（coreId → exe/jar 路径）。 */
    var externalCorePaths: MutableMap<String, String> = mutableMapOf(),
    /** 双核心平台的默认核心（platform → coreId）。 */
    var defaultCores: MutableMap<String, String> = mutableMapOf(),
    /** 启动时记住「双核心平台」选择不再询问。 */
    var rememberCoreChoice: Boolean = true,

    // ===== 关于 =====
    var firstRunDone: Boolean = false
)

object SettingsStore {
    private val json = Json {
        ignoreUnknownKeys = true
        prettyPrint = true
        encodeDefaults = true
    }

    @Volatile
    private var cached: AppSettings? = null

    private val file: File get() = File(Paths.dataDir, "settings.json")

    fun load(): AppSettings {
        cached?.let { return it }
        val s = runCatching {
            json.decodeFromString<AppSettings>(file.readText())
        }.getOrDefault(AppSettings())
        cached = s
        return s
    }

    fun save(s: AppSettings) {
        cached = s
        runCatching { file.writeText(json.encodeToString(s)) }
    }

    /** 线程安全的字段级更新（后台线程也可直接调用）。 */
    fun update(block: (AppSettings) -> Unit) {
        synchronized(this) {
            val s = load()
            block(s)
            save(s)
        }
    }
}
