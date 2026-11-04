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
| 核心 | Android arm64 预编译 .so | **PC x64 / arm64 核心**（buildbot 官方预编译 / 源码集成） |
| 构建 | Gradle + Android Gradle Plugin | **CMake**（Windows MSVC / Linux GCC） |
| 已移除 | — | **DraStic（激烈，无 PC 版）**、**Citra MMJ（无 PC 版）** |

> NDS 由 melonDS（PC 版）继续支持；3DS 由 Azahar 官方 libretro PC 核心继续支持，
> 双平台均可游玩 —— 只有那两个 Android 专属核心被移除。

## 🎮 支持平台（18 平台 · 15 libretro 核心 + 8 外置独立核心）

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
| **PS2** | PCSX2（实验性） | libretro + 外置 **PCSX2**（推荐） | `.iso` `.cso` `.chd` | 需正版 BIOS |
| **DC** | Flycast | libretro | `.gdi` `.cdi` `.chd` `.lst` | `dc_boot.bin`+`dc_flash.bin` |
| **PSP** | PPSSPP（实验性） | libretro + 外置 **PPSSPP**（推荐） | `.iso` `.cso` `.pbp` | 否 |
| **3DS** | **Azahar**（官方 libretro PC 核心） | libretro | `.3ds` `.cci` `.cxi` `.cia` | 加密游戏需 `aes_keys.txt` |
| **Java ME** | SquirrelJME | libretro | `.jar` | 否 |
| **NGC / Wii** | **Ishiiruka（Dolphin 系）** | 外置独立核心 | `.gcm` `.rvz` `.wbfs` `.gcz` | 大部分免 BIOS |
| **Wii U** | **Cemu** | 外置独立核心（GitHub 自动下载） | `.wud` `.wux` `.wua` `.rpx` | 免（WUD 解密需 otp.bin） |
| **Switch** | **Eden** | 外置独立核心 | `.xci` `.nsp` `.nro` | 需 `prod.keys` |

> - libretro 核心来自 **libretro 官方 buildbot**（nightly，与 RetroArch 同源）：Windows x64 / Linux x86_64 / **Linux arm64（aarch64）**，`scripts/fetch_cores.sh / .ps1` 自动识别架构一键下载。
> - **核心回退链**：部分架构无某核心预编译包时自动改用替代核心（如 3DS：Azahar → Citra；NES：FCEUmm → Nestopia → QuickNES；PS1：PCSX-ReARMed → SwanStation 等）。核心缺失时启动游戏会弹出提示框列出全部候选与获取方式。
> - **外置独立核心（8 个，对齐 Android CoreRegistry.EXTERNAL）**：PCSX2 / PPSSPP / Flycast / Azahar / **Ishiiruka（Dolphin 系）** / **Cemu** / **Eden** / FreeJ2ME。
>   设置 → 核心页可直接**自动下载**（GitHub Releases，带进度）或**打开官网**；
>   也可手动选择可执行文件。探测顺序：配置路径 → `数据目录/external/<id>/` →
>   `emulators/<id>/` → 常见安装位置（Program Files / /usr/bin / /opt 等）。
> - **双形态平台（PS2 / PSP / DC / 3DS / Java）**：进程内 libretro 与外置独立核心并存，
>   启动时弹核心选择器（外置「推荐」绿徽章 / 进程内「实验性」金徽章，
>   对齐 Android CorePickerDialog），可勾选「记住此游戏的选择」。
> - NGC/Wii 的外置核心为 **Ishiiruka（Dolphin 系）**（对齐 Android 注册表）；
>   标准 Dolphin 同参数兼容（`-e` 启动），探测时同时认 Ishiiruka.exe / Dolphin.exe。

## 🚀 快速开始

### 1. 编译

**Linux**
```bash
./scripts/build.sh                 # 自动 FetchContent SDL2 + 构建后自动下载核心
# 或系统 SDL2：sudo apt install libsdl2-dev && ./scripts/build.sh --system-sdl
# 离线构建：./scripts/build.sh --no-cores
```

**Windows**（VS2022 x64 Native Tools）
```bat
scripts\build.bat                  # 自动 FetchContent SDL2
```

产物：`build/bin/nesstation`（Linux）/ `build/bin/Release/nesstation.exe`（Windows）。

