#!/bin/bash
# ============================================================================
# NesStation PC — 三大独立模拟器本地源码构建（Linux / macOS / Windows-GitBash）
#
# 源码已完整内置在仓库 emulators/ 目录（无需联网下载模拟器本体）：
#   emulators/dolphin  — Dolphin 2609（NGC/Wii，34 个 Externals 全内置）
#   emulators/cemu     — Cemu v2.6（Wii U，dependencies/vcpkg 全内置）
#   emulators/eden     — Eden v0.2.1（Switch 镜像快照，CPM 拉依赖需联网）
#
# 用法：
#   scripts/build_emulators.sh [dolphin|cemu|eden|all] [构建线程数]
# 环境变量：
#   EDEN_FORCE_SYSTEM=ON/OFF  Eden 用系统库(ON,离线)或 CPM 拉上游锁定版本(OFF,默认)
#   QT_DIR=<Qt6 目录>         Windows 预装 Qt 时传入 → Eden 跳过 CPM 下载 Qt
#                             （例：C:/Qt/6.8.3/msvc2022_64/lib/cmake/Qt6）
# 产物：dist/emulators/<名称>/（把整个 dist/emulators 拷到 nesstation 可执行
# 文件旁即可被前端自动识别；或「设置 → 核心设置 → 外置核心 → 手动选择」）
#
# ★v1.2.2 修正（对照上游官方 CI 核实）：
#   1. Cemu 产物在【源码根 emulators/cemu/bin/Cemu_release(.exe)】而非
#      build/cemu/bin —— 旧脚本拷贝空目录导致状态永远 failed
#   2. Windows（VS 多配置生成器）build/install 必须显式 --config Release，
#      否则默认 Debug：安装目录为空
#   3. Dolphin Windows 构建后用 windeployqt 部署 Qt DLL（PATH 中有 Qt 时）
#
# 系统依赖（一次安装，全部离线）：
#   基础：build-essential cmake ninja-build pkg-config git curl ca-certificates unzip python3
#   Dolphin：libegl1-mesa-dev libevdev-dev libudev-dev libx11-dev libxi-dev
#            libxrandr-dev libpulse-dev libasound2-dev libbluetooth-dev
#            libcurl4-openssl-dev qt6-base-dev qt6-tools-dev
#            qt6-svg-dev（Debian 12 名称；Ubuntu 22.04 为 libqt6svg6-dev）
#   Cemu：内置 vcpkg（首次构建需联网拉基线历史）；另需
#         clang-15（上游 Linux CI 指定编译器）ninja-build nasm freeglut3-dev
#         libgcrypt20-dev libglm-dev libgtk-3-dev libpulse-dev libsecret-1-dev
#         libsystemd-dev libudev-dev libbluetooth-dev
#   Eden：qt6-base-dev qt6-charts-dev libboost-all-dev libfmt-dev liblz4-dev
#         nlohmann-json3-dev zlib1g-dev libzstd-dev libopus-dev libssl-dev
#         libgl1-mesa-dev libglvnd-dev libasound2-dev libpulse-dev libusb-1.0-0-dev
#         libvulkan-dev libavcodec-dev libavutil-dev libswscale-dev
#         libavformat-dev libavfilter-dev
# ============================================================================
set -e
cd "$(dirname "$0")/.."
TARGET=${1:-all}
JOBS=${2:-$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)}
OUT="$(pwd)/dist/emulators"
mkdir -p "$OUT"

WINDOWS=0
case "$(uname -s)" in
  MINGW*|MSYS*|CYGWIN*) WINDOWS=1 ;;
esac

echo "★ NesStation 模拟器本地构建（目标：$TARGET，线程：$JOBS，平台：$(uname -s)）"
echo "★ 产物目录：$OUT"

# VS 多配置生成器：需要 --config Release；单配置（Ninja/Make）忽略多余参数安全
build_ok() { # $1=构建目录
  if [ "$WINDOWS" = "1" ]; then
    cmake --build "$1" --config Release -j "$JOBS"
  else
    cmake --build "$1" -j "$JOBS"
  fi
}
install_ok() { # $1=构建目录
  if [ "$WINDOWS" = "1" ]; then
    cmake --install "$1" --config Release
  else
    cmake --install "$1"
  fi
}

