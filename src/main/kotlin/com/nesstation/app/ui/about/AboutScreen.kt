package com.nesstation.app.ui.about

import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
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
import androidx.compose.material.icons.rounded.Favorite
import androidx.compose.material3.Icon
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.nesstation.app.ui.neon.Neon
import java.awt.Desktop

/**
 * 关于页（对应 Android 版 AboutScreen）。
 */
@Composable
fun AboutScreen(onBack: () -> Unit, modifier: Modifier = Modifier) {
    Box(modifier.fillMaxSize().background(Neon.Bg)) {
        Column(
            Modifier
                .fillMaxSize()
                .verticalScroll(rememberScrollState())
                .padding(30.dp)
        ) {
            Row(verticalAlignment = Alignment.CenterVertically) {
                Text(
                    "← 返回", color = Neon.Accent, fontSize = 13.sp,
                    modifier = Modifier
                        .clip(RoundedCornerShape(6.dp))
                        .background(Neon.BgPanel)
                        .clickable(onClick = onBack)
                        .padding(horizontal = 10.dp, vertical = 5.dp)
                )
            }
            Spacer(Modifier.height(26.dp))

            Box(
                Modifier
                    .align(Alignment.CenterHorizontally)
                    .clip(RoundedCornerShape(18.dp))
                    .background(Neon.Accent)
                    .padding(horizontal = 22.dp, vertical = 10.dp)
            ) {
                Text("NesStation", color = Neon.BgDeep, fontSize = 30.sp, fontWeight = FontWeight.Black)
            }
            Spacer(Modifier.height(8.dp))
            Text(
                "for PC · GameBox",
                color = Neon.TextDim, fontSize = 13.sp,
                modifier = Modifier.align(Alignment.CenterHorizontally)
            )
            Spacer(Modifier.height(24.dp))

            AboutBlock("这是什么") {
                AboutText(
                    "高质感多平台复古游戏模拟器 —— 基于 Android 版 NesStation 重写的电脑版。" +
                        "支持 18 大平台，从 8-bit 到 128-bit 再到 Wii U / Switch：" +
                        "NES / SFC / GB / GBA / NDS / PCE / DOS / Arcade / MD / Java ME / DC / PSX / PS2 / PSP / 3DS / NGC&Wii / Wii U / Switch。"
                )
            }

            AboutBlock("两套主题界面") {
                AboutText("· Neon 3D 新UI —— 深空底 + 电光青 + 3D 封面流（默认）")
                AboutText("· FSD 经典桌面 —— Xbox 360 Freestyle Dash 磁贴流 + 像素天空")
                AboutText("设置 → 外观 → 主界面风格 随时切换，立即生效。")
            }

            AboutBlock("PC 核心架构") {
                AboutText("· 进程内 libretro 核心（FCEUmm / snes9x / mGBA / Genesis-Plus-GX / Geargrafx / DOSBox-Pure / FBNeo / melonDS / PCSX-ReARMed / PCSX2 / PPSSPP）—— libretro buildbot 官方 PC 构建，游戏内菜单/快存/快进统一体验。")
                AboutText("· 外置 PC 核心独立模拟器（PCSX2 / PPSSPP / Flycast / Azahar / Ishiiruka(Dolphin 系) / Cemu / Eden / FreeJ2ME）—— GitHub 官方发布，原生 GPU 全速渲染，统一检测 / 下载 / 启动。")
            }

            AboutBlock("与 Android 版的差异") {
                AboutText("· 移除 NDS「激烈 / DraStic」核心 —— Android 专有商业模拟器，无电脑版；PC 用 melonDS。")
                AboutText("· 移除 3DS「Citra MMJ」核心 —— Android 专有社区分支，无电脑版；PC 用 Azahar 官方版。")
                AboutText("· Android 构建（Gradle Android/APK）整体移除，改为 Compose Desktop 电脑版构建（Windows msi/exe · Linux deb/rpm · macOS dmg）。")
            }

            AboutBlock("开源致谢") {
                val projects = listOf(
                    "libretro / FCEUmm · snes9x · mGBA · Genesis-Plus-GX · Geargrafx · DOSBox-Pure · FBNeo · melonDS · PCSX-ReARMed · PCSX2 · PPSSPP · Flycast · FreeJ2ME",
                    "azahar-emu/azahar（3DS）",
                    "Tinob/Ishiiruka · dolphin-emu/dolphin（NGC/Wii）",
                    "cemu-project/Cemu（Wii U）",
                    "eden-emulator（Switch，github.com/eden-emulator/mirror）"
                )
                projects.forEach { AboutText("· $it") }
                Spacer(Modifier.height(8.dp))
                Text(
                    "游戏与 BIOS 请使用您合法拥有的拷贝。",
                    color = Neon.Gold.copy(alpha = 0.85f), fontSize = 11.sp
                )
            }

            Spacer(Modifier.height(16.dp))
            Row(
                Modifier.align(Alignment.CenterHorizontally),
                verticalAlignment = Alignment.CenterVertically
            ) {
                Icon(Icons.Rounded.Favorite, null, tint = Neon.Red, modifier = Modifier.size(13.dp))
                Spacer(Modifier.width(6.dp))
                Text(
                    "Made with nostalgia · MIT License · © 2026 NesStation contributors",
                    color = Neon.TextDim, fontSize = 11.sp
                )
            }
            Spacer(Modifier.height(40.dp))
        }
    }
}

@Composable
private fun AboutBlock(title: String, content: @Composable () -> Unit) {
    Text(title, color = Neon.Accent, fontSize = 13.sp, fontWeight = FontWeight.Bold,
        modifier = Modifier.padding(bottom = 6.dp))
    Column(
        Modifier
            .fillMaxWidth()
            .clip(RoundedCornerShape(10.dp))
            .background(Neon.BgPanel)
            .padding(14.dp)
    ) { content() }
    Spacer(Modifier.height(16.dp))
}

@Composable
private fun AboutText(t: String) {
    Text(t, color = Neon.Text, fontSize = 12.sp, lineHeight = 18.sp)
}
