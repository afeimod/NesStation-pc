package com.nesstation.app.ui.emulator

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
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.rounded.Bolt
import androidx.compose.material.icons.rounded.Pause
import androidx.compose.material.icons.rounded.PhotoCamera
import androidx.compose.material.icons.rounded.PlayArrow
import androidx.compose.material.icons.rounded.Refresh
import androidx.compose.material.icons.rounded.Save
import androidx.compose.material.icons.rounded.Settings
import androidx.compose.material.icons.rounded.Undo
import androidx.compose.material3.Icon
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.vector.ImageVector
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.compose.ui.window.Dialog
import com.nesstation.app.core.storage.Paths
import com.nesstation.app.ui.neon.Neon
import java.io.File
import java.text.SimpleDateFormat
import java.util.Date

/**
 * 游戏内覆盖菜单（对应 Android 版 MenuOverlay）：
 * 继续 / 暂停 / 快进档位 / 重置 / 快存快读 / 截图 / 核心设置 / 退出。
 */
@Composable
fun MenuOverlay(
    gameTitle: String,
    coreTitle: String,
    paused: Boolean,
    fastForward: Int,
    onTogglePause: () -> Unit,
    onCycleFF: () -> Unit,
    onReset: () -> Unit,
    onSaveState: () -> Unit,
    onLoadState: () -> Unit,
    onScreenshot: () -> Unit,
    onCoreOptions: () -> Unit,
    onClose: () -> Unit,
    onExit: () -> Unit
) {
    Box(
        Modifier
            .fillMaxSize()
            .background(Color(0x44000000))
            .clickable(onClick = onClose)
    ) {
        Column(
            Modifier
                .align(Alignment.Center)
                .width(430.dp)
                .clip(RoundedCornerShape(14.dp))
                .background(Color(0xF20A1020))
                .padding(22.dp)
        ) {
            Text(gameTitle, color = Neon.TextHi, fontSize = 17.sp, fontWeight = FontWeight.Bold)
            Text(coreTitle, color = Neon.Accent, fontSize = 11.sp)
            Spacer(Modifier.height(16.dp))

            Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                MenuBtn(
                    if (paused) "继续" else "暂停",
                    if (paused) Icons.Rounded.PlayArrow else Icons.Rounded.Pause,
                    Neon.Green, Modifier.weight(1f), onTogglePause
                )
                MenuBtn(
                    if (fastForward > 0) "快进 ${fastForward}×" else "快进关",
                    Icons.Rounded.Bolt,
                    if (fastForward > 0) Neon.Gold else Neon.TextDim,
                    Modifier.weight(1f), onCycleFF
                )
                MenuBtn("重置", Icons.Rounded.Refresh, Neon.Blue, Modifier.weight(1f), onReset)
            }
            Spacer(Modifier.height(8.dp))
            Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                MenuBtn("快存档", Icons.Rounded.Save, Neon.Accent, Modifier.weight(1f), onSaveState)
                MenuBtn("快读档", Icons.Rounded.Undo, Neon.Accent, Modifier.weight(1f), onLoadState)
                MenuBtn("截图", Icons.Rounded.PhotoCamera, Neon.Purple, Modifier.weight(1f), onScreenshot)
            }
            Spacer(Modifier.height(8.dp))
            MenuBtn("核心设置…", Icons.Rounded.Settings, Neon.Text, Modifier.fillMaxWidth(), onCoreOptions)

            Spacer(Modifier.height(14.dp))
            Row(verticalAlignment = Alignment.CenterVertically) {
                Text(
                    "F5/F7 = 快速存读 · Tab 按住快进 · F12 截图",
                    color = Neon.TextDim, fontSize = 10.sp, modifier = Modifier.weight(1f)
                )
                Text("退出游戏", color = Neon.Red, fontSize = 13.sp, fontWeight = FontWeight.Bold,
                    modifier = Modifier
                        .clip(RoundedCornerShape(6.dp))
                        .clickable(onClick = onExit)
                        .padding(horizontal = 12.dp, vertical = 6.dp))
            }
        }
    }
}

@Composable
private fun MenuBtn(
    label: String,
    icon: ImageVector,
    tint: Color,
    modifier: Modifier = Modifier,
    onClick: () -> Unit
) {
    Column(
        modifier
            .clip(RoundedCornerShape(8.dp))
            .background(Neon.BgPanelHi)
            .clickable(onClick = onClick)
            .padding(vertical = 10.dp),
        horizontalAlignment = Alignment.CenterHorizontally
    ) {
        Icon(icon, null, tint = tint, modifier = Modifier.size(18.dp))
        Spacer(Modifier.height(4.dp))
        Text(label, color = Neon.Text, fontSize = 11.sp)
    }
}

/**
 * 存档槽选择器（10 槽，对应 Android 版 SlotPickerDialog）。
 */
