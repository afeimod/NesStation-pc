# NesStation PC — v1.3.5 补丁应用脚本（PowerShell，在仓库根目录执行）
# 作用：删除已迁移到 assets/system/ 的旧散放资产路径（幂等，可重复执行）
$ErrorActionPreference = "Stop"
Set-Location $PSScriptRoot

$oldPaths = @(
  "assets/dc",
  "assets/fbneo",
  "assets/genesis",
  "assets/pce",
  "assets/pcsx2",
  "assets/azahar",
  "assets/PPSSPP",
  "assets/disksys.rom"
)

Write-Host "== 1/3 检查新载荷 assets/system/ =="
if (-not (Test-Path "assets/system/disksys.rom")) {
  Write-Host "[错误] assets/system/disksys.rom 不存在 —— 请先把补丁 zip 解压到仓库根目录再运行本脚本"
  exit 1
}
$n = (Get-ChildItem "assets/system" -Recurse -File).Count
Write-Host "[OK] assets/system/ 就位（$n 个文件）"

Write-Host "== 2/3 删除旧散放路径 =="
foreach ($p in $oldPaths) {
  if (Test-Path $p) {
    Remove-Item -Recurse -Force $p
    Write-Host "  已删除 $p"
  }
}

Write-Host "== 3/3 结果 =="
Write-Host "[OK] 完成。接下来：git add -A; git commit -m '修复 CI：资产移入 assets/system'; git push"
