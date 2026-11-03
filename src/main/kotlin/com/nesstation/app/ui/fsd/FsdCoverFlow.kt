package com.nesstation.app.ui.fsd

import androidx.compose.animation.core.animateFloatAsState
import androidx.compose.animation.core.tween
import androidx.compose.foundation.Canvas
import androidx.compose.foundation.Image
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.gestures.detectHorizontalDragGestures
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.BoxWithConstraints
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.offset
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.rounded.Star
import androidx.compose.material3.Icon
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.alpha
import androidx.compose.ui.draw.clip
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.Path
import androidx.compose.ui.graphics.drawscope.DrawScope
import androidx.compose.ui.graphics.drawscope.clipPath
import androidx.compose.ui.graphics.graphicsLayer
import androidx.compose.ui.graphics.vector.ImageVector
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.platform.LocalDensity
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.nesstation.app.ui.components.GameCover
import com.nesstation.app.core.model.GameEntry
import kotlin.math.abs

/**
 * ★ FSD 经典桌面 ★
 *
 * 仿 Xbox 360 Freestyle Dash：
 *  - 主菜单磁贴流（中央大磁贴 + 两侧渐退；磁贴为蓝→黄对角渐变）
 *  - 封面流（showGames=true 时用于游戏选择，与磁贴流同一滚动引擎）
 *  - 底部 Dock + 像素天空背景（由 FsdHomeScreen 组合）
 */

/** FSD 磁贴条目。 */
data class FsdTileItem(
    val key: String,
    val label: String,
    val icon: ImageVector? = null,
    val badge: String? = null,
    val game: GameEntry? = null
)

/**
 * FSD 滚动流：磁贴或封面，中间大、两侧渐退 + 反射倒影。
 */
@Composable
fun FsdCoverFlow(
    items: List<FsdTileItem>,
    selectedIndex: Int,
    onIndexChange: (Int) -> Unit,
    onItemClick: (Int) -> Unit,
    showReflection: Boolean = true,
    tileWidth: Dp = 200.dp,
    modifier: Modifier = Modifier,
    isGameCover: Boolean = false
) {
    val density = LocalDensity.current
    var dragAccum by remember { mutableFloatOf() }
    val safeIndex = selectedIndex.coerceIn(0, (items.size - 1).coerceAtLeast(0))
    val anim by animateFloatAsState(safeIndex.toFloat(), tween(300), label = "fsdFlow")

    BoxWithConstraints(
        modifier
            .fillMaxSize()
            .pointerInput(items.size) {
                detectHorizontalDragGestures { change, dragAmount ->
                    change.consume()
                    dragAccum -= dragAmount
                    val tilePx = with(density) { tileWidth.toPx() } * 0.6f
                    var idx = safeIndex
                    while (abs(dragAccum) > tilePx / 2) {
                        idx += if (dragAccum > 0) -1 else 1
                        dragAccum += if (dragAccum > 0) -tilePx else tilePx
                    }
                    idx = idx.coerceIn(0, (items.size - 1).coerceAtLeast(0))
                    if (idx != safeIndex) onIndexChange(idx)
                }
            }
    ) {
        val w = constraints.maxWidth.toFloat()
        val h = constraints.maxHeight.toFloat()
        with(density) {
            val tw = tileWidth.toPx()
            val th = tw * if (isGameCover) 1.36f else 0.68f
            // 远 → 近
            for (side in 4 downTo 1) {
                for (dir in intArrayOf(-1, 1)) {
                    val i = safeIndex + dir * side
                    if (i !in items.indices) continue
                    val dist = abs(i - anim)
                    val scale = (1f - dist * 0.11f).coerceAtLeast(0.62f)
                    val alpha = (0.92f - dist * 0.18f).coerceIn(0.15f, 0.85f)
                    val cx = w / 2f + dir * dist * tw * 0.66f
                    val cy = h / 2f
                    FsdTile(
                        item = items[i],
                        modifier = Modifier
                            .size(tileWidth, tileWidth * if (isGameCover) 1.36f else 0.68f)
                            .graphicsLayer {
                                translationX = cx - tw / 2f
                                translationY = cy - th / 2f
                                scaleX = scale; scaleY = scale
                                this.alpha = alpha
                            }
                            .clickable { onIndexChange(i) },
                        isGameCover = isGameCover
                    )
                }
            }
            // 中心
            if (items.isNotEmpty()) {
                FsdTile(
                    item = items[safeIndex],
                    modifier = Modifier
                        .size(tileWidth, tileWidth * if (isGameCover) 1.36f else 0.68f)
                        .graphicsLayer {
                            translationX = w / 2f - tw / 2f
                            translationY = h / 2f - th / 2f
                        }
                        .clickable { onItemClick(safeIndex) },
                    isGameCover = isGameCover
                )
            }
        }
    }
}

