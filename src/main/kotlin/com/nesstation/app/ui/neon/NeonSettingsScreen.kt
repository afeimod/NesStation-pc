package com.nesstation.app.ui.neon

import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.rounded.ArrowBack
import androidx.compose.material.icons.rounded.Dns
import androidx.compose.material.icons.rounded.Gamepad
import androidx.compose.material.icons.rounded.Info
import androidx.compose.material.icons.rounded.Memory
import androidx.compose.material.icons.rounded.Palette
import androidx.compose.material.icons.rounded.Percent
import androidx.compose.material.icons.rounded.Speed
import androidx.compose.material.icons.rounded.Tune
import androidx.compose.material3.Icon
import androidx.compose.material3.Switch
import androidx.compose.material3.SwitchDefaults
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
import com.nesstation.app.core.storage.SettingsStore
import com.nesstation.app.ui.library.FileBrowserDialog

/**
 * ★★ Neon 3D 总设置 ★★
 *
 * 左侧分类导航（外观/视频/显示/性能/输入/存储/核心设置/关于）+ Neon 面板。
 * 「外观 → 主界面风格」即两套主题切换：Neon 3D（默认）/ FSD 经典。
 */
@Composable
fun NeonSettingsScreen(
    onOpenCoreManager: () -> Unit,
    onOpenKeyMap: () -> Unit,
    onOpenAbout: () -> Unit,
    onBack: () -> Unit,
    modifier: Modifier = Modifier
) {
    var category by remember { mutableStateOf("appearance") }
    var s by remember { mutableStateOf(SettingsStore.load()) }

    fun save(block: (com.nesstation.app.core.storage.AppSettings) -> Unit) {
        val cur = SettingsStore.load()
        block(cur)
        SettingsStore.save(cur)
        s = cur
    }

    Box(modifier.fillMaxSize()) {
        NeonBackdrop()
        Row(Modifier.fillMaxSize()) {
            // ===== 左侧分类 =====
            Column(
                Modifier
                    .width(220.dp)
                    .fillMaxHeight()
                    .background(Neon.BgPanel.copy(alpha = 0.6f))
                    .padding(vertical = 10.dp)
            ) {
                Row(
                    Modifier
                        .fillMaxWidth()
                        .clip(neonChamfer(0.3f))
                        .clickable(onClick = onBack)
                        .padding(horizontal = 16.dp, vertical = 9.dp),
                    verticalAlignment = Alignment.CenterVertically
                ) {
                    Icon(Icons.AutoMirrored.Rounded.ArrowBack, null, tint = Neon.Accent, modifier = Modifier.size(16.dp))
                    Spacer(Modifier.width(8.dp))
                    Text("返回主页", color = Neon.Text, fontSize = 13.sp)
                }
                Spacer(Modifier.height(10.dp))
                val cats = listOf(
                    Cat("appearance", "外观", Icons.Rounded.Palette, Neon.Accent),
                    Cat("video", "视频", Icons.Rounded.Tune, Neon.Blue),
                    Cat("display", "显示", Icons.Rounded.Speed, Neon.Green),
                    Cat("performance", "性能", Icons.Rounded.Percent, Neon.Gold),
                    Cat("input", "输入", Icons.Rounded.Gamepad, Neon.BtnX),
                    Cat("storage", "存储", Icons.Rounded.Dns, Neon.Purple),
                    Cat("cores", "核心设置", Icons.Rounded.Memory, Neon.Red),
                    Cat("about", "关于", Icons.Rounded.Info, Neon.Text)
                )
                cats.forEach { c ->
                    Row(
                        Modifier
                            .fillMaxWidth()
                            .padding(horizontal = 8.dp, vertical = 1.dp)
                            .clip(neonChamfer(0.3f))
                            .background(if (category == c.key) Neon.BgPanelHi else Color.Transparent)
                            .clickable { category = c.key }
                            .padding(horizontal = 12.dp, vertical = 9.dp),
                        verticalAlignment = Alignment.CenterVertically
                    ) {
                        Icon(c.icon, null, tint = if (category == c.key) c.color else Neon.TextDim, modifier = Modifier.size(16.dp))
                        Spacer(Modifier.width(10.dp))
                        Text(c.label, color = if (category == c.key) Neon.TextHi else Neon.Text, fontSize = 13.sp)
                    }
                }
            }

            // ===== 右侧内容 =====
            Column(
                Modifier
                    .weight(1f)
                    .fillMaxHeight()
                    .verticalScroll(rememberScrollState())
                    .padding(26.dp)
            ) {
                when (category) {
                    "appearance" -> AppearancePanel(s, ::save)
                    "video" -> VideoPanel(s, ::save)
                    "display" -> DisplayPanel(s, ::save)
                    "performance" -> PerformancePanel(s, ::save)
                    "input" -> InputPanel(onOpenKeyMap)
                    "storage" -> StoragePanel(s, ::save)
                    "cores" -> CorePanel(onOpenCoreManager)
                    "about" -> AboutPanel(onOpenAbout)
                }
            }
        }
    }
}

