package com.nesstation.app.ui.settings

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
import androidx.compose.material.icons.rounded.Dns
import androidx.compose.material.icons.rounded.Gamepad
import androidx.compose.material.icons.rounded.Memory
import androidx.compose.material3.Icon
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Switch
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
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
import com.nesstation.app.core.storage.SettingsStore
import com.nesstation.app.ui.library.FileBrowserDialog
import com.nesstation.app.ui.neon.NeonUi
import com.nesstation.app.ui.theme.NesTheme

/**
 * 经典设置页（FSD 风格使用；与 Neon 3D 设置页同语义、Material 视觉）。
 * ★「外观 → 主界面风格」= 两套主题切换入口。
 */
@Composable
fun SettingsScreen(
    onOpenCoreManager: () -> Unit,
    onOpenKeyMap: () -> Unit,
    onOpenAbout: () -> Unit,
    onBack: () -> Unit,
    modifier: Modifier = Modifier
) {
    NesTheme(darkTheme = SettingsStore.load().darkTheme) {
        var s by remember { mutableStateOf(SettingsStore.load()) }
        var pickRoot by remember { mutableStateOf(false) }

        fun save(block: (com.nesstation.app.core.storage.AppSettings) -> Unit) {
            val cur = SettingsStore.load()
            block(cur)
            SettingsStore.save(cur)
            s = cur
        }

        Column(
            modifier
                .fillMaxSize()
                .verticalScroll(rememberScrollState())
                .padding(24.dp)
        ) {
            // 顶栏
            Row(verticalAlignment = Alignment.CenterVertically) {
                Icon(
                    Icons.AutoMirrored.Rounded.ArrowBack, null,
                    tint = MaterialTheme.colorScheme.onSurface,
                    modifier = Modifier.size(20.dp).clickable(onClick = onBack)
                )
                Spacer(Modifier.width(12.dp))
                Text(
                    "总设置", color = MaterialTheme.colorScheme.onSurface,
                    fontSize = 20.sp, fontWeight = FontWeight.Bold
                )
            }

            SettingsSection("外观") {
                // ★★ 主界面风格切换（两套主题）★★
                Row(horizontalArrangement = Arrangement.spacedBy(10.dp)) {
                    StyleOption(
                        "Neon 3D 新UI", "深空底 + 电光青 + 3D 封面流（默认）",
                        selected = NeonUi.isNeon, Modifier.weight(1f)
                    ) { NeonUi.set("neon") }
                    StyleOption(
                        "FSD 经典桌面", "Xbox 360 磁贴流 + 像素天空",
                        selected = !NeonUi.isNeon, Modifier.weight(1f)
                    ) { NeonUi.set("fsd") }
                }
                SettingRow("深色主题（FSD 风格）", "Neon 3D 恒为深空配色") {
                    Switch(checked = s.darkTheme, onCheckedChange = { v -> save { it.darkTheme = v } })
                }
            }

            SettingsSection("视频") {
                SettingRow("保持宽高比", "按核心宽高比缩放") {
                    Switch(checked = s.keepAspect, onCheckedChange = { v -> save { it.keepAspect = v } })
                }
                SettingRow("整数缩放", "像素完美放大") {
                    Switch(checked = s.integerScaling, onCheckedChange = { v -> save { it.integerScaling = v } })
                }
                SettingRow("启动游戏时全屏", "进入游戏即全屏") {
                    Switch(checked = s.fullscreenOnLaunch, onCheckedChange = { v -> save { it.fullscreenOnLaunch = v } })
                }
                SettingRow("显示帧率 HUD", "游戏右上角 FPS") {
                    Switch(checked = s.showFps, onCheckedChange = { v -> save { it.showFps = v } })
                }
            }

            SettingsSection("性能") {
                SettingRow("默认快进倍速", "当前：${s.fastForwardSpeed}×（游戏内 Tab 按住快进）") {}
                SettingRow("音频缓冲", "当前：${s.audioBufferMs}ms（爆音时调大）") {}
            }

            SettingsSection("输入") {
                SettingRow("键位映射", "每平台键盘键位（Windows XInput / Linux 手柄自动支持）") {
                    Text(
                        "进入映射", color = MaterialTheme.colorScheme.primary,
                        fontSize = 13.sp, fontWeight = FontWeight.Bold,
                        modifier = Modifier.clickable(onClick = onOpenKeyMap)
                    )
                }
            }

            SettingsSection("存储") {
                SettingRow("ROM 扫描目录", "启动时自动入库") {
                    Text(
                        "添加", color = MaterialTheme.colorScheme.primary,
                        fontSize = 13.sp, fontWeight = FontWeight.Bold,
                        modifier = Modifier.clickable { pickRoot = true }
                    )
                }
                s.romRoots.split(';', ';').filter { it.isNotBlank() }.forEach { root ->
                    Text(
                        "· $root   [移除]", color = MaterialTheme.colorScheme.onSurfaceVariant,
                        fontSize = 12.sp,
                        modifier = Modifier
                            .padding(vertical = 2.dp)
                            .clickable {
                                save {
                                    it.romRoots = it.romRoots.split(';', ';')
                                        .filter { r -> r.isNotBlank() && r != root }
                                        .joinToString(";")
                                }
                            }
                    )
                }
                SettingRow(
                    "数据目录",
                    com.nesstation.app.core.storage.Paths.dataDir.absolutePath
                ) {}
            }

            SettingsSection("核心设置") {
                SettingRow("核心管理", "libretro 进程内核心 + 外置 PC 核心下载/检测") {
                    Row {
                        Icon(
                            Icons.Rounded.Memory, null,
                            tint = MaterialTheme.colorScheme.primary,
                            modifier = Modifier.size(20.dp).clickable(onClick = onOpenCoreManager)
                        )
                    }
                }
            }

            SettingsSection("关于") {
                SettingRow("NesStation for PC", "v1.0.0 · 基于 Android 版重写") {
                    Text(
                        "查看", color = MaterialTheme.colorScheme.primary,
                        fontSize = 13.sp, fontWeight = FontWeight.Bold,
                        modifier = Modifier.clickable(onClick = onOpenAbout)
                    )
                }
            }
            Spacer(Modifier.height(40.dp))
        }

        if (pickRoot) {
            FileBrowserDialog(
                title = "选择 ROM 扫描目录",
                mode = FileBrowserDialog.Mode.PICK_FOLDER,
                onDismiss = { pickRoot = false },
                onSelected = { path ->
                    pickRoot = false
                    save {
                        val roots = it.romRoots.split(';', ';').filter { r -> r.isNotBlank() }.toMutableList()
                        if (roots.none { r -> r == path }) roots.add(path)
                        it.romRoots = roots.joinToString(";")
                    }
                }
            )
        }
    }
}

