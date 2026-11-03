package com.nesstation.app

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.runtime.Composable
import androidx.compose.runtime.DisposableEffect
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.input.key.Key
import androidx.compose.ui.input.key.KeyEventType
import androidx.compose.ui.input.key.key
import androidx.compose.ui.input.key.onPreviewKeyEvent
import androidx.compose.ui.input.key.type
import androidx.compose.ui.graphics.painter.BitmapPainter
import androidx.compose.ui.unit.dp
import androidx.compose.ui.window.Window
import androidx.compose.ui.window.application
import androidx.compose.ui.window.rememberWindowState
import com.nesstation.app.core.input.Gamepad
import com.nesstation.app.core.model.GameEntry
import com.nesstation.app.core.storage.PlatformDetector
import com.nesstation.app.core.storage.RomLibrary
import com.nesstation.app.core.storage.SettingsStore
import com.nesstation.app.ui.about.AboutScreen
import com.nesstation.app.ui.emulator.EmulatorScreen
import com.nesstation.app.ui.fsd.FsdHomeScreen
import com.nesstation.app.ui.library.LibraryScreen
import com.nesstation.app.ui.neon.NeonHomeScreen
import com.nesstation.app.ui.neon.NeonLibraryScreen
import com.nesstation.app.ui.neon.NeonSettingsScreen
import com.nesstation.app.ui.neon.NeonUi
import com.nesstation.app.ui.settings.CoreManagerScreen
import com.nesstation.app.ui.settings.KeyMapScreen
import com.nesstation.app.ui.settings.SettingsScreen
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import androidx.compose.ui.graphics.toComposeImageBitmap
import java.awt.image.BufferedImage

/**
 * NesStation for PC —— 入口。
 *
 * 基于 Android 版 NesStation 重写的电脑版：
 *  - 双主题：Neon 3D 新UI（默认）/ FSD 经典桌面（设置里即时切换）
 *  - 全核心替换为 PC 端移植（进程内 libretro + 外置 PC 独立核心）
 *  - 移除 Android 构建，桌面三平台打包（Windows/Linux/macOS）
 */
fun main() = application {
    val windowState = rememberWindowState(width = 1360.dp, height = 820.dp)

    Window(
        onCloseRequest = ::exitApplication,
        state = windowState,
        title = "NesStation — 多平台复古游戏模拟器（PC 版）",
        icon = appIcon()?.let { BitmapPainter(it.toComposeImageBitmap()) }
    ) {
        androidx.compose.runtime.CompositionLocalProvider(LocalAppWindow provides window) {
            App()
        }
    }
}

/** 程序图标（像素红白机卡带，运行时生成，无需二进制资源）。 */
fun appIcon(): java.awt.image.BufferedImage? = runCatching {
    val size = 64
    val img = BufferedImage(size, size, BufferedImage.TYPE_INT_ARGB)
    val sky = 0xFFB7D7F2.toInt()
    val red = 0xFFE74C3C.toInt()
    val dark = 0xFF27313F.toInt()
    val gray = 0xFFD8DEE8.toInt()
    // 天空底
    for (y in 0 until size) for (x in 0 until size) img.setRGB(x, y, sky)
    // 卡带主体
    for (y in 8 until 56) for (x in 14 until 50) {
        val inLabel = y in 14..40 && x in 20..44
        val inPin = y in 46..55 && x in 20..43
        when {
            inPin -> if ((x - 20) % 4 < 2) img.setRGB(x, y, dark) else img.setRGB(x, y, gray)
            inLabel -> img.setRGB(x, y, 0xFFFFFFFF.toInt())
            else -> img.setRGB(x, y, red)
        }
    }
    // 标签条纹
    for (y in 20 until 26) for (x in 22 until 42) img.setRGB(x, y, dark)
    for (y in 30 until 36) for (x in 22 until 42) img.setRGB(x, y, 0xFF4F8C4A.toInt())
    img
}.getOrNull()

// ============================================================
// 导航宿主
// ============================================================

