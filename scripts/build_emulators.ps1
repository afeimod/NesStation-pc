# ============================================================================
# NesStation PC — 三大独立模拟器本地源码构建（Windows / PowerShell 7+）
#
# 源码已完整内置在仓库 emulators/ 目录（无需联网下载模拟器本体）：
#   emulators/dolphin  — Dolphin 2609（NGC/Wii，34 个 Externals 全内置）
#   emulators/cemu     — Cemu v2.6（Wii U，含内置 vcpkg）
#   emulators/eden     — Eden v0.2.1（Switch 镜像快照；首次配置需联网拉
#                        CPM 依赖）
#
# 用法：
#   powershell -File scripts\build_emulators.ps1 [-Target all|dolphin|cemu|eden] [-Jobs 8]
#   可选 -QtDir C:\Qt\6.8.3\msvc2022_64（预装 Qt → Eden 跳过 CPM 下载 Qt）
# 产物：dist\emulators\<名称>\（拷到 nesstation.exe 旁即自动识别）
#
# ★v1.2.2 修正（对照上游官方 CI 核实）：
#   1. Cemu 产物在【源码根 emulators\cemu\bin\Cemu_release.exe】而非
#      build\cemu\bin —— 旧脚本拷贝空目录导致产物恒为空
#   2. VS 多配置生成器 build/install 必须显式 --config Release（默认 Debug）
#   3. Dolphin/Eden 构建后 windeployqt 部署 Qt DLL
#
# 前置：Visual Studio 2022（C++ 桌面开发工作负载）+ CMake（VS 自带）+ git
# ============================================================================
param(
  [string]$Target = "all",
  [int]$Jobs = 8,
  [string]$QtDir = ""
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
Set-Location $root
$out = Join-Path $root "dist\emulators"
New-Item -ItemType Directory -Force -Path $out | Out-Null

Write-Host "== NesStation 模拟器本地构建（目标：$Target，线程：$Jobs）=="
Write-Host "== 产物目录：$out =="

function Invoke-Windeployqt([string]$Exe) {
  if (-not (Get-Command windeployqt -ErrorAction SilentlyContinue) -and $QtDir) {
    $env:PATH = "$QtDir\bin;$env:PATH"
  }
  if (Get-Command windeployqt -ErrorAction SilentlyContinue) {
    Write-Host "（windeployqt 部署 Qt 运行库 → $Exe）"
    & windeployqt --release --no-translations --no-system-d3d-compiler `
      --no-opengl-sw $Exe
    if ($LASTEXITCODE -ne 0) { Write-Warning "windeployqt 失败，请手动部署 Qt DLL" }
  }
}

function Build-Dolphin {
  Write-Host "=== Dolphin（NGC/Wii）==="
  cmake -S emulators\dolphin -B build\dolphin `
    -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$out\dolphin" `
    -DENABLE_TESTS=OFF -DUSE_MSVC_PRECOMPILED_HEADERS=ON
  if ($LASTEXITCODE -ne 0) { throw "Dolphin 配置失败" }
  cmake --build build\dolphin --config Release -j $Jobs
  if ($LASTEXITCODE -ne 0) { throw "Dolphin 编译失败" }
  # ★VS 多配置生成器：install 必须显式 --config Release（默认 Debug → 空安装）
  cmake --install build\dolphin --config Release
  if ($LASTEXITCODE -ne 0) { throw "Dolphin 安装失败" }
  $exe = Get-ChildItem "$out\dolphin" -Recurse -Filter Dolphin.exe | Select-Object -First 1
  if ($exe) { Invoke-Windeployqt $exe.FullName }
  Write-Host "OK Dolphin -> $out\dolphin"
}

function Build-Cemu {
  Write-Host "=== Cemu（Wii U，使用内置 vcpkg）==="
  # vcpkg 快照不带 .git：manifest 模式解析 builtin-baseline 必须有 git 历史。
  # 一次性修复：init + 拉取基线提交并硬对齐（需联网）。vcpkg.exe 缺失时 bootstrap。
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
    git -C $vcpkgDir remote add origin https://github.com/microsoft/vcpkg.git 2>$null
    git -C $vcpkgDir fetch -q --depth 1 origin $baseline
    if ($LASTEXITCODE -ne 0) { throw "vcpkg 基线拉取失败（检查网络）" }
    git -C $vcpkgDir reset -q --hard FETCH_HEAD
  }
  if (-not (Test-Path (Join-Path $vcpkgDir "vcpkg.exe"))) {
    Push-Location $vcpkgDir
    & cmd /c bootstrap-vcpkg.bat
    if ($LASTEXITCODE -ne 0) { Pop-Location; throw "vcpkg bootstrap 失败" }
    Pop-Location
  }
  # 工具链由 Cemu CMakeLists 自动指向内置 vcpkg（对齐上游 CI，无需手动传）
  cmake -S emulators\cemu -B build\cemu `
    -DCMAKE_BUILD_TYPE=Release `
    -DVCPKG_INSTALL_OPTIONS="--clean-after-build"
  if ($LASTEXITCODE -ne 0) { throw "Cemu 配置失败" }
  cmake --build build\cemu --config Release -j $Jobs
  if ($LASTEXITCODE -ne 0) { throw "Cemu 编译失败" }
  # ★产物在源码根 emulators\cemu\bin\Cemu_release.exe（上游 CI 同款重命名），
  #   不是 build\cemu\bin —— 旧脚本从错误目录拷贝导致产物恒为空
  $rel = Join-Path $root "emulators\cemu\bin"
  if (-not (Test-Path (Join-Path $rel "Cemu_release.exe"))) {
    throw "Cemu 构建完成但未找到 emulators\cemu\bin\Cemu_release.exe"
  }
  New-Item -ItemType Directory -Force -Path "$out\cemu" | Out-Null
  Copy-Item -Path "$rel\*" -Destination "$out\cemu" -Recurse -Force
  Rename-Item "$out\cemu\Cemu_release.exe" "Cemu.exe"
  Write-Host "OK Cemu -> $out\cemu"
}

function Build-Eden {
  Write-Host "=== Eden（Switch）==="
  $qtArgs = @()
  if ($QtDir) {
    # 预装 Qt：走系统 Qt，跳过 CPM 下载 ~1GB 内置 Qt
    Write-Host "（使用预装 Qt：$QtDir）"
    $qtArgs = @("-DYUZU_USE_BUNDLED_QT=OFF", "-DQt6_DIR=$QtDir\lib\cmake\Qt6")
  }
  cmake -S emulators\eden -B build\eden -DCMAKE_BUILD_TYPE=Release @qtArgs
  if ($LASTEXITCODE -ne 0) { throw "Eden 配置失败（首次配置需联网拉取 CPM 依赖）" }
  cmake --build build\eden --config Release -j $Jobs
  if ($LASTEXITCODE -ne 0) { throw "Eden 编译失败" }
  New-Item -ItemType Directory -Force -Path "$out\eden" | Out-Null
  # VS 多配置产物在 build\eden\bin\Release\
  $binRel = Join-Path $root "build\eden\bin\Release"
  $binPlain = Join-Path $root "build\eden\bin"
  if (Test-Path $binRel) {
    Copy-Item -Path "$binRel\*" -Destination "$out\eden" -Recurse -Force
  } elseif (Test-Path $binPlain) {
    Copy-Item -Path "$binPlain\*" -Destination "$out\eden" -Recurse -Force -ErrorAction SilentlyContinue
  }
  $exe = Get-ChildItem "$out\eden" -Recurse -Include eden.exe,Eden.exe | Select-Object -First 1
  if ($exe) { Invoke-Windeployqt $exe.FullName }
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
