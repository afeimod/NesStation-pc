#!/bin/bash
# ============================================================================
# NesStation PC — 三大独立模拟器本地源码构建（Linux / macOS）
#
# 源码已完整内置在仓库 emulators/ 目录（无需联网）：
#   emulators/dolphin  — Dolphin 2609（NGC/Wii，34 个 Externals 全内置）
#   emulators/cemu     — Cemu v2.6（Wii U，dependencies 全内置）
#   emulators/eden     — Eden v0.2.1（Switch 镜像快照）
#
# 用法：
#   scripts/build_emulators.sh [dolphin|cemu|eden|all] [构建线程数]
# 产物：dist/emulators/<名称>/（把整个 dist/emulators 拷到 nesstation 可执行
# 文件旁即可被前端自动识别；或「设置 → 核心设置 → 外置核心 → 手动选择」）
#
# 系统依赖（一次安装，全部离线）：
#   基础：build-essential cmake ninja-build pkg-config git
#   Dolphin：libegl1-mesa-dev libevdev-dev libudev-dev libx11-dev
#            libxi-dev libxrandr-dev libpulse-dev libasound2-dev
#            qt6-base-dev qt6-tools-dev（或 qtbase5-dev）
#   Cemu：内置 vcpkg（首次构建需联网拉基线历史）；另需
#         libgtk-3-dev libwebkit2gtk-4.1-dev libsecret-1-dev
#         libgcrypt20-dev libsystemd-dev libpulse-dev
#   Eden：libboost-all-dev libfmt-dev liblz4-dev nlohmann-json3-dev
#         zlib1g-dev libzstd-dev libopus-dev libssl-dev
#         libquazip1-qt6-dev（或 qt5 版）libgl1-mesa-dev
# ============================================================================
set -e
cd "$(dirname "$0")/.."
TARGET=${1:-all}
JOBS=${2:-$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)}
OUT="$(pwd)/dist/emulators"
mkdir -p "$OUT"

echo "★ NesStation 模拟器本地构建（目标：$TARGET，线程：$JOBS）"
echo "★ 产物目录：$OUT"

build_dolphin() {
  echo "=== Dolphin（NGC/Wii）==="
  cmake -S emulators/dolphin -B build/dolphin \
    -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$OUT/dolphin" \
    -DENABLE_TESTS=OFF
  cmake --build build/dolphin -j"$JOBS"
  cmake --install build/dolphin
  echo "✓ Dolphin → $OUT/dolphin"
}

build_cemu() {
  echo "=== Cemu（Wii U，内置 vcpkg）==="
  # vcpkg 快照不带 .git：manifest 模式解析 builtin-baseline 必须有 git 历史。
  # 一次性修复：init + 拉取基线提交并硬对齐（需联网；基线即快照所在提交，
  # 树内容不变，只是补上历史）。vcpkg 二进制缺失时同时 bootstrap。
  local VCPKG_DIR="$PWD/emulators/cemu/dependencies/vcpkg"
  local BASELINE
  BASELINE=$(sed -n 's/.*"builtin-baseline"[[:space:]]*:[[:space:]]*"\([0-9a-f]\{40\}\)".*/\1/p' \
              emulators/cemu/vcpkg.json)
  if [ ! -d "$VCPKG_DIR/.git" ] && [ -n "$BASELINE" ]; then
    echo "（vcpkg 快照无 .git：拉取基线 $BASELINE）"
    git -C "$VCPKG_DIR" init -q
    git -C "$VCPKG_DIR" remote add origin https://github.com/microsoft/vcpkg.git
    git -C "$VCPKG_DIR" fetch -q --depth 1 origin "$BASELINE"
    git -C "$VCPKG_DIR" reset -q --hard FETCH_HEAD
  fi
  if [ ! -x "$VCPKG_DIR/vcpkg" ] && [ ! -x "$VCPKG_DIR/vcpkg.exe" ]; then
    bash "$VCPKG_DIR/bootstrap-vcpkg.sh"
  fi
  cmake -S emulators/cemu -B build/cemu -DCMAKE_BUILD_TYPE=Release \
    -DVCPKG_INSTALL_OPTIONS="--clean-after-build"
  cmake --build build/cemu -j"$JOBS"
  mkdir -p "$OUT/cemu"
  cp -r build/cemu/bin/* "$OUT/cemu/"
  echo "✓ Cemu → $OUT/cemu"
}

build_eden() {
  echo "=== Eden（Switch）==="
  # EDEN_FORCE_SYSTEM=ON：用发行版系统库离线构建（默认，适配无网环境）；
  # OFF：由 CPM 在配置期联网拉取上游锁定版本的依赖（CI 推荐，依赖更精确）
  EDEN_FORCE_SYSTEM="${EDEN_FORCE_SYSTEM:-ON}"
  echo "（CPMUTIL_FORCE_SYSTEM=$EDEN_FORCE_SYSTEM）"
  cmake -S emulators/eden -B build/eden \
    -DCMAKE_BUILD_TYPE=Release -DCPMUTIL_FORCE_SYSTEM="$EDEN_FORCE_SYSTEM" \
    -DCMAKE_INSTALL_PREFIX="$OUT/eden"
  cmake --build build/eden -j"$JOBS"
  mkdir -p "$OUT/eden"
  cp -r build/eden/bin/* "$OUT/eden/" 2>/dev/null || true
  echo "✓ Eden → $OUT/eden"
}

case "$TARGET" in
  dolphin) build_dolphin ;;
  cemu)    build_cemu ;;
  eden)    build_eden ;;
  all|*)
    build_dolphin
    build_cemu
    build_eden
    ;;
esac

echo ""
echo "全部完成。把 dist/emulators/ 整个目录复制到 nesstation 可执行文件旁："
echo "  NesStation/"
echo "  ├── nesstation"
echo "  └── emulators/"
echo "      ├── dolphin/   (Dolphin.exe / dolphin-emu)"
echo "      ├── cemu/      (Cemu.exe / Cemu)"
echo "      └── eden/      (eden / Eden.exe)"
echo "前端启动后自动识别（设置 → 核心设置 → 外置核心 应显示「✓ 已就绪」）。"
