package com.nesstation.app.ui.library

import androidx.compose.foundation.background
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
import androidx.compose.foundation.lazy.grid.GridCells
import androidx.compose.foundation.lazy.grid.LazyVerticalGrid
import androidx.compose.foundation.lazy.grid.items
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.rounded.ArrowUpward
import androidx.compose.material.icons.rounded.Folder
import androidx.compose.material.icons.rounded.Home
import androidx.compose.material.icons.rounded.InsertDriveFile
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
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.compose.ui.window.Dialog
import java.io.File

/**
 * 自绘文件浏览器（对应 Android 版 FileBrowserDialog 的桌面版）。
 * 支持选择文件夹 / 选择文件（可按扩展名过滤）。
 */
object FileBrowserDialog {
    enum class Mode { PICK_FOLDER, PICK_FILE }
}

@Composable
fun FileBrowserDialog(
    title: String,
    mode: FileBrowserDialog.Mode,
    onDismiss: () -> Unit,
    onSelected: (String) -> Unit,
    extensions: List<String> = emptyList()
) {
    var current by remember { mutableStateOf(File(System.getProperty("user.home"))) }
    val entries = remember(current) {
        val files = current.listFiles()
            ?.sortedWith(compareByDescending<File> { it.isDirectory }.thenBy { it.name.lowercase() })
            ?: emptyList()
        files.filter { f ->
            f.isDirectory || extensions.isEmpty() ||
                extensions.any { f.extension.lowercase() == it.lowercase() }
        }
    }

    Dialog(
        onCloseRequest = onDismiss,
        title = title
    ) {
        Column(
            Modifier
                .fillMaxSize()
                .background(Color(0xFF141B29))
        ) {
            // 顶部路径栏
            Row(
                Modifier
                    .fillMaxWidth()
                    .background(Color(0xFF0D1524))
                    .padding(horizontal = 12.dp, vertical = 8.dp),
                verticalAlignment = Alignment.CenterVertically
            ) {
                Icon(
                    Icons.Rounded.Home, null, tint = Color(0xFF00E5FF), modifier = Modifier
                        .size(18.dp)
                        .clickable { current = File(System.getProperty("user.home")) }
                )
                Spacer(Modifier.width(8.dp))
                Icon(
                    Icons.Rounded.ArrowUpward, null, tint = Color(0xFFB8C7D9),
                    modifier = Modifier
                        .size(18.dp)
                        .clickable { current.parentFile?.let { current = it } }
                )
                Spacer(Modifier.width(12.dp))
                Text(
                    current.absolutePath,
                    color = Color(0xFFB8C7D9), fontSize = 12.sp,
                    maxLines = 1, overflow = TextOverflow.MiddleEllipsis,
                    modifier = Modifier.weight(1f)
                )
                if (mode == FileBrowserDialog.Mode.PICK_FOLDER) {
                    Text(
                        "选择此文件夹",
                        color = Color(0xFF05070E),
                        fontSize = 12.sp, fontWeight = FontWeight.Bold,
                        modifier = Modifier
                            .clip(RoundedCornerShape(4.dp))
                            .background(Color(0xFF00E5FF))
                            .clickable { onSelected(current.absolutePath) }
                            .padding(horizontal = 10.dp, vertical = 5.dp)
                    )
                }
            }
            Spacer(Modifier.height(1.dp))

            // 文件列表
            LazyVerticalGrid(
                columns = GridCells.Adaptive(230.dp),
                modifier = Modifier.weight(1f).padding(8.dp),
                horizontalArrangement = Arrangement.spacedBy(6.dp),
                verticalArrangement = Arrangement.spacedBy(6.dp)
            ) {
                items(entries) { f ->
                    Row(
                        Modifier
                            .clip(RoundedCornerShape(6.dp))
                            .background(Color(0xFF1B2438))
                            .clickable {
                                if (f.isDirectory) current = f
                                else if (mode == FileBrowserDialog.Mode.PICK_FILE) onSelected(f.absolutePath)
                            }
                            .padding(10.dp),
                        verticalAlignment = Alignment.CenterVertically
                    ) {
                        Icon(
                            if (f.isDirectory) Icons.Rounded.Folder else Icons.Rounded.InsertDriveFile,
                            null,
                            tint = if (f.isDirectory) Color(0xFFF2C200) else Color(0xFF8FA3BC),
                            modifier = Modifier.size(20.dp)
                        )
                        Spacer(Modifier.width(10.dp))
                        Column {
                            Text(
                                f.name, color = Color(0xFFE6EDF7), fontSize = 13.sp,
                                maxLines = 1, overflow = TextOverflow.Ellipsis
                            )
                            Text(
                                if (f.isDirectory) "文件夹" else "${f.length() / 1024} KB",
                                color = Color(0xFF5E6F85), fontSize = 10.sp
                            )
                        }
                    }
                }
            }

            // 底部状态
            Row(
                Modifier
                    .fillMaxWidth()
                    .background(Color(0xFF0D1524))
                    .padding(horizontal = 12.dp, vertical = 6.dp)
            ) {
                Text("${entries.size} 项", color = Color(0xFF5E6F85), fontSize = 11.sp)
                Box(Modifier.weight(1f))
                Text(
                    "取消", color = Color(0xFF8FA3BC), fontSize = 12.sp,
                    modifier = Modifier.clickable(onClick = onDismiss)
                )
            }
        }
    }
}
