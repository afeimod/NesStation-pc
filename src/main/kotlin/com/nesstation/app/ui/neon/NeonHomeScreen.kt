package com.nesstation.app.ui.neon

import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.horizontalScroll
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
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.rounded.HelpOutline
import androidx.compose.material.icons.automirrored.rounded.Logout
import androidx.compose.material.icons.rounded.Favorite
import androidx.compose.material.icons.rounded.GridView
import androidx.compose.material.icons.rounded.History
import androidx.compose.material.icons.rounded.PlayArrow
import androidx.compose.material.icons.rounded.Settings
import androidx.compose.material.icons.rounded.SportsEsports
import androidx.compose.material.icons.rounded.Terminal
import androidx.compose.material3.Icon
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.vector.ImageVector
import androidx.compose.ui.input.key.Key
import androidx.compose.ui.input.key.KeyEventType
import androidx.compose.ui.input.key.key
import androidx.compose.ui.input.key.onKeyEvent
import androidx.compose.ui.input.key.type
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.nesstation.app.core.model.GameEntry
import com.nesstation.app.core.model.GamePlatform
import com.nesstation.app.core.model.formatLastPlayed
import com.nesstation.app.core.model.formatPlayTime
import com.nesstation.app.core.storage.RomLibrary
import com.nesstation.app.core.storage.SettingsStore
import com.nesstation.app.ui.components.PlatformBadge

/**
 * ★★ Neon 3D 主页 ★★
 *
 *   - 顶部：品牌徽标 + 游戏总数 + 时钟
 *   - 菜单行（横滑）：游戏库 / 收藏 / 最近 / 设置 / 核心管理 / 关于 / 退出
 *   - 主视觉：中央 3D 封面流（最近游玩优先排前）—— 选中即预览
 *   - 信息区：选中游戏大标题 + 平台徽章 + 游玩信息 + 操作按钮
 *   - 底部：键盘按键提示栏
 */
@Composable
fun NeonHomeScreen(
    games: List<GameEntry>,
    onOpenLibrary: () -> Unit,
    onOpenFavorites: () -> Unit,
    onOpenHistory: () -> Unit,
    onOpenSettings: () -> Unit,
    onOpenCoreManager: () -> Unit,
    onOpenAbout: () -> Unit,
    onExit: () -> Unit,
    onOpenGame: (GameEntry) -> Unit,
    onToggleFavorite: (GameEntry) -> Unit,
    modifier: Modifier = Modifier
) {
    // 主视觉列表：最近游玩优先
    val flowGames = remember(games) {
        games.sortedWith(
            compareByDescending<GameEntry> { it.lastPlayedAt > 0L }
                .thenByDescending { it.lastPlayedAt }
        )
    }

    var selectedIndex by remember { mutableIntStateOf(0) }
    LaunchedEffect(flowGames.size) {
        if (selectedIndex >= flowGames.size) selectedIndex = 0
    }
    val selIdx = selectedIndex.coerceIn(0, (flowGames.size - 1).coerceAtLeast(0))
    val currentGame = flowGames.getOrNull(selIdx)

    Box(modifier.fillMaxSize()) {
        NeonCoverBackdrop(currentGame)

        Column(Modifier.fillMaxSize()) {
            NeonClockBar(gameCount = games.size)

            NeonMenuRow(
                items = listOf(
                    NeonChip("游戏库", Icons.Rounded.GridView, Neon.Red, onOpenLibrary),
                    NeonChip("收藏", Icons.Rounded.Favorite, Neon.Gold, onOpenFavorites),
                    NeonChip("最近", Icons.Rounded.History, Neon.Blue, onOpenHistory),
                    NeonChip("核心管理", Icons.Rounded.Terminal, Neon.Purple, onOpenCoreManager),
                    NeonChip("设置", Icons.Rounded.Settings, Neon.Text, onOpenSettings),
                    NeonChip("关于", Icons.AutoMirrored.Rounded.HelpOutline, Neon.Text, onOpenAbout),
                    NeonChip("退出", Icons.AutoMirrored.Rounded.Logout, Neon.Red, onExit)
                )
            )

            // ===== 主视觉 3D 封面流 =====
            Box(Modifier.fillMaxWidth().weight(1f)) {
                if (flowGames.isEmpty()) {
                    Column(
                        Modifier.align(Alignment.Center),
                        horizontalAlignment = Alignment.CenterHorizontally
                    ) {
                        Icon(
                            Icons.Rounded.SportsEsports, null,
                            tint = Neon.TextDim, modifier = Modifier.size(64.dp)
                        )
                        Spacer(Modifier.height(14.dp))
                        Text("还没有游戏", color = Neon.TextHi, fontSize = 20.sp, fontWeight = FontWeight.Bold)
                        Spacer(Modifier.height(6.dp))
                        Text("去游戏库导入 ROM 开始游玩", color = Neon.TextDim, fontSize = 12.sp)
                        Spacer(Modifier.height(18.dp))
                        NeonPrimaryButton("进入游戏库", Modifier, Icons.Rounded.GridView, onOpenLibrary)
                    }
                } else {
                    NeonFlow(
                        count = flowGames.size,
                        selectedIndex = selIdx,
                        onIndexChange = { selectedIndex = it },
                        onItemClick = { flowGames.getOrNull(it)?.let(onOpenGame) },
                        modifier = Modifier
                            .fillMaxSize()
                            .onKeyNav(
                                onLeft = { if (selectedIndex > 0) selectedIndex-- },
                                onRight = { if (selectedIndex < flowGames.size - 1) selectedIndex++ },
                                onConfirm = { flowGames.getOrNull(selectedIndex)?.let(onOpenGame) },
                                onFavorite = { flowGames.getOrNull(selectedIndex)?.let(onToggleFavorite) }
                            )
                    ) { i ->
                        NeonCoverCard(
                            game = flowGames[i],
                            glow = if (i == selIdx) 1f else 0f,
                            modifier = Modifier.fillMaxSize()
                        )
                    }
                }
            }

            // ===== 信息区 =====
            if (currentGame != null) {
                Row(
                    Modifier
                        .fillMaxWidth()
                        .background(Neon.BgDeep.copy(alpha = 0.75f))
                        .padding(horizontal = 28.dp, vertical = 12.dp),
                    verticalAlignment = Alignment.CenterVertically
                ) {
                    Column(Modifier.weight(1f)) {
                        Text(
                            currentGame.title,
                            color = Neon.TextHi,
                            fontSize = 19.sp,
                            fontWeight = FontWeight.Bold,
                            maxLines = 1,
                            overflow = TextOverflow.Ellipsis
                        )
                        Spacer(Modifier.height(4.dp))
                        Row(verticalAlignment = Alignment.CenterVertically) {
                            PlatformBadge(currentGame.platformEnum)
                            Spacer(Modifier.width(10.dp))
                            Text(
                                "${formatPlayTime(currentGame.playDurationMs)} · ${formatLastPlayed(currentGame.lastPlayedAt)} · ${currentGame.playCount} 次",
                                color = Neon.TextDim, fontSize = 11.sp
                            )
                            if (currentGame.favorite) {
                                Spacer(Modifier.width(8.dp))
                                Text("★ 已收藏", color = Neon.Gold, fontSize = 11.sp)
                            }
                        }
                    }
                    Spacer(Modifier.width(16.dp))
                    NeonPrimaryButton("开始游戏", Modifier, Icons.Rounded.PlayArrow) { onOpenGame(currentGame) }
                }
            }

            // ===== 底部按键提示栏 =====
            Row(
                Modifier
                    .fillMaxWidth()
                    .background(Neon.BgDeep)
                    .padding(horizontal = 28.dp, vertical = 6.dp),
                horizontalArrangement = Arrangement.spacedBy(18.dp)
            ) {
                NeonKeyHint("←→", "切换", Neon.TextDim)
                NeonKeyHint("Enter", "开始", Neon.Green)
                NeonKeyHint("F", "收藏", Neon.Gold)
                NeonKeyHint("Esc", "返回", Neon.TextDim)
            }
        }
    }
}