@Composable
fun SlotPickerDialog(
    mode: String, // save | load
    gameId: String,
    onSlotSelected: (Int) -> Unit,
    onDismiss: () -> Unit
) {
    Dialog(
        onCloseRequest = onDismiss,
        title = if (mode == "save") "保存即时存档" else "读取即时存档"
    ) {
        Column(Modifier.background(Color(0xFF0A1020)).padding(16.dp)) {
            for (row in 0 until 5) {
                Row(Modifier.fillMaxWidth().padding(vertical = 3.dp)) {
                    for (col in 0 until 2) {
                        val slot = row * 2 + col
                        val f = File(Paths.savesDir, "${gameId}_slot$slot.state")
                        val info = if (f.isFile) {
                            val ts = SimpleDateFormat("MM-dd HH:mm").format(Date(f.lastModified()))
                            "· $ts"
                        } else ""
                        Box(
                            Modifier
                                .weight(1f)
                                .padding(3.dp)
                                .clip(RoundedCornerShape(6.dp))
                                .background(if (f.isFile) Neon.BgPanelHi else Neon.BgDeep)
                                .clickable { onSlotSelected(slot) }
                                .padding(vertical = 8.dp, horizontal = 10.dp),
                            contentAlignment = Alignment.Center
                        ) {
                            Text(
                                "槽位 ${slot + 1} $info",
                                color = if (f.isFile) Neon.TextHi else Neon.TextDim,
                                fontSize = 12.sp
                            )
                        }
                    }
                }
            }
            Text("提示：F5 / F7 = 槽位 1 快速存读", color = Neon.TextDim, fontSize = 10.sp,
                modifier = Modifier.padding(top = 8.dp))
        }
    }
}

/**
 * 双核心平台选择器（PS2/PSP；对应 Android 版 NdsCorePickerDialog 的交互，
 * PC 版因「激烈 / MMJ 无电脑版」改为「外置核心（推荐） / libretro（实验性）」）。
 */
@Composable
fun CorePickerDialog(
    platform: com.nesstation.app.core.model.GamePlatform,
    choices: List<com.nesstation.app.core.engine.EngineFactory.CoreChoice>,
    onPick: (com.nesstation.app.core.engine.EngineFactory.CoreChoice, rememberChoice: Boolean) -> Unit,
    onDismiss: () -> Unit
) {
    var rememberChoice by remember { mutableStateOf(true) }
    Dialog(
        onCloseRequest = onDismiss,
        title = "选择 ${platform.displayName} 核心"
    ) {
        Column(Modifier.background(Color(0xFF0A1020)).padding(18.dp)) {
            Text(
                "该平台有两个 PC 核心可用 —— 外置核心为推荐配置（原生 GPU 全速），libretro 核心进程内运行（软件渲染，兼容实验性）。",
                color = Neon.Text, fontSize = 12.sp, lineHeight = 17.sp
            )
            Spacer(Modifier.height(12.dp))
            choices.forEach { c ->
                val isExternal = c is com.nesstation.app.core.engine.EngineFactory.CoreChoice.External
                Row(
                    Modifier
                        .fillMaxWidth()
                        .padding(vertical = 4.dp)
                        .clip(RoundedCornerShape(8.dp))
                        .background(Neon.BgPanelHi)
                        .clickable { onPick(c, rememberChoice) }
                        .padding(12.dp),
                    verticalAlignment = Alignment.CenterVertically
                ) {
                    Column(Modifier.weight(1f)) {
                        Text(c.title(), color = Neon.TextHi, fontSize = 14.sp, fontWeight = FontWeight.Bold)
                        Text(
                            when {
                                isExternal -> "原生 PC 版 · GPU 全速 · 原生存档系统"
                                else -> "进程内 libretro · 软件渲染 · NesStation 统一菜单/快存"
                            },
                            color = Neon.TextDim, fontSize = 11.sp
                        )
                    }
                    if (isExternal) {
                        Box(
                            Modifier
                                .clip(RoundedCornerShape(4.dp))
                                .background(Neon.Green.copy(alpha = 0.2f))
                                .padding(horizontal = 8.dp, vertical = 3.dp)
                        ) {
                            Text("推荐", color = Neon.Green, fontSize = 10.sp, fontWeight = FontWeight.Bold)
                        }
                    } else {
                        Box(
                            Modifier
                                .clip(RoundedCornerShape(4.dp))
                                .background(Neon.Gold.copy(alpha = 0.2f))
                                .padding(horizontal = 8.dp, vertical = 3.dp)
                        ) {
                            Text("实验性", color = Neon.Gold, fontSize = 10.sp, fontWeight = FontWeight.Bold)
                        }
                    }
                }
            }
            Spacer(Modifier.height(10.dp))
            Row(verticalAlignment = Alignment.CenterVertically) {
                Box(
                    Modifier
                        .clip(RoundedCornerShape(4.dp))
                        .background(if (rememberChoice) Neon.Accent.copy(alpha = 0.25f) else Neon.BgPanelHi)
                        .clickable { rememberChoice = !rememberChoice }
                        .padding(horizontal = 10.dp, vertical = 5.dp)
                ) {
                    Text(
                        if (rememberChoice) "✓ 记住此游戏的选择" else "记住此游戏的选择",
                        color = if (rememberChoice) Neon.Accent else Neon.TextDim,
                        fontSize = 11.sp
                    )
                }
                Box(Modifier.weight(1f))
                Text("取消", color = Neon.TextDim, fontSize = 12.sp,
                    modifier = Modifier.clickable(onClick = onDismiss).padding(8.dp))
            }
        }
    }
}
