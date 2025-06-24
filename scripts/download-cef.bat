@echo off
REM CEF自动下载脚本 - Windows版本
REM 支持32位Windows 7 SP1和现代64位系统

setlocal enabledelayedexpansion

REM 设置脚本目录
set "SCRIPT_DIR=%~dp0"
set "PROJECT_ROOT=%SCRIPT_DIR%.."
set "CEF_DIR=%PROJECT_ROOT%\third_party\cef"

REM 日志函数定义
goto :main

:log_info
echo [INFO] %~1
goto :eof

:log_success
echo [SUCCESS] %~1
goto :eof

:log_warning
echo [WARNING] %~1
goto :eof

:log_error
echo [ERROR] %~1
goto :eof

REM 检测系统架构
:detect_platform
call :log_info "检测系统架构..."

REM 检测处理器架构
if "%PROCESSOR_ARCHITECTURE%"=="x86" (
    if "%PROCESSOR_ARCHITEW6432%"=="AMD64" (
        set "PLATFORM=windows64"
        set "ARCH_TYPE=64位 (WOW64)"
    ) else (
        set "PLATFORM=windows32"
        set "ARCH_TYPE=32位"
    )
) else if "%PROCESSOR_ARCHITECTURE%"=="AMD64" (
    set "PLATFORM=windows64"
    set "ARCH_TYPE=64位"
) else (
    call :log_warning "未知架构: %PROCESSOR_ARCHITECTURE%，默认使用64位"
    set "PLATFORM=windows64"
    set "ARCH_TYPE=64位(默认)"
)

call :log_info "检测到平台: Windows %ARCH_TYPE% -> %PLATFORM%"
goto :eof

REM 根据平台选择CEF版本
:select_cef_version
if "%PLATFORM%"=="windows32" (
    REM Windows 32位使用CEF 75 - 最后支持Windows 7 SP1的版本
    set "CEF_VERSION=75.1.16+g16a67c4+chromium-75.0.3770.100"
    set "CEF_PLATFORM=windows32"
    call :log_info "选择CEF 75版本 - 支持Windows 7 SP1 32位"
) else (
    REM Windows 64位使用较新版本
    set "CEF_VERSION=118.6.8+g1e19f4c+chromium-118.0.5993.119"
    set "CEF_PLATFORM=windows64"
    call :log_info "选择CEF 118版本 - Windows 64位"
)

set "CEF_BINARY_NAME=cef_binary_!CEF_VERSION!_!CEF_PLATFORM!"
set "CEF_ARCHIVE_NAME=!CEF_BINARY_NAME!.tar.bz2"
goto :eof

REM 构建下载URL
:build_download_url
REM 根据版本构建不同的URL
echo !CEF_VERSION! | findstr "^75\." >nul
if !errorlevel! equ 0 (
    REM CEF 75版本使用旧的URL格式
    set "DOWNLOAD_URL=https://cef-builds.spotifycdn.com/!CEF_ARCHIVE_NAME!"
) else (
    REM 较新版本使用GitHub releases
    for /f "tokens=1 delims=." %%a in ("!CEF_VERSION!") do set "CEF_MAJOR_VERSION=%%a"
    set "DOWNLOAD_URL=https://github.com/chromiumembedded/cef/releases/download/v!CEF_MAJOR_VERSION!.0.0/!CEF_ARCHIVE_NAME!"
)

call :log_info "下载URL: !DOWNLOAD_URL!"
goto :eof

REM 检查CEF是否已存在
:check_existing_cef
set "CEF_INSTALL_DIR=%CEF_DIR%\%CEF_BINARY_NAME%"

if exist "%CEF_INSTALL_DIR%\include\cef_version.h" (
    call :log_success "CEF已存在: %CEF_INSTALL_DIR%"
    call :log_info "如需重新下载，请删除该目录后重新运行脚本"
    exit /b 0
)

exit /b 1

REM 下载CEF
:download_cef
call :log_info "开始下载CEF %CEF_VERSION%..."

REM 创建临时目录
set "TEMP_DIR=%TEMP%\cef_download_%RANDOM%"
mkdir "%TEMP_DIR%"

REM 下载文件
call :log_info "下载 %CEF_ARCHIVE_NAME%..."

REM 检查是否有PowerShell
powershell -Command "exit" >nul 2>&1
if !errorlevel! equ 0 (
    REM 使用PowerShell下载
    call :log_info "使用PowerShell下载..."
    powershell -Command "& { [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12; Invoke-WebRequest -Uri '%DOWNLOAD_URL%' -OutFile '%TEMP_DIR%\%CEF_ARCHIVE_NAME%' -UserAgent 'Mozilla/5.0' }"
    if !errorlevel! neq 0 (
        call :log_error "PowerShell下载失败"
        rmdir /s /q "%TEMP_DIR%"
        exit /b 1
    )
) else (
    call :log_error "需要PowerShell支持，请升级到Windows 7 SP1或更高版本"
    rmdir /s /q "%TEMP_DIR%"
    exit /b 1
)

