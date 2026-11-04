# ============================================================================
# NesStation PC — 一键下载 libretro 官方预编译核心（Windows PowerShell）
#
# 用法：
#   powershell -ExecutionPolicy Bypass -File scripts\fetch_cores.ps1
#   powershell -ExecutionPolicy Bypass -File scripts\fetch_cores.ps1 nes psx 3ds
#     （参数 = 平台短名：nes sfc gb gba md pce nds psx ps2 dc n3ds psp arcade dos java）
#   powershell -ExecutionPolicy Bypass -File scripts\fetch_cores.ps1 -Dest DIR [平台...]
#     （-Dest：下载到指定目录，打包/便携分发用）
#
# 核心来源：https://buildbot.libretro.com/nightly/windows/x86_64/latest/
# 每个平台按候选链依次尝试（与 src/core/Platform.cpp 一致），某个核心
# 无预编译包（404）时自动落到替代核心（如 3DS：azahar → citra）。
#
# 下载后自动解压到 <数据目录>\cores\（%APPDATA%\NesStation\cores\；
# exe 旁有 portable.flag 时为 <exe 目录>\UserData\cores\）
# ============================================================================
param([string]$Dest, [string[]]$Only)

$ErrorActionPreference = 'Continue'
[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12

# 平台 → 核心候选链（与 src/core/Platform.cpp 一致）
$PlatformCores = @(
  @{ key='nes';     label='NES / FC';      cores=@('fceumm','nestopia','quicknes') },
  @{ key='sfc';     label='SNES / SFC';    cores=@('snes9x','bsnes') },
  @{ key='gb';      label='GB / GBC';      cores=@('mgba','gambatte') },
  @{ key='gba';     label='GBA';           cores=@('mgba','vba_next','gpsp') },
  @{ key='md';      label='MD / SEGA';     cores=@('genesis_plus_gx','picodrive') },
  @{ key='pce';     label='PCE / TG16';    cores=@('geargrafx','mednafen_pce_fast') },
  @{ key='nds';     label='NDS';           cores=@('melonds','desmume') },
  @{ key='psx';     label='PS1';           cores=@('pcsx_rearmed','swanstation') },
  @{ key='ps2';     label='PS2';           cores=@('pcsx2') },
  @{ key='dc';      label='Dreamcast';     cores=@('flycast') },
  @{ key='n3ds';    label='3DS';           cores=@('azahar','citra') },
  @{ key='psp';     label='PSP';           cores=@('ppsspp') },
  @{ key='arcade';  label='Arcade';        cores=@('fbneo','mame2003_plus') },
  @{ key='dos';     label='MS-DOS';        cores=@('dosbox_pure','dosbox_svn') },
  @{ key='java';    label='Java ME';       cores=@('squirreljme') }
)

$exeDir = Split-Path -Parent $PSScriptRoot
if ($Dest) {
  $coresDir = $Dest
} elseif (Test-Path "$exeDir\portable.flag") {
  $coresDir = "$exeDir\UserData\cores"
} else {
  $coresDir = "$env:APPDATA\NesStation\cores"
}
New-Item -ItemType Directory -Force -Path $coresDir | Out-Null

$base = 'https://buildbot.libretro.com/nightly/windows/x86_64/latest'

Write-Host "=========================================="
Write-Host " NesStation PC 核心下载器 (Windows x64)"
Write-Host " 目录：$coresDir"
Write-Host "=========================================="

$ok = 0; $skip = 0; $miss = 0
foreach ($plat in $PlatformCores) {
  if ($Only -and $Only.Count -gt 0 -and $Only -notcontains $plat.key) { continue }

  # 链上已有任一核心 → 跳过
  $installed = $null
  foreach ($core in $plat.cores) {
    if (Test-Path "$coresDir\${core}_libretro.dll") { $installed = $core; break }
  }
  if ($installed) {
    Write-Host "  [跳过] $($plat.label)（已安装 $installed）"
    $skip++
    continue
  }

  # 依次尝试候选核心（404 → 下一个）
  $got = $null
  foreach ($core in $plat.cores) {
    $url = "$base/${core}_libretro.dll.zip"
    $tmpZip = "$env:TEMP\${core}_libretro.zip"
    $dest = "$coresDir\${core}_libretro.dll"
    Write-Host "  [下载] $($plat.label) ← $core ..."
    try {
      Invoke-WebRequest -Uri $url -OutFile $tmpZip -UseBasicParsing -TimeoutSec 600
      Expand-Archive -Path $tmpZip -DestinationPath $coresDir -Force
      if (Test-Path $dest) { $got = $core; break }
      Write-Host "         $core 解压后未找到核心，尝试替代..."
    } catch {
      if ($_.Exception.Response -and [int]$_.Exception.Response.StatusCode -eq 404) {
        Write-Host "         $core 在本架构不可用（404），尝试替代核心..."
      } else {
        # 网络抖动重试一次
        try {
          Start-Sleep -Seconds 5
          Invoke-WebRequest -Uri $url -OutFile $tmpZip -UseBasicParsing -TimeoutSec 600
          Expand-Archive -Path $tmpZip -DestinationPath $coresDir -Force
          if (Test-Path $dest) { $got = $core; break }
        } catch {
          Write-Host "         $core 下载失败（$($_.Exception.Message)），尝试替代核心..."
        }
      }
    } finally {
      if (Test-Path $tmpZip) { Remove-Item $tmpZip -Force }
    }
  }

  if ($got) {
    Write-Host "         [完成] $($plat.label) → $got"
    $ok++
  } else {
    Write-Host "         [缺失] $($plat.label)：无任何候选核心可下载"
    $miss++
  }
}

Write-Host "=========================================="
Write-Host " 完成：$ok 新装，$skip 已有，$miss 平台无可用核心"
Write-Host " 提示：NGC/Wii、Wii U、Switch 为独立模拟器"
Write-Host " （Dolphin / Cemu / Eden），放 emulators\ 目录"
Write-Host " 或到设置页配置路径。"
Write-Host "=========================================="
# 软失败：缺核心是正常情况（运行时候选链会自动回退/提示），
# 永远不因平台缺失或网络问题中断 CMake 构建
exit 0
