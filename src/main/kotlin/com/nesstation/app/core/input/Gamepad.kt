package com.nesstation.app.core.input

import com.sun.jna.Library
import com.sun.jna.Native
import com.sun.jna.Pointer
import com.sun.jna.Structure
import java.io.File
import java.util.concurrent.CopyOnWriteArrayList
import kotlin.concurrent.thread

/**
 * PC 手柄输入：
 *  - Windows：XInput（系统自带 xinput1_4.dll，Xbox 系手柄 / 多数 PC 手柄即插即用）
 *  - Linux：/dev/input/js*（内核 joydev，无需任何依赖）
 *  - macOS：暂不启用（键盘完整可用；可外接工具映射）
 *
 * 后台线程 60Hz 轮询，直接合入 [InputState]（玩家 1）。
 * 任何平台初始化失败都静默降级，绝不影响主程序。
 */
object Gamepad {

    /** 控制器按键 → 标准按钮（XInput 语义）。 */
    private val listeners = CopyOnWriteArrayList<() -> Unit>()
    fun addPollListener(l: () -> Unit) { listeners.add(l) }

    @Volatile private var running = false
    @Volatile var attachedNames: List<String> = emptyList()
        private set

    /** 手柄是否可用（用于设置页/按键映射页的状态显示）。 */
    val available: Boolean get() = attachedNames.isNotEmpty()

    fun start() {
        if (running) return
        running = true
        val os = System.getProperty("os.name").lowercase()
        when {
            os.contains("win") -> startXinput()
            os.contains("linux") -> startLinuxJs()
            else -> { /* macOS：键盘模式 */ }
        }
    }

    fun stop() { running = false }

    // ================= Windows XInput =================

    private fun startXinput() {
        val lib = runCatching {
            Native.load(
                if (File("C:/Windows/System32/xinput1_4.dll").isFile) "xinput1_4" else "xinput9_1_0",
                XInputLib::class.java
            )
        }.getOrNull() ?: return

        thread(name = "xinput-poll", isDaemon = true) {
            val state = XInputState()
            val prev = IntArray(4)
            while (running) {
                var names = ArrayList<String>()
                for (u in 0 until 4) {
                    val ok = runCatching { lib.XInputGetState(u, state) }.getOrDefault(-1)
                    if (ok == 0) {
                        names.add("XInput 玩家 ${u + 1}")
                        val bits = translateXInput(state.Gamepad.wButtons)
                        InputState.setPad(u, bits)
                    } else {
                        InputState.setPad(u, 0)
                    }
                }
                attachedNames = names
                if (names.isEmpty()) names = ArrayList()
                Thread.sleep(16)
            }
        }
    }

    private fun translateXInput(w: Short): Int {
        var b = 0
        if (w.toInt() and 0x0001 != 0) b = b or InputState.BIT_UP
        if (w.toInt() and 0x0002 != 0) b = b or InputState.BIT_DOWN
        if (w.toInt() and 0x0004 != 0) b = b or InputState.BIT_LEFT
        if (w.toInt() and 0x0008 != 0) b = b or InputState.BIT_RIGHT
        if (w.toInt() and 0x1000 != 0) b = b or InputState.BIT_A
        if (w.toInt() and 0x2000 != 0) b = b or InputState.BIT_B
        if (w.toInt() and 0x4000 != 0) b = b or InputState.BIT_X
        if (w.toInt() and 0x8000 != 0) b = b or InputState.BIT_Y
        if (w.toInt() and 0x0100 != 0) b = b or InputState.BIT_START
        if (w.toInt() and 0x0200 != 0) b = b or InputState.BIT_SELECT
        if (w.toInt() and 0x0400 != 0) b = b or InputState.BIT_L
        if (w.toInt() and 0x0800 != 0) b = b or InputState.BIT_R
        if (w.toInt() and 0x0040 != 0) b = b or InputState.BIT_L3
        if (w.toInt() and 0x0080 != 0) b = b or InputState.BIT_R3
        return b
    }

    private interface XInputLib : Library {
        fun XInputGetState(userIndex: Int, state: XInputState): Int
    }

    @Structure.FieldOrder("dwPacketNumber", "Gamepad")
    class XInputState : Structure() {
        @JvmField var dwPacketNumber: Int = 0
        @JvmField var Gamepad: XInputGamepad = XInputGamepad()
    }

    @Structure.FieldOrder("wButtons", "bLeftTrigger", "bRightTrigger", "sThumbLX", "sThumbLY", "sThumbRX", "sThumbRY")
    class XInputGamepad : Structure() {
        @JvmField var wButtons: Short = 0
        @JvmField var bLeftTrigger: Byte = 0
        @JvmField var bRightTrigger: Byte = 0
        @JvmField var sThumbLX: Short = 0
        @JvmField var sThumbLY: Short = 0
        @JvmField var sThumbRX: Short = 0
        @JvmField var sThumbRY: Short = 0
    }

    // ================= Linux /dev/input/js* =================

    private fun startLinuxJs() {
        thread(name = "js-poll", isDaemon = true) {
            val devs = File("/dev/input").listFiles { f -> f.name.startsWith("js") } ?: emptyArray()
            if (devs.isEmpty()) return@thread
            val readers = devs.mapNotNull { d -> runCatching { d.inputStream() }.getOrNull()?.let { d.name to it } }
            attachedNames = readers.map { "手柄 ${it.first}" }
            if (readers.isEmpty()) return@thread
            val buf = ByteArray(8)
            val pressed = HashMap<String, Int>() // dev → bits
            while (running) {
                for ((name, ins) in readers) {
                    val n = runCatching {
                        // 非阻塞式读：js 事件 8 字节（time, value, type, number）
                        if (ins.available() >= 8) ins.readNBytes(buf, 0, 8) else 0
                    }.getOrDefault(0)
                    if (n == 8) {
                        val value = buf[4].toInt()
                        val type = buf[6].toInt() and 0xFF
                        val number = buf[7].toInt() and 0xFF
                        if (type == 0x01) { // JS_EVENT_BUTTON
                            val bit = jsBit(number)
                            if (bit != 0) {
                                val cur = pressed[name] ?: 0
                                pressed[name] = if (value > 0) cur or bit else cur and bit.inv()
                            }
                        }
                    }
                }
                val total = pressed.values.fold(0) { acc, i -> acc or i }
                InputState.setPad(0, total)
                Thread.sleep(4)
            }
        }
    }

    /** Linux js 标准按键号 → 按钮位（Linux Gamepad Specification）。 */
    private fun jsBit(n: Int): Int = when (n) {
        0 -> InputState.BIT_A
        1 -> InputState.BIT_B
        2 -> InputState.BIT_Y
        3 -> InputState.BIT_X
        4 -> InputState.BIT_L
        5 -> InputState.BIT_R
        6 -> InputState.BIT_L3
        7 -> InputState.BIT_R3
        8 -> InputState.BIT_SELECT
        9 -> InputState.BIT_START
        10 -> 0
        11 -> 0
        12 -> InputState.BIT_UP
        13 -> InputState.BIT_DOWN
        14 -> InputState.BIT_L2
        15 -> InputState.BIT_R2
        else -> 0
    }
}
