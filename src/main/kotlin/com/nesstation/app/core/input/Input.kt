package com.nesstation.app.core.input

import com.nesstation.app.core.model.GamePlatform
import com.nesstation.app.core.storage.KeyMapStore
import java.awt.event.KeyEvent
import java.util.concurrent.ConcurrentHashMap

/**
 * 手柄按键（与 libretro RETRO_DEVICE_ID_JOYPAD_* 对齐的统一按钮空间）。
 * 所有平台共用同一按钮集，按平台渲染不同的默认键位（对应 Android 版 PadBits）。
 */
enum class PadButton(val label: String) {
    UP("上"), DOWN("下"), LEFT("左"), RIGHT("右"),
    A("A"), B("B"), X("X"), Y("Y"),
    L("L"), R("R"), L2("L2"), R2("R2"), L3("L3"), R3("R3"),
    START("Start"), SELECT("Select")
}

/** 统一输入状态：键盘（玩家1）+ 手柄（每玩家）合并后的位掩码。 */
object InputState {

    // 位定义（libretro joypad id 顺序）
    const val BIT_B = 1 shl 0
    const val BIT_Y = 1 shl 1
    const val BIT_SELECT = 1 shl 2
    const val BIT_START = 1 shl 3
    const val BIT_UP = 1 shl 4
    const val BIT_DOWN = 1 shl 5
    const val BIT_LEFT = 1 shl 6
    const val BIT_RIGHT = 1 shl 7
    const val BIT_A = 1 shl 8
    const val BIT_X = 1 shl 9
    const val BIT_L = 1 shl 10
    const val BIT_R = 1 shl 11
    const val BIT_L2 = 1 shl 12
    const val BIT_R2 = 1 shl 13
    const val BIT_L3 = 1 shl 14
    const val BIT_R3 = 1 shl 15

    fun bitOf(b: PadButton): Int = when (b) {
        PadButton.B -> BIT_B; PadButton.Y -> BIT_Y
        PadButton.SELECT -> BIT_SELECT; PadButton.START -> BIT_START
        PadButton.UP -> BIT_UP; PadButton.DOWN -> BIT_DOWN
        PadButton.LEFT -> BIT_LEFT; PadButton.RIGHT -> BIT_RIGHT
        PadButton.A -> BIT_A; PadButton.X -> BIT_X
        PadButton.L -> BIT_L; PadButton.R -> BIT_R
        PadButton.L2 -> BIT_L2; PadButton.R2 -> BIT_R2
        PadButton.L3 -> BIT_L3; PadButton.R3 -> BIT_R3
    }

    /** 键盘位掩码（仅玩家 1；独立于手柄，最终按位 OR 合并）。 */
    @Volatile private var keyboardBits = 0

    /** 手柄位掩码（每玩家）。 */
    private val pads = ConcurrentHashMap<Int, Int>()

    fun pad(port: Int): Int =
        if (port == 0) (pads[0] ?: 0) or keyboardBits else (pads[port] ?: 0)

    /** 手柄线程：整组写入该玩家的位掩码。 */
    fun setPad(port: Int, bits: Int) { pads[port] = bits }

    /** 键盘事件：单键按下/抬起（玩家 1）。 */
    fun applyKeyboard(button: PadButton, pressed: Boolean) {
        val bit = bitOf(button)
        keyboardBits = if (pressed) keyboardBits or bit else keyboardBits and bit.inv()
    }

    /** 游戏退出时清空全部输入。 */
    fun clearAll() { keyboardBits = 0; pads.clear() }
}

/**
 * 键盘默认键位 + 自定义覆盖（对应 Android 版各平台默认手柄映射的 PC 键盘等价物）。
 *
 * 通用布局（RetroArch 桌面习惯）：
 *   方向 = 方向键或 WASD；Z=B X=A A=Y S=X；Q=L E=R；1=L2 3=R2；
 *   Enter=Start 右Shift=Select；C=L3 V=R3。
 * 街机（FBNeo）6 键布局：A S D = 弱中重拳，Z X C = 弱中重脚（映射到 A/X/B/Y/L/R）。
 */
object KeyboardMap {

    /** 平台 → 默认 AWT 键位。 */
    fun defaultMap(platform: GamePlatform): Map<PadButton, Int> {
        val base = linkedMapOf(
            PadButton.UP to KeyEvent.VK_UP,
            PadButton.DOWN to KeyEvent.VK_DOWN,
            PadButton.LEFT to KeyEvent.VK_LEFT,
            PadButton.RIGHT to KeyEvent.VK_RIGHT,
            PadButton.A to KeyEvent.VK_X,
            PadButton.B to KeyEvent.VK_Z,
            PadButton.X to KeyEvent.VK_S,
            PadButton.Y to KeyEvent.VK_A,
            PadButton.L to KeyEvent.VK_Q,
            PadButton.R to KeyEvent.VK_E,
            PadButton.START to KeyEvent.VK_ENTER,
            PadButton.SELECT to KeyEvent.VK_SHIFT
        )
        return when (platform) {
            // 街机 6 键：ASD 拳(轻中重=Z X L)、ZXC 脚(轻中重=B Y R)，
            // Coin=Select, Start=1（街机习惯）
            GamePlatform.ARCADE -> base + mapOf(
                PadButton.A to KeyEvent.VK_D,
                PadButton.B to KeyEvent.VK_Z,
                PadButton.X to KeyEvent.VK_A,
                PadButton.Y to KeyEvent.VK_X,
                PadButton.L to KeyEvent.VK_S,
                PadButton.R to KeyEvent.VK_C,
                PadButton.START to KeyEvent.VK_1,
                PadButton.SELECT to KeyEvent.VK_5
            )
            // NDS：A/B/X/Y + L/R + Start/Select，触屏用鼠标（melonDS 软件光标）
            GamePlatform.NDS -> base
            // PSX/PS2/PSP/DC：完整 12 键（含 L2/R2/L3/R3）
            GamePlatform.PSX, GamePlatform.PS2, GamePlatform.PSP, GamePlatform.DC,
            GamePlatform.NGCWII, GamePlatform.N3DS -> base + mapOf(
                PadButton.L2 to KeyEvent.VK_1,
                PadButton.R2 to KeyEvent.VK_3,
                PadButton.L3 to KeyEvent.VK_C,
                PadButton.R3 to KeyEvent.VK_V
            )
            else -> base
        }
    }

    /** 生效键位：默认 + 用户覆盖（actionId = platform_button）。 */
    fun effectiveMap(platform: GamePlatform): Map<PadButton, Int> {
        val defaults = defaultMap(platform)
        val pid = platform.name.lowercase()
        return defaults.mapValues { (btn, def) ->
            KeyMapStore.get("${pid}_${btn.name.lowercase()}") ?: def
        }
    }

    /** AWT 按键事件 → PadButton。 */
    fun buttonFor(platform: GamePlatform, keyCode: Int): PadButton? =
        effectiveMap(platform).firstNotNullOfOrNull { (b, k) -> if (k == keyCode) b else null }
}