private data class Cat(val key: String, val label: String, val icon: ImageVector, val color: Color)

@Composable
private fun SettingsBlock(title: String, content: @Composable () -> Unit) {
    Text(title, color = Neon.Accent, fontSize = 12.sp, fontWeight = FontWeight.Bold,
        modifier = Modifier.padding(top = 16.dp, bottom = 6.dp))
    NeonPanel(Modifier.fillMaxWidth()) { Column { content() } }
}

@Composable
private fun NeonRow(label: String, sub: String? = null, trailing: @Composable () -> Unit = {}) {
    Row(
        Modifier.fillMaxWidth().padding(vertical = 7.dp),
        verticalAlignment = Alignment.CenterVertically
    ) {
        Column(Modifier.weight(1f)) {
            Text(label, color = Neon.TextHi, fontSize = 13.sp)
            if (sub != null) Text(sub, color = Neon.TextDim, fontSize = 11.sp, lineHeight = 15.sp)
        }
        trailing()
    }
}

@Composable
private fun NeonSwitch(checked: Boolean, onChange: (Boolean) -> Unit) {
    Switch(
        checked = checked,
        onCheckedChange = onChange,
        colors = SwitchDefaults.colors(
            checkedThumbColor = Neon.BgDeep,
            checkedTrackColor = Neon.Accent,
            uncheckedThumbColor = Neon.TextDim,
            uncheckedTrackColor = Neon.BgPanelHi
        )
    )
}

// ================= 外观 =================

@Composable
private fun AppearancePanel(
    s: com.nesstation.app.core.storage.AppSettings,
    save: ((com.nesstation.app.core.storage.AppSettings) -> Unit) -> Unit
) {
    Text("外观", color = Neon.TextHi, fontSize = 20.sp, fontWeight = FontWeight.Bold)

    SettingsBlock("主界面风格（★ 两套主题切换）") {
        Row(Modifier.fillMaxWidth().padding(vertical = 6.dp), horizontalArrangement = Arrangement.spacedBy(12.dp)) {
            StyleCard(
                title = "Neon 3D 新UI",
                desc = "深空底 + 电光青 + 3D 封面流（默认）",
                selected = NeonUi.isNeon,
                accent = Neon.Accent,
                modifier = Modifier.weight(1f)
            ) { NeonUi.set("neon") }
            StyleCard(
                title = "FSD 经典桌面",
                desc = "Xbox 360 磁贴流 + 像素天空",
                selected = !NeonUi.isNeon,
                accent = Color(0xFF3E64AF),
                modifier = Modifier.weight(1f)
            ) { NeonUi.set("fsd") }
        }
    }

    SettingsBlock("FSD 经典界面明暗") {
        NeonRow("深色主题", "FSD 风格下使用夜间配色（Neon 3D 恒为深空色系）") {
            NeonSwitch(s.darkTheme) { v -> save { it.darkTheme = v } }
        }
    }

    SettingsBlock("自定义背景") {
        var pickBg by remember { mutableStateOf(false) }
        NeonRow(
            "主页背景图片",
            if (s.homeBackgroundUri.isBlank()) "默认：Neon 深空 / FSD 像素天空"
            else s.homeBackgroundUri.substringAfterLast('/')
        ) {
            Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                NeonPrimaryButton("选择", Modifier, null) { pickBg = true }
                if (s.homeBackgroundUri.isNotBlank()) {
                    NeonPrimaryButton("清除", Modifier, null) { save { it.homeBackgroundUri = "" } }
                }
            }
        }
        if (pickBg) {
            FileBrowserDialog(
                title = "选择背景图片",
                mode = FileBrowserDialog.Mode.PICK_FILE,
                extensions = listOf("png", "jpg", "jpeg", "webp", "bmp"),
                onDismiss = { pickBg = false },
                onSelected = { path -> pickBg = false; save { it.homeBackgroundUri = path } }
            )
        }
    }
}

