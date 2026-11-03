# ---------------------------------------------------------------------------
# SDL2 dependency resolution
#
# 1. NESSTATION_USE_SYSTEM_SDL2 = ON  -> find_package only (fail if missing)
# 2. NESSTATION_USE_SYSTEM_SDL2 = OFF -> always build SDL2 via FetchContent
# 3. AUTO (default)                    -> try find_package, fall back to
#                                         FetchContent with a static build.
#
# 源码标签必须是 libsdl-org/SDL 仓库真实存在的 release tag（曾因误写
# release-2.30.14 导致 Windows CI "invalid reference" 失败，已验证
# release-2.32.4 存在，且与本机 libsdl2 2.32.4 验证版本一致）。
# ---------------------------------------------------------------------------
set(NESSTATION_SDL2_TAG "release-2.32.4" CACHE STRING
    "libsdl-org/SDL tag fetched via FetchContent (must exist in the repo)")

set(SDL_STATIC          ON  CACHE INTERNAL "")
set(SDL_SHARED          OFF CACHE INTERNAL "")
set(SDL_TEST            OFF CACHE INTERNAL "")
set(SDL_INSTALL         OFF CACHE INTERNAL "")
set(SDL2_DISABLE_INSTALL ON CACHE INTERNAL "")

macro(nesstation_fetch_sdl2)
  include(FetchContent)
  FetchContent_Declare(SDL2
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG        ${NESSTATION_SDL2_TAG}
    GIT_SHALLOW    TRUE)
  FetchContent_MakeAvailable(SDL2)
  message(STATUS "NesStation: vendored SDL2 via FetchContent (${NESSTATION_SDL2_TAG})")
endmacro()

if(NESSTATION_USE_SYSTEM_SDL2 STREQUAL "ON")
  find_package(SDL2 REQUIRED)
  message(STATUS "NesStation: using system SDL2 (${SDL2_VERSION})")
elseif(NESSTATION_USE_SYSTEM_SDL2 STREQUAL "OFF")
  nesstation_fetch_sdl2()
else()
  find_package(SDL2 QUIET)
  if(SDL2_FOUND)
    message(STATUS "NesStation: using system SDL2 (${SDL2_VERSION})")
  else()
    message(STATUS "NesStation: system SDL2 not found - fetching ${NESSTATION_SDL2_TAG}")
    nesstation_fetch_sdl2()
  endif()
endif()
