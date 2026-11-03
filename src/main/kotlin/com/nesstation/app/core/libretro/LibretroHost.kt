package com.nesstation.app.core.libretro

import com.nesstation.app.core.audio.AudioOut
import com.nesstation.app.core.input.InputState
import com.nesstation.app.core.storage.Paths
import com.sun.jna.Callback
import com.sun.jna.Memory
import com.sun.jna.Native
import com.sun.jna.Pointer
import java.io.File
import java.util.concurrent.atomic.AtomicBoolean
import kotlin.concurrent.thread

/** 一帧画面的不可变快照（ARGB）。 */
data class LrFrame(val pixels: IntArray, val width: Int, val height: Int)

/**
 * ★ libretro 通用宿主（PC 版核心引擎）★
 *
 * 对应 Android 版 NesEngine/SnesEngine/… 的统一替代：原版每平台一个
 * JNI 桥（libnescore.so dlopen 各自核心），PC 版由本类用 JNA 直接加载
 * buildbot 的 PC 核心（.dll/.so/.dylib），一套宿主跑全部 libretro 平台。
 *
 * 生命周期（retro_* 严格在模拟线程或持 [runLock] 时调用）：
 *   loadCore → retro_set_* → retro_init → retro_load_game
 *   → [模拟线程 retro_run] → retro_unload_game → retro_deinit
 *
 * 渲染：软件帧缓冲（XRGB8888 / RGB565 / 0RGB1555 → ARGB），
 * UI 层每帧 [snapshot] 拉取 —— 与原版 frameBuffer 位图回退同构的拉模型。
 */
