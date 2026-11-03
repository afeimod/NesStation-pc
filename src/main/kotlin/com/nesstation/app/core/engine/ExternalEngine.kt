package com.nesstation.app.core.engine

import com.nesstation.app.core.coremgmt.ExternalCoreDef
import com.nesstation.app.core.libretro.LrFrame
import com.nesstation.app.core.storage.Paths
import java.io.File
import java.util.concurrent.atomic.AtomicBoolean

/**
 * ★ 外置 PC 核心引擎 ★
 *
 * 3DS / NGC-Wii / Wii U / Switch / PS2(推荐) / PSP(推荐) / DC / J2ME 平台
 * 使用 GitHub 上各模拟器的官方 PC 版，由 NesStation 检测安装、以独立进程
 * 启动并接管生命周期 —— 这是 PC 平台运行现代 3D 主机的标准方式
 * （原生 GPU 全速 + 完整原生存档系统）。
 *
 * NesStation 负责：路径检测 / 自动下载 / 统一启动参数 / 便携化数据目录 /
 * 进程监控（退出后回到游戏站并记录游玩时长）。
 */
class ExternalEngine(val def: ExternalCoreDef) : EmulatorEngine {

    @Volatile var process: Process? = null
        private set

    @Volatile var exitedNormally = true
        private set

    private val running = AtomicBoolean(false)
    private var _lastError = ""

    override val isLoaded: Boolean get() = running.get()

    /** 解析可执行文件（含 JAVA 特例：用 NesStation 自带 JRE 跑 FreeJ2ME.jar）。 */
    fun resolveLaunchCommand(rom: File): List<String>? {
        val exe = def.resolveExecutable()
            ?: return null
        return when (def.id) {
            "freej2me" -> {
                val java = File(System.getProperty("java.home"), binJava())
                if (!java.isFile) { _lastError = "未找到 Java 运行时"; return null }
                listOf(java.absolutePath, "-jar", exe.absolutePath, rom.absolutePath)
            }
            else -> {
                val args = def.argTemplate.split(" ").filter { it.isNotBlank() }
                    .map { if (it == "{rom}") rom.absolutePath else it }
                listOf(exe.absolutePath) + args
            }
        }
    }

    private fun binJava(): String =
        if (System.getProperty("os.name").lowercase().contains("win")) "bin/java.exe" else "bin/java"

    override fun loadRom(rom: File, srmBasename: String): Boolean {
        val cmd = resolveLaunchCommand(rom)
        if (cmd == null) {
            _lastError = "未找到 ${def.name} 可执行文件 —— 请到 设置 → 核心管理 安装或手动选择路径"
            return false
        }
        return try {
            val pb = ProcessBuilder(cmd)
            // 便携化：能识别配置目录的核心切到 NesStation 数据目录
            pb.environment()["HOME"] = pb.environment()["HOME"] // 保持不变（避免破坏核心）
            val logDir = Paths.logsDir
            val out = File(logDir, "${def.id}-stdout.log").outputStream()
            val err = File(logDir, "${def.id}-stderr.log").outputStream()
            val p = pb.start()
            Thread {
                runCatching {
                    p.inputStream.bufferedReader().forEachLine { out.write((it + "\n").toByteArray()); out.flush() }
                }
            }.apply { isDaemon = true; name = "ext-${def.id}-out"; start() }
            Thread {
                runCatching {
                    p.errorStream.bufferedReader().forEachLine { err.write((it + "\n").toByteArray()); err.flush() }
                }
            }.apply { isDaemon = true; name = "ext-${def.id}-err"; start() }
            process = p
            running.set(true)
            exitedNormally = false
            Thread {
                val code = runCatching { p.waitFor() }.getOrDefault(-1)
                running.set(false)
                exitedNormally = code == 0
                runCatching { out.close(); err.close() }
                onExited?.invoke(code)
            }.apply { isDaemon = true; name = "ext-${def.id}-watch"; start() }
            true
        } catch (t: Throwable) {
            _lastError = "启动 ${def.name} 失败: ${t.message}"
            false
        }
    }

    /** 进程退出回调（ExternalCoreScreen 监听后返回游戏站）。 */
    @Volatile var onExited: ((Int) -> Unit)? = null

    val isRunning: Boolean get() = running.get()

    fun stop() {
        runCatching { process?.destroy() }
    }

    fun stopForcibly() {
        runCatching { process?.destroyForcibly() }
    }

    override fun snapshot(): LrFrame? = null
    override fun videoWidth(): Int = 0
    override fun videoHeight(): Int = 0

    override fun setPaused(paused: Boolean) { /* 外部核心由其自身管理 */ }
    override fun reset() { /* 外部核心由其自身管理 */ }

    override fun unload() {
        stop()
        running.set(false)
    }

    override fun setFastForward(speed: Int) { /* 外部核心用其原生加速 */ }
    override fun setCoreOption(key: String, value: String) { }
    override fun coreOptionsReady(): Boolean = false
    override fun notifyOptionsChanged() { }
    override fun saveState(slot: Int, dst: File): Boolean = false
    override fun loadState(slot: Int, src: File): Boolean = false

    override fun coreTitle(): String = def.name
    override fun lastError(): String = _lastError

    override var onFrame: (() -> Unit)?
        get() = null
        set(_) { }
}
