#!/bin/bash
# NesStation PC — 三大模拟器源码拉取（可断点续传，容忍进程中断）
# 用法: fetch_emu_src.sh [dolphin|cemu|eden|all]
# 产物: NesStation-PC/emulators/<name>/（含子模块内容，构建无需联网）
set -u
DEST_ROOT="$(cd "$(dirname "$0")/.." && pwd)/emulators"
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

# —— 单个仓库源码（tarball）——
fetch_repo() { # owner repo ref outdir
  local owner="$1" repo="$2" ref="$3" outdir="$4"
  local tgz="/tmp/emudl/${repo}-${ref//\//_}.tar.gz"
  if [ -f "$outdir/.srcok" ]; then echo "  [ok] $repo 已就绪"; return 0; fi
  echo "  [dl] $owner/$repo @ $ref"
  dl "https://codeload.github.com/$owner/$repo/tar.gz/$ref" "$tgz" || { echo "  [FAIL] $repo 下载失败"; return 1; }
  rm -rf "$outdir"
  mkdir -p "$outdir"
  tar xzf "$tgz" -C "$outdir" --strip-components=1 || { echo "  [FAIL] $repo 解压失败"; return 1; }
  rm -f "$tgz"
  touch "$outdir/.srcok"
  return 0
}

# —— 子模块树（git trees API：mode=160000 即子模块固定位点）——
submodule_shas() { # owner repo ref -> 输出 "path sha url" 行
  python3 "$(dirname "$0")/emu_submodules.py" "$1/$2" "$3"
}

fetch_emu() { # name owner repo tag
  local name="$1" owner="$2" repo="$3" tag="$4"
  echo "=== $name（$owner/$repo @ $tag）==="
  local out="$DEST_ROOT/$name"
  fetch_repo "$owner" "$repo" "$tag" "$out" || return 1
  # 子模块（一级）
  local list="/tmp/emudl/${name}.subs"
  submodule_shas "$owner" "$repo" "$tag" > "$list" || true
  local total; total=$(grep -c . "$list" 2>/dev/null || echo 0)
  echo "  子模块 $total 个"
  local n=0
  while read -r path sha url; do
    [ -z "${path:-}" ] && continue
    n=$((n+1))
    # url 形如 https://github.com/x/y.git
    local sowner srepo
    sowner=$(echo "$url" | sed -e 's#https://github.com/##' -e 's#\.git$##' | cut -d/ -f1)
    srepo=$(echo "$url" | sed -e 's#https://github.com/##' -e 's#\.git$##' | cut -d/ -f2)
    if [ -z "$sowner" ] || [ -z "$srepo" ]; then echo "  [skip] $path（无 URL）"; continue; fi
    if [ -e "$out/$path/.srcok" ] || [ -n "$(ls -A "$out/$path" 2>/dev/null)" ]; then
      # 已有内容但可能不完整——以 .srcok 为准
      [ -f "$out/$path/.srcok" ] && { echo "  [ok] $path"; continue; }
    fi
    echo "  [sub $n/$total] $path ← $sowner/$srepo @ ${sha:0:8}"
    fetch_repo "$sowner" "$srepo" "$sha" "$out/$path" || echo "  [warn] $path 拉取失败（可重跑续传）"
  done < "$list"
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
