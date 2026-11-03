package com.nesstation.app.ui.neon

import androidx.compose.animation.core.animateFloatAsState
import androidx.compose.animation.core.tween
import androidx.compose.foundation.clickable
import androidx.compose.foundation.gestures.detectHorizontalDragGestures
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.BoxWithConstraints
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.size
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableFloatStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.graphicsLayer
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.platform.LocalDensity
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.dp
import kotlin.math.abs

/**
 * ★★ NeonFlow —— Neon 3D 封面滚动引擎 ★★
 *
 * 选中封面居中放大、正对用户、青色辉光；两侧封面沿「弧形导轨」排布：
 * Y 轴透视旋转、逐级缩小变暗、越远越下沉。左右拖拽换位、点击侧卡居中、
 * 点击中卡确认。
 */
@Composable
fun NeonFlow(
    count: Int,
    selectedIndex: Int,
    onIndexChange: (Int) -> Unit,
    onItemClick: (Int) -> Unit,
    cardWidth: Dp = 190.dp,
    cardRatio: Float = 1.36f,
    tiltDegrees: Float = 30f,
    visible: Int = 4,
    modifier: Modifier = Modifier,
    content: @Composable (Int) -> Unit
) {
    val localDensity = LocalDensity.current
    var dragAccum by remember { mutableFloatStateOf(0f) }
    val safeIndex = selectedIndex.coerceIn(0, (count - 1).coerceAtLeast(0))

    val anim by animateFloatAsState(
        targetValue = safeIndex.toFloat(),
        animationSpec = tween(280),
        label = "neonFlowIndex"
    )

    BoxWithConstraints(
        modifier
            .fillMaxSize()
            .pointerInput(count) {
                detectHorizontalDragGestures(
                    onHorizontalDrag = { change, dragAmount ->
                        change.consume()
                        dragAccum -= dragAmount
                        val cardPx = with(density) { cardWidth.toPx() }
                        var idx = safeIndex
                        while (abs(dragAccum) > cardPx / 2) {
                            idx += if (dragAccum > 0) -1 else 1
                            dragAccum += if (dragAccum > 0) -cardPx else cardPx
                        }
                        idx = idx.coerceIn(0, (count - 1).coerceAtLeast(0))
                        if (idx != safeIndex) onIndexChange(idx)
                    }
                )
            }
    ) {
        val w = constraints.maxWidth.toFloat()
        val h = constraints.maxHeight.toFloat()
        with(localDensity) {
            val cardW = cardWidth.toPx()
            val cardH = cardW * cardRatio

            // 远 → 近绘制（近处覆盖远处）
            for (side in visible downTo 1) {
                for (dir in intArrayOf(-1, 1)) {
                    val i = safeIndex + dir * side
                    if (i !in 0 until count) continue
                    // 考虑动画中的滑动距离
                    val animDist = abs(i - anim)
                    val scale = (1f - animDist * 0.12f).coerceAtLeast(0.55f)
                    val alpha = (0.95f - animDist * 0.16f).coerceIn(0.18f, 0.85f)
                    val cx = w / 2f + dir * animDist * cardW * 0.72f
                    val cy = h / 2f + animDist * 24f
                    Box(
                        Modifier
                            .size(cardWidth, cardWidth * cardRatio)
                            .graphicsLayer {
                                translationX = cx - cardW / 2f
                                translationY = cy - cardH / 2f
                                scaleX = scale
                                scaleY = scale
                                rotationY = -dir * tiltDegrees * (0.4f + animDist * 0.12f)
                                this.alpha = alpha
                                cameraDistance = 14f * localDensity.density
                            }
                            .clickable(enabled = true) { onIndexChange(i) }
                    ) { content(i) }
                }
            }
            // 中心卡
            if (count > 0) {
                Box(
                    Modifier
                        .size(cardWidth, cardWidth * cardRatio)
                        .graphicsLayer {
                            translationX = w / 2f - cardW / 2f
                            translationY = h / 2f - cardH / 2f - 14f
                            cameraDistance = 14f * localDensity.density
                        }
                        .clickable(enabled = true) { onItemClick(safeIndex) }
                ) { content(safeIndex) }
            }
        }
    }
}
