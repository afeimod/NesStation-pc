// NesStation PC — libretro 核心选项中文化（实现）
//
// 三层匹配（大小写不敏感）：
//   1) 精确表（optionDesc / optionValue / category）
//   2) 前缀规则（"Show " → "显示 " 等）
//   3) 未命中 → 原文
#include <algorithm>
#include <cctype>
#include <cstring>
#include <string>
#include <unordered_map>
#include <vector>

#include "ui/OptionZh.h"

namespace ns {
namespace {

std::string toLowerCopy(const std::string& s) {
  std::string o = s;
  std::transform(o.begin(), o.end(), o.begin(),
                 [](unsigned char c) { return (char)std::tolower(c); });
  return o;
}

// —— 通用取值（所有核心共用）——
const std::unordered_map<std::string, std::string>& valueMap() {
  static const std::unordered_map<std::string, std::string> m = {
      {"enabled", "开启"}, {"disabled", "关闭"}, {"on", "开"}, {"off", "关"},
      {"true", "是"}, {"false", "否"},
      {"none", "无"}, {"never", "从不"}, {"always", "总是"}, {"auto", "自动"},
      {"automatic", "自动"}, {"default", "默认"}, {"detect", "自动检测"},
      {"detect (default)", "自动检测（默认）"}, {"custom", "自定义"},
      {"yes", "是"}, {"no", "否"}, {"all", "全部"}, {"normal", "普通"},
      {"original", "原始"}, {"optimized", "优化"}, {"smart", "智能"},
      {"forced", "强制"}, {"force", "强制"}, {"optional", "可选"},
      {"required", "必需"}, {"unknown", "未知"}, {"other", "其他"},
      {"both", "两者"}, {"single", "单个"}, {"double", "双倍"},
      {"fast", "快速"}, {"slow", "慢速"}, {"medium", "中"}, {"low", "低"},
      {"high", "高"}, {"highest", "最高"}, {"lowest", "最低"},
      {"very low", "极低"}, {"very high", "极高"},
      // 地区 / 制式
      {"ntsc", "NTSC（美/日）"}, {"pal", "PAL（欧）"}, {"ntsc-u", "NTSC-U（美）"},
      {"ntsc-j", "NTSC-J（日）"}, {"pal-e", "PAL-E（欧）"},
      {"japan", "日本"}, {"usa", "美国"}, {"europe", "欧洲"},
      {"japanese", "日语"}, {"english", "英语"}, {"chinese", "中文"},
      {"region auto-detect", "地区自动检测"},
      // 画面
      {"4:3", "4:3"}, {"16:9", "16:9"}, {"8:7", "8:7"}, {"4:3 (default)", "4:3（默认）"},
      {"16:9 (recommended)", "16:9（推荐）"}, {"corrected", "校正"},
      {"uncorrected", "未校正（原始）"}, {"par", "原始像素比"},
      {"1x", "1×"}, {"2x", "2×"}, {"3x", "3×"}, {"4x", "4×"}, {"5x", "5×"},
      {"6x", "6×"}, {"8x", "8×"}, {"window", "窗口"}, {"fullscreen", "全屏"},
      {"pixel perfect", "像素完美"}, {"sharp", "锐利"}, {"soft", "柔和"},
      {"smooth", "平滑"}, {"nearest", "近邻（像素）"}, {"linear", "线性（平滑）"},
      {"bilinear", "双线性"}, {"integer scale", "整数缩放"},
      // 音频
      {"mono", "单声道"}, {"stereo", "立体声"}, {"stereo (default)", "立体声（默认）"},
      {"mute", "静音"}, {"48khz", "48kHz"}, {"44.1khz", "44.1kHz"},
      // 设备
      {"joypad", "手柄"}, {"keyboard", "键盘"}, {"mouse", "鼠标"},
      {"analog", "模拟摇杆"}, {"dualshock", "DualShock 手柄"},
      {"analog controller", "模拟手柄"}, {"digital controller", "数字手柄"},
      {"gun", "光枪"}, {"touchscreen", "触摸屏"},
      // 存档 / 状态
      {"save", "保存"}, {"load", "读取"}, {"increment", "递增"},
      {"rewind", "回溯"}, {"preload", "预载"}, {"on load", "读取时"},
      {"on save", "保存时"}, {"immediate", "立即"}, {"deferred", "延迟"},
      // 常见兼容档位
      {"safe", "安全（兼容优先）"}, {"aggressive", "激进（性能优先）"},
      {"balanced", "均衡"}, {"accurate", "精确（高精度）"},
      {"less accurate", "较低精度"}, {"more accurate", "较高精度"},
      {"dynamic", "动态"}, {"static", "静态"},
      {"performance", "性能优先"}, {"accuracy", "精度优先"},
      {"compatibility", "兼容性优先"}, {"hacks", " Hack（速度优先）"},
  };
  return m;
}

// —— 选项显示名（精确匹配，覆盖高频核心）——
const std::unordered_map<std::string, std::string>& descMap() {
  static const std::unordered_map<std::string, std::string> m = {
      // 系统 / 地区
      {"region", "主机地区"},
      {"console region", "主机地区"},
      {"system region", "主机地区"},
      {"system language", "主机语言"},
      {"system bios", "系统 BIOS"},
      {"bios", "BIOS"},
      {"use bios if available", "有 BIOS 时使用"},
      {"skip bios", "跳过 BIOS 开机画面"},
      {"root directory", "system 根目录"},
      {"system directory", "system 目录"},
      {"savefile directory", "存档目录"},
      {"savestate directory", "快速存档目录"},
      {"share saves with other emulators", "与其他模拟器共享存档"},
      {"automatic save state loading", "自动读取快速存档"},
      {"auto save state", "自动快速存档"},
      {"incremental save state", "递增快速存档"},
      // 视频
      {"aspect ratio", "画面比例"},
      {"aspect ratio (core)", "画面比例（核心）"},
      {"preferred aspect ratio", "首选画面比例"},
      {"video filter", "视频滤镜"},
      {"video scale", "画面缩放"},
      {"integer scaling", "整数缩放"},
      {"overscan", "过扫描（画面边缘）"},
      {"overscan (top)", "过扫描（上）"},
      {"overscan (bottom)", "过扫描（下）"},
      {"overscan (left)", "过扫描（左）"},
      {"overscan (right)", "过扫描（右）"},
      {"crop overscan", "裁剪过扫描"},
      {"remove sprite limit", "移除精灵数量限制（减少闪烁）"},
      {"reduce sprite flickering", "减少精灵闪烁"},
      {"sprite limit", "精灵数量限制"},
      {"video output", "视频输出"},
      {"interlaced output", "隔行扫描输出"},
      {"internal resolution", "内部分辨率"},
      {"render resolution", "渲染分辨率"},
      {"gpu resolution scale", "GPU 分辨率倍率"},
      {"framebuffer", "帧缓冲"},
      {"cpu frame skip", "跳帧"},
      {"frame skip", "跳帧"},
      {"frameskip", "跳帧"},
      {"audio buffer size", "音频缓冲大小"},
      {"vertical sync (vsync)", "垂直同步（VSync）"},
      {"throttle", "限速"},
      {"fast-forward ratio", "快进倍速"},
      {"show advanced settings", "显示高级设置"},
      {"osd messages", "屏幕消息提示"},
      // 音频
      {"audio output", "音频输出"},
      {"audio output sample rate", "音频输出采样率"},
      {"audio latency", "音频延迟"},
      {"audio sync", "音频同步"},
      {"audio rate control", "音频速率控制"},
      {"volume", "音量"},
      {"sound quality", "音质"},
      {"sound output", "声音输出"},
      {"mute audio", "静音"},
      // 输入
      {"input device", "输入设备"},
      {"input device 1", "输入设备 1"},
      {"input device 2", "输入设备 2"},
      {"controller port 1", "控制器端口 1"},
      {"controller port 2", "控制器端口 2"},
      {"analog self-calibration", "摇杆自动校准"},
      {"mouse sensitivity", "鼠标灵敏度"},
      {"user 1 analog to digital type", "玩家 1 摇杆→数字键映射"},
      {"user 2 analog to digital type", "玩家 2 摇杆→数字键映射"},
      {"input overlay", "输入叠加层"},
      // 性能 / Hack
      {"cpu clock", "CPU 主频"},
      {"overclock", "超频"},
      {"overclock cpu", "CPU 超频"},
      {" overclock", "超频"},
      {"enhancement", "画面增强"},
      {"enhancements", "画面增强"},
      {"accuracy hacks", "精度 Hack"},
      {"speed hacks", "速度 Hack"},
      {"graphic hacks", "图形 Hack"},
      {"memory hacks", "内存 Hack"},
      {"gpu hacks", "GPU Hack"},
      {"run ahead", "前置运行（降低输入延迟）"},
      {"run-ahead frames", "前置运行帧数"},
      {"second instance run-ahead", "双实例前置运行"},
      {"rewind settings", "回溯设置"},
      {"rewind enable", "开启回溯"},
      {"rewind buffer size", "回溯缓冲大小"},
      {"auto fast forward", "自动快进"},
      // 兼容性
      {"game fixes", "游戏兼容修正"},
      {"game-specific hacks", "游戏专用 Hack"},
      {"compatibility hacks", "兼容性 Hack"},
      {"force compatibility settings", "强制兼容设置"},
      {"legacy blitter", "传统图像输出"},
      // 其他高频
      {"save state thumbnails", "快速存档缩略图"},
      {"screenshot", "截图"},
      {"screenshots", "截图"},
      {"show startup screen", "显示开机画面"},
      {"show light gun crosshair", "显示光枪准星"},
      {"light gun settings", "光枪设置"},
      {"crosshair", "准星"},
      {"rumble", "震动"},
      {"vibration", "震动"},
      {"cheats", "作弊码"},
      {"cheat file", "作弊码文件"},
      {"netplay", "联机对战"},
      {"disk controls", "换碟控制"},
      {"disk image tray", "碟片托盘"},
      {"cd access method", "CD 读取方式"},
      {"cd image cache", "CD 镜像缓存"},
      {"cd bios", "CD BIOS"},
      {"cartridge", "卡带"},
      {"cartridge type", "卡带类型"},
      {"memory size", "内存大小"},
      {"battery saves", "电池存档"},
      {"rtc", "实时时钟（RTC）"},
      {"language", "语言"},
      {"translation", "翻译补丁"},
      {"show fps", "显示帧率"},
      {"show input", "显示输入状态"},
      {"show on-screen display", "显示屏幕信息（OSD）"},
      {"status bar", "状态栏"},
      {"toolbar", "工具栏"},
      {"virtual keyboard", "虚拟键盘"},
      {"on-screen keyboard", "屏幕键盘"},
      // FCEUmm / Nestopia 常见
      {"fds auto load disk side", "FDS 自动加载碟面"},
      {"fds fast forward load", "FDS 读取快进"},
      {"save ram before exit", "退出前保存 RAM"},
      {"power-on reset", "开机复位"},
      // snes9x / bsnes
      {"blargg ntsc filter", "Blargg NTSC 滤镜"},
      {"superscope (port 2)", "超级瞄准镜（端口 2）"},
      {"overclock superfx", "SuperFX 芯片超频"},
      {"superfx overclock", "SuperFX 芯片超频"},
      {"reduce slowdown", "减少游戏卡顿"},
      {"overclock superfx (needs reload)", "SuperFX 超频（需重载游戏）"},
      // mGBA / Gambatte
      {"use bios file if found", "找到 BIOS 文件则使用"},
      {"skip bios intro", "跳过 BIOS 开机动画"},
      {"border style", "边框样式"},
      {"color correction", "色彩校正"},
      {"color palette", "调色板"},
      {"ghosting", "残影（LCD 模拟）"},
      {"lcd ghosting", "LCD 残影"},
      {"lcd filter", "LCD 滤镜"},
      {"interframe blending", "帧间混合"},
      // Genesis Plus GX
      {"cd access time", "CD 读取时间"},
      {"master system fm", "Master System FM 音源"},
      {"game gear extended screen", "Game Gear 扩展画面"},
      {"68k address error", "68K 地址错误"},
      {"z80 dac precision", "Z80 DAC 精度"},
      {"ym2612 dac precision", "YM2612 DAC 精度"},
      {"sms fm sound", "SMS FM 音源"},
      {"cartridge lock-on", "卡带叠加（Lock-On）"},
      // PSX / PCSX-ReARMed
      {"dynarec (cpu core)", "动态重编译（CPU 核心）"},
      {"dynarec", "动态重编译（Dynarec）"},
      {"psx cpu clock", "PSX CPU 主频"},
      {"gpu blending", "GPU 半透明混合"},
      {"gpu dithering", "GPU 抖动"},
      {"frame duping", "重复帧利用"},
      {"display internal fps", "显示内部帧率"},
      {"enable vram ui block", "启用 VRAM UI 区块"},
      {"memcard 1 type", "记忆卡 1 类型"},
      {"memcard 2 type", "记忆卡 2 类型"},
      {"shared memory cards (2 players)", "共享记忆卡（双人）"},
      {"initial scanline", "起始扫描线"},
      {"last scanline", "结束扫描线"},
      {"ntsc filter", "NTSC 滤镜"},
      // melonDS
      {"console mode", "主机模式"},
      {"boot to firmware", "直接启动到系统界面"},
      {"dsi sd card", "DSi SD 卡"},
      {"screen layout", "屏幕布局"},
      {"screen gap", "屏幕间距"},
      {"screen scaling", "屏幕缩放"},
      {"cursor style", "光标样式"},
      {"hybrid layout ratio", "混合布局比例"},
      {"render mode", "渲染模式"},
      {"3d renderer", "3D 渲染器"},
      {"opengl resolution", "OpenGL 分辨率倍率"},
      {"threaded rendering", "多线程渲染"},
      {"software renderer", "软件渲染器"},
      {"hardware renderer", "硬件渲染器"},
      // FBNeo
      {"diagnostic input", "诊断输入"},
      {"cpu clock scale", "CPU 主频缩放"},
      {"neogeo mode", "NeoGeo 模式"},
      {"neogeo system", "NeoGeo 主板类型"},
      {"memcard mode", "记忆卡模式"},
      {"hiscore support", "最高分记录支持"},
      {"dsp fmv sound", "过场动画音效"},
      // DOSBox-Pure
      {"cpu type", "CPU 类型"},
      {"cpu core", "CPU 核心"},
      {"cycles", "CPU 速度（Cycles）"},
      {"sound blaster type", "声卡类型（Sound Blaster）"},
      {"midi output", "MIDI 输出"},
      {"gravis ultrasound", "Gravis Ultrasound 声卡"},
      {"mixer", "混音器"},
      {"joystick", "摇杆"},
      {"mouse wheel sensitivity", "滚轮灵敏度"},
      {"force 4:3 aspect ratio", "强制 4:3 画面比例"},
      {"correct aspect ratio", "校正画面比例"},
      {"keyboard layout", "键盘布局"},
      {"gamepad auto mapping", "手柄自动映射"},
      {"gamepad analog sensitivity", "手柄摇杆灵敏度"},
      {"start menu program", "开始菜单程序"},
      {"core profile", "核心配置方案"},
      {"load startup batch file", "加载启动批处理"},
      // PPSSPP
      {"internal resolution (hardware)", "内部分辨率（硬件）"},
      {"rendering mode", "渲染模式"},
      {"rendering resolution", "渲染分辨率"},
      {"hardware transform", "硬件变换"},
      {"software skinning", "软件蒙皮"},
      {"lazy texture caching", "纹理缓存优化"},
      {"spline/bezier curve quality", "样条/贝塞尔曲线质量"},
      {"gpu backend", "GPU 后端"},
      {"io timing method", "IO 时序方式"},
      {"ignore bad memory access", "忽略非法内存访问"},
      {"fast memory (unstable)", "快速内存（不稳定）"},
      {"force real clock sync", "强制真实时钟同步"},
      {"locked cpu speed", "锁定 CPU 速度"},
      {"cheats in save states", "快速存档包含作弊码状态"},
  };
  return m;
}

// —— 分类（retro_core_option_v2 分类 key）——
const std::unordered_map<std::string, std::string>& categoryMap() {
  static const std::unordered_map<std::string, std::string> m = {
      {"system", "系统"}, {"video", "视频"}, {"audio", "音频"},
      {"input", "输入"}, {"performance", "性能"}, {"hacks", "速度 Hack"},
      {"compatibility", "兼容性"}, {"diagnostics", "诊断"},
      {"internal", "内部"}, {"frontend", "前端"}, {"osd", "屏幕显示"},
      {"media", "介质"}, {"storage", "存储"}, {"gamepad", "手柄"},
      {"general", "通用"}, {"advanced", "高级"}, {"sound", "声音"},
      {"graphics", "图形"}, {"ui", "界面"}, {"cpu", "CPU"},
      {"gpu", "GPU"}, {"memory", "内存"}, {"cheat", "作弊"},
      {"save states", "快速存档"}, {"saves", "存档"}, {"emulation", "模拟"},
      {"enhancements", "画面增强"}, {"display", "显示"},
  };
  return m;
}

// 前缀/包含规则（英文 → 中文片段）
struct Rule {
  const char* en;   // 小写前缀或子串
  const char* zh;   // 中文替换（en 为前缀时拼接剩余部分不再译，直接整句替换）
  bool prefix;      // true = 前缀匹配（保留剩余原文），false = 子串替换
};

const std::vector<Rule>& rules() {
  static const std::vector<Rule> r = {
      {"show ", "显示 ", true},
      {"enable ", "启用 ", true},
      {"disable ", "禁用 ", true},
      {"use ", "使用 ", true},
      {"force ", "强制 ", true},
      {"allow ", "允许 ", true},
      {"preferred ", "首选 ", true},
      {"auto ", "自动", true},
      {"custom ", "自定义 ", true},
      {"override ", "覆盖 ", true},
      {"select ", "选择 ", true},
      {"internal ", "内部 ", true},
      {"sound ", "声音 ", true},
      {"audio ", "音频 ", true},
      {"video ", "视频 ", true},
      {"display ", "显示 ", true},
      {"memory ", "内存 ", true},
      {"input ", "输入 ", true},
      {"graphics ", "图形 ", true},
      {"graphic ", "图形 ", true},
      {"emulate ", "模拟 ", true},
      {"emulation ", "模拟 ", true},
      {"cd ", "CD ", true},
      {"bios ", "BIOS ", true},
      {"game ", "游戏 ", true},
      {"hardware ", "硬件 ", true},
      {"software ", "软件 ", true},
      {"vertical ", "垂直 ", true},
      {"horizontal ", "水平 ", true},
      {" (needs reload)", "（需重载游戏）", false},
      {" (requires restart)", "（需重启核心）", false},
      {" (restart core)", "（重启核心生效）", false},
      {" (default)", "（默认）", false},
      {" (recommended)", "（推荐）", false},
      {" (experimental)", "（实验性）", false},
  };
  return r;
}

std::string applyRules(const std::string& en) {
  std::string low = toLowerCopy(en);
  for (const auto& rule : rules()) {
    if (low.rfind(rule.en, 0) == 0) {
      if (rule.prefix) {
        std::string rest = en.substr(strlen(rule.en));
        return std::string(rule.zh) + rest;
      }
    }
  }
  // 子串替换规则
  std::string out = en;
  low = toLowerCopy(out);
  for (const auto& rule : rules()) {
    if (rule.prefix) continue;
    size_t pos;
    while ((pos = low.find(rule.en)) != std::string::npos) {
      out.replace(pos, strlen(rule.en), rule.zh);
      low = toLowerCopy(out);
    }
  }
  return out;
}

}  // namespace

std::string zhOptionDesc(const std::string& en) {
  if (en.empty()) return en;
  const auto& m = descMap();
  auto it = m.find(toLowerCopy(en));
  if (it != m.end()) return it->second;
  return applyRules(en);
}

std::string zhOptionInfo(const std::string& en) {
  // 说明文本太长且高度核心相关，仅做取值级替换 + 少量常见句式
  return en;  // 保持原文（未翻译的说明比错误的翻译好）
}

std::string zhOptionCategory(const std::string& en) {
  const auto& m = categoryMap();
  auto it = m.find(toLowerCopy(en));
  if (it != m.end()) return it->second;
  return applyRules(en);
}

std::string zhOptionValue(const std::string& en) {
  if (en.empty()) return en;
  const auto& m = valueMap();
  auto it = m.find(toLowerCopy(en));
  if (it != m.end()) return it->second;
  // "X (default)" / "Enabled (default)" 之类后缀
  const std::string low = toLowerCopy(en);
  const size_t pos = low.find(" (");
  if (pos != std::string::npos) {
    const std::string head = en.substr(0, pos);
    const std::string tail = en.substr(pos);
    auto it2 = m.find(toLowerCopy(head));
    if (it2 != m.end()) {
      std::string zhTail = tail;
      if (tail == " (default)") zhTail = "（默认）";
      else if (tail == " (recommended)") zhTail = "（推荐）";
      return it2->second + zhTail;
    }
  }
  return en;
}

}  // namespace ns
