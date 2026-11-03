package com.nesstation.app.ui.emulator

import androidx.compose.foundation.Canvas
import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.rounded.ArrowBack
import androidx.compose.material.icons.rounded.Bolt
import androidx.compose.material.icons.rounded.Pause
import androidx.compose.material.icons.rounded.PhotoCamera
import androidx.compose.material.icons.rounded.PlayArrow
import androidx.compose.material.icons.rounded.Refresh
import androidx.compose.material.icons.rounded.Save
import androidx.compose.material.icons.rounded.Settings
import androidx.compose.material.icons.rounded.Tune
import androidx.compose.material3.Icon
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.DisposableEffect
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.unit.IntOffset
import androidx.compose.ui.unit.IntSize
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.FilterQuality
import androidx.compose.ui.graphics.ImageBitmap
import androidx.compose.ui.graphics.drawscope.scale
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.nesstation.app.LocalAppWindow
import com.nesstation.app.core.engine.EngineFactory
import com.nesstation.app.core.engine.ExternalEngine
import com.nesstation.app.core.engine.LibretroEngine
import com.nesstation.app.core.input.InputState
import com.nesstation.app.core.input.KeyboardMap
import com.nesstation.app.core.libretro.LrFrame
import com.nesstation.app.core.model.GameEntry
import com.nesstation.app.core.storage.Paths
import com.nesstation.app.core.storage.RomLibrary
import com.nesstation.app.core.storage.SettingsStore
import com.nesstation.app.ui.neon.Neon
import com.nesstation.app.ui.settings.CoreOptionsInGameDialog
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import androidx.compose.ui.graphics.toComposeImageBitmap
import java.awt.event.KeyEvent
import java.awt.event.KeyListener
import java.awt.image.BufferedImage
import java.io.File
import java.text.SimpleDateFormat
import java.util.Date
import kotlin.math.max
import kotlin.math.min
import kotlin.math.roundToInt

/**
 * ★ 游戏运行界面（PC 版 EmulatorScreen）★
 *
 *  - libretro 核心：Canvas 软件帧渲染（像素完美/双线性/CRT 滤镜、保持比例、整数缩放），
 *    FPS HUD、游戏内覆盖菜单（Esc）、快进（Tab 按住）、快存快读（F5/F7）、截图（F12）。
 *  - 外置核心：交给 [ExternalCoreScreen]（进程管理）。
 *  - 双核心平台（PS2/PSP）：进入时弹 [CorePickerDialog] 选择。
 */
