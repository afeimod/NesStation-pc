package com.nesstation.app.ui.emulator

import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.rounded.ArrowBack
import androidx.compose.material.icons.rounded.Bolt
import androidx.compose.material.icons.rounded.Close
import androidx.compose.material.icons.rounded.OpenInNew
import androidx.compose.material.icons.rounded.FolderOpen
import androidx.compose.material.icons.rounded.PlayArrow
import androidx.compose.material.icons.rounded.Terminal
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
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.nesstation.app.core.engine.EngineFactory
import com.nesstation.app.core.engine.ExternalEngine
import com.nesstation.app.core.model.GameEntry
import com.nesstation.app.core.storage.Paths
import com.nesstation.app.core.storage.RomLibrary
import com.nesstation.app.ui.neon.Neon
import com.nesstation.app.ui.neon.NeonPrimaryButton
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import java.awt.Desktop
import java.io.File

/**
 * ★ 外置核心运行面板 ★
 *
 * PS2（PCSX2）/ PSP（PPSSPP）/ DC（Flycast）/ 3DS（Azahar）/
 * NGC-Wii（Ishiiruka）/ Wii U（Cemu）/ Switch（Eden）/ J2ME（FreeJ2ME）
 * —— 原生 PC 独立模拟器以子进程运行，NesStation 负责启动/监控/退出回收，
 * 并记录游玩时长。核心配置/存档使用其原生系统。
 */
@Composable
fun ExternalCoreScreen(
    game: GameEntry,
    choice: EngineFactory.CoreChoice.External,
    onExit: () -> Unit,
    modifier: Modifier = Modifier
) {
    val engine = remember(choice) { ExternalEngine(choice.def) }
    var state by remember { mutableStateOf("idle") } // idle | running | exited
    var message by remember { mutableStateOf<String?>(null) }
    val startTime = remember { System.currentTimeMillis() }

    // 进程退出 → 提示返回
    LaunchedEffect(Unit) {
        engine.onExited = { code ->
            state = "exited"
            message = if (code == 0) "核心已正常退出" else "核心退出（代码 $code）——详见日志 ${Paths.logsDir.absolutePath}/${choice.def.id}-stderr.log"
        }
    }

    // 自动启动
    LaunchedEffect(choice) {
        val ok = withContext(Dispatchers.IO) { engine.loadRom(game.romFile, game.id) }
        if (ok) state = "running" else {
            state = "failed"
            message = engine.lastError()
        }
    }

    DisposableEffect(choice) {
        onDispose {
            engine.onExited = null
            if (engine.isRunning) {
                engine.stop()
                RomLibrary.recordPlay(game.id, System.currentTimeMillis() - startTime)
            }
        }
    }

    Box(
        modifier
            .fillMaxWidth()
            .height(320.dp)
            .clip(RoundedCornerShape(12.dp))
            .background(Neon.BgPanel)
            .padding(26.dp)
    ) {
        Column {
            Row(verticalAlignment = Alignment.CenterVertically) {
                Icon(Icons.Rounded.Terminal, null, tint = Neon.Purple, modifier = Modifier.size(22.dp))
                Spacer(Modifier.width(10.dp))
                Column {
                    Text(choice.def.name, color = Neon.TextHi, fontSize = 17.sp, fontWeight = FontWeight.Bold)
                    Text("外置 PC 核心进程运行中 —— 配置与存档使用核心原生系统", color = Neon.TextDim, fontSize = 11.sp)
                }
                Box(Modifier.weight(1f))
                Text(
                    when (state) {
                        "running" -> "● RUNNING"
                        "exited" -> "○ EXITED"
                        "failed" -> "✕ FAILED"
                        else -> "… STARTING"
                    },
                    color = when (state) {
                        "running" -> Neon.Green
                        "exited" -> Neon.TextDim
                        "failed" -> Neon.Red
                        else -> Neon.Gold
                    },
                    fontSize = 12.sp, fontWeight = FontWeight.Bold
                )
            }
            Spacer(Modifier.height(14.dp))

            NeonPanelLite {
                Text("游戏：${game.title}", color = Neon.Text, fontSize = 13.sp)
                Spacer(Modifier.height(4.dp))
                Text("参数：${engine.resolveLaunchCommand(game.romFile)?.joinToString(" ")?.substringBefore(game.romFile.name)}${game.romFile.name}", color = Neon.TextDim, fontSize = 11.sp)
                choice.def.notes.takeIf { it.isNotBlank() }?.let {
                    Spacer(Modifier.height(6.dp))
                    Text(it, color = Neon.Gold.copy(alpha = 0.9f), fontSize = 11.sp, lineHeight = 15.sp)
                }
                message?.let {
                    Spacer(Modifier.height(6.dp))
                    Text(it, color = if (state == "failed") Neon.Red else Neon.Text, fontSize = 11.sp)
                }
            }
            Spacer(Modifier.height(14.dp))

            Row(horizontalArrangement = Arrangement.spacedBy(10.dp)) {
                if (state == "running") {
                    NeonPrimaryButton("结束核心进程", Modifier, Icons.Rounded.Close) {
                        engine.stopForcibly()
                    }
                }
                if (state == "failed") {
                    NeonPrimaryButton("重试启动", Modifier, Icons.Rounded.PlayArrow) {
                        val ok = engine.loadRom(game.romFile, game.id)
                        state = if (ok) "running" else "failed"
                        if (!ok) message = engine.lastError()
                    }
                }
                NeonPrimaryButton("打开核心目录", Modifier, Icons.Rounded.FolderOpen) {
                    runCatching {
                        val dir = choice.def.resolveExecutable()?.parentFile ?: choice.def.installDir()
                        if (Desktop.isDesktopSupported()) Desktop.getDesktop().open(dir)
                    }
                }
                NeonPrimaryButton("打开日志", Modifier, Icons.Rounded.OpenInNew) {
                    runCatching {
                        if (Desktop.isDesktopSupported()) Desktop.getDesktop().open(Paths.logsDir)
                    }
                }
                Box(Modifier.weight(1f))
                if (state != "running") {
                    NeonPrimaryButton("返回游戏站", Modifier, Icons.AutoMirrored.Rounded.ArrowBack, onExit)
                }
            }

            if (state == "running") {
                Spacer(Modifier.height(10.dp))
                Text(
                    "游戏正在核心窗口中运行 —— 退出核心后自动回到 NesStation",
                    color = Neon.TextDim, fontSize = 11.sp
                )
            }
        }
    }
}

@Composable
fun NeonPanelLite(content: @Composable () -> Unit) {
    Box(
        Modifier
            .fillMaxWidth()
            .clip(RoundedCornerShape(8.dp))
            .background(Neon.BgDeep)
            .padding(12.dp)
    ) { content() }
}
