cmake_minimum_required(VERSION 3.16)

project(NesStation VERSION 1.0.0 LANGUAGES C CXX
        DESCRIPTION "NesStation PC - Multi-platform retro game station for Windows/Linux")

# ---------------------------------------------------------------------------
# Options
# ---------------------------------------------------------------------------
option(NESSTATION_USE_SYSTEM_SDL2 "Use system-installed SDL2 instead of FetchContent" AUTO)
option(NESSTATION_SUPERBUILD      "Build standalone emulators (Dolphin/Cemu/Eden) from source" OFF)
option(NESSTATION_BUILD_DOLPHIN   "Superbuild: Nintendo GameCube / Wii (Dolphin ${NESSTATION_DOLPHIN_TAG})" ON)
option(NESSTATION_BUILD_CEMU      "Superbuild: Nintendo Wii U (Cemu)" ON)
option(NESSTATION_BUILD_EDEN      "Superbuild: Nintendo Switch (Eden)" ON)
set(NESSTATION_DOLPHIN_TAG "2512" CACHE STRING "Dolphin release tag (NGC/Wii)")
set(NESSTATION_CEMU_TAG    "v2.6" CACHE STRING "Cemu release tag (Wii U)")
set(NESSTATION_EDEN_TAG    "v0.2.1" CACHE STRING "Eden release tag (Switch)")

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

set_target_properties(nesstation PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

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

if(TARGET SDL2::SDL2main)
  target_link_libraries(nesstation PRIVATE SDL2::SDL2main)
endif()
target_link_libraries(nesstation PRIVATE SDL2::SDL2)

# stb 单头库实现聚合单元
target_sources(nesstation PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/third_party/stb/stb_impl.cpp)

# Embed resources: fonts + app icon

# ---------------------------------------------------------------------------
# Install / package layout (portable)
# ---------------------------------------------------------------------------
if(WIN32)
  set(NESSTATION_EXE_DIR ${CMAKE_BINARY_DIR}/bin)
  install(TARGETS nesstation RUNTIME DESTINATION .)
  install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/assets/fonts DESTINATION assets)
else()
  install(TARGETS nesstation RUNTIME DESTINATION bin)
  install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/assets/fonts DESTINATION share/nesstation/assets)
endif()

# ---------------------------------------------------------------------------
# Optional superbuild: standalone emulators from source (like the Android
# project's :cemu-core / :eden-core source-level integration).
# ---------------------------------------------------------------------------
if(NESSTATION_SUPERBUILD)
  include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/EmulatorSuperbuild.cmake)
endif()
