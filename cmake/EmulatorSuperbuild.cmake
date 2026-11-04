# ---------------------------------------------------------------------------
# NesStation PC 超级构建 —— 独立模拟器「源码级集成」本地构建
#
# ★ 源码已完整拉入仓库 emulators/ 目录（含全部子模块 / CPM 依赖说明），
#   构建全程无需联网（Linux 下 Eden 走系统库方案；Windows 下 Cemu/Dolphin
#   依赖均随源码内置）。
#
# 产物安装到 <build>/bundle/emulators/<name>/，前端按
# 「exe 同目录 emulators/<id> → 用户设置路径 → 常见安装位置」自动探测。
#
# 开关：
#   -DNESSTATION_SUPERBUILD=ON     启用（源码存在时）
#   -DNESSTATION_BUILD_DOLPHIN=OFF 跳过 NGC/Wii（Dolphin 2609 源码内置）
#   -DNESSTATION_BUILD_CEMU=OFF    跳过 Wii U（Cemu v2.6 源码内置）
#   -DNESSTATION_BUILD_EDEN=OFF    跳过 Switch（Eden v0.2.1 源码内置）
#
# 版本锚点（emulators/ 内为以下版本的完整快照）：
#   Dolphin 2609   — NGC/Wii（Ishiiruka 原仓库已停止维护，PC 采用其上游
#                    Dolphin；原 Ishiiruka 的画质选项已大部分被上游吸收）
#   Cemu v2.6      — Wii U
#   Eden  v0.2.1   — Switch（yuzu 社区延续分支官方镜像）
#
# 依赖说明：
#   Dolphin（Linux）：cmake ≥3.16 + GCC/Clang；可选系统库由 CMake 自检
#   Cemu（Linux）：cmake ≥3.21 + GTK3/Wayland 开发包（发行版软件源安装）
#   Cemu（Windows）：VS2022 + vcpkg（emulators/cemu/dependencies/vcpkg 已内置）
#   Eden（Linux）：CPMUTIL_FORCE_SYSTEM=ON + 发行版开发包
#     （libboost*-dev libfmt-dev liblz4-dev nlohmann-json3-dev zlib1g-dev
#      libzstd-dev libopus-dev libssl-dev libquazip1-qt*-dev 等）
#   Eden（Windows）：首次配置需联网拉取 CPM 依赖（或运行
#     emulators/eden/tools/cpm-fetch-all.sh 预取后离线）
#
# 提示：三个项目体量巨大（各需 30~90 分钟编译）；不想本地编译的用户
# 仍可用「设置 → 核心设置 → 外置核心」的 GitHub 自动下载或手动选择
# 已安装的官方发行版。
# ---------------------------------------------------------------------------
include(ExternalProject)

set(NESSTATION_EMU_BUNDLE ${CMAKE_BINARY_DIR}/bundle/emulators)
set(NESSTATION_EMU_SRC ${CMAKE_SOURCE_DIR}/emulators)

# ---- Nintendo GameCube / Wii — Dolphin（源码内置，含 34 个 Externals）----
if(NESSTATION_BUILD_DOLPHIN AND EXISTS ${NESSTATION_EMU_SRC}/dolphin/CMakeLists.txt)
  ExternalProject_Add(dolphin
    SOURCE_DIR        ${NESSTATION_EMU_SRC}/dolphin
    INSTALL_DIR       ${NESSTATION_EMU_BUNDLE}/dolphin
    CMAKE_ARGS
      -DCMAKE_BUILD_TYPE=Release
      -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>
      -DCMAKE_POLICY_DEFAULT_CMP0092=NEW
      -DENABLE_TESTS=OFF
      -DENABLE_QT=ON
      -DUSE_SHARED=OFF
      $<$<BOOL:${MSVC}>:-DUSE_MSVC_PRECOMPILED_HEADERS=ON>
    BUILD_ALWAYS   OFF)
  add_dependencies(nesstation dolphin)
  message(STATUS "Superbuild: Dolphin 2609（NGC/Wii）← emulators/dolphin 本地源码")
endif()

# ---- Nintendo Wii U — Cemu（源码内置，含 5 个 dependencies 子模块）----
if(NESSTATION_BUILD_CEMU AND EXISTS ${NESSTATION_EMU_SRC}/cemu/CMakeLists.txt)
  set(CEMU_EXTRA_ARGS "")
  if(WIN32)
    # vcpkg 已随源码内置（emulators/cemu/dependencies/vcpkg）
    set(CEMU_EXTRA_ARGS
      -DCMAKE_TOOLCHAIN_FILE=${NESSTATION_EMU_SRC}/cemu/dependencies/vcpkg/scripts/buildsystems/vcpkg.cmake)
  endif()
  ExternalProject_Add(cemu
    SOURCE_DIR        ${NESSTATION_EMU_SRC}/cemu
    INSTALL_DIR       ${NESSTATION_EMU_BUNDLE}/cemu
    CMAKE_ARGS
      -DCMAKE_BUILD_TYPE=Release
      -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>
      ${CEMU_EXTRA_ARGS}
    BUILD_COMMAND ${CMAKE_COMMAND} --build . --config Release
    INSTALL_COMMAND ""
    BUILD_ALWAYS   OFF)
  # Cemu 上游无 install 规则：编译完成后拷贝产物（Cemu.exe / bin/Cemu）
  ExternalProject_Add_Step(cemu bundle
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            <BINARY_DIR>/bin ${NESSTATION_EMU_BUNDLE}/cemu
    DEPENDEES build
    ALWAYS 0)
  add_dependencies(nesstation cemu)
  message(STATUS "Superbuild: Cemu v2.6（Wii U）← emulators/cemu 本地源码")
endif()

# ---- Nintendo Switch — Eden（源码内置镜像快照）----
if(NESSTATION_BUILD_EDEN AND EXISTS ${NESSTATION_EMU_SRC}/eden/CMakeLists.txt)
  set(EDEN_EXTRA_ARGS "")
  if(UNIX AND NOT APPLE)
    # Linux：强制使用系统库（离线构建；依赖清单见文件头注释）
    set(EDEN_EXTRA_ARGS -DCPMUTIL_FORCE_SYSTEM=ON)
  endif()
  ExternalProject_Add(eden
    SOURCE_DIR        ${NESSTATION_EMU_SRC}/eden
    INSTALL_DIR       ${NESSTATION_EMU_BUNDLE}/eden
    CMAKE_ARGS
      -DCMAKE_BUILD_TYPE=Release
      -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>
      -DEDEN_ENABLE_LTO=OFF
      ${EDEN_EXTRA_ARGS}
    BUILD_COMMAND ${CMAKE_COMMAND} --build . --config Release
    INSTALL_COMMAND ""
    BUILD_ALWAYS   OFF)
  ExternalProject_Add_Step(eden bundle
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            <BINARY_DIR>/bin ${NESSTATION_EMU_BUNDLE}/eden
    DEPENDEES build
    ALWAYS 0)
  add_dependencies(nesstation eden)
  message(STATUS "Superbuild: Eden v0.2.1（Switch）← emulators/eden 本地源码")
endif()
