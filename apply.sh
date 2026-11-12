#!/bin/bash
# NesStation PC — v1.3.5 补丁应用脚本（在仓库根目录执行）
# 本轮修复：GBA 颜色根因（RGB565 被拒）/ 游戏内横排图标菜单 /
#   游戏库顶栏横排工具按钮（按核心差异）/ thunar 生态文件选择链 /
#   dolphin/eden 运行库随包（ngcwii 集合根治）/ FBNeo BIOS 补齐 /
#   渲染后端色彩探针 + 软件渲染兜底。
set -e
cd "$(dirname "$0")"

echo "== 1/2 校验关键文件就位 =="
CHECKS=(
  "src/retro/LibretroCore.cpp"
  "src/ui/EmulatorScene.cpp"
  "src/ui/LibraryScene.cpp"
  "scripts/build_emulators.sh"
  ".github/workflows/build.yml"
  "assets/system/fbneo/naomi.zip"
)
for f in "${CHECKS[@]}"; do
  if [ ! -f "$f" ]; then
    echo "[错误] $f 不存在 —— 请先把补丁 zip 解压到仓库根目录再运行本脚本"
    exit 1
  fi
done
echo "[OK] 关键文件就绪（$(find assets/system -type f | wc -l) 个系统档案文件）"

echo "== 2/2 提示 =="
echo "[OK] 完成。接下来："
echo "  git add -A"
echo "  git commit -m \"v1.3.5：GBA颜色根因修复(RGB565)+横排菜单+库顶栏工具行+thunar选择链+dolphin运行库随包\""
echo "  git push"
echo "  然后 GitHub Actions → Build workflow → Run workflow 触发 CI（full 包含模拟器）"
