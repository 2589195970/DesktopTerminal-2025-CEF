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
    REM Windows 32位使用CEF 75.1.14 - 确认可用的Windows 7 SP1支持版本
    set "CEF_VERSION=75.1.14+gc81164e+chromium-75.0.3770.100"
    set "CEF_PLATFORM=windows32"
    call :log_info "选择CEF 75.1.14版本 - 支持Windows 7 SP1 32位"
) else (
    REM Windows 64位使用较新版本
    set "CEF_VERSION=118.7.1+g99817d2+chromium-118.0.5993.119"
    set "CEF_PLATFORM=windows64"
    call :log_info "选择CEF 118.7.1版本 - Windows 64位"
)

set "CEF_BINARY_NAME=cef_binary_!CEF_VERSION!_!CEF_PLATFORM!"
set "CEF_ARCHIVE_NAME=!CEF_BINARY_NAME!.tar.bz2"
goto :eof

REM 构建下载URL
:build_download_url
REM 所有CEF版本统一使用Spotify CDN下载，需要URL编码特殊字符
set "CEF_ARCHIVE_NAME_ENCODED=!CEF_ARCHIVE_NAME!"
REM 将+号替换为%2B进行URL编码
set "CEF_ARCHIVE_NAME_ENCODED=!CEF_ARCHIVE_NAME_ENCODED:+=%%2B!"
set "DOWNLOAD_URL=https://cef-builds.spotifycdn.com/!CEF_ARCHIVE_NAME_ENCODED!"
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
    REM 使用PowerShell下载，带重试机制
    call :log_info "使用PowerShell下载，最多重试3次..."
    set "DOWNLOAD_SUCCESS=false"
    for /l %%i in (1,1,3) do (
        if "!DOWNLOAD_SUCCESS!"=="false" (
            call :log_info "尝试下载 (%%i/3)..."
            powershell -Command "& { [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12; try { Invoke-WebRequest -Uri '%DOWNLOAD_URL%' -OutFile '%TEMP_DIR%\%CEF_ARCHIVE_NAME%' -UserAgent 'Mozilla/5.0' -TimeoutSec 300 -UseBasicParsing; exit 0 } catch { Write-Host $_.Exception.Message; exit 1 } }"
            if !errorlevel! equ 0 (
                set "DOWNLOAD_SUCCESS=true"
                call :log_success "下载成功！"
            ) else (
                call :log_warning "下载失败，等待5秒后重试..."
                ping -n 6 127.0.0.1 >nul
            )
        )
    )
    if "!DOWNLOAD_SUCCESS!"=="false" (
        call :log_error "PowerShell下载失败，已重试3次"
        rmdir /s /q "%TEMP_DIR%"
        exit /b 1
    )
) else (
    call :log_error "需要PowerShell支持，请升级到Windows 7 SP1或更高版本"
    rmdir /s /q "%TEMP_DIR%"
    exit /b 1
)

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
call :log_info "验证CEF解压结果..."
set "CEF_INCLUDE_CHECK1=%CEF_DIR%\%CEF_BINARY_NAME%\include\cef_version.h"
set "CEF_INCLUDE_CHECK2=%CEF_DIR%\include\cef_version.h"
set "CEF_INCLUDE_CHECK3=%CEF_DIR%\%CEF_BINARY_NAME%\cef_version.h"

if exist "!CEF_INCLUDE_CHECK1!" (
    call :log_success "CEF头文件验证成功: !CEF_INCLUDE_CHECK1!"
) else if exist "!CEF_INCLUDE_CHECK2!" (
    call :log_success "CEF头文件验证成功: !CEF_INCLUDE_CHECK2!"
) else if exist "!CEF_INCLUDE_CHECK3!" (
    call :log_success "CEF头文件验证成功: !CEF_INCLUDE_CHECK3!"
) else (
    call :log_error "CEF解压后未找到cef_version.h文件"
    call :log_info "检查解压目录结构:"
    if exist "%CEF_DIR%" (
        dir "%CEF_DIR%" /s | findstr /i "cef_version.h" || call :log_warning "未找到cef_version.h文件"
        dir "%CEF_DIR%" /s | findstr /i "include" || call :log_warning "未找到include目录"
    )
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