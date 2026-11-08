cmake_minimum_required(VERSION 3.16)

project(NesStation VERSION 1.3.2 LANGUAGES C CXX
        DESCRIPTION "NesStation PC - Multi-platform retro game station for Windows/Linux")

# ---------------------------------------------------------------------------
# Options
# ---------------------------------------------------------------------------
option(NESSTATION_USE_SYSTEM_SDL2 "Use system-installed SDL2 instead of FetchContent" AUTO)
# 构建完成后自动下载 libretro 官方预编译核心到 build/bin/cores/（exe 同目录，
# CoreManager 自动识别），源码构建开箱即玩。离线/CI 环境传 OFF。
option(NESSTATION_FETCH_CORES     "Download libretro cores next to the built binary (needs network)" ON)
# ★ 源码级集成：emulators/ 目录已内置三大模拟器完整源码（含全部子模块），
#   超级构建全程无联网；也可用 scripts/build_emulators.sh(.ps1) 独立构建
option(NESSTATION_SUPERBUILD      "Build standalone emulators (Dolphin/Cemu/Eden) from the vendored source in emulators/" OFF)
option(NESSTATION_BUILD_DOLPHIN   "Superbuild: Nintendo GameCube / Wii (Dolphin 2609, vendored)" ON)
option(NESSTATION_BUILD_CEMU      "Superbuild: Nintendo Wii U (Cemu v2.6, vendored)" ON)
option(NESSTATION_BUILD_EDEN      "Superbuild: Nintendo Switch (Eden v0.2.1, vendored)" ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

if(MSVC)
  add_compile_options(/utf-8 /W3)
  add_definitions(-D_CRT_SECURE_NO_WARNINGS)
else()
  add_compile_options(-Wall -Wextra -Wno-unused-parameter)
endif()

# ---------------------------------------------------------------------------
# Dependencies (SDL2). Vendored stb / nlohmann-json / libretro.h need nothing.
# ---------------------------------------------------------------------------
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/Dependencies.cmake)

# ---------------------------------------------------------------------------
# NesStation frontend
# ---------------------------------------------------------------------------
file(GLOB_RECURSE NESSTATION_SOURCES CONFIGURE_DEPENDS
     ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)

add_executable(nesstation ${NESSTATION_SOURCES}
               ${CMAKE_CURRENT_SOURCE_DIR}/assets/icon.rc)

if(MSVC)
  # rc.exe 以构建目录为工作目录解析 .rc 里的相对路径；显式把 assets/ 加入
  # RC 搜索路径，确保 icon.rc 能找到同目录的 icon.ico（RC2135 兜底）
  set_source_files_properties(${CMAKE_CURRENT_SOURCE_DIR}/assets/icon.rc PROPERTIES
    COMPILE_FLAGS "/I\"${CMAKE_CURRENT_SOURCE_DIR}/assets\"")
endif()

set_target_properties(nesstation PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# ★版本号注入（主页徽章 / 窗口标题 / 关于页 —— 供用户确认所跑构建）
target_compile_definitions(nesstation PRIVATE NESSTATION_VERSION="${PROJECT_VERSION}")

if(MSVC)
  # Release: GUI subsystem; Debug keeps console for log output.
  set_target_properties(nesstation PROPERTIES WIN32_EXECUTABLE $<CONFIG:Release>)
endif()

target_include_directories(nesstation PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/src
  ${CMAKE_CURRENT_SOURCE_DIR}/third_party
  ${CMAKE_CURRENT_SOURCE_DIR}/third_party/stb
  ${CMAKE_CURRENT_SOURCE_DIR}/third_party/libretro)

if(UNIX AND NOT APPLE)
  # dlopen / dlclose for libretro cores
  target_link_libraries(nesstation PRIVATE ${CMAKE_DL_LIBS})
  find_package(Threads REQUIRED)
  target_link_libraries(nesstation PRIVATE Threads::Threads)
endif()

if(MSVC)
  # ★v1.2.9 FSD 顶/底栏（FsdChrome.cpp）：winsock 局域网 IP 探测
  target_link_libraries(nesstation PRIVATE ws2_32)
endif()

if(WIN32)
  # ★v1.3.0 系统原生文件对话框（util/NativeDialogs.cpp）：COM IFileDialog
  #   + SHGetFolderPathW（util/Fs.cpp 中文用户目录）
  target_link_libraries(nesstation PRIVATE ole32 shell32)
endif()

if(TARGET SDL2::SDL2main)
  target_link_libraries(nesstation PRIVATE SDL2::SDL2main)
endif()
target_link_libraries(nesstation PRIVATE SDL2::SDL2)

# stb 单头库实现聚合单元
target_sources(nesstation PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/third_party/stb/stb_impl.cpp)

# ★v1.2.8：HQX 放大滤镜库（Maxim Stepin 原版，APK core/jni/hqx 同源移植；
#   C 编译，随主目标静态链接）
target_sources(nesstation PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/third_party/hqx/hq2x.c
  ${CMAKE_CURRENT_SOURCE_DIR}/third_party/hqx/hq4x.c
  ${CMAKE_CURRENT_SOURCE_DIR}/third_party/hqx/init.c)
target_include_directories(nesstation PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/third_party/hqx)

# ---------------------------------------------------------------------------
# 核心自动下载（默认开）：构建完成后从 libretro 官方 buildbot 下载当前架构
# 全部可用核心到 $<TARGET_FILE_DIR>/cores（即 build/bin/cores，exe 同目录，
# CoreManager 的 bundledCoreDir 自动识别）。增量构建时脚本跳过已下载平台，
# 只做存在性检查，不会重复下载。核心缺失/断网不会使构建失败（脚本软退出）。
# ---------------------------------------------------------------------------
if(NESSTATION_FETCH_CORES)
  set(NESSTATION_CORES_DEST "$<TARGET_FILE_DIR:nesstation>/cores")
  if(WIN32)
    add_custom_command(TARGET nesstation POST_BUILD
      COMMAND powershell -NoProfile -ExecutionPolicy Bypass
              -File ${CMAKE_CURRENT_SOURCE_DIR}/scripts/fetch_cores.ps1
              -Dest "${NESSTATION_CORES_DEST}"
      COMMENT "Downloading libretro cores (buildbot) -> bin/cores/"
      VERBATIM)
  else()
    add_custom_command(TARGET nesstation POST_BUILD
      COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/scripts/fetch_cores.sh
              --dest "${NESSTATION_CORES_DEST}"
      COMMENT "Downloading libretro cores (buildbot) -> bin/cores/"
      VERBATIM)
  endif()
endif()

# Embed resources: app icon

# ---------------------------------------------------------------------------
# Install / package layout (portable)
# ★v1.2.4：不再随包附带字体（用户可选放 exe 旁 assets/fonts/ 自备），
#   运行时自动选系统字体（App::initFontChain）
# ---------------------------------------------------------------------------
if(WIN32)
  set(NESSTATION_EXE_DIR ${CMAKE_BINARY_DIR}/bin)
  install(TARGETS nesstation RUNTIME DESTINATION .)
else()
  install(TARGETS nesstation RUNTIME DESTINATION bin)
endif()

# ---------------------------------------------------------------------------
# Optional superbuild: standalone emulators from source (like the Android
# project's :cemu-core / :eden-core source-level integration).
# ---------------------------------------------------------------------------
if(NESSTATION_SUPERBUILD)
  include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/EmulatorSuperbuild.cmake)
endif()
