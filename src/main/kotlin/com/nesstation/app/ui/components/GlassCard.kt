package com.nesstation.app.ui.components

import androidx.compose.foundation.Image
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.rounded.SportsEsports
import androidx.compose.material3.Icon
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.remember
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.ImageBitmap
import androidx.compose.ui.graphics.painter.Painter
import androidx.compose.ui.layout.ContentScale
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.nesstation.app.core.model.GameEntry
import com.nesstation.app.core.model.GamePlatform
import java.awt.image.BufferedImage
import java.io.File
import javax.imageio.ImageIO
import androidx.compose.ui.graphics.toComposeImageBitmap

/**
 * 封面图加载（带进程内 LRU 缓存；对应 Android 版的 LruCache<Bitmap> 用法）。
 */
object CoverCache {
    private val cache = LinkedHashMap<String, ImageBitmap?>(64, 0.75f, true)

    @Synchronized
    fun get(path: String): ImageBitmap? {
        cache[path]?.let { return it }
        val img = decode(path)
        if (cache.size > 128) {
            val iter = cache.entries.iterator()
            repeat(64) { if (iter.hasNext()) { iter.next(); iter.remove() } }
        }
        cache[path] = img
        return img
    }

    private fun decode(path: String): ImageBitmap? = runCatching {
        val bi: BufferedImage = ImageIO.read(File(path)) ?: return null
        bi.toComposeImageBitmap()
    }.getOrNull()
}

/** 游戏封面（找不到文件时画平台色占位卡）。 */
@Composable
fun GameCover(
    game: GameEntry,
    modifier: Modifier = Modifier,
    contentScale: ContentScale = ContentScale.Crop
) {
    val bmp = remember(game.coverPath) { if (game.coverPath.isNotBlank()) CoverCache.get(game.coverPath) else null }
    if (bmp != null) {
        Image(
            bitmap = bmp,
            contentDescription = game.title,
            modifier = modifier,
            contentScale = contentScale
        )
    } else {
        PlaceholderCover(platform = game.platformEnum, title = game.title, modifier = modifier)
    }
}

/** 平台色占位封面（像素渐变 + 首字母）。 */
@Composable
fun PlaceholderCover(platform: GamePlatform, title: String, modifier: Modifier = Modifier) {
    val (c1, c2) = platformColors(platform)
    Box(
        modifier = modifier
            .clip(RoundedCornerShape(6.dp))
            .background(Brush.verticalGradient(listOf(c1, c2)))
            .padding(10.dp)
    ) {
        Column(
            modifier = Modifier.fillMaxSize(),
            horizontalAlignment = Alignment.CenterHorizontally,
            verticalArrangement = androidx.compose.foundation.layout.Arrangement.Center
        ) {
            Icon(
                Icons.Rounded.SportsEsports, contentDescription = null,
                tint = Color.White.copy(alpha = 0.9f),
                modifier = Modifier.size(34.dp)
            )
            Spacer(Modifier.height(8.dp))
            Text(
                platform.displayName,
                color = Color.White,
                fontSize = 13.sp,
                fontWeight = FontWeight.Bold,
                maxLines = 1,
                overflow = TextOverflow.Ellipsis
            )
            Text(
                title,
                color = Color.White.copy(alpha = 0.85f),
                fontSize = 10.sp,
                maxLines = 2,
                overflow = TextOverflow.Ellipsis,
                lineHeight = 12.sp
            )
        }
    }
}

/** 平台主题色对。 */
fun platformColors(p: GamePlatform): Pair<Color, Color> = when (p) {
    GamePlatform.NES -> Color(0xFFE74C3C) to Color(0xFF922B21)
    GamePlatform.SFC -> Color(0xFF7D3C98) to Color(0xFF4A235A)
    GamePlatform.GB -> Color(0xFF7FB069) to Color(0xFF4F622F)
    GamePlatform.GBA -> Color(0xFF5D6D7E) to Color(0xFF34495E)
    GamePlatform.MD -> Color(0xFF3E64AF) to Color(0xFF253C7A)
    GamePlatform.PCE -> Color(0xFFD4AC0D) to Color(0xFF9A7D0A)
    GamePlatform.PSX -> Color(0xFF95A5A6) to Color(0xFF5D6D7E)
    GamePlatform.PS2 -> Color(0xFF5499C7) to Color(0xFF2E6DA4)
    GamePlatform.NDS -> Color(0xFFAF7AC5) to Color(0xFF6C3483)
    GamePlatform.N3DS -> Color(0xFFE67E22) to Color(0xFF935116)
    GamePlatform.NGCWII -> Color(0xFF76D7C4) to Color(0xFF48C9B0)
    GamePlatform.WIIU -> Color(0xFF85C1E9) to Color(0xFF5499C7)
    GamePlatform.DC -> Color(0xFFF1948A) to Color(0xFFCB4335)
    GamePlatform.ARCADE -> Color(0xFFF1C40F) to Color(0xFFB7950B)
    GamePlatform.DOS -> Color(0xFFB2BABB) to Color(0xFF7F8C8D)
    GamePlatform.JAVA -> Color(0xFF7E5109) to Color(0xFF4A3B0C)
    GamePlatform.PSP -> Color(0xFF9B59B6) to Color(0xFF6C3483)
    GamePlatform.NSWITCH -> Color(0xFFE74C3C) to Color(0xFF27AE60)
}

/**
 * Frosted-glass tile（对应 Android 版 GlassCard）：焦点/悬停高亮的玻璃卡。
 */
@Composable
fun GlassCard(
    title: String,
    subtitle: String? = null,
    accent: Color = MaterialTheme.colorScheme.primary,
    modifier: Modifier = Modifier,
    onClick: (() -> Unit)? = null,
    content: @Composable () -> Unit = {}
) {
    val bg = MaterialTheme.colorScheme.surface.copy(alpha = 0.82f)
    Box(
        modifier = modifier
            .clip(RoundedCornerShape(14.dp))
            .background(bg)
            .border(1.5.dp, accent.copy(alpha = 0.45f), RoundedCornerShape(14.dp))
            .let { m -> if (onClick != null) m.clickable(onClick = onClick) else m }
            .padding(14.dp)
    ) {
        Column {
            Text(
                title,
                color = MaterialTheme.colorScheme.onSurface,
                fontSize = 16.sp,
                fontWeight = FontWeight.Bold,
                maxLines = 1,
                overflow = TextOverflow.Ellipsis
            )
            if (subtitle != null) {
                Spacer(Modifier.height(2.dp))
                Text(
                    subtitle,
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                    fontSize = 12.sp,
                    maxLines = 1,
                    overflow = TextOverflow.Ellipsis
                )
            }
            Spacer(Modifier.height(8.dp))
            content()
        }
    }
}

/** 平台徽章（小圆角标签）。 */
@Composable
fun PlatformBadge(platform: GamePlatform, modifier: Modifier = Modifier) {
    val (c1, _) = platformColors(platform)
    Box(
        modifier = modifier
            .clip(RoundedCornerShape(4.dp))
            .background(c1.copy(alpha = 0.22f))
            .padding(horizontal = 8.dp, vertical = 2.dp)
    ) {
        Text(platform.displayName, color = c1, fontSize = 11.sp, fontWeight = FontWeight.SemiBold)
    }
}