@Composable
private fun StyleCard(
    title: String, desc: String, selected: Boolean,
    accent: Color, modifier: Modifier = Modifier, onClick: () -> Unit
) {
    Column(
        modifier
            .clip(neonChamfer(0.14f))
            .background(if (selected) accent.copy(alpha = 0.16f) else Neon.BgPanelHi)
            .androidBorder(selected, accent)
            .clickable(onClick = onClick)
            .padding(14.dp)
    ) {
        Text(title, color = if (selected) accent else Neon.TextHi, fontSize = 15.sp, fontWeight = FontWeight.Bold)
        Spacer(Modifier.height(4.dp))
        Text(desc, color = Neon.TextDim, fontSize = 11.sp, lineHeight = 15.sp)
    }
}

private fun Modifier.androidBorder(selected: Boolean, accent: Color): Modifier =
    this.then(
        Modifier.border(
            1.dp,
            if (selected) accent else Neon.Line,
            neonChamfer(0.14f)
        )
    )

// ================= 视频 / 显示 / 性能 =================

@Composable
private fun VideoPanel(
    s: com.nesstation.app.core.storage.AppSettings,
    save: ((com.nesstation.app.core.storage.AppSettings) -> Unit) -> Unit
) {
    Text("视频", color = Neon.TextHi, fontSize = 20.sp, fontWeight = FontWeight.Bold)
    SettingsBlock("游戏画面") {
        NeonRow("保持宽高比", "按核心宽高比缩放（关闭则拉伸满屏）") {
            NeonSwitch(s.keepAspect) { v -> save { it.keepAspect = v } }
        }
        NeonRow("整数缩放", "像素完美放大（配合保持宽高比）") {
            NeonSwitch(s.integerScaling) { v -> save { it.integerScaling = v } }
        }
        NeonRow("画面滤镜", "0 无（像素完美）/ 1 双线性 / 2 CRT 扫描线") {
            SpinnerRow(
                options = listOf("无", "双线性", "CRT 扫描线"),
                selected = s.videoFilter,
                onSelect = { v -> save { cfg -> cfg.videoFilter = v } }
            )
        }
        NeonRow("启动游戏时全屏", "进入游戏即切换系统全屏") {
            NeonSwitch(s.fullscreenOnLaunch) { v -> save { it.fullscreenOnLaunch = v } }
        }
    }
    SettingsBlock("FPS 显示") {
        NeonRow("显示帧率 HUD", "游戏画面右上角显示实时 FPS") {
            NeonSwitch(s.showFps) { v -> save { it.showFps = v } }
        }
    }
}

@Composable
private fun DisplayPanel(
    s: com.nesstation.app.core.storage.AppSettings,
    save: ((com.nesstation.app.core.storage.AppSettings) -> Unit) -> Unit
) {
    Text("显示", color = Neon.TextHi, fontSize = 20.sp, fontWeight = FontWeight.Bold)
    SettingsBlock("游戏内菜单") {
        NeonRow("菜单呼出键", "游戏中按此键呼出快捷菜单") {
            Text(s.menuKey, color = Neon.Accent, fontSize = 13.sp, fontWeight = FontWeight.Bold)
        }
        NeonRow("快进按住键", "按住快进（可切换倍速档位）") {
            Text(s.fastForwardKey, color = Neon.Accent, fontSize = 13.sp, fontWeight = FontWeight.Bold)
        }
    }
}

@Composable
private fun PerformancePanel(
    s: com.nesstation.app.core.storage.AppSettings,
    save: ((com.nesstation.app.core.storage.AppSettings) -> Unit) -> Unit
) {
    Text("性能", color = Neon.TextHi, fontSize = 20.sp, fontWeight = FontWeight.Bold)
    SettingsBlock("快进") {
        NeonRow("默认快进倍速", "2 / 4 / 6 / 8 / 16 倍（游戏内可临时切换）") {
            SpinnerRow(
                options = listOf("2×", "4×", "6×", "8×", "16×"),
                selected = s.fastForwardSpeed,
                onSelect = { v -> save { it.fastForwardSpeed = listOf(2, 4, 6, 8, 16)[v] } }
            )
        }
    }
    SettingsBlock("音频") {
        NeonRow("音频缓冲", "毫秒（越大越稳定、延迟越高；爆音时调大）") {
            SpinnerRow(
                options = listOf("32ms", "64ms", "96ms", "128ms"),
                selected = listOf(32, 64, 96, 128).indexOf(s.audioBufferMs).coerceAtLeast(0),
                onSelect = { v -> save { it.audioBufferMs = listOf(32, 64, 96, 128)[v] } }
            )
        }
    }
}

