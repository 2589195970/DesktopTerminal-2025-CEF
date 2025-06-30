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

REM 全平台统一使用CEF 109版本
:select_cef_version
set "CEF_VERSION=109.1.18+gf1c41e4+chromium-109.0.5414.120"

if "%PLATFORM%"=="windows32" (
    set "CEF_PLATFORM=windows32"
    call :log_info "选择CEF 109.1.18版本 - Windows 32位统一版本"
) else (
    set "CEF_PLATFORM=windows64" 
    call :log_info "选择CEF 109.1.18版本 - Windows 64位统一版本"
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
    call :log_info "使用7-Zip两步解压tar.bz2文件..."
    
    REM 第一步：解压.bz2得到.tar文件
    call :log_info "步骤1: 解压bzip2压缩层..."
    7z x "%TEMP_DIR%\%CEF_ARCHIVE_NAME%" -o"%TEMP_DIR%" -y
    if !errorlevel! neq 0 (
        call :log_error "第一步解压失败（bzip2层）"
        rmdir /s /q "%TEMP_DIR%"
        exit /b 1
    )
    
    REM 检查.tar文件是否存在
    set "TAR_FILE=%TEMP_DIR%\%CEF_BINARY_NAME%.tar"
    if not exist "!TAR_FILE!" (
        call :log_error "未找到解压后的tar文件: !TAR_FILE!"
        call :log_info "临时目录内容："
        dir "%TEMP_DIR%"
        rmdir /s /q "%TEMP_DIR%"
        exit /b 1
    )
    
    REM 第二步：解压.tar文件到最终目录
    call :log_info "步骤2: 解压tar归档到CEF目录..."
    7z x "!TAR_FILE!" -o"%CEF_DIR%" -y
    if !errorlevel! neq 0 (
        call :log_error "第二步解压失败（tar归档）"
        rmdir /s /q "%TEMP_DIR%"
        exit /b 1
    )
    
    call :log_success "7-Zip两步解压完成"
) else (
    REM 尝试使用PowerShell解压（对于tar.bz2格式支持有限）
    call :log_warning "未找到7-Zip，PowerShell对tar.bz2支持有限"
    call :log_info "使用PowerShell解压..."
    powershell -Command "& { Add-Type -AssemblyName System.IO.Compression.FileSystem; try { [System.IO.Compression.ZipFile]::ExtractToDirectory('%TEMP_DIR%\%CEF_ARCHIVE_NAME%', '%CEF_DIR%') } catch { exit 1 } }"
    if !errorlevel! neq 0 (
        call :log_error "PowerShell解压失败，建议安装7-Zip处理tar.bz2文件"
        rmdir /s /q "%TEMP_DIR%"
        exit /b 1
    )
)

REM 清理临时文件
rmdir /s /q "%TEMP_DIR%"

REM 检查解压后的目录结构
call :log_info "检查CEF解压后的目录结构..."
if exist "%CEF_DIR%" (
    call :log_info "CEF目录内容："
    dir "%CEF_DIR%" /b
    
    REM 查找实际解压出的CEF目录
    for /d %%d in ("%CEF_DIR%\cef_binary_*") do (
        call :log_info "找到CEF目录: %%d"
        if exist "%%d\include" (
            call :log_info "  - 包含include目录"
        )
        if exist "%%d\Release" (
            call :log_info "  - 包含Release目录"
        )
        if exist "%%d\Resources" (
            call :log_info "  - 包含Resources目录"
        )
    )
) else (
    call :log_error "CEF目录不存在: %CEF_DIR%"
)

call :log_success "CEF解压完成: %CEF_DIR%\%CEF_BINARY_NAME%"

REM 验证解压结果
call :log_info "验证CEF解压结果..."

REM 多路径验证策略 - 支持不同的CEF目录结构
set "CEF_FOUND=false"
set "CEF_VALID_PATH="

REM 可能的CEF头文件路径（按优先级排序）
set "CEF_CHECK[1]=%CEF_DIR%\%CEF_BINARY_NAME%\include\cef_version.h"
set "CEF_CHECK[2]=%CEF_DIR%\include\cef_version.h"
set "CEF_CHECK[3]=%CEF_DIR%\%CEF_BINARY_NAME%\cef_version.h"

REM 遍历所有可能的路径
for /l %%i in (1,1,3) do (
    if "!CEF_FOUND!"=="false" (
        if exist "!CEF_CHECK[%%i]!" (
            set "CEF_FOUND=true"
            set "CEF_VALID_PATH=!CEF_CHECK[%%i]!"
            call :log_success "CEF头文件验证成功: !CEF_CHECK[%%i]!"
        )
    )
)