class LibretroHost(
    val coreId: String,
    private val coreFile: File
) {

    var api: LibretroApi? = null
        private set

    @Volatile var coreName: String = coreFile.nameWithoutExtension
        private set
    @Volatile var coreVersion: String = ""
        private set

    @Volatile var systemDir: File = Paths.coreSystemDir(coreId)
    @Volatile var saveDir: File = Paths.savesDir

    // ---- 视频 ----
    @Volatile var videoW: Int = 0
        private set
    @Volatile var videoH: Int = 0
        private set
    @Volatile var aspectRatio: Float = 4f / 3f
        private set

    private val frameLock = Any()
    @Volatile private var framePixels: IntArray = IntArray(0)
    @Volatile var frameCount: Long = 0
        private set

    // ---- 音频 ----
    val audio = AudioOut()
    @Volatile var sampleRate = 0
        private set
    @Volatile var fps: Double = 60.0
        private set

    // ---- 系统信息 ----
    @Volatile var needFullpath = false
    @Volatile var supportsNoGame = false

    @Volatile private var pixelFormat = LR.PIXEL_XRGB8888

    // ---- 生命周期 ----
    private val running = AtomicBoolean(false)
    @Volatile private var paused = false
    @Volatile private var shutdownRequested = false
    @Volatile var loaded = false
        private set
    private var emuThread: Thread? = null

    /** retro_run 与存档/选项变更共用的大锁（存档期间安全冻结模拟线程）。 */
    private val runLock = Any()

    /** 快进倍速（0=关闭）。 */
    @Volatile var fastForward = 0

    @Volatile private var variablesDirty = false

    /** 键盘直通回调（DOSBox-Pure 等：核心请求 DOS 键盘事件）。 */
    @Volatile var keyboardSink: ((down: Boolean, keycode: Int) -> Unit)? = null

    /** 最近一条 SET_MESSAGE（UI 提示用）。 */
    @Volatile var lastMessage: String = ""
        private set

    @Volatile var lastError: String = ""
        private set

    private var romData: Memory? = null
    private var srmFile: File? = null

    /** UI 每帧回调（触发重绘）。 */
    @Volatile var onFrame: (() -> Unit)? = null

    /** env 回调中分配的本地字符串/键盘回调内存（防 GC 回收）。 */
    private val nativeRefs = ArrayList<Memory>()

    private val rgb565Lut = IntArray(65536).also { lut ->
        for (i in 0 until 65536) {
            val r = ((i shr 11) and 0x1F) shl 3
            val g = ((i shr 5) and 0x3F) shl 2
            val b = (i and 0x1F) shl 3
            lut[i] = 0xFF000000.toInt() or (r shl 16) or (g shl 8) or b
        }
    }

    // ============================================================
    // 回调实现
    // ============================================================

    private val envCb = RetroEnvironmentCallback { cmd, data ->
        try { environment(cmd, data) } catch (t: Throwable) {
            lastError = "env(0x${cmd.toString(16)}): ${t.message}"
            false
        }
    }

    private fun environment(cmd: Int, data: Pointer?): Boolean = when (cmd) {
        LR.ENV_GET_OVERSCAN -> { data?.setInt(0, 1); true }
        LR.ENV_GET_CAN_DUPE -> { data?.setInt(0, 1); true }
        LR.ENV_SET_ROTATION -> true
        LR.ENV_SET_MESSAGE -> {
            val p = runCatching { data?.getPointer(0) }.getOrNull()
            val msg = p?.getString(0)
            if (!msg.isNullOrBlank()) lastMessage = msg
            true
        }
        LR.ENV_SHUTDOWN -> { shutdownRequested = true; true }
        LR.ENV_SET_PERFORMANCE_LEVEL -> true
        LR.ENV_GET_SYSTEM_DIRECTORY -> {
            systemDir.mkdirs(); writeStringPtr(data, systemDir.absolutePath)
        }
        LR.ENV_GET_SAVE_DIRECTORY -> {
            saveDir.mkdirs(); writeStringPtr(data, saveDir.absolutePath)
        }
        LR.ENV_SET_PIXEL_FORMAT -> { pixelFormat = data?.getInt(0) ?: LR.PIXEL_XRGB8888; true }
        LR.ENV_SET_INPUT_DESCRIPTORS -> true
        LR.ENV_SET_KEYBOARD_CALLBACK -> setKeyboardCallback(data)
        LR.ENV_SET_DISK_CONTROL_INTERFACE, LR.ENV_SET_DISK_CONTROL_EXT_INTERFACE -> true
        LR.ENV_SET_HW_RENDER -> {
            lastError = "核心请求硬件渲染上下文（本宿主为软件渲染；该核心请使用外置版本）"
            false
        }
        LR.ENV_GET_VARIABLE -> {
            if (data == null) false else {
                val v = RetroVariable(data)
                v.read()
                val key = v.key ?: return@environment false
                v.value = CoreOptions.effectiveValue(coreId, key)
                v.write()
                true
            }
        }
        LR.ENV_SET_VARIABLES -> { parseV1Variables(data); true }
        LR.ENV_GET_VARIABLE_UPDATE -> {
            data?.setInt(0, if (variablesDirty) 1 else 0)
            variablesDirty = false
            true
        }
        LR.ENV_SET_SUPPORT_NO_GAME -> { supportsNoGame = data?.getInt(0) == 1; true }
        LR.ENV_GET_LIBRETRO_PATH -> writeStringPtr(data, coreFile.absolutePath)
        LR.ENV_SET_FRAME_TIME_CALLBACK, LR.ENV_SET_AUDIO_CALLBACK -> false
        LR.ENV_GET_RUMBLE_INTERFACE -> false
        LR.ENV_GET_INPUT_DEVICE_CAPABILITIES -> { data?.setLong(0, 1L shl LR.DEVICE_JOYPAD); true }
        LR.ENV_GET_INPUT_BITMASKS -> true
        LR.ENV_GET_LOG_INTERFACE, LR.ENV_GET_PERF_INTERFACE -> false
        LR.ENV_SET_SYSTEM_AV_INFO -> {
            if (data != null) {
                val av = RetroSystemAvInfo(data); av.read(); applyAvInfo(av)
            }
            true
        }
        LR.ENV_SET_GEOMETRY -> {
            if (data != null) {
                val g = RetroGameGeometry(data); g.read()
                if (g.base_width > 0 && g.base_height > 0) {
                    videoW = g.base_width; videoH = g.base_height
                    if (g.aspect_ratio > 0f) aspectRatio = g.aspect_ratio
                }
            }
            true
        }
        LR.ENV_GET_USERNAME -> writeStringPtr(data, System.getProperty("user.name") ?: "player")
        LR.ENV_GET_LANGUAGE -> { data?.setInt(0, 0); true }
        LR.ENV_GET_CURRENT_SOFTWARE_FRAMEBUFFER -> false
        LR.ENV_GET_HW_RENDER_INTERFACE -> false
        LR.ENV_SET_SUPPORT_ACHIEVEMENTS -> true
        LR.ENV_SET_CONTROLLER_INFO -> true
        LR.ENV_SET_MEMORY_MAPS -> true
        LR.ENV_SET_CORE_OPTIONS_V2 -> { parseV2Options(data); true }
        LR.ENV_SET_CORE_OPTIONS_V2_INTL -> { parseV2Options(data); true }
        LR.ENV_SET_CORE_OPTIONS_UPDATE_DISPLAY_CALLBACK -> true
        LR.ENV_GET_INPUT_MAX_USERS -> { data?.setInt(0, 2); true }
        LR.ENV_GET_MESSAGE_INTERFACE_VERSION -> { data?.setInt(0, 1); true }
        else -> false
    }

    private fun writeStringPtr(data: Pointer?, s: String): Boolean {
        if (data == null) return false
        val mem = Memory((s.length + 8).toLong())
        mem.setString(0, s)
        data.setPointer(0, mem)
        synchronized(nativeRefs) {
            nativeRefs.add(mem)
            while (nativeRefs.size > 256) nativeRefs.removeAt(0)
        }
        return true
    }

    private fun setKeyboardCallback(data: Pointer?): Boolean {
        if (data == null) return false
        val cb = object : Callback {
            @Suppress("unused")
            fun invoke(down: Boolean, keycode: Int, character: Int, key: Int) {
                keyboardSink?.invoke(down, keycode)
            }
        }
        val holder = Memory(Native.POINTER_SIZE.toLong())
        holder.setPointer(0, CallbackReference.getFunctionPointer(cb))
        data.setPointer(0, holder)
        synchronized(nativeRefs) { nativeRefs.add(holder) }
        return true
    }

    private fun applyAvInfo(av: RetroSystemAvInfo) {
        videoW = av.geometry.base_width
        videoH = av.geometry.base_height
        if (av.geometry.aspect_ratio > 0f) aspectRatio = av.geometry.aspect_ratio
        fps = av.timing.fps.takeIf { it > 1.0 } ?: 60.0
        val rate = av.timing.sample_rate.toInt()
        if (rate > 0) {
            if (sampleRate != rate) {
                sampleRate = rate
                if (loaded) audio.start(rate)
            }
        }
    }

    // ---- 核心选项解析（v1 SET_VARIABLES / v2 SET_CORE_OPTIONS_V2） ----

    private fun parseV1Variables(data: Pointer?) {
        if (data == null) return
        val defs = ArrayList<CoreOptionDef>()
        var off = 0L
        val step = Native.POINTER_SIZE.toLong() * 2
        while (defs.size < 512) {
            val keyPtr = runCatching { data.getPointer(off) }.getOrNull() ?: break
            val key = runCatching { keyPtr.getString(0) }.getOrNull() ?: break
            if (key.isBlank()) break
            val valuePtr = runCatching { data.getPointer(off + Native.POINTER_SIZE.toLong()) }.getOrNull()
            val valueDesc = valuePtr?.getString(0) ?: ""
            // v1 值格式："描述; 默认|选项1|选项2|…"
            val parts = valueDesc.split(";")
            val desc = parts.getOrNull(0)?.trim().orEmpty().ifEmpty { key }
            val opts = (parts.getOrNull(1) ?: "").trim().split("|").filter { it.isNotBlank() }
            defs.add(
                CoreOptionDef(
                    key = key, desc = desc,
                    choices = opts.map { CoreOptionChoice(it) },
                    default = opts.firstOrNull().orEmpty()
                )
            )
            off += step
        }
        if (defs.isNotEmpty()) CoreOptions.setDefinitions(coreId, defs)
    }

    private fun parseV2Options(data: Pointer?) {
        if (data == null) return
        // struct retro_core_options_v2 { categories*; definitions*; }
        val defsPtr = data.getPointer(Native.POINTER_SIZE.toLong()) ?: return
        val defs = ArrayList<CoreOptionDef>()
        val proto = RetroCoreOptionV2Definition()
        val stride = proto.size().toLong()
        var i = 0
        while (i < 512) {
            val d = RetroCoreOptionV2Definition(defsPtr.share(i * stride))
            runCatching { d.read() }.onFailure { return }
            val key = d.key ?: break
            if (key.isBlank()) break
            val choices = ArrayList<CoreOptionChoice>()
            for (c in d.values) {
                val v = c.value ?: break
                choices.add(CoreOptionChoice(v, c.label ?: v))
            }
            defs.add(
                CoreOptionDef(
                    key = key,
                    desc = d.desc ?: key,
                    info = d.info ?: "",
                    category = d.category_key ?: "",
                    choices = choices,
                    default = d.default_value ?: choices.firstOrNull()?.value.orEmpty()
                )
            )
            i++
        }
        if (defs.isNotEmpty()) CoreOptions.setDefinitions(coreId, defs)
    }

    // ---- 视频 / 音频 / 输入回调 ----

    private val rowBytes = ByteArray(4096 * 4)

    private val videoCb = RetroVideoRefreshCallback { data, w, h, pitch ->
        if (data == null || w <= 0 || h <= 0 || w > 4096 || h > 4096) return@RetroVideoRefreshCallback
        val need = w * h
        if (framePixels.size != need) framePixels = IntArray(need)
        synchronized(frameLock) {
            when (pixelFormat) {
                LR.PIXEL_XRGB8888 -> {
                    val rowLen = w * 4
                    val buf = if (rowBytes.size < rowLen) ByteArray(rowLen).also { } else rowBytes
                    for (y in 0 until h) {
                        data.read(y * pitch, buf, 0, rowLen)
                        var o = 0
                        val base = y * w
                        for (x in 0 until w) {
                            framePixels[base + x] = 0xFF000000.toInt() or
                                ((buf[o].toInt() and 0xFF) shl 16) or
                                ((buf[o + 1].toInt() and 0xFF) shl 8) or
                                (buf[o + 2].toInt() and 0xFF)
                            o += 4
                        }
                    }
                }
                LR.PIXEL_RGB565, LR.PIXEL_0RGB1555 -> {
                    val rowLen = w * 2
                    val buf = if (rowBytes.size < rowLen) ByteArray(rowLen) else rowBytes
                    for (y in 0 until h) {
                        data.read(y * pitch, buf, 0, rowLen)
                        var o = 0
                        val base = y * w
                        for (x in 0 until w) {
                            val lo = buf[o].toInt() and 0xFF
                            val hi = buf[o + 1].toInt() and 0xFF
                            val s = (hi shl 8) or lo
                            framePixels[base + x] = if (pixelFormat == LR.PIXEL_RGB565) rgb565Lut[s]
                            else {
                                val r = ((s shr 10) and 0x1F) shl 3
                                val g = ((s shr 5) and 0x3F) shl 2
                                val b = (s and 0x1F) shl 3
                                0xFF000000.toInt() or (r shl 16) or (g shl 8) or b
                            }
                            o += 2
                        }
                    }
                }
            }
            frameCount++
        }
        onFrame?.invoke()
    }

    private val audioBatchCb = RetroAudioBatchCallback { data, frames ->
        if (data == null || frames <= 0) return@RetroAudioBatchCallback 0L
        val n = frames.toInt() * 2
        val samples = ShortArray(n)
        data.read(0, samples, 0, n * 2)
        audio.write(samples, frames.toInt())
        frames
    }

    private val audioSampleCb = RetroAudioSampleCallback { l, r ->
        audio.write(shortArrayOf(l, r), 1)
    }

    private val inputPollCb = RetroInputPollCallback { }

    private val inputStateCb = RetroInputStateCallback { port, device, _index, id ->
        if (device == LR.DEVICE_JOYPAD) {
            val pad = InputState.pad(port)
            if (id == LR.ID_JOYPAD_MASK) pad.toShort()
            else ((pad shr id) and 1).toShort()
        } else 0
    }

    // ============================================================
    // 公开生命周期
    // ============================================================

    /** 加载核心库并 init。 */
    fun loadCore(): Boolean {
        if (api != null) return true
        return try {
            val lib = Native.load(coreFile.absolutePath, LibretroApi::class.java)
            api = lib
            lib.retro_set_environment(envCb)
            val ver = lib.retro_api_version()
            if (ver != 1) {
                lastError = "核心 API 版本不兼容（期望 1，实际 $ver）"
                api = null
                return false
            }
            lib.retro_init()
            true
        } catch (t: Throwable) {
            lastError = "加载核心失败: ${t.message}"
            api = null
            false
        }
    }

    /** 读取核心信息（loadCore 后调用一次）。 */
    fun readSystemInfo() {
        val lib = api ?: return
        runCatching {
            val info = RetroSystemInfo()
            lib.retro_get_system_info(info)
            info.read()
            coreName = info.library_name ?: coreName
            coreVersion = info.library_version ?: ""
            needFullpath = info.need_fullpath
        }
    }

    /** 加载 ROM 并启动模拟线程。 */
    fun loadRom(rom: File, srmBasename: String): Boolean {
        val lib = api ?: return false
        if (!loadCore()) return false
        shutdownRequested = false
        srmFile = File(saveDir, "$srmBasename.srm")

        val game = RetroGameInfo()
        game.path = rom.absolutePath
        if (!needFullpath) {
            val bytes = runCatching { rom.readBytes() }.getOrElse {
                lastError = "读取 ROM 失败: ${it.message}"
                return false
            }
            val mem = Memory(bytes.size.toLong())
            mem.write(0, bytes, 0, bytes.size)
            romData = mem
            game.data = mem
            game.size = bytes.size.toLong()
        }
        val ok = runCatching { lib.retro_load_game(game) }.getOrDefault(false)
        if (!ok) {
            if (lastError.isBlank()) lastError = "核心无法加载该 ROM（格式或 BIOS 不匹配）"
            return false
        }
        loaded = true
        lib.retro_set_controller_port_device(0, LR.DEVICE_JOYPAD)
        lib.retro_set_controller_port_device(1, LR.DEVICE_JOYPAD)

        val av = RetroSystemAvInfo()
        runCatching {
            lib.retro_get_system_av_info(av)
            av.read()
            applyAvInfo(av)
        }
        restoreSrm()
        audio.start(sampleRate.takeIf { it > 0 } ?: 48000)

        running.set(true)
        paused = false
        emuThread = Thread({ emuLoop(lib) }, "libretro-$coreId").apply {
            isDaemon = true
            start()
        }
        return true
    }

    private fun emuLoop(lib: LibretroApi) {
        var next = System.nanoTime()
        var localFrames = 0L
        val period = { (1_000_000_000.0 / fps).toLong() }
        while (running.get() && !shutdownRequested) {
            if (paused) {
                try { Thread.sleep(16) } catch (_: InterruptedException) { break }
                next = System.nanoTime()
                continue
            }
            synchronized(runLock) {
                try {
                    lib.retro_run()
                } catch (t: Throwable) {
                    lastError = "核心运行错误: ${t.message}"
                    running.set(false)
                    return
                }
            }
            localFrames++
            val ff = fastForward
            if (ff <= 1 || localFrames % ff == 0L) {
                next += period()
            }
            val now = System.nanoTime()
            if (next > now) {
                val sleepMs = ((next - now) / 1_000_000) - 1
                if (sleepMs > 0) {
                    try { Thread.sleep(sleepMs) } catch (_: InterruptedException) { break }
                }
                while (running.get() && System.nanoTime() < next) { /* 精确自旋 */ }
            } else if (now - next > 200_000_000L) {
                next = now // 落后太多重新对表
            }
        }
    }

    fun setPaused(p: Boolean) {
        paused = p
        if (p) audio.flush()
    }

    val isPaused: Boolean get() = paused

    fun reset() {
        synchronized(runLock) { runCatching { api?.retro_reset() } }
    }

    // ---- 即时存档（runLock 冻结模拟线程，绝对安全） ----

    fun saveState(dst: File): Boolean {
        val lib = api ?: return false
        synchronized(runLock) {
            return try {
                val size = lib.retro_serialize_size()
                if (size <= 0) { lastError = "该核心不支持即时存档"; return false }
                val mem = Memory(size)
                if (!runCatching { lib.retro_serialize(mem, size) }.getOrDefault(false)) {
                    lastError = "核心序列化状态失败"
                    return false
                }
                dst.writeBytes(mem.getByteArray(0, size.toInt()))
                true
            } catch (t: Throwable) {
                lastError = "存档失败: ${t.message}"
                false
            }
        }
    }

    fun loadState(src: File): Boolean {
        val lib = api ?: return false
        synchronized(runLock) {
            return try {
                val bytes = src.readBytes()
                val mem = Memory(bytes.size.toLong())
                mem.write(0, bytes, 0, bytes.size)
                runCatching { lib.retro_unserialize(mem, bytes.size.toLong()) }.getOrDefault(false)
            } catch (t: Throwable) {
                lastError = "读档失败: ${t.message}"
                false
            }
        }
    }

    // ---- SRAM（电池存档） ----

    private fun restoreSrm() {
        val lib = api ?: return
        val f = srmFile ?: return
        if (!f.isFile) return
        runCatching {
            val size = lib.retro_get_memory_size(LR.MEMORY_SAVE_RAM)
            if (size <= 0) return
            val p = lib.retro_get_memory(LR.MEMORY_SAVE_RAM) ?: return
            val bytes = f.readBytes()
            p.write(0, bytes, 0, minOf(bytes.size, size.toInt()))
        }
    }

    private fun persistSrm() {
        val lib = api ?: return
        val f = srmFile ?: return
        runCatching {
            val size = lib.retro_get_memory_size(LR.MEMORY_SAVE_RAM)
            if (size > 0) {
                val p = lib.retro_get_memory(LR.MEMORY_SAVE_RAM)
                if (p != null) f.writeBytes(p.getByteArray(0, size.toInt()))
            }
        }
    }

    /** UI 每帧拉取画面快照（内部拷贝，避免撕裂）。 */
    fun snapshot(): LrFrame? {
        val w = videoW
        val h = videoH
        val px = framePixels
        if (w <= 0 || h <= 0 || px.size < w * h) return null
        val out = IntArray(w * h)
        synchronized(frameLock) { System.arraycopy(px, 0, out, 0, w * h) }
        return LrFrame(out, w, h)
    }

    /** 核心选项被 UI 修改后调用（下一帧让核心重读）。 */
    fun notifyVariablesChanged() { variablesDirty = true }

    fun unload() {
        running.set(false)
        try { emuThread?.join(800) } catch (_: InterruptedException) { }
        emuThread = null
        val lib = api ?: return
        if (loaded) {
            synchronized(runLock) {
                persistSrm()
                runCatching { lib.retro_unload_game() }
            }
        }
        loaded = false
        romData = null
        audio.stop()
    }

    fun shutdown() {
        unload()
        api?.let { lib ->
            runCatching { lib.retro_deinit() }
        }
        api = null
        synchronized(nativeRefs) { nativeRefs.clear() }
    }

    private object CallbackReference {
        fun getFunctionPointer(cb: Callback): Pointer =
            com.sun.jna.CallbackReference.getFunctionPointer(cb)
    }
}
