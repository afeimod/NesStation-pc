package com.nesstation.app.ui.settings

import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
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
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.rounded.ArrowBack
import androidx.compose.material.icons.rounded.Gamepad
import androidx.compose.material3.Icon
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
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.nesstation.app.core.input.KeyboardMap
import com.nesstation.app.core.input.PadButton
import com.nesstation.app.core.model.GamePlatform
import com.nesstation.app.core.storage.KeyMapStore
import com.nesstation.app.ui.components.platformColors
import com.nesstation.app.ui.neon.Neon
import com.nesstation.app.ui.neon.neonChamfer

/**
 * 键位映射页（对应 Android 版 KeyMapScreen）：
 * 选平台 → 点击某个按钮 → 按下新键即完成绑定（AWT 键码持久化到 keymap.json）。
 */
@Composable
fun KeyMapScreen(onBack: () -> Unit, modifier: Modifier = Modifier) {
    var platform by remember { mutableStateOf(GamePlatform.NES) }
    var listening by remember { mutableStateOf<PadButton?>(null) }

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
                Icon(Icons.Rounded.Gamepad, null, tint = Neon.Green, modifier = Modifier.size(20.dp))
                Spacer(Modifier.width(8.dp))
                Text("键位映射", color = Neon.TextHi, fontSize = 17.sp, fontWeight = FontWeight.Bold)
                Box(Modifier.weight(1f))
                Text(
                    "重置全部", color = Neon.Red, fontSize = 12.sp,
                    modifier = Modifier.clickable {
                        KeyMapStore.resetAll()
                    }.padding(4.dp)
                )
            }

            // 平台选择
            androidx.compose.foundation.lazy.LazyRow(
                Modifier
                    .fillMaxWidth()
                    .padding(horizontal = 16.dp, vertical = 8.dp),
                horizontalArrangement = Arrangement.spacedBy(8.dp)
            ) {
                items(GamePlatform.entries.size) { i ->
                    val p = GamePlatform.entries[i]
                    val selected = p == platform
                    Box(
                        Modifier
                            .clip(neonChamfer(0.35f))
                            .background(if (selected) platformColors(p).first.copy(alpha = 0.25f) else Neon.BgPanel)
                            .border(
                                1.dp,
                                if (selected) platformColors(p).first else Neon.Line,
                                neonChamfer(0.35f)
                            )
                            .clickable { platform = p; listening = null }
                            .padding(horizontal = 12.dp, vertical = 6.dp)
                    ) {
                        Text(
                            p.displayName,
                            color = if (selected) platformColors(p).first else Neon.Text,
                            fontSize = 12.sp
                        )
                    }
                }
            }

            Column(
                Modifier
                    .weight(1f)
                    .verticalScroll(rememberScrollState())
                    .padding(16.dp)
            ) {
                Text(
                    "${platform.displayName} 键盘键位（点击按钮后按下新按键重新绑定）",
                    color = Neon.TextDim, fontSize = 11.sp,
                    modifier = Modifier.padding(bottom = 10.dp)
                )
                val map = remember(platform) { KeyboardMap.effectiveMap(platform) }
                val pid = platform.name.lowercase()
                for (r in 0 until (map.size + 1) / 2) {
                    Row(Modifier.fillMaxWidth().padding(vertical = 4.dp)) {
                        for (c in 0 until 2) {
                            val idx = r * 2 + c
                            if (idx >= map.keys.size) { Box(Modifier.weight(1f)); continue }
                            val btn = map.keys.toList()[idx]
                            val keyCode = map[btn]!!
                            val custom = KeyMapStore.get("${pid}_${btn.name.lowercase()}") != null
                            val isListening = listening == btn
                            Box(
                                Modifier
                                    .weight(1f)
                                    .padding(horizontal = 4.dp)
                                    .clip(RoundedCornerShape(8.dp))
                                    .background(
                                        when {
                                            isListening -> Neon.Accent.copy(alpha = 0.22f)
                                            custom -> Neon.Gold.copy(alpha = 0.12f)
                                            else -> Neon.BgPanel
                                        }
                                    )
                                    .border(
                                        1.dp,
                                        when {
                                            isListening -> Neon.Accent
                                            custom -> Neon.Gold.copy(alpha = 0.6f)
                                            else -> Neon.Line
                                        },
                                        RoundedCornerShape(8.dp)
                                    )
                                    .clickable {
                                        listening = if (isListening) null else btn
                                    }
                                    .padding(12.dp)
                            ) {
                                Row(verticalAlignment = Alignment.CenterVertically) {
                                    Text(btn.label, color = Neon.Text, fontSize = 13.sp)
                                    Box(Modifier.weight(1f))
                                    Text(
                                        when {
                                            isListening -> "按任意键…"
                                            else -> KeyMapStore.get("${pid}_${btn.name.lowercase()}")
                                                ?.let { KeyMapStore.keyCodeToLabel(it) }
                                                ?: KeyMapStore.keyCodeToLabel(keyCode)
                                        },
                                        color = when {
                                            isListening -> Neon.Accent
                                            custom -> Neon.Gold
                                            else -> Neon.TextHi
                                        },
                                        fontSize = 12.sp, fontWeight = FontWeight.SemiBold
                                    )
                                }
                            }
                        }
                    }
                }
                if (listening != null) {
                    Spacer(Modifier.height(12.dp))
                    KeyCaptureArea(listening!!) { keyCode ->
                        val actionId = "${pid}_${listening!!.name.lowercase()}"
                        if (keyCode == 27 /* ESC */) {
                            KeyMapStore.remove(actionId)
                        } else {
                            KeyMapStore.put(actionId, keyCode)
                        }
                        listening = null
                    }
                    Text(
                        "提示：按 ESC 恢复默认键位",
                        color = Neon.TextDim, fontSize = 10.sp,
                        modifier = Modifier.padding(top = 6.dp)
                    )
                }
                Spacer(Modifier.height(30.dp))
            }
        }
    }
}

/** 捕获下一次按键（全局 dispatcher，避免焦点问题）。 */
@Composable
private fun KeyCaptureArea(button: PadButton, onKey: (Int) -> Unit) {
    androidx.compose.runtime.DisposableEffect(button) {
        val kfm = java.awt.KeyboardFocusManager.getCurrentKeyboardFocusManager()
        val d = java.awt.KeyEventDispatcher { e ->
            if (e.id == java.awt.event.KeyEvent.KEY_PRESSED) {
                onKey(e.keyCode)
                true
            } else false
        }
        kfm.addKeyEventDispatcher(d)
        onDispose { kfm.removeKeyEventDispatcher(d) }
    }
    Box(
        Modifier
            .fillMaxWidth()
            .clip(RoundedCornerShape(8.dp))
            .background(Neon.BgPanelHi)
            .padding(14.dp)
    ) {
        Text("正在监听「${button.label}」的按键…", color = Neon.Accent, fontSize = 13.sp)
    }
}
