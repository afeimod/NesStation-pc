#!/usr/bin/env bash
# ============================================================================
# NesStation PC — 源码构建脚本（Linux）
#
# 依赖：git、cmake ≥ 3.16、C++17 编译器、SDL2 开发库（缺失时自动 FetchContent）
#
# 用法：
#   ./scripts/build.sh                       # 前端（自动获取 SDL2）
#   ./scripts/build.sh --system-sdl          # 用系统 SDL2
#   ./scripts/build.sh --superbuild          # 前端 + Dolphin/Cemu/Eden 源码编译
#   ./scripts/build.sh --no-dolphin --no-cemu --no-eden  # 超级构建子集
# ============================================================================
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(dirname "$SCRIPT_DIR")"
BUILD="$ROOT/build"

ARGS=()
SUPERBUILD=OFF
for arg in "$@"; do
  case "$arg" in
    --superbuild)   SUPERBUILD=ON ;;
    --no-dolphin)   ARGS+=(-DNESSTATION_BUILD_DOLPHIN=OFF) ;;
    --no-cemu)      ARGS+=(-DNESSTATION_BUILD_CEMU=OFF) ;;
    --no-eden)      ARGS+=(-DNESSTATION_BUILD_EDEN=OFF) ;;
    --system-sdl)   ARGS+=(-DNESSTATION_USE_SYSTEM_SDL2=ON) ;;
    *)              ARGS+=("$arg") ;;
  esac
done

echo "==> 配置（superbuild=$SUPERBUILD）"
cmake -B "$BUILD" -S "$ROOT" -DCMAKE_BUILD_TYPE=Release \
      -DNESSTATION_SUPERBUILD="$SUPERBUILD" "${ARGS[@]}"

echo "==> 编译（$(nproc) 线程）"
cmake --build "$BUILD" -j"$(nproc)"

echo ""
echo "==> 完成：$BUILD/bin/nesstation"
echo "    下一步：./scripts/fetch_cores.sh 下载核心"