// ================= 输入 / 存储 / 核心 / 关于 =================

@Composable
private fun InputPanel(onOpenKeyMap: () -> Unit) {
    Text("输入", color = Neon.TextHi, fontSize = 20.sp, fontWeight = FontWeight.Bold)
    SettingsBlock("键盘 / 手柄") {
        NeonRow(
            "键位映射",
            "每个平台的键盘键位自定义；Windows 使用 XInput、Linux 使用系统手柄驱动"
        ) {
            NeonPrimaryButton("进入映射", Modifier, Icons.Rounded.Gamepad, onOpenKeyMap)
        }
        NeonRow(
            "默认键位",
            "方向键=十字键 · Z=B X=A A=Y S=X · Q=L E=R · Enter=Start 右Shift=Select"
        )
    }
}

@Composable
private fun StoragePanel(
    s: com.nesstation.app.core.storage.AppSettings,
    save: ((com.nesstation.app.core.storage.AppSettings) -> Unit) -> Unit
) {
    Text("存储", color = Neon.TextHi, fontSize = 20.sp, fontWeight = FontWeight.Bold)
    var pickRoot by remember { mutableStateOf(false) }
    SettingsBlock("ROM 扫描目录") {
        NeonRow(
            "扫描目录",
            "启动/手动刷新时自动入库这些目录（含子目录）"
        ) {
            NeonPrimaryButton("添加", Modifier, null) { pickRoot = true }
        }
        s.romRoots.split(';', ';').filter { it.isNotBlank() }.forEach { root ->
            Row(
                Modifier.fillMaxWidth().padding(vertical = 3.dp),
                verticalAlignment = Alignment.CenterVertically
            ) {
                Text(root, color = Neon.Text, fontSize = 12.sp, modifier = Modifier.weight(1f))
                Text(
                    "移除", color = Neon.Red, fontSize = 12.sp,
                    modifier = Modifier.clickable {
                        val roots = s.romRoots.split(';', ';').filter { it.isNotBlank() && it != root }
                        save { it.romRoots = roots.joinToString(";") }
                    }
                )
            }
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
    SettingsBlock("数据目录") {
        NeonRow("存档/核心/截图位置", com.nesstation.app.core.storage.Paths.dataDir.absolutePath)
        NeonRow(
            "便携模式",
            "在程序目录放置 portable.marker 文件即切换为绿色便携模式"
        )
    }
}

@Composable
private fun CorePanel(onOpenCoreManager: () -> Unit) {
    Text("核心设置", color = Neon.TextHi, fontSize = 20.sp, fontWeight = FontWeight.Bold)
    SettingsBlock("核心管理") {
        NeonRow(
            "下载 / 更新核心",
            "进程内 libretro 核心（buildbot 官方 PC 构建）+ 外置 PC 独立核心（GitHub Release）"
        ) {
            NeonPrimaryButton("核心管理", Modifier, Icons.Rounded.Memory, onOpenCoreManager)
        }
    }
}

@Composable
private fun AboutPanel(onOpenAbout: () -> Unit) {
    Text("关于", color = Neon.TextHi, fontSize = 20.sp, fontWeight = FontWeight.Bold)
    SettingsBlock("NesStation for PC") {
        NeonRow("版本", "1.0.0（PC 首版 · 基于 Android 版 NesStation 重写）") {
            NeonPrimaryButton("详情", Modifier, null, onOpenAbout)
        }
    }
}

/** 选项行（下拉替代：点击循环）。 */
@Composable
fun SpinnerRow(options: List<String>, selected: Int, onSelect: (Int) -> Unit) {
    Row(
        Modifier
            .clip(neonChamfer(0.4f))
            .background(Neon.BgPanelHi)
            .clickable { onSelect((selected + 1).mod(options.size)) }
            .padding(horizontal = 14.dp, vertical = 6.dp)
    ) {
        Text(options.getOrElse(selected) { options.firstOrNull() ?: "" },
            color = Neon.Accent, fontSize = 13.sp, fontWeight = FontWeight.Bold)
    }
}
