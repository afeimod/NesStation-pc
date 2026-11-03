package com.nesstation.app.ui.settings

import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.compose.ui.window.Dialog
import com.nesstation.app.core.libretro.CoreOptions
import com.nesstation.app.ui.neon.Neon

/**
 * 核心选项通用面板（对应 Android 版 CoreSettingsPanel 的通用部分）：
 * 解析 SET_CORE_OPTIONS_V2 / SET_VARIABLES 得到的选项定义逐项渲染，
 * 修改即写回 CoreOptions（核心下一帧经 GET_VARIABLE_UPDATE 重读）。
 */
@Composable
fun CoreOptionsInGameDialog(
    coreId: String,
    onDismiss: () -> Unit,
    onChanged: (key: String, value: String) -> Unit
) {
    val defs = remember { CoreOptions.definitions(coreId) }
    Dialog(
        onCloseRequest = onDismiss,
        title = "核心设置"
    ) {
        Column(
            Modifier
                .background(Color0)
                .padding(16.dp)
        ) {
            if (defs.isEmpty()) {
                Text(
                    "该核心尚未上报选项定义（进入游戏后可用）。",
                    color = Neon.Text, fontSize = 12.sp
                )
            } else {
                Column(
                    Modifier
                        .fillMaxWidth()
                        .height(430.dp)
                        .verticalScroll(rememberScrollState())
                ) {
                    defs.forEach { d ->
                        var cur by remember(d.key) {
                            mutableStateOf(CoreOptions.effectiveValue(coreId, d.key))
                        }
                        Column(
                            Modifier
                                .fillMaxWidth()
                                .padding(vertical = 6.dp)
                                .clip(RoundedCornerShape(8.dp))
                                .background(Neon.BgPanelHi)
                                .padding(10.dp)
                        ) {
                            Text(d.desc, color = Neon.TextHi, fontSize = 12.sp, fontWeight = FontWeight.SemiBold)
                            Spacer(Modifier.height(6.dp))
                            Row {
                                d.choices.forEach { c ->
                                    val selected = c.value == cur
                                    Box(
                                        Modifier
                                            .padding(end = 6.dp)
                                            .clip(RoundedCornerShape(5.dp))
                                            .background(
                                                if (selected) Neon.Accent.copy(alpha = 0.28f)
                                                else Neon.BgDeep
                                            )
                                            .clickable {
                                                cur = c.value
                                                onChanged(d.key, c.value)
                                            }
                                            .padding(horizontal = 10.dp, vertical = 5.dp)
                                    ) {
                                        Text(
                                            c.label,
                                            color = if (selected) Neon.Accent else Neon.TextDim,
                                            fontSize = 11.sp
                                        )
                                    }
                                }
                            }
                        }
                    }
                }
            }
            Spacer(Modifier.height(10.dp))
            Box(Modifier.fillMaxWidth(), contentAlignment = Alignment.Center) {
                Text(
                    "完成", color = Neon.Accent, fontSize = 13.sp, fontWeight = FontWeight.Bold,
                    modifier = Modifier
                        .clip(RoundedCornerShape(6.dp))
                        .clickable(onClick = onDismiss)
                        .padding(horizontal = 20.dp, vertical = 8.dp)
                )
            }
        }
    }
}

private val Color0 = androidx.compose.ui.graphics.Color(0xFF0A1020)
