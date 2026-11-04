#!/bin/bash
# NesStation PC — 一键构建（Linux x86_64 沙箱/本机）
set -e
cd "$(dirname "$0")/.."
OUT=${1:-/tmp/nesbuild/nesstation}
mkdir -p "$(dirname "$OUT")"
SDL=${SDL_INC:-/tmp/sdl2dev/include}
if [ ! -f "$SDL/SDL.h" ]; then
  for c in /usr/include/SDL2 /usr/local/include/SDL2; do
    [ -f "$c/SDL.h" ] && SDL="$c"
  done
fi
g++ -std=c++17 -O1 -w -I src -I third_party -I third_party/stb -I third_party/libretro \
  -I "$SDL" \
  $(ls src/*.cpp src/app/*.cpp src/core/*.cpp src/emu/*.cpp src/retro/*.cpp src/ui/*.cpp src/util/*.cpp) \
  third_party/stb/stb_impl.cpp ${SDL_LIB:-/usr/lib/x86_64-linux-gnu/libSDL2-2.0.so.0} \
  -ldl -lpthread -o "$OUT"
echo "BUILD OK → $OUT"
