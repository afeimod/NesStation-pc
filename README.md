# NesStation PC

一个为 **Windows / Linux 桌面** 打造的高质感多平台复古游戏模拟器前端。
基于 [afeimod/NesStation](https://github.com/afeimod/NesStation)（Android 版）重写，
保留标志性的**双主题界面**，全部核心替换为 **PC 平台核心**。

C++17 + SDL2 单二进制原生实现，libretro 核心进程内加载（RetroArch 同款架构）。

---

## ✨ 与 Android 版的对应关系

| | Android 版 | PC 版 |
| --- | --- | --- |
| 界面框架 | Kotlin + Compose | C++17 + SDL2 立即模式 UI |
| 主题 | **Neon 3D 游戏站 / FSD 经典** 双风格 | **完全保留**，设置里即时切换 |
| 核心 | Android arm64 预编译 .so | **PC x64 核心**（buildbot 官方预编译 / 源码集成） |
| 构建 | Gradle + Android Gradle Plugin | **CMake**（Windows MSVC / Linux GCC） |
| 已移除 | — | **DraStic（激烈，无 PC 版）**、**Citra MMJ（无 PC 版）** |

> NDS 由 melonDS（PC 版）继续支持；3DS 由 Azahar 官方 libretro PC 核心继续支持，
> 双平台均可游玩 —— 只有那两个 Android 专属核心被移除。

## 🎮 支持平台（18 平台 · 15 libretro 核心 + 3 独立模拟器）

| 平台 | PC 核心 | 形态 | 文件扩展名 | BIOS |
| --- | --- | --- | --- | --- |
| **NES / FC** | FCEUmm | libretro | `.nes` `.fds` `.unf` | FDS 需 `disksys.rom` |
| **SNES / SFC** | snes9x | libretro | `.smc` `.sfc` `.fig` | 否 |
| **GB / GBC** | mGBA | libretro | `.gb` `.gbc` `.sgb` | 否 |
| **GBA** | mGBA | libretro | `.gba` | 否 |
| **PCE / TG16** | Geargrafx | libretro | `.pce` `.sgx` `.hes` `.cue` | PCE-CD 需 System Card |
| **DOS** | DOSBox-Pure | libretro | `.bat` `.exe` `.dosz` `.conf` | 否 |
| **Arcade** | FBNeo | libretro | `.zip` `.7z` | NeoGeo→`neogeo.zip` PGM→`pgm.zip` |
| **MD / SEGA** | Genesis-Plus-GX | libretro | `.md` `.sms` `.gg` `.sg` `.cue` | Mega-CD 需 `bios_CD_*.zip` |
| **NDS** | melonDS | libretro | `.nds` `.ids` `.dsi` | 否（FreeBIOS） |
| **PSX** | PCSX-ReARMed | libretro | `.cue` `.pbp` `.chd` `.m3u` | 可选 |
| **PS2** | PCSX2 | libretro | `.iso` `.cso` `.chd` | 推荐 |
| **DC** | Flycast | libretro | `.gdi` `.cdi` `.chd` `.lst` | `dc_boot.bin`+`dc_flash.bin` |
| **PSP** | PPSSPP | libretro | `.iso` `.cso` `.pbp` | 否 |
| **3DS** | **Azahar**（官方 libretro PC 核心） | libretro | `.3ds` `.cci` `.cxi` `.cia` | 加密游戏需 `aes_keys.txt` |
| **Java ME** | SquirrelJME | libretro | `.jar` | 否 |
| **NGC / Wii** | **Dolphin 2512** | 独立模拟器（源码集成 / 外部探测） | `.gcm` `.rvz` `.wbfs` `.gcz` | 大部分免 BIOS |
| **Wii U** | **Cemu 2.6** | 独立模拟器（源码集成 / 外部探测） | `.wud` `.wux` `.wua` `.rpx` | 免（WUD 解密需 otp.bin） |
| **Switch** | **Eden 0.2.1** | 独立模拟器（源码集成 / 外部探测） | `.xci` `.nsp` `.nro` | 需 `prod.keys` |

> - libretro 核心来自 **libretro 官方 buildbot**（nightly x86_64，与 RetroArch 同源），`scripts/fetch_cores.sh / .ps1` 一键下载。
> - NGC/Wii、Wii U、Switch 与 Android 版 Cemu-AZ / Eden 源码级集成同构：`-DNESSTATION_SUPERBUILD=ON` 从官方源码树编译（ExternalProject），产物进 `emulators/`；也可以直接使用已安装的官方 PC 版（设置页配置路径即可）。
> - Ishiiruka 原仓库（TinobIshiiruka）已停止维护且无后续版本，PC 版采用其上游 **Dolphin**（Ishiiruka 的画质增强选项大部分已被上游吸收）。

## 🚀 快速开始

### 1. 编译

**Linux**
```bash
./scripts/build.sh                 # 自动 FetchContent SDL2（无需系统依赖）
# 或系统 SDL2：sudo apt install libsdl2-dev && ./scripts/build.sh --system-sdl
```

**Windows**（VS2022 x64 Native Tools）
```bat
scripts\build.bat                  # 自动 FetchContent SDL2
```

产物：`build/bin/nesstation`（Linux）/ `build/bin/Release/nesstation.exe`（Windows）。

### 2. 下载核心

```bash
# Linux
./scripts/fetch_cores.sh                     # 全部 14 个 libretro 核心
# Windows
powershell -ExecutionPolicy Bypass -File scripts\fetch_cores.ps1
```

核心落到 `~/.config/NesStation/cores/`（Linux）/ `%APPDATA%\NesStation\cores\`（Windows）。
exe 旁放 `portable.flag` 即转便携模式（全部数据进 `UserData/`）。

### 3. 放入 ROM

- 主界面 → 游戏库 → **＋ 添加 ROM 目录**（递归扫描，`.zip/.bin/.iso` 等歧义扩展按平台页消歧）
- 封面自动从 libretro 官方缩略图库获取（`thumbnails.libretro.com` + jsDelivr 镜像）
- BIOS 放 `~/.config/NesStation/system/`（详见设置 → BIOS / 系统）

### 4. NGC/Wii · Wii U · Switch（独立模拟器）

两种方式任选：
1. **外部安装**：装好官方 PC 版后，设置 → 独立模拟器 → 配置路径（或放 `emulators/<名称>/`，前端自动探测常见安装位置）
2. **源码集成**（对齐 Android 版 Cemu-AZ/Eden 模式）：
   ```bash
   ./scripts/build.sh --superbuild    # 需 vcpkg（Cemu）、系统依赖见下
   ```
   编译产物自动进 `emulators/`，前端直接接管启动。

## 🕹️ 操作

### 手柄 / 键盘（UI 与游戏通用）

| 动作 | 键盘 | 手柄 |
| --- | --- | --- |
| 十字 / 摇杆 | 方向键 / WASD 之外的箭头键 | D-pad / 左摇杆 |
| 确认 | Enter / 鼠标左键 | A |
| 返回 / 呼出游戏菜单 | Esc / F1 | B / Start |
| 选项 | E | Y |
| 收藏 | F | X |
| 翻页 | PgUp / PgDn / 滚轮 | L / R |

### 游戏内（默认映射，与原项目 README 控制表一致，设置页可改）

| 游戏按键 | 键盘 | 手柄 |
| --- | --- | --- |
| A / B / X / Y | X / Z / S / A | A / B / X / Y |
| L / R / L2 / R2 | Q / W / D / F | L / R / 摇杆按下 |
| Start / Select | Enter / Shift | Start / Back |

### 快捷键

| 键 | 功能 |
| --- | --- |
| F1 / Esc | 游戏内菜单（暂停/存档/读档/重置/画面/核心设置/换碟） |
| F5 / F8 | 快速存档 / 读档（槽位 1） |
| F6 / Tab（按住） | 快进 ×2 |
| F11 | 全屏切换 |

## 🖥️ 界面

- **Neon 3D 游戏站**（默认）：深空黑蓝 + 电光青 + 警示红；中央 3D 封面流（透视 + 倒影 + 辉光）、切角边框语言
- **FSD 经典**：Xbox 360 Freestyle Dash 磁贴桌面；深蓝渐变 + 斜向光带 + 蓝/黄对角色块
- 两套主题在 **设置 → 外观 → 主界面风格** 即时切换（持久化），游戏库 / 设置页全套跟随换肤

功能全景：
- 游戏库：平台过滤 / 收藏 / 最近游玩、封面网格、重命名、移除、重新获取封面
- 游戏内菜单：即时存档 ×11 槽（A=自动）、读档、重置、画面设置（缩放 / 扫描线 / CRT 荫罩 / 平滑 / 双屏布局）、核心设置（热生效）、磁盘切换（FDS/Mega-CD 换碟）
- 核心选项：libretro 标准核心选项 UI（v1/v2 统一解析，分类显示，持久化到 `core-options/*.json`）
- 双屏平台（NDS/3DS）：双屏 / 仅上屏 / 仅下屏布局
- 按键映射：双端口 × 16 键，键位捕获重绑定，恢复默认
- 数据：`~/.config/NesStation/`（library.json / config.json / cores / system / saves / states / covers / 日志）

## 🔨 超级构建（源码集成独立模拟器）

```bash
cmake -B build -DNESSTATION_SUPERBUILD=ON \
      -DNESSTATION_BUILD_DOLPHIN=ON \
      -DNESSTATION_BUILD_CEMU=ON \
      -DNESSTATION_BUILD_EDEN=ON
cmake --build build -j$(nproc)
```

| 模拟器 | 版本锚点 | 系统依赖（Linux） | Windows |
| --- | --- | --- | --- |
| Dolphin | `2512` | cmake、gcc≥10、libgtk（可关）、libevdev | vcpkg 工具链自动 |
| Cemu | `v2.6` | vcpkg（`VCPKG_ROOT`）+ C++20 | vcpkg 工具链自动 |
| Eden | `v0.2.1` | cmake、gcc≥10、nasm（可选） | vcpkg 可选 |

> 三个项目体量巨大（各 30~90 分钟编译）。**即便跳过超级构建**，前端也能直接使用
> 用户已安装的官方 PC 版 —— 与 Android 版“探测到即用”的体验一致。

## 📁 目录结构

```
NesStation-PC/
├── CMakeLists.txt              # 构建入口（选项：SUPERBUILD / SYSTEM_SDL2 / 各模拟器开关）
├── cmake/
│   ├── Dependencies.cmake      # SDL2：find_package 回退 FetchContent
│   └── EmulatorSuperbuild.cmake# Dolphin / Cemu / Eden ExternalProject
├── src/
│   ├── main.cpp                # 入口
│   ├── app/App.{h,cpp}         # 应用中枢：窗口 / 场景栈 / 配置 / 纹理缓存
│   ├── core/                   # 领域层：平台矩阵 / 游戏库 / 封面获取
│   ├── retro/                  # libretro 宿主：核心加载 / 音视频桥 / 输入 / 选项 / 快存
│   ├── emu/                    # EmuRunner（会话）+ ExternalEmulators（外部启动）
│   ├── ui/                     # 双主题 / 字体 / 控件 / 各场景
│   └── util/                   # 日志 / 文件系统
├── third_party/                # stb · nlohmann/json · libretro.h（全部单头内嵌）
├── assets/fonts/               # Noto Sans SC（中文字体内嵌分发）
├── scripts/                    # build.sh/.bat · fetch_cores.sh/.ps1
├── tests/smoke_libretro.cpp    # 真实核心链路冒烟测试
└── .github/workflows/build.yml # Windows + Linux CI
```

## 🧪 测试

```bash
# 编译并运行 libretro 链路冒烟测试（dlopen 真实核心 → 载入 ROM → 跑帧 → 存读档）
g++ -std=c++17 -I src -I third_party -I third_party/stb -I third_party/libretro \
    $(sdl2-config --cflags) tests/smoke_libretro.cpp \
    src/util/*.cpp src/retro/LibretroCore.cpp src/retro/AudioBridge.cpp \
    src/retro/VideoBridge.cpp src/retro/InputManager.cpp third_party/stb/stb_impl.cpp \
    $(sdl2-config --libs) -ldl -lpthread -o smoke && ./smoke
```

## 📜 许可证

| 组件 | 许可证 |
| --- | --- |
| 应用代码 | MIT |
| libretro 核心各自遵循上游许可证 | FCEUmm GPLv2 · snes9x 非商业 · mGBA MPL-2.0 · Genesis-Plus-GX GPLv2 · FBNeo 非商业 · DOSBox-Pure GPLv2 · melonDS GPLv3 · PCSX-ReARMed GPLv2 · PCSX2 GPLv3 · Flycast GPLv2 · PPSSPP GPLv2 · Azahar GPLv2 · SquirrelJME GPLv3 |
| Dolphin（NGC/Wii） | GPLv2+ |
| Cemu（Wii U） | MPL-2.0 |
| Eden（Switch） | GPLv3 |
| stb 单头库 | 公有领域 / MIT |
| nlohmann/json | MIT |
| Noto Sans SC 字体 | OFL 1.1 |

⚠️ **法律声明**：BIOS 与密钥文件（`neogeo.zip`、`bios_CD_*.zip`、`prod.keys`、`aes_keys.txt` 等）包含受版权保护的内容，本仓库不含任何此类文件；请仅使用从自己的设备合法提取的文件，不得分发。

## 🙌 致谢

- **[afeimod/NesStation](https://github.com/afeimod/NesStation)** —— Android 原版（界面设计与平台矩阵蓝本）
- **libretro / RetroArch 社区** —— 核心规范与 buildbot 预编译
- 各上游模拟器项目：FCEUX / snes9x / mGBA / Eke-Eke / FBNeo / melonDS / PCSX2 / stenzek（DuckStation/Flycast）/ PPSSPP / Azahar / Dolphin / Cemu / Eden
- **Analogue / Pico-8 / Xbox 360 FSD** —— 视觉风格灵感