call :log_success "下载完成"

REM 验证下载的文件
if not exist "%TEMP_DIR%\%CEF_ARCHIVE_NAME%" (
    call :log_error "下载的文件不存在"
    rmdir /s /q "%TEMP_DIR%"
    exit /b 1
)

REM 检查文件大小
for %%I in ("%TEMP_DIR%\%CEF_ARCHIVE_NAME%") do set "FILE_SIZE=%%~zI"
if !FILE_SIZE! lss 10000000 (
    call :log_error "下载的文件太小，可能下载失败"
    rmdir /s /q "%TEMP_DIR%"
    exit /b 1
)

set /a FILE_SIZE_MB=!FILE_SIZE!/1024/1024
call :log_info "文件大小: !FILE_SIZE_MB!MB"

REM 解压文件
call :log_info "解压CEF二进制包..."
if not exist "%CEF_DIR%" mkdir "%CEF_DIR%"

REM 检查是否有7-Zip
where 7z >nul 2>&1
if !errorlevel! equ 0 (
    call :log_info "使用7-Zip解压..."
    7z x "%TEMP_DIR%\%CEF_ARCHIVE_NAME%" -o"%CEF_DIR%" -y
    if !errorlevel! neq 0 (
        call :log_error "7-Zip解压失败"
        rmdir /s /q "%TEMP_DIR%"
        exit /b 1
    )
) else (
    REM 尝试使用PowerShell解压
    call :log_info "使用PowerShell解压..."
    powershell -Command "& { Add-Type -AssemblyName System.IO.Compression.FileSystem; try { [System.IO.Compression.ZipFile]::ExtractToDirectory('%TEMP_DIR%\%CEF_ARCHIVE_NAME%', '%CEF_DIR%') } catch { exit 1 } }"
    if !errorlevel! neq 0 (
        call :log_error "解压失败，请安装7-Zip或确保PowerShell版本支持"
        rmdir /s /q "%TEMP_DIR%"
        exit /b 1
    )
)

REM 清理临时文件
rmdir /s /q "%TEMP_DIR%"

call :log_success "CEF解压完成: %CEF_DIR%\%CEF_BINARY_NAME%"

REM 验证解压结果
if not exist "%CEF_DIR%\%CEF_BINARY_NAME%\include\cef_version.h" (
    call :log_error "CEF解压后文件不完整"
    exit /b 1
)

exit /b 0

REM 显示使用帮助
:show_help
echo CEF下载脚本 - Windows版本
echo 用法: %~nx0 [选项]
echo.
echo 选项:
echo   /f, /force     强制重新下载，即使CEF已存在
echo   /h, /help      显示此帮助信息
echo   /platform=PLATFORM  指定平台 (windows32, windows64)
echo.
echo 示例:
echo   %~nx0                # 自动检测平台并下载对应版本
echo   %~nx0 /force        # 强制重新下载
echo   %~nx0 /platform=windows32  # 下载Windows 32位版本
goto :eof

REM 主函数
:main
set "FORCE_DOWNLOAD=false"
set "CUSTOM_PLATFORM="

REM 解析命令行参数
:parse_args
if "%~1"=="" goto :start_download
if /i "%~1"=="/f" set "FORCE_DOWNLOAD=true" & shift & goto :parse_args
if /i "%~1"=="/force" set "FORCE_DOWNLOAD=true" & shift & goto :parse_args
if /i "%~1"=="/h" goto :show_help_and_exit
if /i "%~1"=="/help" goto :show_help_and_exit
if "%~1"=="/platform=windows32" set "CUSTOM_PLATFORM=windows32" & shift & goto :parse_args
if "%~1"=="/platform=windows64" set "CUSTOM_PLATFORM=windows64" & shift & goto :parse_args
call :log_error "未知参数: %~1"
goto :show_help_and_exit

:show_help_and_exit
call :show_help
exit /b 1

:start_download
call :log_info "开始CEF下载流程..."

REM 检测或使用自定义平台
if not "%CUSTOM_PLATFORM%"=="" (
    set "PLATFORM=%CUSTOM_PLATFORM%"
    call :log_info "使用指定平台: !PLATFORM!"
) else (
    call :detect_platform
)

REM 选择CEF版本
call :select_cef_version

REM 构建下载URL
call :build_download_url

REM 检查是否已存在
if "%FORCE_DOWNLOAD%"=="false" (
    call :check_existing_cef
    if !errorlevel! equ 0 exit /b 0
)

REM 下载CEF
call :download_cef
if !errorlevel! equ 0 (
    call :log_success "CEF下载和安装成功！"
    call :log_info "CEF路径: %CEF_DIR%\%CEF_BINARY_NAME%"
    call :log_info "现在可以运行CMake构建项目了"
) else (
    call :log_error "CEF下载失败"
    exit /b 1
)

endlocal