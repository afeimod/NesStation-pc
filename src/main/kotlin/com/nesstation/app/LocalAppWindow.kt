package com.nesstation.app

import androidx.compose.runtime.staticCompositionLocalOf
import androidx.compose.ui.awt.ComposeWindow

/**
 * 桌面窗口引用（供游戏运行页 / 键位映射页挂 AWT 键盘监听用）。
 * androidx.compose.ui.window.LocalWindow 在当前版本为 internal，故自建。
 */
val LocalAppWindow = staticCompositionLocalOf<ComposeWindow?> { null }
