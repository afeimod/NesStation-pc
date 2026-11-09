#pragma once
// ============================================================================
// NesStation PC — 核心设置面板分区精选表（★v1.3.4 对齐 APK SettingsPanel）
// 详见 PanelSpec.cpp 头部注释。本表由 CoreOptionsScene 渲染：
// 行 = 选项键 + APK 中文标签；核心不存在的键静默跳过。
// ============================================================================

#include "core/Platform.h"

namespace ns::panelSpec {

struct RowSpec {
  const char* key;    // libretro 选项键（APK applyCoreOptions 实证）
  const char* label;  // APK SettingsPanel 中文标签
};

struct SectionSpec {
  const char* title;  // APK 分区标题
  const RowSpec* rows;
  int rowCount;
};

// 平台 → 精选分区表（返回 nullptr = 无精选，全部走「更多选项」）
const SectionSpec* sectionsFor(GamePlatform p, int* outCount);

// 平台的「XX 专属设置」标题（APK SettingsPanel 各平台首行）
const char* platformSectionTitle(GamePlatform p);

}  // namespace ns::panelSpec