private fun mutableFloatOf() = mutableStateOf(0f)

/**
 * FSD 磁贴 —— 蓝/黄对角渐变 + 白色图标 + 左下标签
 * （isGameCover=true 时渲染游戏封面卡）。
 */
@Composable
fun FsdTile(
    item: FsdTileItem,
    modifier: Modifier = Modifier,
    isGameCover: Boolean = false
) {
    Box(modifier) {
        if (isGameCover && item.game != null) {
            Box(
                Modifier
                    .fillMaxSize()
                    .clip(RoundedCornerShape(10.dp))
                    .background(Color(0xFF0D2C55))
                    .border(2.dp, Color(0xFF3E64AF), RoundedCornerShape(10.dp))
            ) {
                GameCover(game = item.game, modifier = Modifier.fillMaxSize())
                // 底部渐隐标题
                Box(
                    Modifier
                        .align(Alignment.BottomCenter)
                        .fillMaxSize()
                        .background(
                            Brush.verticalGradient(
                                0.6f to Color.Transparent, 1f to Color(0xCC0D2C55)
                            )
                        )
                )
                Text(
                    item.game.title,
                    color = Color.White, fontSize = 12.sp, fontWeight = FontWeight.Bold,
                    maxLines = 1,
                    modifier = Modifier.align(Alignment.BottomStart).padding(8.dp)
                )
                if (item.game.favorite) {
                    Icon(
                        Icons.Rounded.Star, null, tint = Color(0xFFF2C200),
                        modifier = Modifier.align(Alignment.TopEnd).padding(6.dp).size(14.dp)
                    )
                }
            }
        } else {
            // 经典蓝黄对角磁贴
            FsdGradientTile(
                item = item,
                modifier = Modifier.fillMaxSize()
            )
        }
    }
}

@Composable
private fun FsdGradientTile(item: FsdTileItem, modifier: Modifier = Modifier) {
    Box(
        modifier
            .clip(RoundedCornerShape(10.dp))
            .background(Color(0xFF0D2C55))
    ) {
        // 对角渐变层（上半蓝、下半黄，左低右高斜线）
        Canvas(Modifier.fillMaxSize()) {
            drawDiagonalGradient()
        }
        // 高光斜条
        Canvas(Modifier.fillMaxSize()) {
            drawSpecular()
        }
        // 图标
        item.icon?.let { ic ->
            Icon(
                ic, contentDescription = item.label,
                tint = Color.White,
                modifier = Modifier
                    .align(Alignment.TopCenter)
                    .padding(top = 26.dp)
                    .size(46.dp)
            )
        }
        // 左下标签
        Text(
            item.label,
            color = Color.White,
            fontSize = 17.sp,
            fontWeight = FontWeight.Black,
            modifier = Modifier.align(Alignment.BottomStart).padding(12.dp)
        )
        // 右上徽标
        item.badge?.let { b ->
            Box(
                Modifier
                    .align(Alignment.TopEnd)
                    .padding(8.dp)
                    .clip(RoundedCornerShape(4.dp))
                    .background(Color(0xCC1A4E8A))
                    .padding(horizontal = 8.dp, vertical = 3.dp)
            ) {
                Text(b, color = Color.White, fontSize = 11.sp, fontWeight = FontWeight.Bold)
            }
        }
    }
}

private fun DrawScope.drawDiagonalGradient() {
    val navy = Color(0xFF1565C0)
    val blue = Color(0xFF2196F3)
    val amber = Color(0xFFF9A825)
    val yellow = Color(0xFFFDD835)
    val w = size.width
    val h = size.height
    // 上：蓝渐变（梯形）
    val top = Path().apply {
        moveTo(0f, 0f); lineTo(w, 0f); lineTo(w, h * 0.42f); lineTo(0f, h * 0.68f); close()
    }
    clipPath(top) {
        drawRect(Brush.verticalGradient(listOf(navy, blue), startY = 0f, endY = h * 0.68f))
    }
    // 下：黄渐变
    val bottom = Path().apply {
        moveTo(0f, h * 0.68f); lineTo(w, h * 0.42f); lineTo(w, h); lineTo(0f, h); close()
    }
    clipPath(bottom) {
        drawRect(Brush.verticalGradient(listOf(amber, yellow), startY = h * 0.42f, endY = h))
    }
}

private fun DrawScope.drawSpecular() {
    val w = size.width
    val h = size.height
    val band = Path().apply {
        moveTo(w * 0.62f, 0f); lineTo(w, 0f); lineTo(w, h); lineTo(w * 0.32f, h); close()
    }
    clipPath(band) {
        drawRect(Color.White.copy(alpha = 0.05f))
    }
}
