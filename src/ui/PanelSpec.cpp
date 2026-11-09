// ============================================================================
// NesStation PC — 核心设置面板分区精选表（★v1.3.4 对齐 APK SettingsPanel）
//
// Android 版的游戏内「核心设置」是按平台手写的中文分区面板
// （EmulatorScreen.kt SettingsPanel，选项键经 applyCoreOptions 实证下发），
// 而不是核心原始英文选项长表。本表把每个平台的分区标题与选项行按 APK
// 原样移植：行 = 选项键 + APK 中文标签；渲染时按键从核心实际选项表取值，
// 核心不存在的键静默跳过。未被收录的选项全部落在末尾「更多选项」分区
// （沿用 OptionZh 中文化层，未命中回退原文）。
// ============================================================================

#include "ui/PanelSpec.h"

namespace ns {
namespace panelSpec {

// —— 便捷宏（行表紧凑书写）——
#define ROWS(...) \
  { __VA_ARGS__ }
#define SEC(t, r) \
  { t, r, (int)(sizeof(r) / sizeof(r[0])) }

// NES（FCEUmm）
static const RowSpec kNes[] = {
    {"fceumm_ntsc_filter", "NTSC 滤镜"},
    {"fceumm_palette", "调色板"},
    {"fceumm_region", "主机区域"},
    {"fceumm_overscan_h_left", "裁剪过扫描（水平）"},
    {"fceumm_overscan_v_top", "裁剪过扫描（垂直）"},
    {"fceumm_overclocking", "超频"},
};
static const SectionSpec kNesS[] = {
    SEC("画面与调色", kNes),
};

// SFC（Snes9x）
static const RowSpec kSfcVideo[] = {
    {"snes9x_aspect", "画面比例"},
    {"snes9x_blargg", "NTSC 滤镜"},
    {"snes9x_overscan", "裁剪过扫描"},
    {"snes9x_gfx_transp", "半透明效果"},
    {"snes9x_gfx_hires", "高分辨率模式"},
    {"snes9x_hires_blend", "并排模式混合"},
};
static const RowSpec kSfcPerf[] = {
    {"snes9x_overclock", "SuperFX 超频"},
    {"snes9x_overclock_cycles", "超频周期（减少拖慢）"},
    {"snes9x_reduce_sprite_flicker", "减少精灵闪烁"},
};
static const RowSpec kSfcAudio[] = {
    {"snes9x_audio_interpolation", "音频插值"},
    {"snes9x_echo_buffer_hack", "回声缓冲区"},
};
static const RowSpec kSfcInput[] = {
    {"snes9x_up_down_allowed", "允许同时按上+下"},
};
static const RowSpec kSfcLayers[] = {
    {"snes9x_layer_1", "图层 1（BG1）"},
    {"snes9x_layer_2", "图层 2（BG2）"},
    {"snes9x_layer_3", "图层 3（BG3）"},
    {"snes9x_layer_4", "图层 4（BG4）"},
    {"snes9x_layer_5", "图层 5（精灵）"},
};
static const SectionSpec kSfcS[] = {
    SEC("画面", kSfcVideo),   SEC("性能", kSfcPerf),
    SEC("音频", kSfcAudio),   SEC("输入", kSfcInput),
    SEC("图层显示", kSfcLayers),
};

// GB / GBA（mGBA）
static const RowSpec kGbSys[] = {
    {"mgba_gb_model", "主机型号"},
    {"mgba_sgb_borders", "SGB 边框"},
    {"mgba_gba_forceRTC", "强制 RTC（时钟）"},
};
static const RowSpec kGbColor[] = {
    {"mgba_gb_colors", "GB 色彩校正"},
    {"mgba_gb_colors_preset", "GBC 色彩预设"},
    {"mgba_gba_colors", "GBA 色彩校正"},
    {"mgba_gba_colors_preset", "GBA 色彩预设"},
};
static const RowSpec kGbVideo[] = {
    {"mgba_interframe_blending", "帧混合（去闪烁）"},
};
static const RowSpec kGbAudio[] = {
    {"mgba_audio_low_pass_filter", "低通滤波"},
    {"mgba_audio_low_pass_range", "低通滤波范围"},
};
static const RowSpec kGbPerf[] = {
    {"mgba_frameskip", "跳帧"},
    {"mgba_frameskip_type", "跳帧类型"},
    {"mgba_frameskip_threshold", "跳帧阈值"},
    {"mgba_solar_sensor_level", "光照传感器等级"},
};
static const RowSpec kGbAdv[] = {
    {"mgba_gba_idle_optimization", "空闲优化"},
    {"mgba_allow_opposite_directions", "允许同时按反向键"},
};
static const SectionSpec kGbS[] = {
    SEC("系统", kGbSys),       SEC("色彩校正", kGbColor),
    SEC("画面", kGbVideo),     SEC("音频", kGbAudio),
    SEC("性能", kGbPerf),      SEC("高级", kGbAdv),
};

// DOS（DOSBox-Pure）
static const RowSpec kDosMachine[] = {
    {"dosbox_pure_machine", "机器类型"},
    {"dosbox_pure_memory_size", "内存大小"},
    {"dosbox_pure_cpu_type", "CPU 类型"},
};
static const RowSpec kDosCpu[] = {
    {"dosbox_pure_cycles", "CPU 性能（cycles）"},
    {"dosbox_pure_cpu_core", "CPU 核心"},
};
static const RowSpec kDosAudio[] = {
    {"dosbox_pure_sblaster_type", "声霸卡型号"},
    {"dosbox_pure_audiorate", "采样率"},
};
static const RowSpec kDosMouse[] = {
    {"dosbox_pure_mouse_input", "鼠标输入"},
};
static const RowSpec kDosKbd[] = {
    {"dosbox_pure_keyboard_layout", "键盘布局"},
};
static const RowSpec kDosVideo[] = {
    {"dosbox_pure_force60fps", "强制 60fps"},
    {"dosbox_pure_aspect_correction", "画面比例修正"},
};
static const RowSpec kDosAdv[] = {
    {"dosbox_pure_auto_mapping", "自动按键映射"},
    {"dosbox_pure_savestate", "即时存档支持"},
    {"dosbox_pure_voodoo", "Voodoo 显卡"},
};
static const SectionSpec kDosS[] = {
    SEC("机器类型", kDosMachine), SEC("CPU 性能", kDosCpu),
    SEC("音频", kDosAudio),       SEC("鼠标", kDosMouse),
    SEC("键盘", kDosKbd),         SEC("画面", kDosVideo),
    SEC("高级", kDosAdv),
};

// 街机（FBNeo）
static const RowSpec kArcVideo[] = {
    {"fbneo-aspect", "画面比例"},
    {"fbneo-crop-overscan", "裁剪过扫描"},
    {"fbneo-vertical-mode", "竖屏模式"},
    {"fbneo-rotate-mode", "旋转模式"},
};
static const RowSpec kArcPerf[] = {
    {"fbneo-cpu-speed", "CPU 超频"},
    {"fbneo-cpu-frameskip", "跳帧"},
    {"fbneo-force-60hz", "强制 60Hz"},
};
static const RowSpec kArcAudio[] = {
    {"fbneo-samplerate", "采样率"},
    {"fbneo-audio-interpolation", "音频插值"},
    {"fbneo-lowpass", "低通滤波"},
};
static const RowSpec kArcNeo[] = {
    {"fbneo-neogeo-mode", "NeoGeo 模式"},
    {"fbneo-memcard-mode", "记忆卡"},
};
static const RowSpec kArcMisc[] = {
    {"fbneo-hiscores", "高分榜保存"},
    {"fbneo-allow-patched-romsets", "改版 ROM 支持"},
};
static const SectionSpec kArcS[] = {
    SEC("画面", kArcVideo),  SEC("性能", kArcPerf),
    SEC("音频", kArcAudio),  SEC("NeoGeo", kArcNeo),
    SEC("兼容性", kArcMisc),
};

// MD / SEGA（Genesis-Plus-GX）
static const RowSpec kMdSys[] = {
    {"genesis_plus_gx_region", "区域"},
    {"genesis_plus_gx_system", "主机系统"},
};
static const RowSpec kMdVideo[] = {
    {"genesis_plus_gx_aspect_ratio", "画面比例"},
    {"genesis_plus_gx_render", "渲染滤镜"},
    {"genesis_plus_gx_blargg_ntsc_filter", "NTSC 滤镜"},
    {"genesis_plus_gx_lcd_filter", "LCD 残影滤镜"},
    {"genesis_plus_gx_overscan", "裁剪过扫描"},
    {"genesis_plus_gx_left_border", "左边框"},
};
static const RowSpec kMdInput[] = {
    {"genesis_plus_gx_input", "输入设备"},
    {"genesis_plus_gx_allow_up_down_allowed", "允许同时按上+下"},
};
static const RowSpec kMdPerf[] = {
    {"genesis_plus_gx_overclock", "68000 超频"},
    {"genesis_plus_gx_frameskip", "跳帧"},
};
static const RowSpec kMdCd[] = {
    {"genesis_plus_gx_cd_fastboot", "Mega-CD 快速启动"},
};
static const RowSpec kMdSms[] = {
    {"genesis_plus_gx_sms_fm", "SMS FM 音源"},
    {"genesis_plus_gx_gg_extra", "Game Gear 扩展列"},
    {"genesis_plus_gx_gg_stretch", "Game Gear 拉伸"},
};
static const SectionSpec kMdS[] = {
    SEC("系统", kMdSys),     SEC("画面", kMdVideo),
    SEC("输入", kMdInput),   SEC("性能", kMdPerf),
    SEC("Mega-CD", kMdCd),   SEC("Master System", kMdSms),
};

// PCE / TG16（Geargrafx）
static const RowSpec kPceSys[] = {
    {"geargrafx_console_type", "主机类型"},
};
static const RowSpec kPceVideo[] = {
    {"geargrafx_aspect_ratio", "画面比例"},
    {"geargrafx_overscan", "裁剪过扫描"},
    {"geargrafx_no_sprite_limit", "解除精灵数量限制"},
    {"geargrafx_palette", "调色板"},
};
static const RowSpec kPceCd[] = {
    {"geargrafx_cdrom_bios", "PCE-CD BIOS"},
};
static const RowSpec kPceMisc[] = {
    {"geargrafx_turbotap", "TurboTap（多手柄）"},
    {"geargrafx_mb128", "MB128 存档卡"},
    {"geargrafx_up_down_allowed", "允许同时按上+下"},
};
static const SectionSpec kPceS[] = {
    SEC("系统", kPceSys),   SEC("画面", kPceVideo),
    SEC("PCE-CD", kPceCd),  SEC("外设", kPceMisc),
};

// NDS（melonDS）
static const RowSpec kNdsSys[] = {
    {"melonds_console_mode", "主机模式（DS / DSi）"},
    {"melonds_boot_directly", "跳过开机画面直进游戏"},
};
static const RowSpec kNdsVideo[] = {
    {"melonds_opengl_renderer", "OpenGL 渲染器"},
    {"melonds_opengl_resolution", "渲染分辨率"},
    {"melonds_opengl_filtering", "纹理过滤"},
    {"melonds_opengl_better_polygons", "多边形优化"},
};
static const RowSpec kNdsTouch[] = {
    {"melonds_touch_mode", "触摸模式"},
};
static const RowSpec kNdsPerf[] = {
    {"melonds_jit_enable", "JIT 编译"},
    {"melonds_audio_interpolation", "音频插值"},
};
static const RowSpec kNdsLayout[] = {
    {"melonds_screen_layout", "屏幕排列"},
    {"melonds_screen_gap", "屏幕间隔"},
    {"melonds_swapscreen_mode", "换屏模式"},
    {"melonds_hybrid_small_screen", "混合小屏位置"},
};
static const RowSpec kNdsMisc[] = {
    {"melonds_mic_input", "麦克风输入"},
    {"melonds_language", "语言"},
    {"melonds_dsi_sdcard", "DSi SD 卡"},
    {"melonds_randomize_mac_address", "随机 MAC 地址"},
    {"melonds_use_fw_settings", "使用固件设置"},
    {"melonds_audio_bitrate", "音频位率"},
    {"melonds_jit_block_size", "JIT 块大小"},
    {"melonds_jit_fast_memory", "JIT 快速内存"},
    {"melonds_jit_branch_optimisations", "JIT 分支优化"},
    {"melonds_jit_literal_optimisations", "JIT 立即数优化"},
};
static const SectionSpec kNdsS[] = {
    SEC("系统", kNdsSys),         SEC("画面 / OpenGL", kNdsVideo),
    SEC("触摸 / 输入", kNdsTouch), SEC("性能 / 音频", kNdsPerf),
    SEC("布局", kNdsLayout),      SEC("高级", kNdsMisc),
};

// PSX（PCSX-ReARMed）
static const RowSpec kPsxBios[] = {
    {"pcsx_rearmed_bios", "BIOS（HLE / 真实）"},
    {"pcsx_rearmed_region", "区域"},
    {"pcsx_rearmed_show_bios_bootlogo", "显示 BIOS 开机画面"},
};
static const RowSpec kPsxCpu[] = {
    {"pcsx_rearmed_drc", "动态重编译（DRC）"},
    {"pcsx_rearmed_drc_thread", "编译器线程化"},
    {"pcsx_rearmed_psxclock", "PSX CPU 频率（%）"},
    {"pcsx_rearmed_icache_emulation", "ICache 模拟"},
    {"pcsx_rearmed_cd_readahead", "CD 预读取"},
    {"pcsx_rearmed_cd_turbo", "CD 加速"},
};
static const RowSpec kPsxVideo[] = {
    {"pcsx_rearmed_gpu_thread_rendering", "GPU 渲染线程化"},
    {"pcsx_rearmed_dithering", "抖动"},
    {"pcsx_rearmed_rgb32_output", "32 位色彩输出"},
    {"pcsx_rearmed_scale_hires", "高分辨率缩放"},
    {"pcsx_rearmed_show_overscan", "显示过扫描"},
    {"pcsx_rearmed_screen_centering", "画面居中"},
    {"pcsx_rearmed_neon_enhancement_enable", "画质增强（2x）"},
    {"pcsx_rearmed_display_internal_fps", "显示内部帧率"},
};
static const RowSpec kPsxPad[] = {
    {"pcsx_rearmed_vibration", "震动"},
    {"pcsx_rearmed_analog_axis_modifier", "模拟轴修正"},
    {"pcsx_rearmed_multitap", "多插手柄"},
    {"pcsx_rearmed_negcon_response", "neGcon 响应"},
    {"pcsx_rearmed_negcon_deadzone", "neGcon 死区"},
};
static const RowSpec kPsxAudio[] = {
    {"pcsx_rearmed_spu_interpolation", "SPU 插值"},
    {"pcsx_rearmed_spu_reverb", "SPU 混响"},
    {"pcsx_rearmed_nocdaudio", "CD-DA 音轨"},
    {"pcsx_rearmed_noxadecoding", "XA 解码"},
    {"pcsx_rearmed_spu_thread", "SPU 线程化"},
};
static const RowSpec kPsxMem[] = {
    {"pcsx_rearmed_memcard1", "记忆卡 1"},
    {"pcsx_rearmed_memcard2", "记忆卡 2"},
};
static const RowSpec kPsxFs[] = {
    {"pcsx_rearmed_frameskip_type", "跳帧类型"},
    {"pcsx_rearmed_frameskip_threshold", "跳帧阈值"},
    {"pcsx_rearmed_frameskip_interval", "跳帧间隔"},
};
static const SectionSpec kPsxS[] = {
    SEC("BIOS / 区域", kPsxBios), SEC("CPU / 性能", kPsxCpu),
    SEC("画面", kPsxVideo),       SEC("手柄", kPsxPad),
    SEC("音频", kPsxAudio),       SEC("CD / 记忆卡", kPsxMem),
    SEC("跳帧", kPsxFs),
};

// PS2（PCSX2 / LRPS2）
static const RowSpec kPs2Video[] = {
    {"pcsx2_upscale_multiplier", "分辨率倍数"},
    {"pcsx2_renderer", "渲染后端"},
    {"pcsx2_texture_filtering", "双线性过滤"},
    {"pcsx2_trilinear_filtering", "三线性过滤"},
    {"pcsx2_anisotropic_filtering", "各向异性过滤"},
    {"pcsx2_dithering", "抖动"},
    {"pcsx2_mipmapping", "Mip 贴图"},
    {"pcsx2_deinterlace_mode", "去隔行模式"},
    {"pcsx2_tv_shader", "TV 着色器"},
    {"pcsx2_shade_boost", "Shade Boost 亮度增强"},
    {"pcsx2_half_pixel_offset", "半像素偏移"},
    {"pcsx2_aspect_ratio", "画面比例"},
};
static const RowSpec kPs2Perf[] = {
    {"pcsx2_mtvu", "多线程 VU（mVU）"},
    {"pcsx2_instant_vu1", "Instant VU1"},
    {"pcsx2_ee_cycle_rate", "EE 周期率"},
    {"pcsx2_ee_cycle_skip", "EE 周期跳过"},
    {"pcsx2_hw_download_mode", "回读模式"},
};
static const RowSpec kPs2Patch[] = {
    {"pcsx2_widescreen_patches", "宽屏补丁"},
    {"pcsx2_no_interlacing_patches", "去隔行补丁"},
};
static const RowSpec kPs2Sys[] = {
    {"pcsx2_fast_boot", "跳过 BIOS 快速启动"},
    {"pcsx2_rumble", "手柄震动"},
};
static const SectionSpec kPs2S[] = {
    SEC("画面", kPs2Video),           SEC("性能（速度作弊）", kPs2Perf),
    SEC("补丁", kPs2Patch),           SEC("系统", kPs2Sys),
};

// DC / NAOMI（Flycast）
static const RowSpec kDcVideo[] = {
    {"reicast_internal_resolution", "内部分辨率"},
    {"reicast_widescreen_hack", "宽屏修正"},
    {"reicast_widescreen_cheats", "宽屏补丁"},
    {"reicast_delay_frame_swapping", "帧交换延迟"},
};
static const RowSpec kDcPerf[] = {
    {"reicast_threaded_rendering", "多线程渲染"},
    {"reicast_frame_skipping", "跳帧"},
    {"reicast_alpha_sorting", "半透明排序"},
};
static const RowSpec kDcSys[] = {
    {"reicast_hle_bios", "HLE BIOS（无 BIOS 可运行）"},
    {"reicast_region", "区域"},
    {"reicast_language", "语言"},
    {"reicast_cable_type", "输出制式"},
    {"reicast_dc_32mb_mod", "32MB 内存改装"},
    {"reicast_force_wince", "强制 Windows CE"},
};
static const RowSpec kDcAudio[] = {
    {"reicast_enable_dsp", "DSP 音频"},
};
static const RowSpec kDcMisc[] = {
    {"reicast_gdrom_fast_loading", "GD-ROM 快速读取"},
    {"reicast_broadcast", "广播制式"},
};
static const SectionSpec kDcS[] = {
    SEC("画面", kDcVideo),       SEC("性能", kDcPerf),
    SEC("系统 / BIOS", kDcSys),  SEC("音频", kDcAudio),
    SEC("光驱", kDcMisc),
};

// PSP（PPSSPP；标签移植自 APK PspCoreOptions.kt 目录）
static const RowSpec kPspVideo[] = {
    {"ppsspp_internal_resolution", "渲染分辨率"},
    {"ppsspp_skip_buffer_effects", "渲染模式（跳过缓冲特效）"},
    {"ppsspp_skip_gpu_readbacks", "跳过 GPU 回读"},
    {"ppsspp_frameskip", "跳帧"},
    {"ppsspp_auto_frameskip", "自动跳帧"},
    {"ppsspp_frame_duplication", "帧重复"},
    {"ppsspp_lower_resolution_for_effects", "特效降分辨率"},
    {"ppsspp_spline_quality", "曲线曲面质量"},
    {"ppsspp_cropto16x9", "画面裁剪至 16:9"},
    {"ppsspp_gpu_hardware_transform", "硬件顶点变换"},
    {"ppsspp_mulitsample_level", "多重采样 (MSAA)"},
    {"ppsspp_detect_vsync_swap_interval", "检测帧率变化"},
    {"ppsspp_software_rendering", "软件渲染"},
    {"ppsspp_backend", "图形后端（高级）"},
};
static const RowSpec kPspTex[] = {
    {"ppsspp_texture_scaling_level", "纹理放大"},
    {"ppsspp_texture_scaling_type", "纹理放大算法"},
    {"ppsspp_texture_deposterize", "纹理去色带"},
    {"ppsspp_texture_filtering", "纹理过滤"},
    {"ppsspp_smart_2d_texture_filtering", "智能 2D 过滤"},
    {"ppsspp_texture_anisotropic_filtering", "各向异性过滤"},
    {"ppsspp_texture_shader", "纹理着色器（硬件放大）"},
    {"ppsspp_texture_replacement", "高清纹理替换"},
};
static const RowSpec kPspPerf[] = {
    {"ppsspp_cpu_core", "CPU 核心"},
    {"ppsspp_fast_memory", "快速内存访问"},
    {"ppsspp_ignore_bad_memory_access", "忽略非法内存访问"},
    {"ppsspp_locked_cpu_speed", "锁定 PSP CPU 时钟"},
    {"ppsspp_io_timing_method", "I/O 时序模拟"},
    {"ppsspp_inflight_frames", "GPU 帧缓冲深度"},
    {"ppsspp_cache_iso", "整盘镜像缓存进内存"},
    {"ppsspp_force_lag_sync", "强制联机滞后同步"},
};
static const RowSpec kPspSys[] = {
    {"ppsspp_language", "游戏内系统语言"},
    {"ppsspp_psp_model", "PSP 型号"},
    {"ppsspp_memstick_inserted", "记忆棒插入状态"},
    {"ppsspp_button_preference", "确认键偏好"},
    {"ppsspp_cheats", "金手指"},
};
static const RowSpec kPspPad[] = {
    {"ppsspp_analog_deadzone", "摇杆死区"},
    {"ppsspp_analog_sensitivity", "摇杆灵敏度"},
    {"ppsspp_analog_is_circular", "圆形摇杆限制"},
};
static const RowSpec kPspNet[] = {
    {"ppsspp_enable_wlan", "WLAN（Ad-hoc 联机）"},
};
static const SectionSpec kPspS[] = {
    SEC("画面", kPspVideo),   SEC("纹理", kPspTex),
    SEC("性能", kPspPerf),    SEC("系统", kPspSys),
    SEC("手柄", kPspPad),     SEC("联机", kPspNet),
};

// 3DS（Azahar）
static const RowSpec k3dsVideo[] = {
    {"azahar_resolution_factor", "分辨率倍数"},
    {"azahar_graphics_api", "图形 API"},
    {"azahar_texture_filter", "纹理过滤"},
    {"azahar_texture_sampling", "纹理采样"},
    {"azahar_use_vsync_new", "垂直同步"},
    {"azahar_use_disk_shader_cache", "着色器磁盘缓存"},
    {"azahar_use_hw_shader", "硬件着色器"},
    {"azahar_enable_shader_jit", "着色器 JIT"},
    {"azahar_shaders_accurate_mul", "精确乘法"},
};
static const RowSpec k3dsLayout[] = {
    {"azahar_3d_mode", "3D 模式"},
    {"azahar_3d_factor", "3D 深度"},
    {"azahar_layout_option", "屏幕布局"},
    {"azahar_swap_screen", "交换上下屏"},
    {"azahar_screen_gap", "屏幕间隔"},
    {"azahar_upright_screen", "竖屏模式"},
    {"azahar_large_screen_proportion", "大屏占比"},
    {"azahar_custom_layout", "自定义布局"},
};
static const RowSpec k3dsSys[] = {
    {"azahar_is_new_3ds", "New 3DS 模式"},
    {"azahar_enable_new_3ds", "New 3DS 模式"},
    {"azahar_region_value", "主机地区"},
    {"azahar_use_hle", "HLE 模式（无系统固件）"},
    {"azahar_use_lle", "LLE 模式（需系统固件）"},
    {"azahar_use_cpu_jit", "CPU JIT"},
    {"azahar_cpu_clock_scale", "CPU 频率缩放"},
    {"azahar_emulate_sdmc", "模拟 SD 卡"},
    {"azahar_use_virtual_sd", "虚拟 SD 卡"},
};
static const RowSpec k3dsAudio[] = {
    {"azahar_audio_emulation", "音频模拟"},
    {"azahar_sound_index", "声音输出"},
    {"azahar_mic_input_type", "麦克风输入"},
};
static const SectionSpec k3dsS[] = {
    SEC("画面", k3dsVideo),         SEC("3D / 布局", k3dsLayout),
    SEC("系统", k3dsSys),           SEC("音频 / 输入", k3dsAudio),
};

#undef ROWS
#undef SEC

// 平台 → 精选分区表（nullptr = 无精选，全部走「更多选项」）
const SectionSpec* sectionsFor(GamePlatform p, int* outCount) {
  switch (p) {
    case GamePlatform::NES:     *outCount = (int)(sizeof(kNesS) / sizeof(kNesS[0])); return kNesS;
    case GamePlatform::SFC:     *outCount = (int)(sizeof(kSfcS) / sizeof(kSfcS[0])); return kSfcS;
    case GamePlatform::GB:
    case GamePlatform::GBA:     *outCount = (int)(sizeof(kGbS) / sizeof(kGbS[0])); return kGbS;
    case GamePlatform::DOS:     *outCount = (int)(sizeof(kDosS) / sizeof(kDosS[0])); return kDosS;
    case GamePlatform::ARCADE:  *outCount = (int)(sizeof(kArcS) / sizeof(kArcS[0])); return kArcS;
    case GamePlatform::MD:      *outCount = (int)(sizeof(kMdS) / sizeof(kMdS[0])); return kMdS;
    case GamePlatform::PCE:     *outCount = (int)(sizeof(kPceS) / sizeof(kPceS[0])); return kPceS;
    case GamePlatform::NDS:     *outCount = (int)(sizeof(kNdsS) / sizeof(kNdsS[0])); return kNdsS;
    case GamePlatform::PSX:     *outCount = (int)(sizeof(kPsxS) / sizeof(kPsxS[0])); return kPsxS;
    case GamePlatform::PS2:     *outCount = (int)(sizeof(kPs2S) / sizeof(kPs2S[0])); return kPs2S;
    case GamePlatform::DC:      *outCount = (int)(sizeof(kDcS) / sizeof(kDcS[0])); return kDcS;
    case GamePlatform::PSP:     *outCount = (int)(sizeof(kPspS) / sizeof(kPspS[0])); return kPspS;
    case GamePlatform::N3DS:    *outCount = (int)(sizeof(k3dsS) / sizeof(k3dsS[0])); return k3dsS;
    default: return nullptr;
  }
}

// 平台的「XX 专属设置」标题（APK SettingsPanel 各平台首行）
const char* platformSectionTitle(GamePlatform p) {
  switch (p) {
    case GamePlatform::NES: return "NES 专属设置";
    case GamePlatform::SFC: return "SFC/SNES 专属设置";
    case GamePlatform::GB: return "GB/GBC 专属设置";
    case GamePlatform::GBA: return "GBA 专属设置";
    case GamePlatform::DOS: return "DOSBox 专属设置";
    case GamePlatform::ARCADE: return "Arcade (FBNeo) 专属设置";
    case GamePlatform::MD: return "MD/SEGA (Genesis-Plus-GX) 专属设置";
    case GamePlatform::PCE: return "PCE/TG16 (Geargrafx) 专属设置";
    case GamePlatform::NDS: return "NDS 专属设置（melonDS）";
    case GamePlatform::PSX: return "PSX/PlayStation 专属设置";
    case GamePlatform::PS2: return "PS2 专属设置 (PCSX2 核心)";
    case GamePlatform::DC: return "DC (Flycast) 专属设置";
    case GamePlatform::PSP: return "PSP (PPSSPP) 核心选项（游戏内即时生效）";
    case GamePlatform::N3DS: return "3DS (Azahar) 专属设置";
    default: return "";
  }
}

}  // namespace panelSpec
}  // namespace ns
