# ---------------------------------------------------------------------------
# SDL2 dependency resolution
#
# 1. NESSTATION_USE_SYSTEM_SDL2 = ON  -> find_package only (fail if missing)
# 2. NESSTATION_USE_SYSTEM_SDL2 = OFF -> always build SDL2 via FetchContent
# 3. AUTO (default)                    -> try find_package, fall back to
#                                         FetchContent with a static build.
# ---------------------------------------------------------------------------
set(SDL_STATIC          ON  CACHE INTERNAL "")
set(SDL_SHARED          OFF CACHE INTERNAL "")
set(SDL_TEST            OFF CACHE INTERNAL "")
set(SDL_INSTALL         OFF CACHE INTERNAL "")
set(SDL2_DISABLE_INSTALL ON CACHE INTERNAL "")

if(NESSTATION_USE_SYSTEM_SDL2 STREQUAL "ON")
  find_package(SDL2 REQUIRED)
  message(STATUS "NesStation: using system SDL2 (${SDL2_VERSION})")
elseif(NESSTATION_USE_SYSTEM_SDL2 STREQUAL "OFF")
  include(FetchContent)
  FetchContent_Declare(SDL2
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG        release-2.30.14
    GIT_SHALLOW    TRUE)
  FetchContent_MakeAvailable(SDL2)
  message(STATUS "NesStation: vendored SDL2 via FetchContent")
else()
  find_package(SDL2 QUIET)
  if(SDL2_FOUND)
    message(STATUS "NesStation: using system SDL2 (${SDL2_VERSION})")
  else()
    message(STATUS "NesStation: system SDL2 not found - fetching release-2.30.14")
    include(FetchContent)
    FetchContent_Declare(SDL2
      GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
      GIT_TAG        release-2.30.14
      GIT_SHALLOW    TRUE)
    FetchContent_MakeAvailable(SDL2)
  endif()
endif()
