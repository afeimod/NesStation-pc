package com.nesstation.app.core.engine

import com.nesstation.app.core.coremgmt.LibretroCoreDef
import com.nesstation.app.core.libretro.CoreOptions
import com.nesstation.app.core.libretro.LrFrame
import com.nesstation.app.core.libretro.LibretroHost
import java.io.File

/**
 * 进程内 libretro 引擎（LibretroHost 的 EmulatorEngine 适配器）。
 */
class LibretroEngine(val def: LibretroCoreDef) : EmulatorEngine {

    private var host: LibretroHost? = null

    override val isLoaded: Boolean
        get() = host?.loaded == true

    override fun loadRom(rom: File, srmBasename: String): Boolean {
        unload()
        val f = def.installedFile()
        if (f == null || !f.isFile) {
            _lastError = "核心未安装：请到 设置 → 核心管理 下载 ${def.name}"
            return false
        }
        val h = LibretroHost(def.id, f)
        if (!h.loadCore()) { _lastError = h.lastError; return false }
        h.readSystemInfo()
        // 首次接触核心时预跑一轮选项定义加载（loadRom 前 env 已被核心调用）
        val ok = h.loadRom(rom, srmBasename)
        if (!ok) { _lastError = h.lastError; h.shutdown(); return false }
        host = h
        return true
    }

    override fun snapshot(): LrFrame? = host?.snapshot()

    override fun videoWidth(): Int = host?.videoW ?: 0
    override fun videoHeight(): Int = host?.videoH ?: 0

    fun aspectRatio(): Float = host?.aspectRatio ?: (4f / 3f)
    fun fps(): Double = host?.fps ?: 60.0
    fun sampleRate(): Int = host?.sampleRate ?: 0
    fun coreName(): String = host?.coreName ?: def.name
    fun coreVersion(): String = host?.coreVersion ?: ""

    override fun setPaused(paused: Boolean) { host?.setPaused(paused) }
    override fun reset() { host?.reset() }

    override fun unload() {
        host?.let { h ->
            h.unload()
            h.shutdown()
        }
        host = null
    }

    override fun setFastForward(speed: Int) { host?.fastForward = speed }

    override fun setCoreOption(key: String, value: String) {
        CoreOptions.setValue(def.id, key, value)
    }

    override fun coreOptionsReady(): Boolean =
        CoreOptions.definitions(def.id).isNotEmpty()

    override fun notifyOptionsChanged() { host?.notifyVariablesChanged() }

    override fun saveState(slot: Int, dst: File): Boolean = host?.saveState(dst) ?: false
    override fun loadState(slot: Int, src: File): Boolean = host?.loadState(src) ?: false

    override fun coreTitle(): String {
        val h = host ?: return def.name
        return h.coreName + if (h.coreVersion.isNotBlank()) " ${h.coreVersion}" else ""
    }

    private var _lastError = ""
    override fun lastError(): String = host?.lastError?.ifBlank { _lastError } ?: _lastError

    override var onFrame: (() -> Unit)?
        get() = host?.onFrame
        set(v) { host?.onFrame = v }
}