### 2. 核心（构建时自动下载 / 发布包已内置）

**为什么不把核心塞进源码包？** 核心是 libretro 官方 buildbot 的**预编译二进制**（每架构 15 个、共 400+ MB，nightly 每日更新，各自遵循上游许可证 GPLv2 / 非商业 / MPL 等）。源码仓库只放源码，核心从官方渠道拉取——与 RetroArch 同源同版本，永远拿到最新安全构建，也避免仓库膨胀。因此源码 zip 里没有核心，但**不需要你手动找**：

| 获取方式 | 说明 |
| --- | --- |
| **① 构建时自动下载（默认）** | `./scripts/build.sh` 编译完成后自动从 buildbot 下载本架构全部可用核心到 `build/bin/cores/`（exe 同目录，启动即识别），源码构建**开箱即玩**；离线构建加 `--no-cores` |
| **② 直接下发布包** | CI 产物已内置全部可用核心：Windows zip → `cores/*.dll`；Linux tar.gz → `cores/*.so`；arm64 deb → `/opt/nesstation/cores/*.so`，解压/安装即玩 |
| **③ 手动补齐/更新** | 脚本一键下载（见下），已有平台自动跳过 |

```bash
# 手动下载（自动识别 x86_64 / aarch64 架构，逐平台下载，404 自动回退替代核心）
bash scripts/fetch_cores.sh                     # 全部 15 个平台
bash scripts/fetch_cores.sh nes psx 3ds         # 只下载指定平台
# Windows
powershell -ExecutionPolicy Bypass -File scripts\fetch_cores.ps1
```

