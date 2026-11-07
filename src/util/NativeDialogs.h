// NesStation PC — 系统原生文件/目录选择对话框
// ★v1.3.0：用户要求「调用系统的文件管理器」——
//   Windows：COM IFileDialog（Vista+，与资源管理器同款；全盘符/收藏/中文原生）
//   Linux：zenity → kdialog（任一可用即走系统 GTK/Qt 对话框）
//   均不可用时返回 Unavailable，调用方回退内置浏览器。
#pragma once
#include <string>

namespace nativedlg {

enum class PickResult { Ok, Cancel, Unavailable };

// 选择目录。title：标题（UTF-8）。
PickResult pickFolder(void* sdlWindow, const char* title, std::string* outPath);

// 选择文件。filterSpec：Windows 通用对话框过滤串
//   （"ROM 文件\0*.nes;*.sfc\0所有文件\0*.*\0" —— 以 \0 结尾的段对；
//    传入 UTF-8，内部转宽）。Linux 端忽略（zenity 用 --file-filter 可选）。
PickResult pickFile(void* sdlWindow, const char* title, const char* filterSpec,
                    std::string* outPath);

}  // namespace nativedlg