build_dolphin() {
  echo "=== Dolphin（NGC/Wii）==="
  # ccache 仅 Linux/macOS（CI 已安装并预热缓存；MSVC 无 ccache）
  local CCACHE_ARGS=""
  if [ "$WINDOWS" != "1" ] && command -v ccache >/dev/null 2>&1; then
    CCACHE_ARGS="-DENABLE_CCACHE=ON"
    echo "（ccache 已启用：$(ccache -s 2>/dev/null | head -1 || true)）"
  fi
  # ★v1.2.6：Windows + 预装 Qt（install-qt-action / 本地 Qt）时显式传 Qt6_DIR。
  #   Dolphin 的 Source/Core/DolphinQt/CMakeLists.txt 在 WIN32 下会把仓库内的
  #   Externals/Qt/Qt6.8.3/x64 追加进 CMAKE_PREFIX_PATH —— 该内置 Qt 快照的
  #   二进制（bin/*.exe、*.dll）不随仓库分发（.gitignore 历史上排除过二进制），
  #   CMake 找到它的 Qt6Config 后会在 Qt6CoreTools 的 syncqt.exe 等工具目标上
  #   FATAL（run 42019801015 EMU dolphin · windows-x64 的死因）。
  #   显式 -DQt6_DIR 直接命中 install-qt-action 装好的完整 Qt，优先级高于
  #   任何前缀搜索（与 build_eden 的处理一致）。
  local QT_ARGS=""
  if [ "$WINDOWS" = "1" ] && [ -n "$QT_DIR" ]; then
    echo "（Dolphin 使用预装 Qt：$QT_DIR）"
    QT_ARGS="-DQt6_DIR=$QT_DIR"
  fi
  # ★v1.2.8：windows-2025-vs2026 镜像的 CMake 4.x 移除了对
  #   cmake_minimum_required < 3.5 的兼容，而 Dolphin 内置的 enet
  #   （2.8.12...3.20）/ hidapi（3.1.3...3.25）等 Externals 仍是老写法，
  #   CMake 4 会在配置期直接 FATAL。CMAKE_POLICY_VERSION_MINIMUM=3.5 是
  #   CMake 4 官方逃生口（对 CMake 3 是无害的未知缓存变量）。
  local POLICY_ARGS=""
  if command -v cmake >/dev/null 2>&1; then
    CMAKE_MAJOR=$(cmake --version | head -1 | sed 's/[^0-9]*\([0-9]*\).*/\1/')
    if [ "${CMAKE_MAJOR:-0}" -ge 4 ] 2>/dev/null; then
      POLICY_ARGS="-DCMAKE_POLICY_VERSION_MINIMUM=3.5"
      echo "（CMake ≥ 4：附加 CMAKE_POLICY_VERSION_MINIMUM=3.5，兼容 Externals 老式最低版本声明）"
    fi
  fi
  cmake -S emulators/dolphin -B build/dolphin \
    -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$OUT/dolphin" \
    -DENABLE_TESTS=OFF $CCACHE_ARGS $QT_ARGS $POLICY_ARGS
  build_ok build/dolphin
  install_ok build/dolphin
  # Windows：windeployqt 部署 Qt DLL（PATH 有 Qt 时；CI 的 install-qt-action 已加）
  if [ "$WINDOWS" = "1" ] && command -v windeployqt >/dev/null 2>&1; then
    EXE=$(find "$OUT/dolphin" -name 'Dolphin.exe' | head -1 || true)
    if [ -n "$EXE" ]; then
      echo "（windeployqt 部署 Qt 运行库 → $(dirname "$EXE")）"
      windeployqt --release --no-translations --no-system-d3d-compiler \
        --no-opengl-sw "$EXE" || echo "::warning::windeployqt 失败，请手动部署 Qt DLL"
    fi
  fi
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
    git -C "$VCPKG_DIR" remote add origin https://github.com/microsoft/vcpkg.git 2>/dev/null || true
    git -C "$VCPKG_DIR" fetch -q --depth 1 origin "$BASELINE"
    git -C "$VCPKG_DIR" reset -q --hard FETCH_HEAD
  fi
  if [ ! -x "$VCPKG_DIR/vcpkg" ] && [ ! -f "$VCPKG_DIR/vcpkg.exe" ]; then
    if [ "$WINDOWS" = "1" ]; then
      ( cd "$VCPKG_DIR" && cmd //c bootstrap-vcpkg.bat )
    else
      bash "$VCPKG_DIR/bootstrap-vcpkg.sh"
    fi
  fi
  # 工具链由 Cemu CMakeLists 自动指向内置 vcpkg（ENABLE_VCPKG=ON 默认），
  # 无需手动传 CMAKE_TOOLCHAIN_FILE（对齐上游 CI）。
  # 编译器经 -D 显式传给 Cemu 本体（上游 CI 同款）；不用 CC/CXX 环境变量，
  # 避免泄漏进 vcpkg 端口构建（端口按 triplet 用自己的默认工具链）
  local COMPILER_ARGS=""
  if [ -n "$CEMU_CC" ] && [ -n "$CEMU_CXX" ]; then
    COMPILER_ARGS="-DCMAKE_C_COMPILER=$CEMU_CC -DCMAKE_CXX_COMPILER=$CEMU_CXX"
  fi
  cmake -S emulators/cemu -B build/cemu -DCMAKE_BUILD_TYPE=Release \
    -DVCPKG_INSTALL_OPTIONS="--clean-after-build" $COMPILER_ARGS
  build_ok build/cemu
  # ★产物在源码根 bin/（上游 CI：mv bin/Cemu_release bin/Cemu），
  #   不是 build/cemu/bin —— 旧脚本从错误目录拷贝导致产物恒为空
  mkdir -p "$OUT/cemu"
  if [ -f emulators/cemu/bin/Cemu_release.exe ]; then
    cp -r emulators/cemu/bin/. "$OUT/cemu/"
    mv "$OUT/cemu/Cemu_release.exe" "$OUT/cemu/Cemu.exe"
  elif [ -f emulators/cemu/bin/Cemu_release ]; then
    cp -r emulators/cemu/bin/. "$OUT/cemu/"
    mv "$OUT/cemu/Cemu_release" "$OUT/cemu/Cemu"
  else
    echo "!! Cemu 构建完成但未找到 emulators/cemu/bin/Cemu_release(.exe)" >&2
    find emulators/cemu/bin build/cemu -maxdepth 3 -type f 2>/dev/null | head -20 || true
    exit 1
  fi
  # vcpkg x64-linux triplet 为动态链接时，把依赖 .so 一并拷到 Cemu 旁边
  find build/cemu/vcpkg_installed -name '*.so*' -type f \
    -exec cp {} "$OUT/cemu/" \; 2>/dev/null || true
  # ★v1.2.4：便携化 rpath —— vcpkg 动态库默认带构建机绝对路径的 RPATH，
  #   拷到用户机器上会找不到库；统一改 $ORIGIN（与 exe 同目录即加载）
  if [ "$WINDOWS" != "1" ] && command -v patchelf >/dev/null 2>&1; then
    for f in "$OUT/cemu/Cemu" "$OUT/cemu"/*.so*; do
      [ -f "$f" ] || continue
      patchelf --set-rpath '$ORIGIN' "$f" 2>/dev/null || true
    done
    echo "（rpath 已统一为 \$ORIGIN）"
  fi
  echo "✓ Cemu → $OUT/cemu"
}

build_eden() {
  echo "=== Eden（Switch）==="
  # EDEN_FORCE_SYSTEM=OFF：CPM 联网拉上游锁定版本依赖（默认，CI 推荐）
  # ON：全部用发行版系统库离线构建
  EDEN_FORCE_SYSTEM="${EDEN_FORCE_SYSTEM:-OFF}"
  echo "（CPMUTIL_FORCE_SYSTEM=$EDEN_FORCE_SYSTEM）"
  local EXTRA_ARGS=""
  # Windows + 预装 Qt（install-qt-action / 本地 Qt）：走系统 Qt，
  # 跳过 CPM 下载 ~1GB 的内置 Qt（YUZU_USE_BUNDLED_QT 在 MSVC 下默认 ON）
  if [ "$WINDOWS" = "1" ] && [ -n "$QT_DIR" ]; then
    echo "（使用预装 Qt：$QT_DIR）"
    EXTRA_ARGS="-DYUZU_USE_BUNDLED_QT=OFF -DQt6_DIR=$QT_DIR"
  fi
  cmake -S emulators/eden -B build/eden \
    -DCMAKE_BUILD_TYPE=Release -DCPMUTIL_FORCE_SYSTEM="$EDEN_FORCE_SYSTEM" $EXTRA_ARGS
  build_ok build/eden
  mkdir -p "$OUT/eden"
  # 单配置生成器产物在 build/eden/bin/；VS 多配置在 build/eden/bin/Release/
  if [ -d build/eden/bin/Release ]; then
    cp -r build/eden/bin/Release/. "$OUT/eden/"
  elif [ -d build/eden/bin ]; then
    cp -r build/eden/bin/. "$OUT/eden/" 2>/dev/null || true
  fi
  # Windows：windeployqt（Charts/Widgets DLL + 插件）
  if [ "$WINDOWS" = "1" ] && command -v windeployqt >/dev/null 2>&1; then
    EXE=$(find "$OUT/eden" -name 'eden.exe' -o -name 'Eden.exe' 2>/dev/null | head -1 || true)
    if [ -n "$EXE" ]; then
      echo "（windeployqt 部署 Qt 运行库 → $(dirname "$EXE")）"
      windeployqt --release --no-translations --no-system-d3d-compiler \
        --no-opengl-sw "$EXE" || echo "::warning::windeployqt 失败"
    fi
  fi
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
