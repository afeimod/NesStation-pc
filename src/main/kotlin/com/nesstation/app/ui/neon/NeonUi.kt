package com.nesstation.app.ui.neon

import androidx.compose.foundation.shape.GenericShape
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.setValue
import androidx.compose.ui.graphics.Color
import com.nesstation.app.core.storage.SettingsStore

/**
 * ★★ Neon 3D 新 UI —— 全局风格状态 + 设计令牌 ★★
 *
 * 总设置「外观 → 主界面风格」切换 FSD 经典 / Neon 3D 新UI（★默认）。
 * 本对象是可观察的全局单例：设置页改风格后主页/游戏库/设置页立即响应。
 *
 *   "neon" → Neon 3D 游戏站（本包全新实现）：
 *              · 主页    —— 顶部菜单行 + 中央 3D 封面流
 *              · 游戏库  —— 左侧核心列表 + 每核心 3D 封面滚动（NeonFlow）
 *              · 总设置  —— 左侧分类导航 + Neon 面板
 *   "fsd"  → FSD 经典桌面（Xbox 360 Freestyle Dash 磁贴流 + 像素天空）
 */
object NeonUi {
    /** 当前风格（"neon" | "fsd"，★默认 neon）。 */
    var style by mutableStateOf("neon")
        private set

    @Volatile private var inited = false

    /** 进程内首次调用时从持久化填充（幂等）。 */
    fun ensureLoaded() {
        if (inited) return
        synchronized(this) {
            if (inited) return
            style = SettingsStore.load().homeUiStyle.let { if (it == "fsd") "fsd" else "neon" }
            inited = true
        }
    }

    /** 设置页切换入口：更新全局状态并立即持久化。 */
    fun set(value: String) {
        val v = if (value == "fsd") "fsd" else "neon"
        style = v
        inited = true
        SettingsStore.update { it.homeUiStyle = v }
    }

    val isNeon: Boolean get() = style == "neon"
}

/**
 * Neon 3D 设计令牌 —— 深空底 + 电光青 + 警示红。
 * 全部页面（主页/游戏库/设置）共用同一套令牌，保证风格统一。
 */
object Neon {
    // ---- 底色 ----
    val Bg = Color(0xFF05070E)            // 深空黑蓝底
    val BgDeep = Color(0xFF02030A)        // 更深的底（渐变端点）
    val BgPanel = Color(0xFF0A1020)       // 面板底
    val BgPanelHi = Color(0xFF101A30)     // 面板高亮底
    val Line = Color(0xFF1C3350)          // 描线

    // ---- 主色 ----
    val Accent = Color(0xFF00E5FF)        // 电光青（选中/边框/箭头）
    val AccentDim = Color(0xFF00798C)     // 青暗调
    val Red = Color(0xFFFF2D3F)           // 警示红（ALL GAMES 撞色）
    val Blue = Color(0xFF2F7BD9)          // 撞色蓝
    val Gold = Color(0xFFF2C200)          // 黄（Y 键 / 收藏）
    val Green = Color(0xFF6DBE28)         // 绿（A 键）
    val Purple = Color(0xFF8B5CF6)        // 紫（核心管理点缀）

    // ---- 文字 ----
    val TextHi = Color(0xFFEAF6FF)        // 主文字
    val Text = Color(0xFFB8C7D9)          // 次文字
    val TextDim = Color(0xFF5E6F85)       // 弱文字

    // ---- 手柄键色（A/B/X/Y，与 FSD 按键色一致保持肌肉记忆）----
    val BtnA = Green
    val BtnB = Color(0xFFE23B3B)
    val BtnX = Blue
    val BtnY = Gold
}

/** 切角（Chamfer）形状 —— Neon 3D 的科技边框语言：左上 + 右下两角斜切。 */
fun neonChamfer(fraction: Float = 0.14f): GenericShape = GenericShape { size, _ ->
    val c = size.minDimension * fraction.coerceIn(0.02f, 0.5f)
    moveTo(c, 0f)
    lineTo(size.width, 0f)
    lineTo(size.width, size.height - c)
    lineTo(size.width - c, size.height)
    lineTo(0f, size.height)
    lineTo(0f, c)
    close()
}
