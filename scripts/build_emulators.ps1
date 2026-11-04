# ============================================================================
# NesStation PC — 三大独立模拟器本地源码构建（Windows / PowerShell）
#
# 源码已完整内置在仓库 emulators/ 目录（无需联网）：
#   emulators/dolphin  — Dolphin 2609（NGC/Wii，34 个 Externals 全内置）
#   emulators/cemu     — Cemu v2.6（Wii U，含内置 vcpkg）
#   emulators/eden     — Eden v0.2.1（Switch 镜像快照；首次配置需联网拉
#                        CPM 依赖，或先用 Git Bash 运行其自带
#                        tools/cpm-fetch-all.sh 预取后离线）
#
# 用法：
#   powershell -File scripts\build_emulators.ps1 [-Target all|dolphin|cemu|eden] [-Jobs 8]
# 产物：dist\emulators\<名称>\（拷到 nesstation.exe 旁即自动识别）
#
# 前置：Visual Studio 2022（C++ 桌面开发工作负载）+ CMake（VS 自带）
# ============================================================================
param(
  [string]$Target = "all",
  [int]$Jobs = 8
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
Set-Location $root
$out = Join-Path $root "dist\emulators"
New-Item -ItemType Directory -Force -Path $out | Out-Null

Write-Host "== NesStation 模拟器本地构建（目标：$Target，线程：$Jobs）=="
Write-Host "== 产物目录：$out =="

function Build-Dolphin {
  Write-Host "=== Dolphin（NGC/Wii）==="
  cmake -S emulators\dolphin -B build\dolphin `
    -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$out\dolphin" `
    -DENABLE_TESTS=OFF -DUSE_MSVC_PRECOMPILED_HEADERS=ON
  if ($LASTEXITCODE -ne 0) { throw "Dolphin 配置失败" }
  cmake --build build\dolphin -j $Jobs
  if ($LASTEXITCODE -ne 0) { throw "Dolphin 编译失败" }
  cmake --install build\dolphin
  Write-Host "OK Dolphin -> $out\dolphin"
}

function Build-Cemu {
  Write-Host "=== Cemu（Wii U，使用内置 vcpkg）==="
  # vcpkg 快照不带 .git：manifest 模式解析 builtin-baseline 必须有 git 历史。
  # 一次性修复：init + 拉取基线提交并硬对齐（需联网；基线即快照所在提交，
  # 树内容不变，只是补上历史）。vcpkg.exe 缺失时同时 bootstrap（下载预编译）。
  $vcpkgDir = Join-Path $root "emulators\cemu\dependencies\vcpkg"
  $baseline = $null
  if (Test-Path "emulators\cemu\vcpkg.json") {
    if ((Get-Content "emulators\cemu\vcpkg.json" -Raw) -match '"builtin-baseline"\s*:\s*"([0-9a-f]{40})"') {
      $baseline = $Matches[1]
    }
  }
  if (-not (Test-Path (Join-Path $vcpkgDir ".git")) -and $baseline) {
    Write-Host "（vcpkg 快照无 .git：拉取基线 $baseline）"
    git -C $vcpkgDir init | Out-Null
    git -C $vcpkgDir remote add origin https://github.com/microsoft/vcpkg.git
    git -C $vcpkgDir fetch -q --depth 1 origin $baseline
    if ($LASTEXITCODE -ne 0) { throw "vcpkg 基线拉取失败（检查网络）" }
    git -C $vcpkgDir reset -q --hard FETCH_HEAD
  }
  if (-not (Test-Path (Join-Path $vcpkgDir "vcpkg.exe"))) {
    & (Join-Path $vcpkgDir "bootstrap-vcpkg.bat")
    if ($LASTEXITCODE -ne 0) { throw "vcpkg bootstrap 失败" }
  }
  $toolchain = Join-Path $vcpkgDir "scripts\buildsystems\vcpkg.cmake"
  cmake -S emulators\cemu -B build\cemu `
    -DCMAKE_BUILD_TYPE=Release `
    -DCMAKE_TOOLCHAIN_FILE="$toolchain" `
    -DVCPKG_INSTALL_OPTIONS="--clean-after-build"
  if ($LASTEXITCODE -ne 0) { throw "Cemu 配置失败" }
  cmake --build build\cemu -j $Jobs
  if ($LASTEXITCODE -ne 0) { throw "Cemu 编译失败" }
  New-Item -ItemType Directory -Force -Path "$out\cemu" | Out-Null
  Copy-Item -Path "build\cemu\bin\*" -Destination "$out\cemu" -Recurse -Force
  Write-Host "OK Cemu -> $out\cemu"
}

function Build-Eden {
  Write-Host "=== Eden（Switch）==="
  cmake -S emulators\eden -B build\eden -DCMAKE_BUILD_TYPE=Release
  if ($LASTEXITCODE -ne 0) { throw "Eden 配置失败（首次配置需联网拉取 CPM 依赖）" }
  cmake --build build\eden -j $Jobs
  if ($LASTEXITCODE -ne 0) { throw "Eden 编译失败" }
  New-Item -ItemType Directory -Force -Path "$out\eden" | Out-Null
  Copy-Item -Path "build\eden\bin\*" -Destination "$out\eden" -Recurse -Force -ErrorAction SilentlyContinue
  Write-Host "OK Eden -> $out\eden"
}

switch ($Target) {
  "dolphin" { Build-Dolphin }
  "cemu"    { Build-Cemu }
  "eden"    { Build-Eden }
  default   { Build-Dolphin; Build-Cemu; Build-Eden }
}

Write-Host ""
Write-Host "全部完成。把 dist\emulators\ 整个目录复制到 nesstation.exe 旁："
Write-Host "  NesStation\"
Write-Host "  |-- nesstation.exe"
Write-Host "  +-- emulators\"
Write-Host "      |-- dolphin\   (Dolphin.exe)"
Write-Host "      |-- cemu\      (Cemu.exe)"
Write-Host "      +-- eden\      (Eden.exe)"
Write-Host "前端启动后自动识别（设置 → 核心设置 → 外置核心 应显示「已就绪」）。"
