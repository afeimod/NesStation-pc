#!/usr/bin/env bash
# ============================================================================
# NesStation PC — 一键下载 libretro 官方预编译核心（按当前架构）
#
# 用法：
#   ./scripts/fetch_cores.sh                # 按平台下载全部核心（自动回退）
#   ./scripts/fetch_cores.sh nes psx 3ds    # 只下载指定平台（平台短名）
#
# 核心来源：libretro 官方 buildbot（nightly）
#   Windows:  https://buildbot.libretro.com/nightly/windows/x86_64/latest/
#   Linux64:  https://buildbot.libretro.com/nightly/linux/x86_64/latest/
#   Linux arm64（注意路径是 aarch64）:
#             https://buildbot.libretro.com/nightly/linux/aarch64/latest/
#
# 每个平台有一条候选链（与 src/core/Platform.cpp 一致），某个核心在当前
# 架构没有预编译包（404）时自动尝试替代核心，例如：
#   3DS: azahar → citra（azahar 无 linux/aarch64 预编译包）
#
# 下载后自动解压到 <数据目录>/cores/（Linux: ~/.config/NesStation/cores/）
# 若 exe 旁存在 portable.flag 则放到 <exe 目录>/UserData/cores/
# ============================================================================
set -uo pipefail

# ---------- 平台 / 架构判定 ----------
EXT=".so"
BASE="https://buildbot.libretro.com/nightly/linux/x86_64/latest"
if [[ "$OSTYPE" == "msys" || "$OSTYPE" == "cygwin" || "$OSTYPE" == "win32" ]]; then
  EXT=".dll"
  BASE="https://buildbot.libretro.com/nightly/windows/x86_64/latest"
else
  ARCH="$(uname -m)"
  case "$ARCH" in
    aarch64|arm64)
      BASE="https://buildbot.libretro.com/nightly/linux/aarch64/latest" ;;
    armv7*|armhf|arm)
      BASE="https://buildbot.libretro.com/nightly/linux/armhf/latest" ;;
    *)
      BASE="https://buildbot.libretro.com/nightly/linux/x86_64/latest" ;;
  esac
fi

# ---------- 平台 → 核心候选链（与 src/core/Platform.cpp 一致）----------
# 格式："平台短名|平台名|候选核心（空格分隔，依次回退）"
PLATFORM_CORES=(
  "nes|NES / FC|fceumm nestopia quicknes"
  "sfc|SNES / SFC|snes9x bsnes"
  "gb|GB / GBC|mgba gambatte"
  "gba|GBA|mgba vba_next gpsp"
  "md|MD / SEGA|genesis_plus_gx picodrive"
  "pce|PCE / TG16|geargrafx mednafen_pce_fast"
  "nds|NDS|melonds desmume"
  "psx|PS1|pcsx_rearmed swanstation"
  "ps2|PS2|pcsx2"
  "dc|Dreamcast|flycast"
  "n3ds|3DS|azahar citra"
  "psp|PSP|ppsspp"
  "arcade|Arcade|fbneo mame2003_plus"
  "dos|MS-DOS|dosbox_pure dosbox_svn"
  "java|Java ME|squirreljme"
)

# ---------- 目标目录：优先便携模式 ----------
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

# ---------- 参数过滤（平台短名，可多个）----------
FILTERS=("$@")
want_platform() {
  [[ ${#FILTERS[@]} -eq 0 ]] && return 0
  local f
  for f in "${FILTERS[@]}"; do [[ "$1" == "$f" ]] && return 0; done
  return 1
}

echo "=========================================="
echo " NesStation PC 核心下载器"
echo " 来源：$BASE"
echo " 目录：$CORES_DIR"
echo "=========================================="

ok=0; skip=0; miss=0
for entry in "${PLATFORM_CORES[@]}"; do
  IFS='|' read -r key label cores <<<"$entry"
  want_platform "$key" || continue

  # 链上已有任一核心 → 跳过
  installed=""
  for core in $cores; do
    [[ -f "$CORES_DIR/${core}_libretro$EXT" ]] && installed="$core" && break
  done
  if [[ -n "$installed" ]]; then
    echo "  [跳过] $label（已安装 $installed）"
    skip=$((skip+1))
    continue
  fi

  # 依次尝试候选核心（404 → 下一个）
  got=""
  for core in $cores; do
    echo "  [下载] $label ← $core ..."
    if curl -fSL --retry 2 --retry-delay 2 --max-time 600 \
         -o "/tmp/${core}_libretro.zip" \
         "$BASE/${core}_libretro$EXT.zip"; then
      if command -v unzip >/dev/null 2>&1; then
        unzip -o -q "/tmp/${core}_libretro.zip" -d "$CORES_DIR"
      else
        python3 -c "import zipfile,sys; zipfile.ZipFile('/tmp/${core}_libretro.zip').extractall(sys.argv[1])" "$CORES_DIR" \
          || python3 -c "import zipfile,sys; zipfile.ZipFile('/tmp/${core}_libretro.zip').extractall('$CORES_DIR')"
      fi
      rm -f "/tmp/${core}_libretro.zip"
      if [[ -f "$CORES_DIR/${core}_libretro$EXT" ]]; then
        got="$core"
        break
      fi
    else
      rm -f "/tmp/${core}_libretro.zip"
      echo "         $core 在本架构不可用（404），尝试替代核心..."
    fi
  done

  if [[ -n "$got" ]]; then
    echo "         [完成] $label → $got"
    ok=$((ok+1))
  else
    echo "         [缺失] $label：本架构（$BASE）无任何候选核心"
    miss=$((miss+1))
  fi
done

echo "=========================================="
echo " 完成：$ok 新装，$skip 已有，$miss 平台无可用核心"
if [[ $miss -gt 0 ]]; then
  echo " 提示：buildbot 未提供该架构的预编译核心，"
  echo "       可自行编译对应 libretro 核心后放入 cores/ 目录。"
fi
echo " 提示：NGC/Wii、Wii U、Switch 为独立模拟器"
echo "       （Dolphin / Cemu / Eden），放 emulators/ 目录"
echo "       或到设置页配置路径。"
echo "=========================================="
exit $miss