@Composable
fun EmulatorScreen(
    game: GameEntry,
    onExit: () -> Unit,
    onOpenCoreSettings: (coreId: String) -> Unit,
    modifier: Modifier = Modifier
) {
    val platform = game.platformEnum
    val settings = remember { SettingsStore.load() }

    // ---- 核心选择（双核心平台） ----
    val choices = remember { EngineFactory.coreChoices(platform) }
    var chosen by remember {
        mutableStateOf<EngineFactory.CoreChoice?>(
            when {
                choices.isEmpty() -> null
                choices.size == 1 -> choices.first()
                else -> {
                    val preferred = game.preferredCore.ifBlank {
                        SettingsStore.load().defaultCores[platform.name] ?: ""
                    }
                    choices.firstOrNull { it.id() == preferred } ?: choices.firstOrNull()
                }
            }
        )
    }
    var showPicker by remember {
        mutableStateOf(choices.size > 1 && game.preferredCore.isBlank() &&
                SettingsStore.load().defaultCores[platform.name].isNullOrBlank())
    }

    if (showPicker && choices.size > 1) {
        CorePickerDialog(
            platform = platform,
            choices = choices,
            onPick = { choice, rememberChoice ->
                chosen = choice
                if (rememberChoice) {
                    RomLibrary.setPreferredCore(game.id, choice.id())
                }
                showPicker = false
            },
            onDismiss = onExit
        )
        return
    }

    val choice = chosen
    if (choice == null) {
        ErrorPane("该平台暂无可用核心：请到 设置 → 核心管理 安装。", onExit)
        return
    }

    // ---- 外置核心 → 专门的运行面板 ----
    if (choice is EngineFactory.CoreChoice.External) {
        ExternalCoreScreen(game = game, choice = choice, onExit = onExit)
        return
    }

    // ---- libretro 进程内核心 ----
    val engine = remember(choice) { EngineFactory.engineFor(choice) as LibretroEngine }

    var loaded by remember { mutableStateOf(false) }
    var errorMsg by remember { mutableStateOf<String?>(null) }
    var paused by remember { mutableStateOf(false) }
    var fastForward by remember { mutableStateOf(0) }
    var showMenu by remember { mutableStateOf(false) }
    var slotPicker by remember { mutableStateOf<String?>(null) }  // save | load
    var showCoreOptions by remember { mutableStateOf(false) }
    var toast by remember { mutableStateOf<String?>(null) }
    var fpsNow by remember { mutableStateOf(0.0) }
    var frameTick by remember { mutableStateOf(0L) }

    // 渲染缓冲
    var awtImage by remember { mutableStateOf<BufferedImage?>(null) }
    var composeBitmap by remember { mutableStateOf<ImageBitmap?>(null) }

    val startTime = remember { System.currentTimeMillis() }

    // ---- 启动 / 退出 ----
    var disposed by remember { mutableStateOf(false) }
    LaunchedEffect(choice) {
        val ok = withContext(Dispatchers.IO) {
            engine.loadRom(game.romFile, game.id)
        }
        // 加载期间用户退出 → 立即回收，防止复活
        if (disposed) { engine.unload(); return@LaunchedEffect }
        loaded = ok
        if (!ok) {
            errorMsg = engine.lastError().ifBlank { "核心加载失败" }
        } else {
            engine.onFrame = {
                val snap = engine.snapshot()
                if (snap != null) {
                    var img = awtImage
                    if (img == null || img.width != snap.width || img.height != snap.height) {
                        img = BufferedImage(snap.width, snap.height, BufferedImage.TYPE_INT_ARGB)
                        awtImage = img
                    }
                    img.raster.setDataElements(0, 0, snap.width, snap.height, snap.pixels)
                    composeBitmap = img.toComposeImageBitmap()
                }
                frameTick++
            }
        }
    }
    DisposableEffect(choice) {
        onDispose {
            disposed = true
            engine.onFrame = null
            engine.unload()
            InputState.clearAll()
            // 记录游玩时长
            RomLibrary.recordPlay(game.id, System.currentTimeMillis() - startTime)
        }
    }

    // ---- 键盘（全局 KeyEventDispatcher：游戏键 + 热键，避免焦点拦截） ----
    DisposableEffect(choice) {
        val kfm = java.awt.KeyboardFocusManager.getCurrentKeyboardFocusManager()
        val dispatcher = java.awt.KeyEventDispatcher { e ->
            when (e.id) {
                KeyEvent.KEY_PRESSED -> {
                    val btn = KeyboardMap.buttonFor(platform, e.keyCode)
                    if (btn != null) {
                        InputState.applyKeyboard(btn, true)
                        true
                    } else when (e.keyCode) {
                        KeyEvent.VK_ESCAPE -> { showMenu = !showMenu; true }
                        KeyEvent.VK_TAB -> {
                            if (fastForward == 0) {
                                fastForward = settings.fastForwardSpeed.takeIf { it > 1 } ?: 4
                                engine.setFastForward(fastForward)
                            }
                            true
                        }
                        KeyEvent.VK_F5 -> {
                            val f = File(Paths.savesDir, "${game.id}_slot0.state")
                            toast = if (engine.saveState(0, f)) "快存已保存 [槽位 1]" else "存档失败"
                            true
                        }
                        KeyEvent.VK_F7 -> {
                            val f = File(Paths.savesDir, "${game.id}_slot0.state")
                            toast = when {
                                f.isFile && engine.loadState(0, f) -> "快存已读取 [槽位 1]"
                                else -> "槽位 1 无存档"
                            }
                            true
                        }
                        KeyEvent.VK_F12 -> {
                            screenshot(game, engine, awtImage)?.let { toast = "截图已保存: $it" }
                            true
                        }
                        KeyEvent.VK_F1 -> { showMenu = true; true }
                        else -> false
                    }
                }
                KeyEvent.KEY_RELEASED -> {
                    val btn = KeyboardMap.buttonFor(platform, e.keyCode)
                    if (btn != null) {
                        InputState.applyKeyboard(btn, false)
                        true
                    } else if (e.keyCode == KeyEvent.VK_TAB && fastForward > 0) {
                        fastForward = 0
                        engine.setFastForward(0)
                        true
                    } else false
                }
                else -> false
            }
        }
        kfm.addKeyEventDispatcher(dispatcher)
        onDispose {
            kfm.removeKeyEventDispatcher(dispatcher)
            InputState.clearAll()
        }
    }

    // FPS 统计
    LaunchedEffect(loaded) {
        if (!loaded) return@LaunchedEffect
        var last = System.currentTimeMillis()
        var lastFrames = 0L
        while (true) {
            kotlinx.coroutines.delay(500)
            val now = System.currentTimeMillis()
            val frames = frameTick
            val dt = (now - last).coerceAtLeast(1)
            fpsNow = (frames - lastFrames) * 1000.0 / dt
            last = now
            lastFrames = frames
        }
    }

    // Toast 自动消失
    LaunchedEffect(toast) {
        if (toast != null) {
            kotlinx.coroutines.delay(1800)
            toast = null
        }
    }

    Box(modifier.fillMaxSize().background(Color.Black)
        .pointerInput(Unit) { }) {

        // ===== 画面 =====
        Canvas(Modifier.fillMaxSize()) {
            composeBitmap?.let { bmp ->
                val src = IntSize(bmp.width, bmp.height)
                var dstW: Int
                var dstH: Int
                if (settings.keepAspect) {
                    val scale = min(size.width / src.width, size.height / src.height)
                    var s = scale
                    if (settings.integerScaling) {
                        s = max(1f, scale.roundToInt().toFloat())
                    }
                    dstW = (src.width * s).toInt()
                    dstH = (src.height * s).toInt()
                } else {
                    dstW = size.width.toInt()
                    dstH = size.height.toInt()
                }
                val off = IntOffset(
                    ((size.width - dstW) / 2).toInt(),
                    ((size.height - dstH) / 2).toInt()
                )
                val quality = when (settings.videoFilter) {
                    1 -> FilterQuality.Low
                    else -> FilterQuality.None
                }
                drawImage(
                    image = bmp,
                    srcOffset = IntOffset.Zero,
                    srcSize = src,
                    dstOffset = off,
                    dstSize = IntSize(dstW, dstH),
                    filterQuality = quality
                )
                // CRT 扫描线（简单暗线叠加）
                if (settings.videoFilter == 2 && dstH > src.height) {
                    val lineEvery = max(2, (dstH / src.height).toInt())
                    var y = off.y.toFloat()
                    while (y < off.y + dstH) {
                        drawRect(
                            color = Color.Black.copy(alpha = 0.22f),
                            topLeft = Offset(off.x.toFloat(), y),
                            size = Size(dstW.toFloat(), 1f)
                        )
                        y += lineEvery
                    }
                }
            }
        }

        // ===== FPS HUD =====
        if (settings.showFps && loaded) {
            Text(
                text = buildString {
                    append("${fpsNow.roundToInt()} FPS")
                    if (fastForward > 0) append(" · 快进 ${fastForward}×")
                    if (paused) append(" · 已暂停")
                },
                color = if (fastForward > 0) Neon.Gold else Neon.Accent,
                fontSize = 12.sp,
                modifier = Modifier
                    .align(Alignment.TopEnd)
                    .padding(10.dp)
                    .clip(RoundedCornerShape(4.dp))
                    .background(Color(0x88000000))
                    .padding(horizontal = 8.dp, vertical = 4.dp)
            )
        }

        // ===== 加载/错误 =====
        if (!loaded && errorMsg == null) {
            Column(
                Modifier.align(Alignment.Center),
                horizontalAlignment = Alignment.CenterHorizontally
            ) {
                Text("正在启动 ${engine.coreTitle()}…", color = Neon.TextHi, fontSize = 15.sp)
                Text(game.title, color = Neon.TextDim, fontSize = 12.sp)
            }
        }
        errorMsg?.let { err ->
            ErrorPane(err, onExit)
        }

        // ===== 暂停标记 =====
        if (paused && loaded) {
            Box(
                Modifier
                    .align(Alignment.Center)
                    .clip(RoundedCornerShape(8.dp))
                    .background(Color(0x99000000))
                    .padding(horizontal = 24.dp, vertical = 12.dp)
            ) {
                Text("已暂停 —— 按 Esc 呼出菜单", color = Neon.TextHi, fontSize = 14.sp)
            }
        }

        // ===== Toast =====
        toast?.let { t ->
            Box(
                Modifier
                    .align(Alignment.BottomCenter)
                    .padding(bottom = 46.dp)
                    .clip(RoundedCornerShape(6.dp))
                    .background(Color(0xCC05070E))
                    .padding(horizontal = 16.dp, vertical = 8.dp)
            ) {
                Text(t, color = Neon.TextHi, fontSize = 12.sp)
            }
        }

        // ===== 游戏内菜单 =====
        if (showMenu && loaded) {
            MenuOverlay(
                gameTitle = game.title,
                coreTitle = engine.coreTitle(),
                paused = paused,
                fastForward = fastForward,
                onTogglePause = {
                    paused = !paused
                    engine.setPaused(paused)
                },
                onCycleFF = {
                    val speeds = listOf(0, 2, 4, 6, 8, 16)
                    val next = speeds[(speeds.indexOf(fastForward) + 1).mod(speeds.size)]
                    fastForward = next
                    engine.setFastForward(next)
                },
                onReset = {
                    engine.reset()
                    toast = "已重置"
                },
                onSaveState = { slotPicker = "save" },
                onLoadState = { slotPicker = "load" },
                onScreenshot = {
                    screenshot(game, engine, awtImage)?.let { toast = "截图已保存: $it" }
                        ?: run { toast = "截图失败：无画面数据" }
                },
                onCoreOptions = { showCoreOptions = true },
                onClose = { showMenu = false },
                onExit = onExit
            )
        }

        // ===== 槽位选择 =====
        slotPicker?.let { mode ->
            SlotPickerDialog(
                mode = mode,
                gameId = game.id,
                onSlotSelected = { slot ->
                    val f = File(Paths.savesDir, "${game.id}_slot$slot.state")
                    if (mode == "save") {
                        toast = if (engine.saveState(slot, f)) "存档已保存 [槽位 ${slot + 1}]" else "存档失败"
                    } else {
                        toast = when {
                            !f.isFile -> "槽位 ${slot + 1} 无存档"
                            engine.loadState(slot, f) -> "存档已读取 [槽位 ${slot + 1}]"
                            else -> "读档失败：存档损坏或版本不兼容"
                        }
                    }
                    slotPicker = null
                },
                onDismiss = { slotPicker = null }
            )
        }

        // ===== 核心选项（游戏内） =====
        if (showCoreOptions) {
            CoreOptionsInGameDialog(
                coreId = (choice as EngineFactory.CoreChoice.Libretro).def.id,
                onDismiss = { showCoreOptions = false },
                onChanged = { key, value ->
                    engine.setCoreOption(key, value)
                    engine.notifyOptionsChanged()
                }
            )
        }
    }
}

