# ============================================================================
# NesStation PC — 源码构建脚本（Windows）
#
# 前置：
#   1. Visual Studio 2022（含 C++ 桌面开发工作负载）
#   2. CMake ≥ 3.16（VS 自带亦可）
#   3. SDL2：无需预装 —— 默认 FetchContent 自动构建静态 SDL2
#      （想用 vcpkg：set VCPKG_ROOT 后传 -DCMAKE_TOOLCHAIN_FILE=%VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake）
#
# 用法（在 "x64 Native Tools Command Prompt" 里）：
#   scripts\build.bat                    # 前端
#   scripts\build.bat --superbuild       # 前端 + Dolphin/Cemu/Eden（需 vcpkg，耗时极长）
# ============================================================================
@echo off
setlocal
set ROOT=%~dp0..
set BUILD=%ROOT%\build

set SUPERBUILD=OFF
set ARGS=
:parse
if "%~1"=="" goto after
if /i "%~1"=="--superbuild" set SUPERBUILD=ON
if /i "%~1"=="--no-dolphin" set ARGS=%ARGS% -DNESSTATION_BUILD_DOLPHIN=OFF
if /i "%~1"=="--no-cemu" set ARGS=%ARGS% -DNESSTATION_BUILD_CEMU=OFF
if /i "%~1"=="--no-eden" set ARGS=%ARGS% -DNESSTATION_BUILD_EDEN=OFF
if /i "%~1"=="--no-cores" set ARGS=%ARGS% -DNESSTATION_FETCH_CORES=OFF
shift
goto parse
:after

echo ==^> 配置（superbuild=%SUPERBUILD%）
cmake -B "%BUILD%" -S "%ROOT%" -DCMAKE_BUILD_TYPE=Release -DNESSTATION_SUPERBUILD=%SUPERBUILD% %ARGS%
if errorlevel 1 exit /b 1

echo ==^> 编译
cmake --build "%BUILD%" --config Release -- -m
if errorlevel 1 exit /b 1

echo.
echo ==^> 完成：%BUILD%\bin\Release\nesstation.exe
echo     核心已自动下载到 %BUILD%\bin\Release\cores\（构建时联网，默认开）
echo     离线构建：scripts\build.bat --no-cores
echo     更新/补齐核心：powershell -ExecutionPolicy Bypass -File scripts\fetch_cores.ps1
endlocal
