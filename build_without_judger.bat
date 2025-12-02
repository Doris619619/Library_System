@echo off
chcp 65001 >nul

echo ========================================
echo    Library System - Visual Studio 构建脚本
echo ========================================

:: 切换到脚本所在目录
cd /d "%~dp0"

:: 设置环境变量
set QTPFX=D:\app\qt2\6.7.3\msvc2022_64
set VS_GENERATOR="Visual Studio 17 2022"

echo [信息] 当前目录: %CD%
echo [信息] Qt路径: %QTPFX%
echo [信息] 生成器: %VS_GENERATOR%
echo [信息] 构建模式: 排除Judger模块

:: 检查必要工具
if not exist "%QTPFX%" (
    echo [错误] Qt路径不存在: %QTPFX%
    pause
    exit /b 1
)

:: 完全清理
echo.
echo [步骤1] 清理构建目录...
rmdir /s /q build_vs 2>nul
if exist "build_vs" (
    echo [警告] build_vs 目录删除失败，可能被占用
) else (
    echo [成功] 构建目录已清理
)

:: 重新配置（使用Visual Studio，排除judger）
echo.
echo [步骤2] 配置项目（使用Visual Studio）...
cmake -S . -B build_vs -G %VS_GENERATOR% -A x64 ^
  -DCMAKE_PREFIX_PATH="%QTPFX%" ^
  -DCMAKE_TOOLCHAIN_FILE="C:\Users\LiangYS\vcpkg\scripts\buildsystems\vcpkg.cmake" ^
  -DBUILD_JUDGER=OFF -DBUILD_DB=ON -DBUILD_VISION=ON ^
  -DBUILD_JUDGER_CLI=OFF

if %errorlevel% neq 0 (
    echo [错误] CMake 配置失败!
    pause
    exit /b 1
)

echo [成功] CMake 配置完成（Visual Studio + Judger已禁用）

:: 构建
echo.
echo [步骤3] 构建项目...
cmake --build build_vs --config Release

if %errorlevel% neq 0 (
    echo [错误] 构建失败!
    pause
    exit /b 1
)

echo.
echo ========================================
echo [成功] 构建完成!
echo 输出目录: build_vs\Release
echo 可执行文件: Library_System.exe
echo ========================================

:: 可选：自动打开输出目录
echo.
set /p OPEN_DIR="是否打开输出目录? (y/n): "
if /i "%OPEN_DIR%"=="y" (
    if exist "build_vs\Release" (
        explorer "build_vs\Release"
    )
)

pause