REM 如果标准路径都找不到，进行深度搜索
if "!CEF_FOUND!"=="false" (
    call :log_info "标准路径未找到，进行深度搜索..."
    
    REM 检查CEF目录是否存在
    if exist "%CEF_DIR%" (
        call :log_info "搜索CEF目录中的所有cef_version.h文件..."
        
        REM 使用for /r递归搜索cef_version.h
        for /r "%CEF_DIR%" %%f in (cef_version.h) do (
            if "!CEF_FOUND!"=="false" (
                if exist "%%f" (
                    set "CEF_FOUND=true"
                    set "CEF_VALID_PATH=%%f"
                    call :log_success "在深度搜索中找到CEF头文件: %%f"
                )
            )
        )
        
        REM 如果还是找不到，显示详细的目录结构用于诊断
        if "!CEF_FOUND!"=="false" (
            call :log_warning "显示CEF目录结构用于诊断:"
            dir "%CEF_DIR%" /s /b | findstr /i "\.h$" | findstr /i "cef" && (
                call :log_info "找到相关CEF头文件，但不是cef_version.h"
            )
            dir "%CEF_DIR%" /s /b | findstr /i "include" && (
                call :log_info "找到include目录"
            ) || (
                call :log_warning "未找到include目录"
            )
        )
    ) else (
        call :log_error "CEF目录不存在: %CEF_DIR%"
    )
)

