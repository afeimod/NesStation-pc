package com.nesstation.app.core.storage

import kotlinx.serialization.builtins.MapSerializer
import kotlinx.serialization.builtins.serializer
import kotlinx.serialization.json.Json
import java.awt.event.KeyEvent
import java.io.File

/**
 * 键盘映射持久化（对应 Android 版 KeyMapStore：actionId → 键码）。
 * PC 版键码为 AWT [KeyEvent.VK_*] 整数（稳定、可持久化、与 Compose 窗口共存）。
 */
object KeyMapStore {
    private val json = Json { ignoreUnknownKeys = true }
    private val file: File get() = File(Paths.dataDir, "keymap.json")

    private val overrides = HashMap<String, Int>()

    init {
        runCatching {
            val obj = json.parseToJsonElement(file.readText()).let { it as? kotlinx.serialization.json.JsonObject }
            obj?.forEach { (k, v) ->
                (v as? kotlinx.serialization.json.JsonPrimitive)?.content?.toIntOrNull()?.let {
                    overrides[k] = it
                }
            }
        }
    }

    fun get(actionId: String): Int? = overrides[actionId]

    fun put(actionId: String, keyCode: Int) {
        overrides[actionId] = keyCode
        persist()
    }

    fun remove(actionId: String) {
        overrides.remove(actionId)
        persist()
    }

    fun resetAll() {
        overrides.clear()
        persist()
    }

    private val mapSerializer = MapSerializer(String.serializer(), Int.serializer())

    private fun persist() {
        runCatching {
            file.writeText(json.encodeToString(mapSerializer, overrides))
        }
    }

    /** AWT 键码 → 可读名称（对应 Android 版 keyCodeToLabel）。 */
    fun keyCodeToLabel(keyCode: Int): String = when (keyCode) {
        KeyEvent.VK_UP -> "方向上"
        KeyEvent.VK_DOWN -> "方向下"
        KeyEvent.VK_LEFT -> "方向左"
        KeyEvent.VK_RIGHT -> "方向右"
        KeyEvent.VK_ENTER -> "Enter"
        KeyEvent.VK_SPACE -> "Space"
        KeyEvent.VK_SHIFT -> "Shift"
        KeyEvent.VK_CONTROL -> "Ctrl"
        KeyEvent.VK_ALT -> "Alt"
        KeyEvent.VK_TAB -> "Tab"
        KeyEvent.VK_ESCAPE -> "Esc"
        KeyEvent.VK_BACK_SPACE -> "Backspace"
        KeyEvent.VK_DELETE -> "Delete"
        KeyEvent.VK_HOME -> "Home"
        KeyEvent.VK_END -> "End"
        KeyEvent.VK_PAGE_UP -> "PageUp"
        KeyEvent.VK_PAGE_DOWN -> "PageDown"
        KeyEvent.VK_INSERT -> "Insert"
        KeyEvent.VK_NUMPAD0 -> "小键盘 0"
        KeyEvent.VK_NUMPAD1 -> "小键盘 1"
        KeyEvent.VK_NUMPAD2 -> "小键盘 2"
        KeyEvent.VK_NUMPAD3 -> "小键盘 3"
        KeyEvent.VK_NUMPAD4 -> "小键盘 4"
        KeyEvent.VK_NUMPAD5 -> "小键盘 5"
        KeyEvent.VK_NUMPAD6 -> "小键盘 6"
        KeyEvent.VK_NUMPAD7 -> "小键盘 7"
        KeyEvent.VK_NUMPAD8 -> "小键盘 8"
        KeyEvent.VK_NUMPAD9 -> "小键盘 9"
        KeyEvent.VK_ADD -> "小键盘 +"
        KeyEvent.VK_SUBTRACT -> "小键盘 -"
        KeyEvent.VK_MULTIPLY -> "小键盘 *"
        KeyEvent.VK_DIVIDE -> "小键盘 /"
        KeyEvent.VK_SEPARATOR -> "小键盘 ."
        else -> KeyEvent.getKeyText(keyCode).ifBlank { "Key $keyCode" }
    }
}
