package com.nesstation.app.ui.neon

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
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.rounded.ArrowBack
import androidx.compose.material.icons.rounded.Delete
import androidx.compose.material.icons.rounded.FolderOpen
import androidx.compose.material.icons.rounded.PlayArrow
import androidx.compose.material.icons.rounded.Refresh
import androidx.compose.material.icons.rounded.Search
import androidx.compose.material.icons.rounded.Star
import androidx.compose.material3.Icon
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.OutlinedTextFieldDefaults
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
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.nesstation.app.core.model.GameEntry
import com.nesstation.app.core.model.GamePlatform
import com.nesstation.app.core.model.platformEra
import com.nesstation.app.core.model.platformFullName
import com.nesstation.app.core.storage.RomLibrary
import com.nesstation.app.ui.components.PlatformBadge
import com.nesstation.app.ui.components.platformColors
import com.nesstation.app.ui.library.FileBrowserDialog

/**
 * ★★ Neon 3D 游戏库 ★★
 *
 * 左侧核心/平台列表（平台徽标 + 数量），右侧每平台 3D 封面滚动（NeonFlow）；
 * mode: all | fav | recent（对应主页菜单入口）。
 */
@Composable
fun NeonLibraryScreen(
    mode: String,
    initialPlatform: GamePlatform?,
    games: List<GameEntry>,
    onOpenGame: (GameEntry) -> Unit,
    onToggleFavorite: (GameEntry) -> Unit,
    onDeleteGame: (GameEntry) -> Unit,
    onImportFolder: (path: String, platform: GamePlatform?) -> Unit,
    onRescan: () -> Unit,
    onBack: () -> Unit,
    modifier: Modifier = Modifier
) {
    var query by remember { mutableStateOf("") }
    var selectedPlatform by remember { mutableStateOf(initialPlatform) }
    var showBrowser by remember { mutableStateOf(false) }
    var browserPlatform by remember { mutableStateOf<GamePlatform?>(null) }

    val platformGroups = remember(games) {
        GamePlatform.entries.map { p -> p to games.filter { it.platformEnum == p } }
            .filter { it.second.isNotEmpty() }
    }

    val shown = remember(games, mode, query, selectedPlatform) {
        var list = when (mode) {
            "fav" -> games.filter { it.favorite }
            "recent" -> games.filter { it.lastPlayedAt > 0 }.sortedByDescending { it.lastPlayedAt }
            else -> games
        }
        if (selectedPlatform != null) list = list.filter { it.platformEnum == selectedPlatform }
        if (query.isNotBlank()) list = list.filter { it.title.lowercase().contains(query.lowercase()) }
        list
    }

    Box(modifier.fillMaxSize()) {
        NeonBackdrop()

        Row(Modifier.fillMaxSize()) {
            // ===== 左侧：核心列表 =====
            Column(
                Modifier
                    .width(230.dp)
                    .fillMaxHeight()
                    .background(Neon.BgPanel.copy(alpha = 0.6f))
                    .padding(vertical = 10.dp)
            ) {
                // 返回
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
                Spacer(Modifier.height(6.dp))

                Text(
                    when (mode) {
                        "fav" -> "收藏"
                        "recent" -> "最近游玩"
                        else -> "全部游戏"
                    },
                    color = Neon.TextDim, fontSize = 11.sp,
                    modifier = Modifier.padding(horizontal = 16.dp)
                )
                Spacer(Modifier.height(4.dp))

                // 平台列表
                LazyColumn(Modifier.weight(1f)) {
                    item {
                        SideEntry("全部平台", "ALL", null, selectedPlatform == null && mode == "all") {
                            selectedPlatform = null
                        }
                    }
                    items(platformGroups) { (p, list) ->
                        SideEntry(
                            label = p.displayName,
                            count = "${list.size}",
                            platform = p,
                            selected = selectedPlatform == p
                        ) { selectedPlatform = if (selectedPlatform == p) null else p }
                    }
                }

                // 搜索 + 导入
                Column(Modifier.padding(horizontal = 12.dp, vertical = 8.dp)) {
                    OutlinedTextField(
                        value = query,
                        onValueChange = { query = it },
                        placeholder = { Text("搜索游戏…", fontSize = 12.sp) },
                        leadingIcon = { Icon(Icons.Rounded.Search, null, tint = Neon.TextDim, modifier = Modifier.size(15.dp)) },
                        singleLine = true,
                        textStyle = androidx.compose.ui.text.TextStyle(fontSize = 12.sp, color = Neon.TextHi),
                        colors = OutlinedTextFieldDefaults.colors(
                            focusedBorderColor = Neon.Accent,
                            unfocusedBorderColor = Neon.Line,
                            focusedTextColor = Neon.TextHi,
                            cursorColor = Neon.Accent
                        ),
                        modifier = Modifier.fillMaxWidth()
                    )
                    Spacer(Modifier.height(8.dp))
                    Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                        Box(
                            Modifier
                                .weight(1f)
                                .clip(neonChamfer(0.35f))
                                .background(Neon.BgPanelHi)
                                .clickable { showBrowser = true; browserPlatform = selectedPlatform }
                                .padding(vertical = 8.dp),
                            contentAlignment = Alignment.Center
                        ) {
                            Icon(Icons.Rounded.FolderOpen, null, tint = Neon.Green, modifier = Modifier.size(16.dp))
                        }
                        Box(
                            Modifier
                                .weight(1f)
                                .clip(neonChamfer(0.35f))
                                .background(Neon.BgPanelHi)
                                .clickable(onClick = onRescan)
                                .padding(vertical = 8.dp),
                            contentAlignment = Alignment.Center
                        ) {
                            Icon(Icons.Rounded.Refresh, null, tint = Neon.Blue, modifier = Modifier.size(16.dp))
                        }
                    }
                }
            }

            // ===== 右侧：封面滚动 + 信息 =====
            Column(Modifier.weight(1f).fillMaxHeight()) {
                // 顶部平台标题
                val p = selectedPlatform
                Box(
                    Modifier
                        .fillMaxWidth()
                        .padding(horizontal = 26.dp, vertical = 14.dp)
                ) {
                    Column {
                        Text(
                            p?.let { platformFullName(it) } ?: when (mode) {
                                "fav" -> "收藏的游戏"
                                "recent" -> "最近游玩"
                                else -> "全部游戏"
                            },
                            color = Neon.TextHi, fontSize = 19.sp, fontWeight = FontWeight.Bold
                        )
                        Text(
                            p?.let { platformEra(it) } ?: "LIBRARY · ${shown.size} GAMES",
                            color = (p?.let { platformColors(it).first } ?: Neon.Accent),
                            fontSize = 10.sp,
                            letterSpacing = 2.sp
                        )
                    }
                }

                if (shown.isEmpty()) {
                    Column(
                        Modifier.weight(1f).fillMaxWidth(),
                        horizontalAlignment = Alignment.CenterHorizontally,
                        verticalArrangement = Arrangement.Center
                    ) {
                        Text("这里还没有游戏", color = Neon.TextDim, fontSize = 14.sp)
                        Spacer(Modifier.height(8.dp))
                        Text(
                            "左下角导入 ROM 文件夹，或把游戏放进 设置→存储 的扫描目录",
                            color = Neon.TextDim.copy(alpha = 0.7f), fontSize = 11.sp
                        )
                    }
                } else {
                    var selIndex by remember(shown, selectedPlatform, query) { mutableStateOf(0) }
                    Box(Modifier.weight(1f).fillMaxWidth()) {
                        NeonFlow(
                            count = shown.size,
                            selectedIndex = selIndex.coerceIn(0, shown.size - 1),
                            onIndexChange = { selIndex = it },
                            onItemClick = { shown.getOrNull(it)?.let(onOpenGame) },
                            cardWidth = 170.dp
                        ) { i ->
                            NeonCoverCard(
                                game = shown[i],
                                glow = if (i == selIndex) 1f else 0f,
                                modifier = Modifier.fillMaxSize()
                            )
                        }
                    }
                    // 底部信息条
                    val g = shown.getOrNull(selIndex.coerceIn(0, shown.size - 1))
                    if (g != null) {
                        Row(
                            Modifier
                                .fillMaxWidth()
                                .background(Neon.BgDeep.copy(alpha = 0.8f))
                                .padding(horizontal = 26.dp, vertical = 10.dp),
                            verticalAlignment = Alignment.CenterVertically
                        ) {
                            Column(Modifier.weight(1f)) {
                                Text(g.title, color = Neon.TextHi, fontSize = 15.sp,
                                    fontWeight = FontWeight.Bold, maxLines = 1, overflow = TextOverflow.Ellipsis)
                                Spacer(Modifier.height(3.dp))
                                Row(verticalAlignment = Alignment.CenterVertically) {
                                    PlatformBadge(g.platformEnum)
                                    Spacer(Modifier.width(8.dp))
                                    Text(g.romFile.parentFile?.name ?: "", color = Neon.TextDim, fontSize = 10.sp, maxLines = 1)
                                }
                            }
                            Icon(
                                Icons.Rounded.Star, null,
                                tint = if (g.favorite) Neon.Gold else Neon.TextDim,
                                modifier = Modifier
                                    .size(18.dp)
                                    .clickable { onToggleFavorite(g) }
                                    .padding(2.dp)
                            )
                            Spacer(Modifier.width(12.dp))
                            Icon(
                                Icons.Rounded.Delete, null, tint = Neon.Red,
                                modifier = Modifier
                                    .size(18.dp)
                                    .clickable { onDeleteGame(g) }
                                    .padding(2.dp)
                            )
                            Spacer(Modifier.width(12.dp))
                            NeonPrimaryButton("开始", Modifier, Icons.Rounded.PlayArrow) { onOpenGame(g) }
                        }
                    }
                }
            }
        }

        if (showBrowser) {
            FileBrowserDialog(
                title = if (browserPlatform != null) "导入 ${browserPlatform!!.displayName} 游戏" else "导入游戏文件夹",
                mode = FileBrowserDialog.Mode.PICK_FOLDER,
                onDismiss = { showBrowser = false },
                onSelected = { path ->
                    showBrowser = false
                    onImportFolder(path, browserPlatform)
                }
            )
        }
    }
}

@Composable
private fun SideEntry(
    label: String,
    count: String,
    platform: GamePlatform?,
    selected: Boolean,
    onClick: () -> Unit
) {
    val accent = platform?.let { platformColors(it).first } ?: Neon.Accent
    Row(
        Modifier
            .fillMaxWidth()
            .padding(horizontal = 8.dp, vertical = 1.dp)
            .clip(neonChamfer(0.3f))
            .background(if (selected) Neon.BgPanelHi else Color.Transparent)
            .clickable(onClick = onClick)
            .padding(horizontal = 10.dp, vertical = 7.dp),
        verticalAlignment = Alignment.CenterVertically
    ) {
        Box(
            Modifier
                .size(6.dp)
                .clip(neonChamfer(0.5f))
                .background(if (selected) accent else Neon.TextDim)
        )
        Spacer(Modifier.width(10.dp))
        Text(label, color = if (selected) Neon.TextHi else Neon.Text, fontSize = 13.sp,
            fontWeight = if (selected) FontWeight.Bold else FontWeight.Normal)
        Box(Modifier.weight(1f))
        Text(count, color = Neon.TextDim, fontSize = 11.sp)
    }
}
