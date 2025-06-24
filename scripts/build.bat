@echo off
setlocal enabledelayedexpansion

REM DesktopTerminal-CEF Windows构建脚本
REM 支持Windows 7 SP1 32位、64位系统

set "SCRIPT_DIR=%~dp0"
set "PROJECT_ROOT=%SCRIPT_DIR%\.."
set "BUILD_DIR=%PROJECT_ROOT%\build"
set "INSTALL_DIR=%PROJECT_ROOT%\install"

REM 默认参数
set "TARGET=Release"
set "ARCH=auto"
set "JOBS=%NUMBER_OF_PROCESSORS%"
set "CLEAN=false"
set "INSTALL=false"
set "VERBOSE=false"
set "CEF_VERSION="
set "QT_DIR="

REM 解析命令行参数
:parse_args
if "%~1"=="" goto end_parse
if "%~1"=="-h" goto show_help
if "%~1"=="--help" goto show_help
if "%~1"=="-t" (
    set "TARGET=%~2"
    shift
    shift
    goto parse_args
)
if "%~1"=="--target" (
    set "TARGET=%~2"
    shift
    shift
    goto parse_args
)
if "%~1"=="-a" (
    set "ARCH=%~2"
    shift
    shift
    goto parse_args
)
if "%~1"=="--arch" (
    set "ARCH=%~2"
    shift
    shift
    goto parse_args
)
if "%~1"=="-j" (
    set "JOBS=%~2"
    shift
    shift
    goto parse_args
)
if "%~1"=="--jobs" (
    set "JOBS=%~2"
    shift
    shift
    goto parse_args
)
if "%~1"=="-c" (
    set "CLEAN=true"
    shift
    goto parse_args
)
if "%~1"=="--clean" (
    set "CLEAN=true"
    shift
    goto parse_args
)
if "%~1"=="-i" (
    set "INSTALL=true"
    shift
    goto parse_args
)
if "%~1"=="--install" (
    set "INSTALL=true"
    shift
    goto parse_args
)
if "%~1"=="--cef-version" (
    set "CEF_VERSION=%~2"
    shift
    shift
    goto parse_args
)
if "%~1"=="--qt-dir" (
    set "QT_DIR=%~2"
    shift
    shift
    goto parse_args
)
if "%~1"=="--verbose" (
    set "VERBOSE=true"
    shift
    goto parse_args
)
echo [ERROR] 未知选项: %~1
goto show_help

:end_parse

REM 检测系统架构
if "%ARCH%"=="auto" (
    if "%PROCESSOR_ARCHITECTURE%"=="AMD64" (
        set "ARCH=x64"
    ) else if "%PROCESSOR_ARCHITECTURE%"=="x86" (
        set "ARCH=x86"
    ) else (
        set "ARCH=x64"
    )
)

echo [INFO] 开始构建 DesktopTerminal-CEF
echo [INFO] 架构: %ARCH%
echo [INFO] 构建类型: %TARGET%

REM 检查依赖
echo [INFO] 检查构建依赖...

where cmake >nul 2>nul
if %errorlevel% neq 0 (
    echo [ERROR] CMake未找到，请安装CMake 3.20或更高版本
    exit /b 1
)

REM 检查编译器
where cl >nul 2>nul
if %errorlevel% neq 0 (
    where gcc >nul 2>nul
    if %errorlevel% neq 0 (
        echo [ERROR] 未找到编译器（MSVC或GCC）
        exit /b 1
    )
)

echo [SUCCESS] 依赖检查完成

REM 清理构建目录
if "%CLEAN%"=="true" (
    echo [INFO] 清理构建目录...
    if exist "%BUILD_DIR%" (
        rmdir /s /q "%BUILD_DIR%"
    )
    echo [SUCCESS] 构建目录已清理
)

REM 检查CEF
echo [INFO] 检查CEF安装...
set "CEF_DIR=%PROJECT_ROOT%\third_party\cef"

if not exist "%CEF_DIR%" (
    echo [INFO] CEF未找到，开始下载...
    call "%PROJECT_ROOT%\scripts\download-cef.bat" "%ARCH%" "%CEF_VERSION%"
    if !errorlevel! neq 0 (
        echo [ERROR] CEF下载失败
        exit /b 1
    )
) else (
    echo [INFO] CEF已存在: %CEF_DIR%
)

