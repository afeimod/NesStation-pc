package com.nesstation.app.ui.settings

import androidx.compose.foundation.background
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
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.rounded.ArrowBack
import androidx.compose.material.icons.automirrored.rounded.OpenInNew
import androidx.compose.material.icons.rounded.CheckCircle
import androidx.compose.material.icons.rounded.CloudDownload
import androidx.compose.material.icons.rounded.Delete
import androidx.compose.material.icons.rounded.FolderOpen
import androidx.compose.material.icons.rounded.Memory
import androidx.compose.material.icons.rounded.Terminal
import androidx.compose.material3.Icon
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.nesstation.app.core.coremgmt.CoreDownloader
import com.nesstation.app.core.coremgmt.CoreRegistry
import com.nesstation.app.core.coremgmt.ExternalCoreDef
import com.nesstation.app.core.coremgmt.LibretroCoreDef
import com.nesstation.app.core.coremgmt.currentOs
import com.nesstation.app.core.storage.SettingsStore
import com.nesstation.app.ui.library.FileBrowserDialog
import com.nesstation.app.ui.neon.Neon
import com.nesstation.app.ui.neon.NeonPrimaryButton
import com.nesstation.app.ui.neon.neonChamfer
import kotlinx.coroutines.launch
import java.awt.Desktop
import java.io.File

/**
 * ★ 核心管理页 ★
 *
 *  - 进程内 libretro 核心：从 libretro buildbot 一键下载/更新（按当前 OS/架构）。
 *  - 外置 PC 核心检测：自动发现安装位置；支持 GitHub Release 自动下载；
 *    也可手动选择可执行文件路径。
 */
@Composable
fun CoreManagerScreen(onBack: () -> Unit, modifier: Modifier = Modifier) {
    val scope = rememberCoroutineScope()
    var progress by remember { mutableStateOf<Pair<Int, String>?>(null) }
    var pickFor by remember { mutableStateOf<ExternalCoreDef?>(null) }
    var refresh by remember { mutableStateOf(0) }

    LaunchedEffect(refresh) { } // 触发重算

    Box(modifier.fillMaxSize().background(Neon.Bg)) {
        Column(Modifier.fillMaxSize()) {
            // 顶栏
            Row(
                Modifier
                    .fillMaxWidth()
                    .background(Neon.BgPanel)
                    .padding(horizontal = 20.dp, vertical = 12.dp),
                verticalAlignment = Alignment.CenterVertically
            ) {
                Icon(
                    Icons.AutoMirrored.Rounded.ArrowBack, null, tint = Neon.Accent,
                    modifier = Modifier.size(20.dp).clickable(onClick = onBack)
                )
                Spacer(Modifier.width(12.dp))
                Icon(Icons.Rounded.Memory, null, tint = Neon.Purple, modifier = Modifier.size(20.dp))
                Spacer(Modifier.width(8.dp))
                Text("核心管理", color = Neon.TextHi, fontSize = 17.sp, fontWeight = FontWeight.Bold)
                Box(Modifier.weight(1f))
                Text(
                    "当前平台：${currentOs().replaceFirstChar { it.uppercase() }}",
                    color = Neon.TextDim, fontSize = 11.sp
                )
            }

            progress?.let { (pct, txt) ->
                Column(
                    Modifier
                        .fillMaxWidth()
                        .background(Neon.BgPanelHi)
                        .padding(horizontal = 20.dp, vertical = 8.dp)
                ) {
                    Text(txt, color = Neon.Accent, fontSize = 12.sp)
                    Spacer(Modifier.height(4.dp))
                    Box(
                        Modifier
                            .fillMaxWidth()
                            .height(4.dp)
                            .clip(RoundedCornerShape(2.dp))
                            .background(Neon.BgDeep)
                    ) {
                        if (pct >= 0) {
                            Box(
                                Modifier
                                    .fillMaxWidth(pct / 100f)
                                    .height(4.dp)
                                    .background(Neon.Accent)
                            )
                        }
                    }
                }
            }

            LazyColumn(Modifier.weight(1f).padding(16.dp)) {
                // ===== libretro =====
                item {
                    SectionHeader("进程内核心（libretro · buildbot 官方 PC 构建）")
                }
                items(CoreRegistry.LIBRETRO.distinctBy { it.id }) { def ->
                    LibretroCoreRow(
                        def = def,
                        installed = def.installed,
                        busy = progress != null,
                        onInstall = {
                            scope.launch {
                                progress = 0 to "准备下载 ${def.name}…"
                                CoreDownloader.installLibretro(def) { p -> progress = p.percent to p.text }
                                    .onSuccess { progress = 100 to "已安装 ${def.name} ✓"; refresh++ }
                                    .onFailure { progress = -1 to "安装失败：${it.message}" }
                            }
                        },
                        onDelete = {
                            def.installedFile()?.delete()
                            refresh++
                        }
                    )
                }

                // ===== 外置核心 =====
                item {
                    SectionHeader("外置 PC 核心独立模拟器（GitHub 官方发布）")
                }
                items(CoreRegistry.EXTERNAL) { def ->
                    ExternalCoreRow(
                        def = def,
                        installed = def.installed,
                        busy = progress != null,
                        refresh = refresh,
                        onInstall = {
                            scope.launch {
                                progress = 0 to "准备下载 ${def.name}…"
                                CoreDownloader.installExternal(def) { p -> progress = p.percent to p.text }
                                    .onSuccess { progress = 100 to "已安装 ${def.name} ✓"; refresh++ }
                                    .onFailure {
                                        progress = -1 to "自动下载失败：${it.message}（可手动安装）"
                                    }
                            }
                        },
                        onPick = { pickFor = def },
                        onOpenPage = {
                            runCatching {
                                if (Desktop.isDesktopSupported())
                                    Desktop.getDesktop().browse(java.net.URI(def.downloadPage))
                            }
                        },
                        onDelete = {
                            def.installDir().deleteRecursively()
                            SettingsStore.update { it.externalCorePaths.remove(def.id) }
                            refresh++
                        }
                    )
                }
                item { Spacer(Modifier.height(30.dp)) }
            }
        }

        if (pickFor != null) {
            val def = pickFor!!
            FileBrowserDialog(
                title = "选择 ${def.name} 可执行文件（exe / AppImage / jar）",
                mode = FileBrowserDialog.Mode.PICK_FILE,
                extensions = emptyList(),
                onDismiss = { pickFor = null },
                onSelected = { path ->
                    SettingsStore.update { it.externalCorePaths[def.id] = path }
                    pickFor = null
                    refresh++
                }
            )
        }
    }
}

