// NesStation PC — libretro 核心选项中文化
//
// libretro 核心的选项（desc / info / 值标签）由核心在运行时以英文提供。
// 本文件提供「通用术语 + 高频核心选项」的翻译层：
//   · 通用术语表（Enabled/Disabled/Aspect Ratio/Region 等，覆盖所有核心的
//     绝大多数取值与常用选项名）
//   · 常用选项名精确翻译（按核心实际使用的英文字符串匹配，大小写不敏感）
//   · 未命中时原文显示（保证功能不缺失）
#pragma once
#include <string>

namespace ns {

// 翻译核心选项显示名 / 说明 / 分类 key / 值标签；未命中返回原文
std::string zhOptionDesc(const std::string& en);
std::string zhOptionInfo(const std::string& en);
std::string zhOptionCategory(const std::string& en);
std::string zhOptionValue(const std::string& en);

// ★v1.3.3 键级翻译（核心选项键 → 中文；PSP/PPSSPP 目录移植自 APK）。
//   先按键精确命中，未命中回落到字符串级翻译。
std::string zhOptionDescByKey(const std::string& key, const std::string& en);
std::string zhOptionValueByKey(const std::string& key, const std::string& en);

}  // namespace ns
