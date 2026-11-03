#!/usr/bin/env bash
# ============================================================================
# NesStation PC — 一键下载 libretro 官方 PC 预编译核心
#
# 用法：
#   ./scripts/fetch_cores.sh                # 下载全部 15 个核心（Windows 下用 fetch_cores.ps1）
#   ./scripts/fetch_cores.sh fceumm mgba    # 只下载指定核心
#
# 核心来源：libretro 官方 buildbot（nightly，x86_64）
#   Windows: https://buildbot.libretro.com/nightly/windows/x86_64/latest/
#   Linux:   https://buildbot.libretro.com/nightly/linux/x86_64/latest/
#
# 下载后自动解压到 <数据目录>/cores/（Linux: ~/.config/NesStation/cores/）
# 若 exe 旁存在 portable.flag 则放到 <exe 目录>/UserData/cores/
# ============================================================================
set -uo pipefail

# 平台判定
EXT=".so"
BASE="https://buildbot.libretro.com/nightly/linux/x86_64/latest"
if [[ "$OSTYPE" == "msys" || "$OSTYPE" == "cygwin" || "$OSTYPE" == "win32" ]]; then
  EXT=".dll"
  BASE="https://buildbot.libretro.com/nightly/windows/x86_64/latest"
fi

# 全部核心（平台 → 核心名，与 src/core/Platform.cpp 的矩阵一致）
ALL_CORES=(
  fceumm            # NES / FC
  snes9x            # SNES / SFC
  mgba              # GB / GBC / GBA
  genesis_plus_gx   # MD / SMS / GG / SG / Mega-CD
  geargrafx         # PCE / TG16 / SuperGrafx / PCE-CD
  fbneo             # Arcade (CPS1/2/3 NeoGeo PGM)
  dosbox_pure       # MS-DOS
  melonds           # NDS
  pcsx_rearmed      # PS1
  pcsx2             # PS2
  flycast           # DC / NAOMI / AtomisWave
  ppsspp            # PSP
  azahar            # 3DS
  squirreljme       # Java ME
)

# 目标目录：优先便携模式
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
EXE_DIR="$(dirname "$SCRIPT_DIR")"
if [[ -f "$EXE_DIR/portable.flag" ]]; then
  CORES_DIR="$EXE_DIR/UserData/cores"
else
  if [[ -n "${XDG_CONFIG_HOME:-}" ]]; then
    CORES_DIR="$XDG_CONFIG_HOME/NesStation/cores"
  else
    CORES_DIR="$HOME/.config/NesStation/cores"
  fi
fi
mkdir -p "$CORES_DIR"

# 命令行参数 = 核心子集
CORES=("${@:-${ALL_CORES[@]}}")
if [[ $# -gt 0 ]]; then CORES=("$@"); else CORES=("${ALL_CORES[@]}"); fi

echo "=========================================="
echo " NesStation PC 核心下载器"
echo " 平台：$EXT　目录：$CORES_DIR"
echo "=========================================="

ok=0; fail=0
for core in "${CORES[@]}"; do
  if [[ -f "$CORES_DIR/${core}_libretro$EXT" ]]; then
    echo "  [跳过] $core（已存在）"
    ok=$((ok+1))
    continue
  fi
  echo "  [下载] $core ..."
  if curl -fSL --retry 3 --retry-delay 2 --max-time 600 \
       -o "/tmp/${core}_libretro.zip" \
       "$BASE/${core}_libretro$EXT.zip"; then
    # 解压（兼容 unzip 与 python）
    if command -v unzip >/dev/null 2>&1; then
      unzip -o -q "/tmp/${core}_libretro.zip" -d "$CORES_DIR"
    else
      python3 -c "import zipfile,sys; zipfile.ZipFile('/tmp/${core}_libretro.zip').extractall('$CORES_DIR')"
    fi
    if [[ -f "$CORES_DIR/${core}_libretro$EXT" ]]; then
      echo "  [完成] $core"
      ok=$((ok+1))
    else
      echo "  [失败] $core（解压后未找到核心文件）"
      fail=$((fail+1))
    fi
  else
    echo "  [失败] $core（下载失败）"
    fail=$((fail+1))
  fi
  rm -f "/tmp/${core}_libretro.zip"
done

echo "=========================================="
echo " 完成：$ok 成功，$fail 失败"
echo " 提示：3DS 需要 azahar；NGC/Wii、Wii U、Switch"
echo "       为独立模拟器（Dolphin / Cemu / Eden），"
echo "       请放在 emulators/ 目录或到设置页配置路径。"
echo "=========================================="
exit $fail
