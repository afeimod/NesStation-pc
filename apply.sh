#!/bin/bash
# NesStation PC — v1.3.5 补丁应用脚本（在仓库根目录执行）
# 作用：删除已迁移到 assets/system/ 的旧散放资产路径（幂等，可重复执行）
set -e
cd "$(dirname "$0")"

OLD_PATHS=(
  assets/dc
  assets/fbneo
  assets/genesis
  assets/pce
  assets/pcsx2
  assets/azahar
  assets/PPSSPP
  assets/disksys.rom
)

echo "== 1/3 检查新载荷 assets/system/ =="
if [ ! -f assets/system/disksys.rom ]; then
  echo "[错误] assets/system/disksys.rom 不存在 —— 请先把补丁 zip 解压到仓库根目录再运行本脚本"
  exit 1
fi
echo "[OK] assets/system/ 就位（$(find assets/system -type f | wc -l) 个文件）"

echo "== 2/3 删除旧散放路径 =="
for p in "${OLD_PATHS[@]}"; do
  if [ -e "$p" ]; then
    rm -rf "$p"
    echo "  已删除 $p"
  fi
done

echo "== 3/3 结果 =="
if [ -e assets/dc ] || [ -e assets/disksys.rom ]; then
  echo "[错误] 旧路径删除失败，请手动检查"
  exit 1
fi
echo "[OK] 完成。接下来：git add -A && git commit -m \"修复 CI：资产移入 assets/system\" && git push"
