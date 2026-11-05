#!/bin/bash
# NesStation PC — 三大独立模拟器源码拉取（可断点续传，容忍进程中断）
# 用法: fetch_emu_src.sh [dolphin|cemu|eden|all]
# 产物: NesStation-PC/emulators/<name>/（含子模块【递归二级】内容，构建无需联网）
#
# ★v1.2.5 修复（run 41994953377 两大死因的源头预防）：
#   旧版只拉【一级】子模块，且只解析 github.com 的 URL，导致快照两处残缺：
#     1. dolphin 的 cubeb（mozilla/cubeb@54217bc）自带嵌套子模块
#        cmake/sanitizers-cmake —— Dolphin 的 cubeb 包装层
#        option(USE_SANITIZERS ON) 默认开，configure 必 FATAL
#        （"Could not find sanitizers-cmake"，本次 CI dolphin 直接死因）。
#     2. dolphin 的 bzip2 子模块源在 gitlab（bzip2/bzip2@6a8690f），
#        旧版 github-only 解析直接跳过 —— find_package(BZip2) 回退内置时
#        目录为空，configure 必挂（修完 cubeb 后的下一个死点）。
#   本版对齐 git submodule update --init --recursive 的语义：
#     - 递归拉取二级子模块（嵌套层数受 NESTED_DEPTH 控制，默认 2）；
#     - 支持 gitlab 归档源（API v4）；
#     - 「父工程确认不引用」的嵌套 googletest 不拉（Dolphin 自建 cubeb
#       包装层只编库不编测试；Cemu 显式 BUILD_TESTS=OFF），节省体积。
set -u
DEST_ROOT="$(cd "$(dirname "$0")/.." && pwd)/emulators"
NESTED_DEPTH="${NESTED_DEPTH:-2}"   # 子模块递归深度（1=仅一级，2=含嵌套）
mkdir -p "$DEST_ROOT" /tmp/emudl

API="https://api.github.com"

# —— 工具：GitHub API JSON 取值（无 jq 依赖）——
jget() { # json key -> value（首匹配字符串字段）
  python3 - "$1" "$2" <<'EOF'
import json,sys
try:
    d=json.loads(sys.argv[1])
    key=sys.argv[2]
    if isinstance(d,list):
        for it in d:
            if isinstance(it,dict) and key in it and isinstance(it[key],str):
                print(it[key]); break
    elif isinstance(d,dict) and key in d and isinstance(d[key],str):
        print(d[key])
except Exception:
    pass
EOF
}

# 下载（断点续传；每片 8 分钟自动放弃，外层可重跑）
dl() { # url out
  local url="$1" out="$2"
  for i in 1 2 3; do
    curl -L -C - --max-time 480 -sS -o "$out" "$url" && return 0
    sleep 2
  done
  return 1
}

# —— URL 解析：host owner repo（支持 github / gitlab）——
parse_url() { # url -> stdout: "host owner repo"
  local url="$1" host rest owner repo
  case "$url" in
    https://github.com/*|git@github.com:*)
      host=github
      rest="${url#*github.com[:/]}"
      rest="${rest%.git}"
      owner="$(echo "$rest" | cut -d/ -f1)"
      repo="$(echo "$rest" | cut -d/ -f2)"
      ;;
    https://gitlab.com/*|git@gitlab.com:*)
      host=gitlab
      rest="${url#*gitlab.com[:/]}"
      rest="${rest%.git}"
      owner="$(echo "$rest" | cut -d/ -f1)"
      repo="$(echo "$rest" | cut -d/ -f2)"
      ;;
    *)
      return 1
      ;;
  esac
  [ -z "$owner" ] || [ -z "$repo" ] && return 1
  echo "$host $owner $repo"
}

# —— 归档下载 URL ——
archive_url() { # host owner repo sha
  case "$1" in
    github) echo "https://codeload.github.com/$2/$3/tar.gz/$4" ;;
    # gitlab API v4：路径需 URL 编码的 project id（owner%2Frepo）
    gitlab) echo "https://gitlab.com/api/v4/projects/$2%2F$3/repository/archive.tar.gz?sha=$4" ;;
    *)      return 1 ;;
  esac
}

# —— 单个仓库源码（tarball）——
fetch_repo() { # owner repo ref outdir [host]
  local owner="$1" repo="$2" ref="$3" outdir="$4" host="${5:-github}"
  local tgz="/tmp/emudl/${repo}-${ref//\//_}.tgz"
  if [ -f "$outdir/.srcok" ]; then echo "  [ok] $repo 已就绪"; return 0; fi
  echo "  [dl] $owner/$repo @ $ref（$host）"
  dl "$(archive_url "$host" "$owner" "$repo" "$ref")" "$tgz" \
    || { echo "  [FAIL] $repo 下载失败"; return 1; }
  rm -rf "$outdir"
  mkdir -p "$outdir"
  tar xzf "$tgz" -C "$outdir" --strip-components=1 \
    || { echo "  [FAIL] $repo 解压失败"; return 1; }
  rm -f "$tgz"
  touch "$outdir/.srcok"
  return 0
}

