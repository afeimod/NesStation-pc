# ============================================================================
# Qt6OpenGLConfig.cmake —— Debian 12 (bookworm) Qt6OpenGL 模块 CMake 配置兜底
#
# 背景（本次 eden · linux-x64 / linux-arm64 两个 job 的死因）：
#   旧工作流拷贝本目录两个文件补 Qt6OpenGL 的 CMake 配置，但仓库里只提交了
#   Qt6OpenGLConfigVersion.cmake —— Qt6OpenGLConfig.cmake 从未入库，
#   `cp: cannot stat` 使 job 在装依赖后 1 秒退出（exit 1）。
#
# ★v1.2.7 正确修法（两级）：
#   1) 首选官方包：apt 安装 libqt6opengl6-dev —— 该包自带完整的
#      /usr/lib/<arch>/cmake/Qt6OpenGL/（Config/ConfigVersion/Dependencies/
#      Targets 全套，已核实 bookworm 文件清单），工作流检测到官方配置存在时
#      不再安装本 shim；
#   2) 兜底：本文件在官方配置缺失/异常时提供最小可用的 Qt6::OpenGL 导入目标
#      （qt6-charts-dev 的依赖链 find_dependency(Qt6OpenGL 6.4.2) 只需要
#      「找到包 + Qt6::OpenGL 目标可链接」）。
#
# 实现要点：
#   · 相对定位：本 shim 被安装到 /usr/lib/<arch>/cmake/Qt6OpenGL/，
#     由本文件所在目录向上推导多架构库目录与头文件目录，
#     x86_64-linux-gnu / aarch64-linux-gnu 双架构通用；
#   · 不依赖 Qt 内部 CMake 函数（qt_internal_* / QtPublicDependencyHelpers），
#     与宿主任意版本的 Qt6 配置共存无冲突；
#   · 库与头文件定位失败时按 find_dependency 语义置 NOT FOUND 并报错。
# ============================================================================

include(CMakeFindDependencyMacro)

# —— 依赖：QtOpenGL 公开链接 Core/Gui/Widgets（系统已有官方配置）——
find_dependency(Qt6Core 6.4.2)
find_dependency(Qt6Gui 6.4.2)
find_dependency(Qt6Widgets 6.4.2)

set(Qt6OpenGL_VERSION "6.4.2")
set(Qt6OpenGL_FOUND TRUE)

# —— 从本文件位置推导多架构路径（…/usr/lib/<arch>/cmake/Qt6OpenGL）——
get_filename_component(_QT6OPENGL_CMAKE_DIR "${CMAKE_CURRENT_LIST_DIR}" DIRECTORY)  # …/cmake
get_filename_component(_QT6OPENGL_LIBDIR "${_QT6OPENGL_CMAKE_DIR}" DIRECTORY)       # …/lib/<arch>
get_filename_component(_QT6OPENGL_ARCH "${_QT6OPENGL_LIBDIR}" NAME)                 # <arch>-linux-gnu

# —— 定位库文件（精确版本 → 开发符号链接 → find_library 全盘兜底）——
set(_QT6OPENGL_LIB "${_QT6OPENGL_LIBDIR}/libQt6OpenGL.so.6.4.2")
if (NOT EXISTS "${_QT6OPENGL_LIB}")
  find_library(_QT6OPENGL_LIB
    NAMES Qt6OpenGL Qt6OpenGL.so.6 Qt6OpenGL.so.6.4.2
    HINTS "${_QT6OPENGL_LIBDIR}"
    NO_DEFAULT_PATH)
  find_library(_QT6OPENGL_LIB NAMES Qt6OpenGL)
endif()

# —— 定位头文件目录（Debian 布局 → find_path 兜底）——
set(_QT6OPENGL_INCLUDE_DIR "/usr/include/${_QT6OPENGL_ARCH}/qt6/QtOpenGL")
if (NOT EXISTS "${_QT6OPENGL_INCLUDE_DIR}")
  find_path(_QT6OPENGL_INCLUDE_DIR
    NAMES qopenglversionfunctionsfactory.h
    HINTS "/usr/include/${_QT6OPENGL_ARCH}/qt6/QtOpenGL"
    PATH_SUFFIXES QtOpenGL qt6/QtOpenGL)
endif()

if (NOT _QT6OPENGL_LIB OR NOT _QT6OPENGL_INCLUDE_DIR)
  set(Qt6OpenGL_FOUND FALSE)
  if (Qt6OpenGL_FIND_REQUIRED)
    message(FATAL_ERROR
      "Qt6OpenGL shim: 未定位到 Qt6OpenGL 库或头文件"
      "（lib=${_QT6OPENGL_LIB}, include=${_QT6OPENGL_INCLUDE_DIR}）。"
      "请安装 libqt6opengl6-dev（Debian 12 官方提供完整 Qt6OpenGL CMake 配置）")
  endif()
  return()
endif()

# —— Qt6::OpenGL 导入目标（qt6-charts 的公开链接依赖）——
if (NOT TARGET Qt6::OpenGL)
  add_library(Qt6::OpenGL SHARED IMPORTED)
  set_target_properties(Qt6::OpenGL PROPERTIES
    IMPORTED_LOCATION "${_QT6OPENGL_LIB}"
    INTERFACE_INCLUDE_DIRECTORIES "${_QT6OPENGL_INCLUDE_DIR}"
    INTERFACE_LINK_LIBRARIES "Qt6::Core;Qt6::Gui;Qt6::Widgets"
    IMPORTED_LINK_INTERFACE_LIBRARIES "")
endif()

mark_as_advanced(_QT6OPENGL_LIB _QT6OPENGL_INCLUDE_DIR)
