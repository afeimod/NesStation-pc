package com.nesstation.app.core.libretro

import com.sun.jna.Callback
import com.sun.jna.Library
import com.sun.jna.Structure

/**
 * libretro 标准 API 的 JNA 绑定（libretro.h 的 Kotlin 投影）。
 *
 * PC 版核心加载方式与 Android 版「dlopen 预编译核心」同构：
 * JNA Native.load 直接加载 fceumm_libretro.dll / .so / .dylib，
 * 统一走一套 retro_* 入口（对应原版 core/jni/nes_loader.cpp 的角色）。
 */

// ================= 常量 =================

object LR {
    // 设备类型
    const val DEVICE_NONE = 0
    const val DEVICE_JOYPAD = 1
    const val DEVICE_MOUSE = 2
    const val DEVICE_KEYBOARD = 3
    const val DEVICE_LIGHTGUN = 4
    const val DEVICE_ANALOG = 5
    const val DEVICE_POINTER = 6

    // Joypad 按键 ID
    const val ID_JOYPAD_B = 0
    const val ID_JOYPAD_Y = 1
    const val ID_JOYPAD_SELECT = 2
    const val ID_JOYPAD_START = 3
    const val ID_JOYPAD_UP = 4
    const val ID_JOYPAD_DOWN = 5
    const val ID_JOYPAD_LEFT = 6
    const val ID_JOYPAD_RIGHT = 7
    const val ID_JOYPAD_A = 8
    const val ID_JOYPAD_X = 9
    const val ID_JOYPAD_L = 10
    const val ID_JOYPAD_R = 11
    const val ID_JOYPAD_L2 = 12
    const val ID_JOYPAD_R2 = 13
    const val ID_JOYPAD_L3 = 14
    const val ID_JOYPAD_R3 = 15
    const val ID_JOYPAD_MASK = 256

    // 内存区域
    const val MEMORY_SAVE_RAM = 0
    const val MEMORY_RTC = 1
    const val MEMORY_SYSTEM_RAM = 2
    const val MEMORY_VIDEO_RAM = 3

    // 像素格式
    const val PIXEL_0RGB1555 = 0
    const val PIXEL_XRGB8888 = 1
    const val PIXEL_RGB565 = 2

    // Environment 命令（libretro.h）
    const val ENV_SET_ROTATION = 1
    const val ENV_GET_OVERSCAN = 2
    const val ENV_GET_CAN_DUPE = 3
    const val ENV_SET_MESSAGE = 6
    const val ENV_SHUTDOWN = 7
    const val ENV_SET_PERFORMANCE_LEVEL = 8
    const val ENV_GET_SYSTEM_DIRECTORY = 9
    const val ENV_SET_PIXEL_FORMAT = 10
    const val ENV_SET_INPUT_DESCRIPTORS = 11
    const val ENV_SET_KEYBOARD_CALLBACK = 12
    const val ENV_SET_DISK_CONTROL_INTERFACE = 13
    const val ENV_SET_HW_RENDER = 14
    const val ENV_GET_VARIABLE = 15
    const val ENV_SET_VARIABLES = 16
    const val ENV_GET_VARIABLE_UPDATE = 17
    const val ENV_SET_SUPPORT_NO_GAME = 18
    const val ENV_GET_LIBRETRO_PATH = 19
    const val ENV_SET_FRAME_TIME_CALLBACK = 21
    const val ENV_SET_AUDIO_CALLBACK = 22
    const val ENV_GET_RUMBLE_INTERFACE = 23
    const val ENV_GET_INPUT_DEVICE_CAPABILITIES = 24
    const val ENV_GET_LOG_INTERFACE = 27
    const val ENV_GET_PERF_INTERFACE = 28
    const val ENV_GET_SAVE_DIRECTORY = 31
    const val ENV_SET_SYSTEM_AV_INFO = 32
    const val ENV_SET_GEOMETRY = 33
    const val ENV_GET_USERNAME = 34
    const val ENV_GET_LANGUAGE = 35
    const val ENV_GET_CURRENT_SOFTWARE_FRAMEBUFFER = 40
    const val ENV_GET_HW_RENDER_INTERFACE = 41
    const val ENV_SET_SUPPORT_ACHIEVEMENTS = 42
    const val ENV_SET_CONTROLLER_INFO = 43
    const val ENV_SET_MEMORY_MAPS = 44 or 0x10000
    const val ENV_GET_INPUT_BITMASKS = 51
    const val ENV_SET_CORE_OPTIONS_V2 = 54
    const val ENV_SET_CORE_OPTIONS_V2_INTL = 55
    const val ENV_SET_CORE_OPTIONS_UPDATE_DISPLAY_CALLBACK = 58
    const val ENV_SET_DISK_CONTROL_EXT_INTERFACE = 60
    const val ENV_GET_MESSAGE_INTERFACE_VERSION = 65
    const val ENV_GET_INPUT_MAX_USERS = 63
}

// ================= 结构体 =================

/** struct retro_system_info */
@Structure.FieldOrder("library_name", "library_version", "valid_extensions", "need_fullpath", "block_extract")
class RetroSystemInfo : Structure() {
    @JvmField var library_name: String? = null
    @JvmField var library_version: String? = null
    @JvmField var valid_extensions: String? = null
    @JvmField var need_fullpath: Boolean = false
    @JvmField var block_extract: Boolean = false
}