手动下载的核心落到 `~/.config/NesStation/cores/`（Linux）/
`%APPDATA%\NesStation\cores\`（Windows），**优先级高于包内内置核心**（可覆盖升级）。
exe 旁放 `portable.flag` 即转便携模式（全部数据进 `UserData/`）。

> - 构建时断网/部分平台无预编译包**不会使构建失败**（脚本软退出，运行时会提示缺失平台与获取方式）。
> - libretro 预编译核心并非所有架构都提供（如 azahar 无 linux/aarch64 包，
>   会自动回退 citra）；下载/打包/运行时使用同一条候选链，某个核心缺失时
>   自动选用链上已安装的替代核心。

### 3. 放入 ROM

- 主界面 → 游戏库 → **＋ 添加 ROM 目录**（递归扫描，最深 32 层，符号链接环自动去重）
- ★ **智能平台嗅探**：`.zip` 解析内部文件名、`.cue/.bin/.iso/.img/.mdf` 读取光盘镜像头
  （ISO9660 PVD / PLAYSTATION 标识 / SEGA 引导区 / GC·Wii 魔数 / PSP 魔数）自动
  识别真实平台；街机包（无扩展名部件文件）自动归入街机。识别失败才回退按扩展名
- ★ **启动自动重扫**：每次启动后台线程重扫全部 ROM 目录，新游戏自动入库
  （对齐 Android 版「启动/手动刷新时自动入库」）
- 封面自动从 libretro 官方缩略图库获取（`thumbnails.libretro.com` + jsDelivr 镜像）
- BIOS 放 `~/.config/NesStation/system/`（详见设置 → 存储）

### 4. NGC/Wii · Wii U · Switch 及外置独立核心

★ **源码级集成（v1.2 新）**：仓库 `emulators/` 目录已内置三大模拟器完整源码快照
（Dolphin 2609 / Cemu v2.6 / Eden v0.2.1，含全部子模块），本地构建无需联网：

```bash
# Linux / macOS（依赖清单见脚本头部注释）
scripts/build_emulators.sh all          # 产物：dist/emulators/<名称>/
# Windows（VS2022）
powershell -File scripts\build_emulators.ps1 -Target all
# 或 CMake 超级构建：cmake -B build -DNESSTATION_SUPERBUILD=ON .
```

产物目录放到 nesstation 可执行文件旁的 `emulators/` 下即自动识别，
详见 `emulators/README.md`。不想编译时三种替代方式：
1. **自动下载**（Cemu / PCSX2 / PPSSPP / Flycast / Azahar）：点击「自动下载」，
   从 GitHub Releases 拉取官方包并自动安装到 `数据目录/external/<id>/`
2. **打开官网**（Eden / FreeJ2ME）：点击「打开官网」到官方发布页下载，
   解压后「手动选择」可执行文件
3. **自动探测**：已安装的官方版放 `emulators/<名称>/`（或装在 Program Files /
   /usr/bin 等常见位置）会被自动识别

启动参数已按各官方推荐配置（Dolphin/Ishiiruka `-e` / Cemu `-g … -f` / Eden 直启等），
双形态平台（PS2/PSP/DC/3DS/Java）启动时可在进程内与外置核心间选择。

### 5. CI 构建（GitHub Actions，手动触发）

Actions 页面 → **Build** → **Run workflow** → 选择构建目标：

| 目标 | 产物 | 说明 |
| --- | --- | --- |
| `linux-x64` | `NesStation-linux-x64.tar.gz` | 便携包（含字体），Ubuntu 22.04 基线 |
| `linux-arm64` | `nesstation_1.1.0_arm64.deb` | **Debian / Ubuntu 安装包**，Debian 12+ / Ubuntu 22.04+ |
| `windows-x64` | `NesStation-windows-x64.zip` | 便携包（SDL2 静态链接，FetchContent） |
| `all` | 以上全部 | — |

> - arm64 deb 在 arm64 runner 的 **debian:12 容器**内原生构建（glibc 2.36 基线 +
>   libstdc++ 静态链接），运行时仅依赖 `libc6` + `libsdl2-2.0-0`，
>   `sudo apt install ./nesstation_1.1.0_arm64.deb` 即装（含桌面图标）。
> - Linux 任务含真实核心冒烟测试（buildbot fceumm + 假视频驱动跑 240 帧 + 快速存读档）。
> - 构建不随 push / PR 自动触发，完全手动。

## 🕹️ 操作

### 三套输入设备全部支持（鼠标 / 键盘 / 手柄）

鼠标与触摸为第一等公民：所有按钮、菜单、列表、封面流、磁贴均可点击；
封面流与磁贴流支持横向拖拽；★ **所有列表/长页支持按住拖拽滚动**（鼠标/
触摸屏一致，拖动与点击自动互斥不误触）；滚轮同步支持；游戏内**右键**呼出菜单。

### 手柄 / 键盘（UI 与游戏通用）

| 动作 | 键盘 | 手柄 | 鼠标 |
| --- | --- | --- | --- |
| 十字 / 摇杆 | 方向键 | D-pad / 左摇杆 | 悬停即选中 |
| 确认 | Enter | A | 左键点击 |
| 返回 / 呼出游戏菜单 | Esc | B / Start | 右键（游戏内）或左上角「‹ 返回」按钮 |
| 选项 | E | Y | — |
| 收藏 | F | X | — |
| 翻页 / 滚动 | PgUp / PgDn / 滚轮 | L / R | 滚轮 |

> 每个页面左上角都有可见的「‹ 返回」按钮，不再依赖手柄 Esc。

### 游戏内（默认映射，与原项目 README 控制表一致，设置页可改）

| 游戏按键 | 键盘 | 手柄 |
| --- | --- | --- |
| A / B / X / Y | X / Z / S / A | A / B / X / Y |
| L / R / L2 / R2 | Q / W / D / F | L / R / 摇杆按下 |
| Start / Select | Enter / Shift | Start / Back |

### 快捷键

| 快捷键 | 功能 |
| --- | --- |
| F1 / Esc / 右键 | 游戏内菜单（暂停/存档/读档/重置/画面/核心设置/换碟） |
| F5 / F8 | 快速存档 / 读档（槽位 1） |
| F6 / Tab（按住） | 快进 ×2 |
| F11 | 全屏切换 |
| F12 | 截图当前帧（存数据目录 shots/） |

## 🖥️ 界面

两套主题与 Android 版**像素级同源**（设计令牌逐项移植）：

- **Neon 3D 游戏站**（默认）：深空黑蓝 + 电光青 + 警示红；顶部品牌徽标 + 时钟，
  7 彩色 chip 菜单行（游戏库/收藏/最近/核心管理/设置/关于/退出），
  中央 3D 封面流（透视缩放 + 逐级渐隐 + 选中辉光，与 Android NeonFlow 同款弧形导轨），
  底部信息区（大标题 + 平台徽章 + 游玩统计 + 「开始游戏」渐变主按钮）+ 键帽式按键提示栏
- **FSD 经典**：Xbox 360 Freestyle Dash 磁贴桌面；像素天空（漂移云 / 闪烁星光 / 海面 / 草地），
  蓝→黄对角渐变磁贴（白图标 + 左下粗体标签 + 右上徽标），底部半透明 Dock
- 两套主题在 **设置 → 外观 → 主界面风格** 即时切换（持久化），游戏库 / 设置页全套跟随换肤

功能全景：
- 游戏库双皮肤（与 Android 版同构）：Neon = 左侧核心列表（彩点 + 计数 + 搜索 +
  导入/刷新）+ 平台大标题/时代副标题 + 3D 封面流 + 底部信息条（徽章 / ★ 收藏 /
  删除 / 开始）；FSD = 顶栏 + 平台过滤 chips + 方形封面网格
- 设置双皮肤：Neon = 左侧彩色图标分类导航（外观/视频/显示/性能/输入/存储/核心设置/关于）
  + NeonPanel 分节面板 + NeonSwitch/SpinnerRow/StyleCard；FSD = 单页分节滚动
- 游戏内菜单：即时存档 ×11 槽（A=自动）、读档、重置、画面设置（缩放 / 扫描线 / CRT 荫罩 / 平滑 / 双屏布局）、核心设置（热生效）、磁盘切换（FDS/Mega-CD 换碟）
- ★ 核心选项全面中文化：选项名 / 分类 / 取值（Enabled→开启、NTSC→NTSC（美/日）等
  200+ 词条 + 前缀规则），未命中词条回退英文原文；核心名全部中文标注
  （如「FCEUmm · FC/NES 模拟」「Snes9x · 超级任天堂模拟」）
- 核心选项持久化到 `core-options/*.json`（v1/v2 统一解析，分类显示）
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

★ **v1.2 起源码已随仓库内置**（`emulators/` 目录，含全部子模块快照），超级构建
**不再联网拉取**；也可用 `scripts/build_emulators.sh(.ps1)` 独立构建（见 emulators/README.md）。

| 模拟器 | 源码快照 | 离线构建依赖（Linux） | Windows |
| --- | --- | --- | --- |
| Dolphin | `2609`（34 Externals 全内置） | cmake≥3.16、gcc≥10、Qt6/Qt5 开发包 | VS2022（Qt 预编译已内置） |
| Cemu | `v2.6`（dependencies 全内置） | cmake≥3.21、GTK3/Wayland 开发包 | VS2022 + 内置 vcpkg |
| Eden | `v0.2.1`（镜像快照） | cmake≥3.22 + 系统库（`CPMUTIL_FORCE_SYSTEM=ON`，清单见脚本） | 首次配置需联网拉 CPM 依赖 |

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
├── scripts/                    # build.sh/.bat（构建后自动下核心）· fetch_cores.sh/.ps1（架构自适应+回退链）· seed_selftest.py（无头测试数据）
├── tests/smoke_libretro.cpp    # 真实核心链路冒烟测试
└── .github/workflows/build.yml # CI（手动触发：linux-x64 / linux-arm64(deb) / windows-x64）
```

## 🧪 测试

```bash
# 编译并运行 libretro 链路冒烟测试（dlopen 真实核心 → 载入 ROM → 跑帧 → 存读档）
g++ -std=c++17 -I src -I third_party -I third_party/stb -I third_party/libretro \
    $(sdl2-config --cflags) tests/smoke_libretro.cpp \
    src/util/*.cpp src/retro/LibretroCore.cpp src/retro/AudioBridge.cpp \
    src/retro/VideoBridge.cpp src/retro/InputManager.cpp third_party/stb/stb_impl.cpp \
    $(sdl2-config --libs) -ldl -lpthread -o smoke && ./smoke

# 无头 UI 自检（dummy 视频驱动渲染各场景并截图，验证两套主题视觉）
python3 scripts/seed_selftest.py           # 先造测试 ROM + 游戏库（到 /tmp/roms）
NESSTATION_SELFTEST="home:90,fsd:120,library:40,settings:40,about:20,exit:1" \
NESSTATION_SHOT_DIR=/tmp/shots ./build/bin/nesstation
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