@Composable
fun App() {
    val nav = remember { AppNav() }
    NeonUi.ensureLoaded()
    Gamepad.start()

    var libraryVersion by remember { mutableStateOf(0) }
    val games by remember(libraryVersion) { mutableStateOf(RomLibrary.all()) }
    DisposableEffect(Unit) {
        val l: () -> Unit = { libraryVersion++ }
        RomLibrary.addListener(l)
        onDispose { RomLibrary.removeListener(l) }
    }

    // 启动时自动扫描 ROM 目录 + 首运行初始化
    LaunchedEffect(Unit) {
        withContext(Dispatchers.IO) {
            if (!SettingsStore.load().firstRunDone) {
                SettingsStore.update { it.firstRunDone = true }
            }
            val added = RomLibrary.rescanRoots()
            if (added > 0) libraryVersion++
        }
    }

    // Esc 全局返回（游戏运行页除外，由其内部处理）
    Box(
        Modifier
            .fillMaxSize()
            .background(Color(0xFF05070E))
            .onPreviewKeyEvent { e ->
                if (e.type == KeyEventType.KeyDown && e.key == Key.Escape) {
                    if (nav.current !is Screen.Emulator) { nav.pop(); true } else false
                } else false
            }
    ) {
        val screen = nav.current
        when (screen) {
            is Screen.Home -> {
                if (NeonUi.isNeon) {
                    NeonHomeScreen(
                        games = games,
                        onOpenLibrary = { nav.push(Screen.Library("all")) },
                        onOpenFavorites = { nav.push(Screen.Library("fav")) },
                        onOpenHistory = { nav.push(Screen.Library("recent")) },
                        onOpenSettings = { nav.push(Screen.Settings) },
                        onOpenCoreManager = { nav.push(Screen.CoreManager) },
                        onOpenAbout = { nav.push(Screen.About) },
                        onExit = { exitApp() },
                        onOpenGame = { g -> nav.push(Screen.Emulator(g.id)) },
                        onToggleFavorite = { g -> RomLibrary.toggleFavorite(g.id); libraryVersion++ }
                    )
                } else {
                    FsdHomeScreen(
                        gameCount = games.size,
                        onOpenLibrary = { nav.push(Screen.Library("all")) },
                        onOpenFavorites = { nav.push(Screen.Library("fav")) },
                        onOpenHistory = { nav.push(Screen.Library("recent")) },
                        onOpenSettings = { nav.push(Screen.Settings) },
                        onOpenCoreManager = { nav.push(Screen.CoreManager) },
                        onOpenAbout = { nav.push(Screen.About) },
                        onExit = { exitApp() }
                    )
                }
            }

            is Screen.Library -> {
                val onOpen: (GameEntry) -> Unit = { g -> nav.push(Screen.Emulator(g.id)) }
                val onFav: (GameEntry) -> Unit = { g -> RomLibrary.toggleFavorite(g.id); libraryVersion++ }
                val onDelete: (GameEntry) -> Unit = { g -> RomLibrary.remove(g.id); libraryVersion++ }
                val onImport: (String, com.nesstation.app.core.model.GamePlatform?) -> Unit = { path, hint ->
                    importFolder(path, hint) { libraryVersion++ }
                }
                val onRescan: () -> Unit = {
                    Thread {
                        RomLibrary.rescanRoots()
                        libraryVersion++
                    }.start()
                }
                if (NeonUi.isNeon) {
                    NeonLibraryScreen(
                        mode = screen.mode,
                        initialPlatform = screen.platform,
                        games = games,
                        onOpenGame = onOpen,
                        onToggleFavorite = onFav,
                        onDeleteGame = onDelete,
                        onImportFolder = onImport,
                        onRescan = onRescan,
                        onBack = { nav.pop() }
                    )
                } else {
                    LibraryScreen(
                        mode = screen.mode,
                        initialPlatform = screen.platform,
                        games = games,
                        onOpenGame = onOpen,
                        onToggleFavorite = onFav,
                        onDeleteGame = onDelete,
                        onImportFolder = onImport,
                        onRescan = onRescan,
                        onBack = { nav.pop() }
                    )
                }
            }

            is Screen.Emulator -> {
                val game = games.firstOrNull { it.id == screen.gameId }
                if (game != null) {
                    EmulatorScreen(
                        game = game,
                        onExit = { nav.pop() },
                        onOpenCoreSettings = { _ -> nav.push(Screen.CoreManager) }
                    )
                } else {
                    nav.pop()
                }
            }

            is Screen.Settings -> {
                if (NeonUi.isNeon) {
                    NeonSettingsScreen(
                        onOpenCoreManager = { nav.push(Screen.CoreManager) },
                        onOpenKeyMap = { nav.push(Screen.KeyMap) },
                        onOpenAbout = { nav.push(Screen.About) },
                        onBack = { nav.pop() }
                    )
                } else {
                    SettingsScreen(
                        onOpenCoreManager = { nav.push(Screen.CoreManager) },
                        onOpenKeyMap = { nav.push(Screen.KeyMap) },
                        onOpenAbout = { nav.push(Screen.About) },
                        onBack = { nav.pop() }
                    )
                }
            }

            is Screen.CoreManager -> CoreManagerScreen(onBack = { nav.pop() })
            is Screen.KeyMap -> KeyMapScreen(onBack = { nav.pop() })
            is Screen.Files -> {
                // 文件浏览（经典）：直接复用游戏库
                LibraryScreen(
                    mode = "all",
                    initialPlatform = null,
                    games = games,
                    onOpenGame = { g -> nav.push(Screen.Emulator(g.id)) },
                    onToggleFavorite = { g -> RomLibrary.toggleFavorite(g.id); libraryVersion++ },
                    onDeleteGame = { g -> RomLibrary.remove(g.id); libraryVersion++ },
                    onImportFolder = { path, hint -> importFolder(path, hint) { libraryVersion++ } },
                    onRescan = {
                        Thread { RomLibrary.rescanRoots(); libraryVersion++ }.start()
                    },
                    onBack = { nav.pop() }
                )
            }
            is Screen.About -> AboutScreen(onBack = { nav.pop() })
        }
    }
}

/** 导入文件夹（增量入库）。 */
private fun importFolder(path: String, hint: com.nesstation.app.core.model.GamePlatform?, onDone: () -> Unit) {
    val t = Thread({
        val dir = java.io.File(path)
        if (dir.isDirectory) {
            PlatformDetector.scanDirectory(dir, hint).forEach { (f, p) ->
                RomLibrary.add(f, p)
            }
        }
        onDone()
    }, "import-folder")
    t.isDaemon = true
    t.start()
}

/** 退出程序。 */
private fun exitApp() {
    Gamepad.stop()
    RomLibrary.persist()
    kotlin.system.exitProcess(0)
}