/** struct retro_game_geometry */
@Structure.FieldOrder("base_width", "base_height", "max_width", "max_height", "aspect_ratio")
class RetroGameGeometry : Structure {
    @JvmField var base_width: Int = 0
    @JvmField var base_height: Int = 0
    @JvmField var max_width: Int = 0
    @JvmField var max_height: Int = 0
    @JvmField var aspect_ratio: Float = 0f
    constructor() : super()
    constructor(p: com.sun.jna.Pointer?) : super(p)
}

/** struct retro_system_timing */
@Structure.FieldOrder("fps", "sample_rate")
class RetroSystemTiming : Structure() {
    @JvmField var fps: Double = 0.0
    @JvmField var sample_rate: Double = 0.0
}

/** struct retro_system_av_info */
@Structure.FieldOrder("geometry", "timing")
class RetroSystemAvInfo : Structure {
    @JvmField var geometry: RetroGameGeometry = RetroGameGeometry()
    @JvmField var timing: RetroSystemTiming = RetroSystemTiming()
    constructor() : super()
    constructor(p: com.sun.jna.Pointer?) : super(p)
}

/** struct retro_game_info */
@Structure.FieldOrder("path", "data", "size", "meta")
class RetroGameInfo : Structure() {
    @JvmField var path: String? = null
    @JvmField var data: com.sun.jna.Pointer? = null
    @JvmField var size: Long = 0L
    @JvmField var meta: String? = null
}

/** struct retro_variable */
@Structure.FieldOrder("key", "value")
class RetroVariable : Structure {
    @JvmField var key: String? = null
    @JvmField var value: String? = null
    constructor() : super()
    constructor(p: com.sun.jna.Pointer?) : super(p)
}

/** struct retro_core_option_v2_value */
@Structure.FieldOrder("value", "label")
class RetroCoreOptionV2Value : Structure() {
    @JvmField var value: String? = null
    @JvmField var label: String? = null
}

/** struct retro_core_option_v2_definition */
@Structure.FieldOrder("key", "desc", "info", "category_key", "values", "default_value")
class RetroCoreOptionV2Definition : Structure {
    @JvmField var key: String? = null
    @JvmField var desc: String? = null
    @JvmField var info: String? = null
    @JvmField var category_key: String? = null
    @JvmField var values: Array<RetroCoreOptionV2Value> =
        Array(32) { RetroCoreOptionV2Value() }
    @JvmField var default_value: String? = null
    constructor() : super()
    constructor(p: com.sun.jna.Pointer?) : super(p)
}

// ================= 回调 =================

fun interface RetroEnvironmentCallback : Callback {
    fun invoke(cmd: Int, data: com.sun.jna.Pointer?): Boolean
}

fun interface RetroVideoRefreshCallback : Callback {
    /** data 可为 null（帧重复）。pitch 为字节数（size_t，按 64 位映射）。 */
    fun invoke(data: com.sun.jna.Pointer?, width: Int, height: Int, pitch: Long)
}

fun interface RetroAudioSampleCallback : Callback {
    fun invoke(left: Short, right: Short)
}

fun interface RetroAudioBatchCallback : Callback {
    fun invoke(data: com.sun.jna.Pointer?, frames: Long): Long
}

fun interface RetroInputPollCallback : Callback {
    fun invoke()
}

fun interface RetroInputStateCallback : Callback {
    fun invoke(port: Int, device: Int, index: Int, id: Int): Short
}

fun interface RetroKeyboardCallback : Callback {
    fun invoke(down: Boolean, keycode: Int, character: UInt, key: Int)
}

// ================= 核心库接口 =================

/**
 * 每个核心动态库导出的标准 retro_* 符号。
 */
interface LibretroApi : Library {
    fun retro_set_environment(cb: RetroEnvironmentCallback)
    fun retro_set_video_refresh(cb: RetroVideoRefreshCallback)
    fun retro_set_audio_sample(cb: RetroAudioSampleCallback)
    fun retro_set_audio_sample_batch(cb: RetroAudioBatchCallback)
    fun retro_set_input_poll(cb: RetroInputPollCallback)
    fun retro_set_input_state(cb: RetroInputStateCallback)
    fun retro_init()
    fun retro_deinit()
    fun retro_api_version(): Int
    fun retro_get_system_info(info: RetroSystemInfo)
    fun retro_get_system_av_info(info: RetroSystemAvInfo)
    fun retro_set_controller_port_device(port: Int, device: Int)
    fun retro_reset()
    fun retro_run()
    fun retro_serialize_size(): Long
    fun retro_serialize(data: com.sun.jna.Pointer, size: Long): Boolean
    fun retro_unserialize(data: com.sun.jna.Pointer, size: Long): Boolean
    fun retro_cheat_reset()
    fun retro_cheat_set(index: Int, enabled: Boolean, code: String)
    fun retro_load_game(game: RetroGameInfo?): Boolean
    fun retro_unload_game()
    fun retro_get_memory(id: Int): com.sun.jna.Pointer
    fun retro_get_memory_size(id: Int): Long
}
