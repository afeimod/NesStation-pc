package com.nesstation.app.ui.fsd

import androidx.compose.foundation.Image
import androidx.compose.foundation.background
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
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.rounded.HelpOutline
import androidx.compose.material.icons.automirrored.rounded.Logout
import androidx.compose.material.icons.rounded.Favorite
import androidx.compose.material.icons.rounded.GridView
import androidx.compose.material.icons.rounded.History
import androidx.compose.material.icons.rounded.Memory
import androidx.compose.material.icons.rounded.Settings
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.layout.ContentScale
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.nesstation.app.core.model.GameEntry
import com.nesstation.app.core.storage.SettingsStore
import com.nesstation.app.ui.components.PixelBackdrop
import kotlinx.coroutines.delay

/**
 * ★ FSD 经典主页（Xbox 360 Freestyle Dash 风格）★
 *
 * 像素天空背景 + 中央蓝黄对角磁贴流 + 底部 Dock。
 * 与 Neon 3D 并列的两套主界面主题之一（设置 → 外观 → 主界面风格 切换）。
 */
@Composable
fun FsdHomeScreen(
    gameCount: Int,
    onOpenLibrary: () -> Unit,
    onOpenFavorites: () -> Unit,
    onOpenHistory: () -> Unit,
    onOpenSettings: () -> Unit,
    onOpenCoreManager: () -> Unit,
    onOpenAbout: () -> Unit,
    onExit: () -> Unit,
    modifier: Modifier = Modifier
) {
    var timeMs by remember { mutableStateOf(0L) }
    LaunchedEffect(Unit) {
        val t0 = System.currentTimeMillis()
        while (true) { timeMs = System.currentTimeMillis() - t0; delay(66) }
    }

    var selected by remember { mutableIntStateOf(0) }
    val items = remember(gameCount) {
        listOf(
            FsdTileItem("library", "游戏库", Icons.Rounded.GridView, "$gameCount"),
            FsdTileItem("favorites", "收藏", Icons.Rounded.Favorite, null),
            FsdTileItem("history", "最近", Icons.Rounded.History, null),
            FsdTileItem("cores", "核心管理", Icons.Rounded.Memory, "PC"),
            FsdTileItem("settings", "设置", Icons.Rounded.Settings, null),
            FsdTileItem("about", "关于", Icons.AutoMirrored.Rounded.HelpOutline, null),
            FsdTileItem("exit", "退出", Icons.AutoMirrored.Rounded.Logout, null)
        )
    }
    val actions = listOf(
        onOpenLibrary, onOpenFavorites, onOpenHistory,
        onOpenCoreManager, onOpenSettings, onOpenAbout, onExit
    )

    Box(modifier.fillMaxSize()) {
        // 背景：自定义背景图 或 像素天空
        val bgUri = SettingsStore.load().homeBackgroundUri
        if (bgUri.isNotBlank()) {
            val bmp = remember(bgUri) {
                com.nesstation.app.ui.components.CoverCache.get(bgUri)
            }
            if (bmp != null) {
                Image(
                    bitmap = bmp, contentDescription = null,
                    modifier = Modifier.fillMaxSize(),
                    contentScale = ContentScale.Crop
                )
                Box(Modifier.fillMaxSize().background(Color(0x66102030)))
            } else {
                PixelBackdrop(timeMs = timeMs)
            }
        } else {
            PixelBackdrop(timeMs = timeMs)
        }

        Column(Modifier.fillMaxSize()) {
            // 顶部状态条
            Row(
                Modifier
                    .fillMaxWidth()
                    .padding(horizontal = 30.dp, vertical = 14.dp),
                verticalAlignment = Alignment.CenterVertically
            ) {
                Text(
                    "NesStation",
                    color = Color.White,
                    fontSize = 22.sp,
                    fontWeight = FontWeight.Black
                )
                Text(
                    "  GameBox for PC",
                    color = Color(0xCCFFFFFF),
                    fontSize = 12.sp,
                    fontWeight = FontWeight.Bold
                )
                Box(Modifier.weight(1f))
                Text(
                    "$gameCount GAMES · 18 PLATFORMS",
                    color = Color(0xEEFFFFFF),
                    fontSize = 12.sp,
                    fontWeight = FontWeight.Bold
                )
            }

            // 中央磁贴流
            Box(Modifier.weight(1f).fillMaxWidth()) {
                FsdCoverFlow(
                    items = items,
                    selectedIndex = selected,
                    onIndexChange = { selected = it },
                    onItemClick = { actions.getOrNull(it)?.invoke() },
                    tileWidth = 210.dp,
                    modifier = Modifier.fillMaxSize()
                )
            }

            // 底部 Dock
            FsdDock(
                selectedLabel = items.getOrNull(selected)?.label ?: "",
                hint = "←→ 切换 · Enter 确认 · Esc 返回"
            )
        }
    }
}

/** 底部 Dock（半透明玻璃条 + 按键提示）。 */
@Composable
fun FsdDock(selectedLabel: String, hint: String) {
    Row(
        Modifier
            .fillMaxWidth()
            .background(Color(0x880D2C55))
            .padding(horizontal = 30.dp, vertical = 12.dp),
        verticalAlignment = Alignment.CenterVertically
    ) {
        Text(
            selectedLabel,
            color = Color.White, fontSize = 16.sp, fontWeight = FontWeight.Bold
        )
        Box(Modifier.weight(1f))
        Text(hint, color = Color(0xCCFFFFFF), fontSize = 11.sp)
    }
}