# —— 子模块树（git trees API：mode=160000 即子模块固定位点）——
submodule_shas() { # owner repo ref -> 输出 "path sha url" 行
  python3 "$(dirname "$0")/emu_submodules.py" "$1/$2" "$3"
}

# —— 嵌套子模块跳过清单（父工程确认不引用，拉取纯属浪费体积）——
#   dolphin/Externals/cubeb/CMakeLists.txt（Dolphin 自建包装层）只编译
#     cubeb 库源码、不 add_subdirectory(cubeb)、不编任何测试 → cubeb 的
#     googletest 嵌套子模块永远用不到；
#   cemu 的 CMakeLists 在 add_subdirectory(dependencies/cubeb) 前显式
#     option(BUILD_TESTS OFF) + option(BUILD_TOOLS OFF) → 同上。
nested_skip() { # path -> 0=跳过 1=拉取
  case "$1" in
    Externals/cubeb/cubeb/googletest)  return 0 ;;
    dependencies/cubeb/googletest)     return 0 ;;
    *)                                 return 1 ;;
  esac
}

# —— 递归拉取子模块 ——
fetch_submodules() { # owner repo ref basepath label depth
  # basepath：子模块 path 的父目录（顶层调用传 "$DEST_ROOT/$name"）
  local owner="$1" repo="$2" ref="$3" base="$4" label="$5" depth="$6"
  [ "$depth" -le 0 ] && return 0
  local list
  list="$(submodule_shas "$owner" "$repo" "$ref" 2>/dev/null)" || true
  [ -z "$list" ] && return 0
  local total; total=$(grep -c . <<<"$list" 2>/dev/null || echo 0)
  echo "  $label 子模块 $total 个"
  local n=0
  while read -r path sha url; do
    [ -z "${path:-}" ] && continue
    n=$((n+1))
    local out="$base/$path"
    if [ -f "$out/.srcok" ]; then
      echo "  [ok $n/$total] $path"
    elif nested_skip "$label/$path"; then
      echo "  [skip $n/$total] $path（父工程不引用，体积优化）"
    else
      local host sowner srepo
      if host="$(parse_url "$url" | cut -d' ' -f1)" \
         && sowner="$(parse_url "$url" | cut -d' ' -f2)" \
         && srepo="$(parse_url "$url" | cut -d' ' -f3)" \
         && [ -n "${host:-}" ] && [ -n "${sowner:-}" ] && [ -n "${srepo:-}" ]; then
        echo "  [sub $n/$total] $path ← $sowner/$srepo @ ${sha:0:8}（$host）"
        if fetch_repo "$sowner" "$srepo" "$sha" "$out" "$host"; then
          # 递归：该子模块自己的子模块（depth-1）
          fetch_submodules "$sowner" "$srepo" "$sha" "$out" "$path" $((depth-1))
        else
          echo "  [warn] $path 拉取失败（可重跑续传）"
        fi
      else
        echo "  [skip] $path（无法解析源：$url）"
      fi
    fi
  done <<<"$list"
}

fetch_emu() { # name owner repo tag
  local name="$1" owner="$2" repo="$3" tag="$4"
  echo "=== $name（$owner/$repo @ $tag）==="
  local out="$DEST_ROOT/$name"
  fetch_repo "$owner" "$repo" "$tag" "$out" || return 1
  # 一级及其嵌套子模块（NESTED_DEPTH=2 ≈ git submodule update --init --recursive）
  fetch_submodules "$owner" "$repo" "$tag" "$out" "$name" "$NESTED_DEPTH"
  echo "  $name 完成"
}

case "${1:-all}" in
  dolphin) fetch_emu dolphin dolphin-emu dolphin 2609 ;;
  cemu)    fetch_emu cemu cemu-project Cemu v2.6 ;;
  eden)    fetch_emu eden eden-emulator mirror v0.2.1 ;;
  all)
    fetch_emu dolphin dolphin-emu dolphin 2609
    fetch_emu cemu cemu-project Cemu v2.6
    fetch_emu eden eden-emulator mirror v0.2.1
    ;;
  *) echo "用法: $0 [dolphin|cemu|eden|all]"; exit 1 ;;
esac
echo "DONE"
