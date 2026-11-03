package com.nesstation.app.ui.library

import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.horizontalScroll
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.aspectRatio
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.lazy.grid.GridCells
import androidx.compose.foundation.lazy.grid.LazyVerticalGrid
import androidx.compose.foundation.lazy.grid.items
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.rounded.ArrowBack
import androidx.compose.material.icons.rounded.FolderOpen
import androidx.compose.material.icons.rounded.PlayArrow
import androidx.compose.material.icons.rounded.Refresh
import androidx.compose.material.icons.rounded.Star
import androidx.compose.material3.Icon
import androidx.compose.material3.MaterialTheme
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
import com.nesstation.app.core.storage.RomLibrary
import com.nesstation.app.ui.components.GameCover
import com.nesstation.app.ui.components.PlatformBadge
import com.nesstation.app.ui.components.platformColors
import com.nesstation.app.ui.theme.NesTheme

/**
 * 经典游戏库（FSD 风格）：顶部平台过滤条 + 封面网格。
 * mode: all | fav | recent；点击开始，⭐ 收藏。
 */
@Composable
fun LibraryScreen(
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
    NesTheme(darkTheme = com.nesstation.app.core.storage.SettingsStore.load().darkTheme) {
        var selectedPlatform by remember { mutableStateOf(initialPlatform) }
        var showBrowser by remember { mutableStateOf(false) }

        val shown = remember(games, mode, selectedPlatform) {
            var list = when (mode) {
                "fav" -> games.filter { it.favorite }
                "recent" -> games.filter { it.lastPlayedAt > 0 }.sortedByDescending { it.lastPlayedAt }
                else -> games.sortedWith(compareBy({ it.platform }, { it.title }))
            }
            if (selectedPlatform != null) list = list.filter { it.platformEnum == selectedPlatform }
            list
        }
        val platforms = remember(games) {
            games.map { it.platformEnum }.distinct().sortedBy { it.ordinal }
        }

        Box(modifier.fillMaxSize().background(MaterialTheme.colorScheme.background)) {
            Column(Modifier.fillMaxSize()) {
                // 顶部栏
                Row(
                    Modifier
                        .fillMaxWidth()
                        .background(MaterialTheme.colorScheme.surface.copy(alpha = 0.9f))
                        .padding(horizontal = 20.dp, vertical = 10.dp),
                    verticalAlignment = Alignment.CenterVertically
                ) {
                    Icon(
                        Icons.AutoMirrored.Rounded.ArrowBack, null,
                        tint = MaterialTheme.colorScheme.onSurface,
                        modifier = Modifier.size(20.dp).clickable(onClick = onBack)
                    )
                    Spacer(Modifier.width(14.dp))
                    Text(
                        when (mode) {
                            "fav" -> "收藏"
                            "recent" -> "最近游玩"
                            else -> "游戏库"
                        },
                        color = MaterialTheme.colorScheme.onSurface,
                        fontSize = 18.sp, fontWeight = FontWeight.Bold
                    )
                    Spacer(Modifier.width(10.dp))
                    Text("${shown.size}", color = MaterialTheme.colorScheme.primary, fontSize = 14.sp, fontWeight = FontWeight.Bold)
                    Box(Modifier.weight(1f))
                    Icon(
                        Icons.Rounded.FolderOpen, "导入",
                        tint = MaterialTheme.colorScheme.onSurfaceVariant,
                        modifier = Modifier.size(20.dp).clickable { showBrowser = true }
                    )
                    Spacer(Modifier.width(14.dp))
                    Icon(
                        Icons.Rounded.Refresh, "重新扫描",
                        tint = MaterialTheme.colorScheme.onSurfaceVariant,
                        modifier = Modifier.size(20.dp).clickable(onClick = onRescan)
                    )
                }

                // 平台过滤条
                Row(
                    Modifier
                        .fillMaxWidth()
                        .horizontalScroll(rememberScrollState())
                        .padding(horizontal = 20.dp, vertical = 6.dp),
                    horizontalArrangement = Arrangement.spacedBy(8.dp)
                ) {
                    FilterChip("全部", selectedPlatform == null) { selectedPlatform = null }
                    platforms.forEach { p ->
                        FilterChip(p.displayName, selectedPlatform == p) {
                            selectedPlatform = if (selectedPlatform == p) null else p
                        }
                    }
                }

                // 封面网格
                LazyVerticalGrid(
                    columns = GridCells.Adaptive(150.dp),
                    modifier = Modifier.weight(1f).padding(horizontal = 20.dp),
                    horizontalArrangement = Arrangement.spacedBy(14.dp),
                    verticalArrangement = Arrangement.spacedBy(14.dp)
                ) {
                    items(shown, key = { it.id }) { g ->
                        Box(
                            Modifier
                                .clip(RoundedCornerShape(10.dp))
                                .background(MaterialTheme.colorScheme.surface)
                                .clickable { onOpenGame(g) }
                        ) {
                            Column {
                                Box(
                                    Modifier
                                        .fillMaxWidth()
                                        .aspectRatio(1f)
                                ) {
                                    GameCover(
                                        game = g,
                                        modifier = Modifier.fillMaxSize()
                                    )
                                    Icon(
                                        Icons.Rounded.Star, null,
                                        tint = if (g.favorite) Color(0xFFF2C200) else Color(0x55FFFFFF),
                                        modifier = Modifier
                                            .align(Alignment.TopEnd)
                                            .padding(6.dp)
                                            .size(18.dp)
                                            .clickable { onToggleFavorite(g) }
                                    )
                                }
                                Column(Modifier.padding(8.dp)) {
                                    Text(
                                        g.title,
                                        color = MaterialTheme.colorScheme.onSurface,
                                        fontSize = 12.sp, fontWeight = FontWeight.SemiBold,
                                        maxLines = 1, overflow = TextOverflow.Ellipsis
                                    )
                                    Spacer(Modifier.height(3.dp))
                                    PlatformBadge(g.platformEnum)
                                }
                            }
                        }
                    }
                }
            }

            if (showBrowser) {
                FileBrowserDialog(
                    title = "导入游戏文件夹",
                    mode = FileBrowserDialog.Mode.PICK_FOLDER,
                    onDismiss = { showBrowser = false },
                    onSelected = { path ->
                        showBrowser = false
                        onImportFolder(path, selectedPlatform)
                    }
                )
            }
        }
    }
}

@Composable
private fun FilterChip(label: String, selected: Boolean, onClick: () -> Unit) {
    Box(
        Modifier
            .clip(RoundedCornerShape(14.dp))
            .background(
                if (selected) MaterialTheme.colorScheme.primary
                else MaterialTheme.colorScheme.surfaceVariant
            )
            .clickable(onClick = onClick)
            .padding(horizontal = 12.dp, vertical = 5.dp)
    ) {
        Text(
            label,
            color = if (selected) MaterialTheme.colorScheme.onPrimary
            else MaterialTheme.colorScheme.onSurfaceVariant,
            fontSize = 12.sp, fontWeight = FontWeight.SemiBold
        )
    }
}