/** 菜单行（横滑彩色 chips）。 */
@Composable
fun NeonMenuRow(items: List<NeonChip>) {
    Row(
        Modifier
            .fillMaxWidth()
            .horizontalScroll(rememberScrollState())
            .padding(horizontal = 24.dp, vertical = 8.dp),
        horizontalArrangement = Arrangement.spacedBy(10.dp)
    ) {
        items.forEach { chip ->
            Row(
                Modifier
                    .clip(neonChamfer(0.4f))
                    .background(Neon.BgPanel)
                    .clickable(onClick = chip.onClick)
                    .padding(horizontal = 14.dp, vertical = 8.dp),
                verticalAlignment = Alignment.CenterVertically
            ) {
                Icon(chip.icon, null, tint = chip.color, modifier = Modifier.size(15.dp))
                Spacer(Modifier.width(6.dp))
                Text(chip.label, color = Neon.Text, fontSize = 13.sp, fontWeight = FontWeight.SemiBold)
            }
        }
    }
}

data class NeonChip(
    val label: String,
    val icon: ImageVector,
    val color: Color,
    val onClick: () -> Unit
)

@Composable
fun NeonKeyHint(key: String, action: String, color: Color) {
    Row(verticalAlignment = Alignment.CenterVertically) {
        Box(
            Modifier
                .clip(neonChamfer(0.5f))
                .background(Neon.BgPanelHi)
                .padding(horizontal = 7.dp, vertical = 2.dp)
        ) {
            Text(key, color = color, fontSize = 11.sp, fontWeight = FontWeight.Bold)
        }
        Spacer(Modifier.width(6.dp))
        Text(action, color = Neon.TextDim, fontSize = 11.sp)
    }
}

/** 主页键盘导航（方向键/Enter/F/Esc）。 */
private fun Modifier.onKeyNav(
    onLeft: () -> Unit,
    onRight: () -> Unit,
    onConfirm: () -> Unit,
    onFavorite: () -> Unit
): Modifier = this.onKeyEvent { e ->
    if (e.type != KeyEventType.KeyDown) return@onKeyEvent false
    when (e.key) {
        Key.DirectionLeft -> { onLeft(); true }
        Key.DirectionRight -> { onRight(); true }
        Key.Enter, Key.NumPadEnter -> { onConfirm(); true }
        Key.F -> { onFavorite(); true }
        else -> false
    }
}