REM 增强的CEF库文件验证 - 修复链接错误的关键检查
if "!CEF_FOUND!"=="true" (
    call :log_info "=== 执行CEF库文件深度验证 ==="
    set "CEF_LIB_FOUND=false"
    set "CEF_LIB_PATH="
    set "CEF_LIB_SIZE=0"
    
    REM 扩展的libcef.lib搜索路径
    set "LIB_CHECK[1]=%CEF_DIR%\%CEF_BINARY_NAME%\Release\libcef.lib"
    set "LIB_CHECK[2]=%CEF_DIR%\%CEF_BINARY_NAME%\Debug\libcef.lib"
    set "LIB_CHECK[3]=%CEF_DIR%\Release\libcef.lib"
    set "LIB_CHECK[4]=%CEF_DIR%\Debug\libcef.lib"
    set "LIB_CHECK[5]=%CEF_DIR%\%CEF_BINARY_NAME%\lib\libcef.lib"
    set "LIB_CHECK[6]=%CEF_DIR%\lib\libcef.lib"
    
    call :log_info "搜索CEF主库文件（libcef.lib）..."
    for /l %%i in (1,1,6) do (
        if "!CEF_LIB_FOUND!"=="false" (
            call :log_info "检查路径 %%i: !LIB_CHECK[%%i]!"
            if exist "!LIB_CHECK[%%i]!" (
                REM 验证文件大小（CEF库文件应该比较大）
                for %%f in ("!LIB_CHECK[%%i]!") do set "CEF_LIB_SIZE=%%~zf"
                set /a CEF_LIB_SIZE_MB=!CEF_LIB_SIZE!/1024/1024
                
                REM CEF 75版本使用Import Library，大小通常为30-50KB，这是正常的
                if !CEF_LIB_SIZE! gtr 10000 (
                    set "CEF_LIB_FOUND=true"
                    set "CEF_LIB_PATH=!LIB_CHECK[%%i]!"
                    set /a CEF_LIB_SIZE_KB=!CEF_LIB_SIZE!/1024
                    call :log_success "✅ 找到有效的CEF主库: !LIB_CHECK[%%i]! (!CEF_LIB_SIZE_KB! KB)"
                    call :log_info "注意：CEF 75使用Import Library，文件大小30-50KB是正常的"
                ) else (
                    call :log_warning "⚠️ 文件太小，可能损坏: !LIB_CHECK[%%i]! (!CEF_LIB_SIZE! bytes)"
                )
            ) else (
                call :log_info "  文件不存在"
            )
        )
    )
    
    REM 如果标准路径未找到，执行深度搜索
    if "!CEF_LIB_FOUND!"=="false" (
        call :log_info "标准路径未找到libcef.lib，执行深度搜索..."
        
        REM 递归搜索所有.lib文件
        for /r "%CEF_DIR%" %%f in (*.lib) do (
            if "!CEF_LIB_FOUND!"=="false" (
                for %%g in ("%%f") do (
                    set "LIB_NAME=%%~ng"
                    set "LIB_SIZE=%%~zg"
                )
                
                if "!LIB_NAME!"=="libcef" (
                    if !LIB_SIZE! gtr 10000 (
                        set "CEF_LIB_FOUND=true"
                        set "CEF_LIB_PATH=%%f"
                        set /a CEF_LIB_SIZE_KB=!LIB_SIZE!/1024
                        call :log_success "✅ 深度搜索找到CEF主库: %%f (!CEF_LIB_SIZE_KB! KB)"
                    )
                )
            )
        )
    )
    
    REM 验证CEF Wrapper源码目录
    call :log_info "验证CEF Wrapper源码目录..."
    set "WRAPPER_FOUND=false"
    set "WRAPPER_CHECK[1]=%CEF_DIR%\%CEF_BINARY_NAME%\libcef_dll"
    set "WRAPPER_CHECK[2]=%CEF_DIR%\libcef_dll"
    set "WRAPPER_CHECK[3]=%CEF_DIR%\%CEF_BINARY_NAME%\wrapper"
    
    for /l %%i in (1,1,3) do (
        if "!WRAPPER_FOUND!"=="false" (
            if exist "!WRAPPER_CHECK[%%i]!" (
                REM 检查源码目录是否包含必要的文件
                if exist "!WRAPPER_CHECK[%%i]!\*.cc" (
                    set "WRAPPER_FOUND=true"
                    call :log_success "✅ 找到CEF Wrapper源码: !WRAPPER_CHECK[%%i]!"
                    
                    REM 统计源码文件数量
                    set "CC_COUNT=0"
                    for %%g in ("!WRAPPER_CHECK[%%i]!\*.cc") do set /a CC_COUNT+=1
                    call :log_info "  包含 !CC_COUNT! 个源码文件"
                ) else (
                    call :log_warning "⚠️ 目录存在但缺少源码文件: !WRAPPER_CHECK[%%i]!"
                )
            )
        )
    )
    
    REM 检查关键的CEF二进制文件
    call :log_info "验证CEF运行时文件..."
    set "CEF_RUNTIME_FOUND=false"
    set "RUNTIME_PATHS[1]=%CEF_DIR%\%CEF_BINARY_NAME%\Release"
    set "RUNTIME_PATHS[2]=%CEF_DIR%\%CEF_BINARY_NAME%\Debug"
    set "RUNTIME_PATHS[3]=%CEF_DIR%\Release"
    
    for /l %%i in (1,1,3) do (
        if "!CEF_RUNTIME_FOUND!"=="false" (
            if exist "!RUNTIME_PATHS[%%i]!\libcef.dll" (
                set "CEF_RUNTIME_FOUND=true"
                call :log_success "✅ 找到CEF运行时: !RUNTIME_PATHS[%%i]!\libcef.dll"
            )
        )
    )
    
    REM 综合验证结果和详细诊断
    call :log_info "=== CEF验证结果汇总 ==="
    if "!CEF_LIB_FOUND!"=="true" (
        call :log_success "✅ CEF主库文件: OK (!CEF_LIB_PATH!)"
    ) else (
        call :log_error "❌ CEF主库文件: 缺失或损坏"
        call :log_error "这是导致LNK2019链接错误的主要原因！"
    )
    
    if "!WRAPPER_FOUND!"=="true" (
        call :log_success "✅ CEF Wrapper源码: OK"
    ) else (
        call :log_warning "⚠️ CEF Wrapper源码: 缺失（将尝试从源码编译）"
    )
    
    if "!CEF_RUNTIME_FOUND!"=="true" (
        call :log_success "✅ CEF运行时文件: OK"
    ) else (
        call :log_warning "⚠️ CEF运行时文件: 缺失（可能影响应用程序运行）"
    )
    
    REM 最终判断
    if "!CEF_LIB_FOUND!"=="true" (
        call :log_success "🎉 CEF库验证基本通过！应该可以成功编译。"
        call :log_info "主要文件位置:"
        call :log_info "  头文件: !CEF_VALID_PATH!"
        set /a CEF_LIB_SIZE_FINAL_KB=!CEF_LIB_SIZE!/1024
        call :log_info "  主库: !CEF_LIB_PATH! (!CEF_LIB_SIZE_FINAL_KB! KB - Import Library)"
    ) else (
        call :log_error "💥 CEF库验证失败！这将导致编译时出现LNK2019错误。"
        call :log_error ""
        call :log_error "问题诊断:"
        call :log_error "1. libcef.lib文件完全缺失"
        call :log_error "2. 文件下载不完整或损坏"
        call :log_error "3. CEF包解压失败"
        call :log_error "4. 版本不匹配或目录结构异常"
        call :log_error ""
        call :log_error "建议解决方案:"
        call :log_error "1. 删除CEF目录: rmdir /s /q %CEF_DIR%"
        call :log_error "2. 重新运行此脚本下载CEF"
        call :log_error "3. 检查网络连接和下载完整性"
        call :log_error "4. 确认有足够的磁盘空间"
        set "CEF_FOUND=false"
    )
)

REM 最终验证结果
if "!CEF_FOUND!"=="true" (
    call :log_success "CEF验证成功！有效路径: !CEF_VALID_PATH!"
) else (
    call :log_error "CEF验证失败：关键文件缺失"
    call :log_error "缺失的文件："
    call :log_error "1. CEF头文件 (cef_version.h)"
    call :log_error "2. CEF主库文件 (libcef.lib)"
    call :log_error "可能的原因："
    call :log_error "1. CEF解压不完整"
    call :log_error "2. CEF版本的目录结构与预期不符"
    call :log_error "3. 下载的CEF包损坏"
    call :log_error "4. CEF库文件路径配置错误"
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