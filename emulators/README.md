# emulators/ — 三大独立模拟器源码级集成

本目录内置 NesStation 支持的三个「外置独立核心」的**完整源码快照**（含全部
子模块），配合仓库根目录的 `scripts/build_emulators.sh`（Linux/macOS）或
`scripts/build_emulators.ps1`（Windows）**本地构建，全程无需下载模拟器本体**。

## 版本锚点

| 目录 | 项目 | 版本 | 平台 | 许可证 |
| --- | --- | --- | --- | --- |
| `dolphin/` | Dolphin（Ishiiruka 上游） | **2609** | NGC / Wii | GPL-2.0-or-later |
| `cemu/` | Cemu | **v2.6** | Wii U | MPL-2.0 |
| `eden/` | Eden（yuzu 社区延续） | **v0.2.1** | Switch | GPL-3.0-or-later |

> Ishiiruka 原仓库（Tinob/Ishiiruka）已停止维护且无 PC 源码构建产物；
> PC 端采用其上游 Dolphin（Ishiiruka 的多数画质增强已被上游吸收），
> 启动参数完全兼容。

## 快照获取方式

各目录为 GitHub 官方仓库对应 tag 的源码快照（codeload tarball），子模块按
上游 `.gitmodules` + git trees API 固定的 commit 位点拉取（Dolphin 34 个、
Cemu 5 个已全部内置；Eden 为自包含镜像快照，CPM 依赖见下）。复现脚本：
`scripts/fetch_emu_src.sh [dolphin|cemu|eden|all]`（需联网，产物即本目录）。

## 构建

```bash
# Linux / macOS（依赖清单见脚本头部注释）
scripts/build_emulators.sh all            # 或 dolphin / cemu / eden
# 产物：dist/emulators/<名称>/
```

```powershell
# Windows（VS2022 + CMake）
powershell -File scripts\build_emulators.ps1 -Target all
```

或走 CMake 超级构建（与前端一次编完）：

```bash
cmake -B build -DNESSTATION_SUPERBUILD=ON .
cmake --build build -j$(nproc)
# 产物：build/bundle/emulators/<名称>/
```

把产物目录（`dolphin/`、`cemu/`、`eden/`）放到 nesstation 可执行文件旁的
`emulators/` 下，或「设置 → 核心设置 → 外置核心 → 手动选择」指定可执行
文件，前端即自动识别为「✓ 已就绪」。

## 离线说明

- **Dolphin**：全部 Externals 已内置，配置与编译均无网络请求。
- **Cemu**：`dependencies/` 5 个子模块已内置（含 vcpkg，Windows 工具链
  直接用 `dependencies/vcpkg/scripts/buildsystems/vcpkg.cmake`）。
- **Eden**：Linux 下 `CPMUTIL_FORCE_SYSTEM=ON` 使用发行版系统库（离线）；
  Windows 下 CPM 依赖在 cmake 配置期拉取（首次需联网），如需完全离线可先
  运行 `emulators/eden/tools/cpm-fetch-all.sh` 预取（需要 curl/jq/7z）。

## 替代方案

不想本地编译（每个项目约 30~90 分钟）时，仍可：
1. 「设置 → 核心设置 → 外置核心 → 自动下载」（GitHub Release 官方包）；
2. 「手动选择」指向已安装的官方发行版（Steam 版 / 官网安装包路径均可）。