@Composable
private fun SettingsSection(title: String, content: @Composable () -> Unit) {
    Text(
        title, color = MaterialTheme.colorScheme.primary,
        fontSize = 13.sp, fontWeight = FontWeight.Bold,
        modifier = Modifier.padding(top = 20.dp, bottom = 6.dp)
    )
    Column(
        Modifier
            .fillMaxWidth()
            .clip(RoundedCornerShape(12.dp))
            .background(MaterialTheme.colorScheme.surface.copy(alpha = 0.9f))
            .padding(14.dp)
    ) { content() }
}

@Composable
private fun SettingRow(label: String, sub: String, trailing: @Composable () -> Unit) {
    Row(
        Modifier.fillMaxWidth().padding(vertical = 6.dp),
        verticalAlignment = Alignment.CenterVertically
    ) {
        Column(Modifier.weight(1f)) {
            Text(label, color = MaterialTheme.colorScheme.onSurface, fontSize = 13.sp)
            if (sub.isNotBlank()) {
                Text(sub, color = MaterialTheme.colorScheme.onSurfaceVariant, fontSize = 11.sp, lineHeight = 15.sp)
            }
        }
        trailing()
    }
}

@Composable
private fun StyleOption(
    title: String, desc: String,
    selected: Boolean, modifier: Modifier = Modifier, onClick: () -> Unit
) {
    Column(
        modifier
            .clip(RoundedCornerShape(10.dp))
            .background(
                if (selected) MaterialTheme.colorScheme.primaryContainer
                else MaterialTheme.colorScheme.surfaceVariant
            )
            .clickable(onClick = onClick)
            .padding(12.dp)
    ) {
        Text(
            title,
            color = if (selected) MaterialTheme.colorScheme.onPrimaryContainer
            else MaterialTheme.colorScheme.onSurface,
            fontSize = 14.sp, fontWeight = FontWeight.Bold
        )
        Spacer(Modifier.height(4.dp))
        Text(desc, color = MaterialTheme.colorScheme.onSurfaceVariant, fontSize = 11.sp, lineHeight = 14.sp)
    }
}
