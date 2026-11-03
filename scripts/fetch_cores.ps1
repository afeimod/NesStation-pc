# ============================================================================
# NesStation PC — 一键下载 libretro 官方 PC 预编译核心（Windows PowerShell）
#
# 用法：
#   powershell -ExecutionPolicy Bypass -File scripts\fetch_cores.ps1
#   powershell -ExecutionPolicy Bypass -File scripts\fetch_cores.ps1 fceumm mgba
#
# 核心来源：https://buildbot.libretro.com/nightly/windows/x86_64/latest/
# 下载后自动解压到 <数据目录>\cores\（%APPDATA%\NesStation\cores\；
# exe 旁有 portable.flag 时为 <exe 目录>\UserData\cores\）
# ============================================================================
param([string[]]$Only)

$ErrorActionPreference = 'Stop'
[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12

$AllCores = @(
  'fceumm',            # NES / FC
  'snes9x',            # SNES / SFC
  'mgba',              # GB / GBC / GBA
  'genesis_plus_gx',   # MD / SMS / GG / SG / Mega-CD
  'geargrafx',         # PCE / TG16
  'fbneo',             # Arcade
  'dosbox_pure',       # MS-DOS
  'melonds',           # NDS
  'pcsx_rearmed',      # PS1
  'pcsx2',             # PS2
  'flycast',           # DC / NAOMI
  'ppsspp',            # PSP
  'azahar',            # 3DS
  'squirreljme'        # Java ME
)

$exeDir = Split-Path -Parent $PSScriptRoot
if (Test-Path "$exeDir\portable.flag") {
  $coresDir = "$exeDir\UserData\cores"
} else {
  $coresDir = "$env:APPDATA\NesStation\cores"
}
New-Item -ItemType Directory -Force -Path $coresDir | Out-Null

$cores = if ($Only -and $Only.Count -gt 0) { $Only } else { $AllCores }

Write-Host "=========================================="
Write-Host " NesStation PC 核心下载器 (Windows x64)"
Write-Host " 目录：$coresDir"
Write-Host "=========================================="

$ok = 0; $fail = 0
foreach ($core in $cores) {
  $dest = "$coresDir\${core}_libretro.dll"
  if (Test-Path $dest) {
    Write-Host "  [跳过] $core（已存在）"
    $ok++
    continue
  }
  $url = "https://buildbot.libretro.com/nightly/windows/x86_64/latest/${core}_libretro.dll.zip"
  $tmpZip = "$env:TEMP\${core}_libretro.zip"
  Write-Host "  [下载] $core ..."
  try {
    Invoke-WebRequest -Uri $url -OutFile $tmpZip -UseBasicParsing -TimeoutSec 600
    Expand-Archive -Path $tmpZip -DestinationPath $coresDir -Force
    if (Test-Path $dest) {
      Write-Host "  [完成] $core"
      $ok++
    } else {
      Write-Host "  [失败] $core（解压后未找到核心）"
      $fail++
    }
  } catch {
    Write-Host "  [失败] $core（$($_.Exception.Message)）"
    $fail++
  } finally {
    if (Test-Path $tmpZip) { Remove-Item $tmpZip -Force }
  }
}

Write-Host "=========================================="
Write-Host " 完成：$ok 成功，$fail 失败"
Write-Host " 提示：NGC/Wii、Wii U、Switch 为独立模拟器"
Write-Host " （Dolphin / Cemu / Eden），放 emulators\ 目录"
Write-Host " 或到设置页配置路径。"
Write-Host "=========================================="
exit $fail