REM 配置构建
echo [INFO] 配置构建...

set "BUILD_SUBDIR=%BUILD_DIR%\%TARGET%_%ARCH%"
mkdir "%BUILD_SUBDIR%" 2>nul
cd /d "%BUILD_SUBDIR%"

REM CMake参数
set "CMAKE_ARGS=-DCMAKE_BUILD_TYPE=%TARGET% -DCMAKE_INSTALL_PREFIX=%INSTALL_DIR%"

REM 架构特定配置
if "%ARCH%"=="x86" (
    set "CMAKE_ARGS=%CMAKE_ARGS% -A Win32"
)

REM Qt配置
if not "%QT_DIR%"=="" (
    set "CMAKE_ARGS=%CMAKE_ARGS% -DQt5_DIR=%QT_DIR%\lib\cmake\Qt5"
)

REM CEF版本配置
if not "%CEF_VERSION%"=="" (
    set "CMAKE_ARGS=%CMAKE_ARGS% -DCEF_VERSION=%CEF_VERSION%"
)

REM 详细输出
if "%VERBOSE%"=="true" (
    set "CMAKE_ARGS=%CMAKE_ARGS% -DCMAKE_VERBOSE_MAKEFILE=ON"
)

echo [INFO] CMake参数: %CMAKE_ARGS%

REM 运行CMake
cmake %CMAKE_ARGS% "%PROJECT_ROOT%"
if %errorlevel% neq 0 (
    echo [ERROR] CMake配置失败
    exit /b 1
)

echo [SUCCESS] 构建配置完成

REM 执行构建
echo [INFO] 开始构建...

set "BUILD_ARGS=--build . --config %TARGET%"

if not "%JOBS%"=="" (
    set "BUILD_ARGS=%BUILD_ARGS% -j %JOBS%"
)

if "%VERBOSE%"=="true" (
    set "BUILD_ARGS=%BUILD_ARGS% --verbose"
)

echo [INFO] 构建参数: %BUILD_ARGS%

cmake %BUILD_ARGS%
if %errorlevel% neq 0 (
    echo [ERROR] 构建失败
    exit /b 1
)

echo [SUCCESS] 构建完成

REM 安装
if "%INSTALL%"=="true" (
    echo [INFO] 开始安装...
    cmake --install . --config %TARGET%
    if !errorlevel! neq 0 (
        echo [ERROR] 安装失败
        exit /b 1
    )
    echo [SUCCESS] 安装完成: %INSTALL_DIR%
)

REM 显示构建摘要
echo.
echo [SUCCESS] === 构建摘要 ===
echo [INFO] 平台: Windows
echo [INFO] 架构: %ARCH%
echo [INFO] 构建类型: %TARGET%
echo [INFO] 构建目录: %BUILD_SUBDIR%

if "%INSTALL%"=="true" (
    echo [INFO] 安装目录: %INSTALL_DIR%
)

if not "%CEF_VERSION%"=="" (
    echo [INFO] CEF版本: %CEF_VERSION%
)

echo [SUCCESS] 构建完成！
goto end

:show_help
echo DesktopTerminal-CEF Windows构建脚本
echo.
echo 用法: %~nx0 [选项]
echo.
echo 选项:
echo   -h, --help              显示此帮助信息
echo   -t, --target TARGET     构建目标 (Debug^|Release^|MinSizeRel^|RelWithDebInfo)
echo   -a, --arch ARCH         目标架构 (x86^|x64^|auto)
echo   -j, --jobs JOBS         并行构建任务数 (默认: CPU核心数)
echo   -c, --clean             清理构建目录
echo   -i, --install           构建后安装
echo   --cef-version VERSION   强制使用指定的CEF版本
echo   --qt-dir DIR            Qt安装目录
echo   --verbose               详细输出
echo.
echo 示例:
echo   %~nx0 -t Release -a x64                 # 构建64位Release版本
echo   %~nx0 -t Debug -a x86 --clean           # 清理并构建32位Debug版本
echo   %~nx0 --cef-version 75 -a x86           # 强制使用CEF 75构建32位版本
echo.

:end
endlocal