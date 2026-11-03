package com.nesstation.app

import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateListOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.setValue
import com.nesstation.app.core.model.GameEntry
import com.nesstation.app.core.model.GamePlatform

/**
 * 导航（PC 版）：简单可回溯的屏幕栈（Esc = 返回），
 * 对应 Android 版 NavHost 的路由集合。
 */
sealed interface Screen {
    data object Home : Screen
    /** 游戏库；mode: all | fav | recent | platform */
    data class Library(val mode: String = "all", val platform: GamePlatform? = null) : Screen
    data class Emulator(val gameId: String) : Screen
    data object Settings : Screen
    data object CoreManager : Screen
    data object KeyMap : Screen
    data object Files : Screen
    data object About : Screen
}

class AppNav {
    private val stack = mutableStateListOf<Screen>(Screen.Home)
    val current: Screen get() = stack.last()
    val depth: Int get() = stack.size

    var onStackChanged by mutableStateOf({})

    fun push(s: Screen) {
        // 模拟器页只允许一个实例
        if (s is Screen.Emulator) stack.removeAll { it is Screen.Emulator }
        stack.add(s)
        bump()
    }

    fun replace(s: Screen) {
        stack[stack.size - 1] = s
        bump()
    }

    fun pop(): Boolean {
        if (stack.size <= 1) return false
        stack.removeAt(stack.size - 1)
        bump()
        return true
    }

    fun popToHome() {
        while (stack.size > 1) stack.removeAt(stack.size - 1)
        bump()
    }

    private fun bump() { onStackChanged = {} }

    /** 监听器（游戏库变化时 UI 重组用）。 */
    var listener: (() -> Unit)? = null
    private fun notifyListener() = listener?.invoke()
}
