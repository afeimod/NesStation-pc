package com.nesstation.app.core.engine

import com.nesstation.app.core.libretro.LrFrame
import java.io.File

/**
 * 引擎通用接口（对应 Android 版 EmulatorEngine；PC 版去掉 Surface，
 * 视频统一为拉模型帧快照）。
 */
interface EmulatorEngine {

    val isLoaded: Boolean

    /** 加载 ROM / 启动（外部核心 = 启动进程）。 */
    fun loadRom(rom: File, srmBasename: String): Boolean

    /** 当前帧（拉模型；外部核心恒为 null）。 */
    fun snapshot(): LrFrame?

    /** 视频宽高/比例（外部核心 0）。 */
    fun videoWidth(): Int
    fun videoHeight(): Int

    fun setPaused(paused: Boolean)
    fun reset()
    fun unload()

    /** 快进倍速（0=关）。 */
    fun setFastForward(speed: Int)

    /** 核心选项（进程内核心）。 */
    fun setCoreOption(key: String, value: String)
    fun coreOptionsReady(): Boolean
    fun notifyOptionsChanged()

    /** 即时存档（外部核心返回 false：使用其原生存档系统）。 */
    fun saveState(slot: Int, dst: File): Boolean
    fun loadState(slot: Int, src: File): Boolean

    /** 核心显示名。 */
    fun coreTitle(): String

    /** 最近错误。 */
    fun lastError(): String

    /** 帧回调（触发 UI 重绘）。 */
    var onFrame: (() -> Unit)?
}
