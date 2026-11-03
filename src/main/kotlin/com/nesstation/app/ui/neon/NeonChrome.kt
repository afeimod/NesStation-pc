package com.nesstation.app.ui.neon

import androidx.compose.animation.core.animateFloatAsState
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.rounded.SportsEsports
import androidx.compose.material3.Icon
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableLongStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.alpha
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.vector.ImageVector
import androidx.compose.ui.layout.ContentScale
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.nesstation.app.core.model.GameEntry
import com.nesstation.app.ui.components.GameCover
import kotlinx.coroutines.delay
import java.text.SimpleDateFormat
import java.util.Date

/** 顶部时钟 + 游戏计数品牌条。 */
@Composable
fun NeonClockBar(gameCount: Int, modifier: Modifier = Modifier) {
    var now by remember { mutableLongStateOf(System.currentTimeMillis()) }
    LaunchedEffect(Unit) {
        while (true) { delay(1000); now = System.currentTimeMillis() }
    }
    val time = remember(now) { SimpleDateFormat("HH:mm").format(Date(now)) }
    Row(
        modifier = modifier.padding(horizontal = 28.dp, vertical = 14.dp),
        verticalAlignment = Alignment.CenterVertically
    ) {
        // 品牌徽标
        Box(
            Modifier
                .clip(neonChamfer(0.5f))
                .background(Neon.Accent)
                .padding(horizontal = 10.dp, vertical = 4.dp)
        ) {
            Text("NES·STATION", color = Neon.BgDeep, fontSize = 15.sp, fontWeight = FontWeight.Black)
        }
        Box(Modifier.weight(1f))
        Text("$gameCount GAMES", color = Neon.TextDim, fontSize = 12.sp, fontWeight = FontWeight.Bold)
        Box(Modifier.padding(horizontal = 10.dp))
        Text(time, color = Neon.Accent, fontSize = 15.sp, fontWeight = FontWeight.Bold)
    }
}

/** Neon 主按钮（切角高亮按钮）。 */
@Composable
fun NeonPrimaryButton(
    label: String,
    modifier: Modifier = Modifier,
    icon: ImageVector? = Icons.Rounded.SportsEsports,
    onClick: () -> Unit
) {
    Row(
        modifier
            .clip(neonChamfer(0.28f))
            .background(Brush.horizontalGradient(listOf(Neon.AccentDim, Neon.Accent)))
            .clickable(onClick = onClick)
            .padding(horizontal = 18.dp, vertical = 10.dp),
        verticalAlignment = Alignment.CenterVertically
    ) {
        if (icon != null) {
            Icon(icon, null, tint = Neon.BgDeep, modifier = Modifier.padding(end = 6.dp))
        }
        Text(label, color = Neon.BgDeep, fontSize = 14.sp, fontWeight = FontWeight.Bold)
    }
}

/** Neon 深空渐变背景（顶部/底部霓虹光带）。 */
@Composable
fun NeonBackdrop(modifier: Modifier = Modifier) {
    Box(modifier.fillMaxSize().background(Brush.verticalGradient(listOf(Neon.Bg, Neon.BgDeep, Color(0xFF040914)))))
    Box(
        Modifier.fillMaxSize().background(
            Brush.verticalGradient(
                0f to Neon.Accent.copy(alpha = 0.10f),
                0.12f to Color.Transparent,
                0.88f to Color.Transparent,
                1f to Neon.AccentDim.copy(alpha = 0.12f)
            )
        )
    )
}

/** 封面全屏背景（当前选中游戏封面暗化，随选中淡入淡出）。 */
@Composable
fun NeonCoverBackdrop(game: GameEntry?, modifier: Modifier = Modifier) {
    val alpha by animateFloatAsState(if (game != null) 1f else 0f)
    Box(modifier.fillMaxSize()) {
        if (game != null && game.coverPath.isNotBlank()) {
            GameCover(
                game = game,
                modifier = Modifier.fillMaxSize().alpha(alpha * 0.35f),
                contentScale = ContentScale.Crop
            )
            Box(
                Modifier.fillMaxSize().background(
                    Brush.verticalGradient(
                        listOf(Neon.Bg.copy(alpha = 0.55f), Neon.Bg.copy(alpha = 0.92f))
                    )
                )
            )
        } else {
            NeonBackdrop()
        }
    }
}

/** 封面卡（弧形导轨上的单元卡：辉光 + 底部渐隐标题层）。 */
@Composable
fun NeonCoverCard(
    game: GameEntry,
    glow: Float,
    modifier: Modifier = Modifier
) {
    val shape = RoundedCornerShape(8.dp)
    val pad = if (glow > 0.01f) 6.dp else 0.dp
    Box(modifier) {
        // 辉光层
        if (glow > 0.01f) {
            Box(
                Modifier
                    .matchParentSize()
                    .background(Neon.Accent.copy(alpha = 0.5f * glow), RoundedCornerShape(14.dp))
            )
        }
        Box(
            Modifier
                .matchParentSize()
                .padding(pad)
                .clip(shape)
                .background(Neon.BgPanel)
        ) {
            GameCover(game = game, modifier = Modifier.fillMaxSize())
            Box(
                Modifier
                    .align(Alignment.BottomCenter)
                    .fillMaxSize()
                    .background(
                        Brush.verticalGradient(
                            0.55f to Color.Transparent,
                            1f to Color(0xCC05070E)
                        )
                    )
            )
            Text(
                game.title,
                color = Neon.TextHi,
                fontSize = 12.sp,
                fontWeight = FontWeight.Bold,
                maxLines = 1,
                overflow = TextOverflow.Ellipsis,
                modifier = Modifier.align(Alignment.BottomStart).padding(8.dp)
            )
            if (game.favorite) {
                Text("★", color = Neon.Gold, fontSize = 12.sp,
                    modifier = Modifier.align(Alignment.TopEnd).padding(6.dp))
            }
        }
    }
}

/** Neon 信息面板（切角面板底 + 描线）。 */
@Composable
fun NeonPanel(modifier: Modifier = Modifier, content: @Composable () -> Unit) {
    Box(
        modifier
            .clip(neonChamfer(0.10f))
            .background(Neon.BgPanel)
            .border(1.dp, Neon.Line, neonChamfer(0.10f))
            .padding(14.dp)
    ) {
        content()
    }
}