@Composable
private fun SectionHeader(title: String) {
    Text(
        title,
        color = Neon.Accent, fontSize = 12.sp, fontWeight = FontWeight.Bold,
        modifier = Modifier.padding(top = 18.dp, bottom = 8.dp, start = 4.dp)
    )
}

@Composable
private fun LibretroCoreRow(
    def: LibretroCoreDef,
    installed: Boolean,
    busy: Boolean,
    onInstall: () -> Unit,
    onDelete: () -> Unit
) {
    Row(
        Modifier
            .fillMaxWidth()
            .padding(vertical = 4.dp)
            .clip(neonChamfer(0.12f))
            .background(Neon.BgPanel)
            .padding(14.dp),
        verticalAlignment = Alignment.CenterVertically
    ) {
        Column(Modifier.weight(1f)) {
            Row(verticalAlignment = Alignment.CenterVertically) {
                Text(def.name, color = Neon.TextHi, fontSize = 14.sp, fontWeight = FontWeight.Bold)
                Spacer(Modifier.width(8.dp))
                Text(def.platform.displayName, color = Neon.TextDim, fontSize = 11.sp)
                if (def.experimental) {
                    Spacer(Modifier.width(6.dp))
                    Text("实验性", color = Neon.Gold, fontSize = 10.sp)
                }
            }
            Spacer(Modifier.height(3.dp))
            Text(
                def.biosNote.ifBlank { def.sourceRepo },
                color = Neon.TextDim, fontSize = 10.sp
            )
        }
        if (installed) {
            Icon(Icons.Rounded.CheckCircle, null, tint = Neon.Green, modifier = Modifier.size(16.dp))
            Spacer(Modifier.width(10.dp))
            Text("已安装", color = Neon.Green, fontSize = 11.sp)
            Spacer(Modifier.width(10.dp))
            Text("更新", color = Neon.Accent, fontSize = 12.sp,
                modifier = Modifier.clickable(enabled = !busy, onClick = onInstall).padding(4.dp))
            Spacer(Modifier.width(6.dp))
            Icon(Icons.Rounded.Delete, null, tint = Neon.Red, modifier = Modifier
                .size(15.dp)
                .clickable(onClick = onDelete))
        } else {
            NeonPrimaryButton("下载", Modifier, Icons.Rounded.CloudDownload, onInstall)
        }
    }
}

@Composable
private fun ExternalCoreRow(
    def: ExternalCoreDef,
    installed: Boolean,
    busy: Boolean,
    refresh: Int,
    onInstall: () -> Unit,
    onPick: () -> Unit,
    onOpenPage: () -> Unit,
    onDelete: () -> Unit
) {
    Row(
        Modifier
            .fillMaxWidth()
            .padding(vertical = 4.dp)
            .clip(neonChamfer(0.12f))
            .background(Neon.BgPanel)
            .padding(14.dp),
        verticalAlignment = Alignment.CenterVertically
    ) {
        Column(Modifier.weight(1f)) {
            Row(verticalAlignment = Alignment.CenterVertically) {
                Icon(Icons.Rounded.Terminal, null, tint = Neon.Purple, modifier = Modifier.size(14.dp))
                Spacer(Modifier.width(6.dp))
                Text(def.name, color = Neon.TextHi, fontSize = 14.sp, fontWeight = FontWeight.Bold)
                Spacer(Modifier.width(8.dp))
                Text(def.platform.displayName, color = Neon.TextDim, fontSize = 11.sp)
            }
            Spacer(Modifier.height(3.dp))
            Text(
                def.notes.ifBlank { def.sourceRepo },
                color = Neon.TextDim, fontSize = 10.sp
            )
            // 已解析的路径
            val exe = remember(installed, refresh) { def.resolveExecutable() }
            if (exe != null) {
                Text("→ ${exe.absolutePath}", color = Neon.Green.copy(alpha = 0.8f), fontSize = 10.sp)
            }
        }
        Row(horizontalArrangement = Arrangement.spacedBy(6.dp), verticalAlignment = Alignment.CenterVertically) {
            if (def.githubApi != null) {
                if (installed) {
                    Text("已就绪", color = Neon.Green, fontSize = 11.sp)
                } else {
                    NeonPrimaryButton("自动下载", Modifier, Icons.Rounded.CloudDownload, onInstall)
                }
            } else {
                Icon(Icons.AutoMirrored.Rounded.OpenInNew, null, tint = Neon.Blue,
                    modifier = Modifier.size(16.dp).clickable(onClick = onOpenPage))
            }
            NeonPrimaryButton("手动选择", Modifier, Icons.Rounded.FolderOpen, onPick)
            if (installed) {
                Icon(Icons.Rounded.Delete, null, tint = Neon.Red, modifier = Modifier
                    .size(15.dp)
                    .clickable(onClick = onDelete))
            }
        }
    }
}
