# ---------------------------------------------------------------------------
# NesStation PC 超级构建 —— 独立模拟器源码级集成
#
# 与 Android 原项目（:cemu-core / :eden-core 源码级集成）同构：
# 通过 CMake ExternalProject 拉取官方源码并从源码编译，产物安装到
#   <build>/bundle/emulators/<name>/
# 前端按「exe 同目录 emulators/ → 用户设置路径 → 常见安装位置」探测。
#
# 开关：
#   -DNESSTATION_SUPERBUILD=ON          启用超级构建（默认 OFF，仅编译前端）
#   -DNESSTATION_BUILD_DOLPHIN=OFF      跳过 NGC/Wii（Dolphin）
#   -DNESSTATION_BUILD_CEMU=OFF         跳过 Wii U（Cemu）
#   -DNESSTATION_BUILD_EDEN=OFF         跳过 Switch（Eden）
#
# 版本锚点（与 scripts/fetch_cores.* 的核心矩阵一并写进 README）：
#   Dolphin 2512   — NGC/Wii（Ishiiruka 原仓库已停止维护，PC 采用其上游
#                     Dolphin；原 Ishiiruka 的画质选项已大部分被上游吸收）
#   Cemu v2.6      — Wii U
#   Eden v0.2.1    — Switch（yuzu 社区延续分支，GitHub 官方镜像仓）
#
# 注意：三个项目都体量巨大（各需 30~90 分钟编译与相应系统依赖），
# Windows 建议配合 vcpkg（设置 VCPKG_ROOT），Linux 依赖清单见 README。
# 即便跳过超级构建，前端也能直接使用用户已安装的官方 PC 版。
# ---------------------------------------------------------------------------
include(ExternalProject)

set(NESSTATION_EMU_BUNDLE ${CMAKE_BINARY_DIR}/bundle/emulators)

# ---- Nintendo GameCube / Wii — Dolphin ------------------------------------
if(NESSTATION_BUILD_DOLPHIN)
  ExternalProject_Add(dolphin
    GIT_REPOSITORY https://github.com/dolphin-emu/dolphin.git
    GIT_TAG        ${NESSTATION_DOLPHIN_TAG}
    GIT_SHALLOW    TRUE
    PREFIX         ${CMAKE_BINARY_DIR}/third_party/dolphin
    CMAKE_ARGS
      -DCMAKE_BUILD_TYPE=Release
      -DCMAKE_INSTALL_PREFIX=${NESSTATION_EMU_BUNDLE}/dolphin
      -DCMAKE_POLICY_DEFAULT_CMP0092=NEW
      -DENABLE_TESTS=OFF
      -DUSE_SHARED=OFF
      $<$<BOOL:${MSVC}>:-DUSE_MSVC_PRECOMPILED_HEADERS=ON>
    INSTALL_COMMAND ${CMAKE_COMMAND} --build . --target install --config Release
    BUILD_ALWAYS   OFF)
  add_dependencies(nesstation dolphin)
endif()

# ---- Nintendo Wii U — Cemu --------------------------------------------------
if(NESSTATION_BUILD_CEMU)
  set(CEMU_TOOLCHAIN_ARGS "")
  if(DEFINED ENV{VCPKG_ROOT} AND EXISTS "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake")
    set(CEMU_TOOLCHAIN_ARGS -DCMAKE_TOOLCHAIN_FILE=$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake)
  endif()
  ExternalProject_Add(cemu
    GIT_REPOSITORY https://github.com/cemu-project/Cemu.git
    GIT_TAG        ${NESSTATION_CEMU_TAG}
    GIT_SHALLOW    TRUE
    GIT_SUBMODULES ""
    PREFIX         ${CMAKE_BINARY_DIR}/third_party/cemu
    CMAKE_ARGS
      -DCMAKE_BUILD_TYPE=Release
      -DCMAKE_INSTALL_PREFIX=${NESSTATION_EMU_BUNDLE}/cemu
      ${CEMU_TOOLCHAIN_ARGS}
    INSTALL_COMMAND ""
    BUILD_ALWAYS   OFF)
  # Cemu 上游无 install 规则：编译完成后拷贝产物（Cemu.exe / bin/Cemu）
  ExternalProject_Add_Step(cemu bundle
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            <BINARY_DIR>/bin ${NESSTATION_EMU_BUNDLE}/cemu
    DEPENDEES build
    ALWAYS 0)
  add_dependencies(nesstation cemu)
endif()

# ---- Nintendo Switch — Eden -------------------------------------------------
if(NESSTATION_BUILD_EDEN)
  ExternalProject_Add(eden
    GIT_REPOSITORY https://github.com/eden-emulator/mirror.git
    GIT_TAG        ${NESSTATION_EDEN_TAG}
    GIT_SHALLOW    TRUE
    GIT_SUBMODULES ""
    PREFIX         ${CMAKE_BINARY_DIR}/third_party/eden
    CMAKE_ARGS
      -DCMAKE_BUILD_TYPE=Release
      -DCMAKE_INSTALL_PREFIX=${NESSTATION_EMU_BUNDLE}/eden
      -DEDEN_ENABLE_LTO=OFF
    INSTALL_COMMAND ""
    BUILD_ALWAYS   OFF)
  ExternalProject_Add_Step(eden bundle
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            <BINARY_DIR>/bin ${NESSTATION_EMU_BUNDLE}/eden
    DEPENDEES build
    ALWAYS 0)
  add_dependencies(nesstation eden)
endif()