private fun screenshot(game: GameEntry, engine: LibretroEngine, awtImage: BufferedImage?): String? {
    return try {
        val img = awtImage ?: return null
        val dir = Paths.screenshotsDir
        val ts = SimpleDateFormat("yyyyMMdd_HHmmss").format(Date())
        val safe = game.title.replace(Regex("[^a-zA-Z0-9_-\\u4e00-\\u9fa5]"), "_")
        val f = File(dir, "${safe}_$ts.png")
        javax.imageio.ImageIO.write(img, "png", f)
        f.name
    } catch (_: Exception) { null }
}

/** 错误面板。 */
@Composable
fun ErrorPane(message: String, onExit: () -> Unit) {
    Column(
        Modifier
            .fillMaxSize()
            .background(Color(0xF205070E))
            .padding(40.dp),
        horizontalAlignment = Alignment.CenterHorizontally,
        verticalArrangement = Arrangement.Center
    ) {
        Text("核心错误", color = Neon.Red, fontSize = 18.sp,
            fontWeight = androidx.compose.ui.text.font.FontWeight.Bold)
        Spacer(Modifier.height(12.dp))
        Text(message, color = Neon.Text, fontSize = 13.sp, lineHeight = 19.sp)
        Spacer(Modifier.height(20.dp))
        com.nesstation.app.ui.neon.NeonPrimaryButton("返回游戏站", Modifier, Icons.AutoMirrored.Rounded.ArrowBack, onExit)
    }
}
