@echo off
REM ======================================================================
REM CEF 109.1.18 下载脚本 (Windows)
REM 用于CEF 75->109迁移测试
REM ======================================================================

echo [INFO] CEF 109.1.18 下载脚本启动...
echo [INFO] 目标版本: 109.1.18+g97a8d9e+chromium-109.0.5414.120
echo.

REM 设置CEF版本和平台
set "CEF109_VERSION=109.1.18+g97a8d9e+chromium-109.0.5414.120"

REM 检测系统架构
if "%PROCESSOR_ARCHITECTURE%"=="AMD64" (
    set "CEF109_PLATFORM=windows64"
    echo [INFO] 检测到64位Windows系统
) else (
    set "CEF109_PLATFORM=windows32"
    echo [INFO] 检测到32位Windows系统
)

REM 构建下载文件名和URL
set "CEF109_FILENAME=cef_binary_%CEF109_VERSION%_%CEF109_PLATFORM%.tar.bz2"
set "CEF109_URL=https://cef-builds.spotifycdn.com/%CEF109_FILENAME%"

REM 设置目录路径
set "SCRIPT_DIR=%~dp0"
set "PROJECT_ROOT=%SCRIPT_DIR%.."
set "CEF109_DIR=%PROJECT_ROOT%\third_party\cef109"
set "DOWNLOAD_DIR=%CEF109_DIR%\downloads"
set "EXTRACT_DIR=%CEF109_DIR%"

echo [INFO] 项目根目录: %PROJECT_ROOT%
echo [INFO] CEF109目标目录: %CEF109_DIR%
echo [INFO] 下载URL: %CEF109_URL%
echo.

REM 创建目录
echo [STEP] 创建CEF109目录结构...
if not exist "%CEF109_DIR%" mkdir "%CEF109_DIR%"
if not exist "%DOWNLOAD_DIR%" mkdir "%DOWNLOAD_DIR%"

REM 检查是否已经下载
set "DOWNLOAD_FILE=%DOWNLOAD_DIR%\%CEF109_FILENAME%"
if exist "%DOWNLOAD_FILE%" (
    echo [INFO] CEF109文件已存在: %DOWNLOAD_FILE%
    goto :extract
)

REM 下载CEF109
echo [STEP] 正在下载CEF 109.1.18...
echo [INFO] 这可能需要几分钟，CEF109文件约200MB
echo.

REM 使用PowerShell下载（Windows 7+兼容）
powershell -Command "& {[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12; try { Invoke-WebRequest -Uri '%CEF109_URL%' -OutFile '%DOWNLOAD_FILE%' -UserAgent 'CEF-Downloader/1.0'; Write-Host '[SUCCESS] CEF109下载完成'; } catch { Write-Host '[ERROR] 下载失败:' $_.Exception.Message; exit 1; }}"

if %ERRORLEVEL% neq 0 (
    echo [ERROR] CEF109下载失败！
    echo [INFO] 可能的原因：
    echo   - 网络连接问题
    echo   - CEF版本URL无效
    echo   - 磁盘空间不足
    goto :error
)

:extract
REM 检查下载文件
if not exist "%DOWNLOAD_FILE%" (
    echo [ERROR] 下载文件不存在: %DOWNLOAD_FILE%
    goto :error
)

echo [STEP] 正在解压CEF109...

REM 使用tar命令解压（Windows 10+内置）
where tar >nul 2>&1
if %ERRORLEVEL% equ 0 (
    echo [INFO] 使用Windows内置tar命令解压...
    cd /d "%EXTRACT_DIR%"
    tar -xf "%DOWNLOAD_FILE%"
    if %ERRORLEVEL% neq 0 (
        echo [ERROR] tar解压失败
        goto :error
    )
) else (
    echo [INFO] 系统未找到tar命令，尝试使用PowerShell解压...
    REM 使用PowerShell和.NET Framework解压
    powershell -Command "& {Add-Type -AssemblyName System.IO.Compression.FileSystem; try { [System.IO.Compression.ZipFile]::ExtractToDirectory('%DOWNLOAD_FILE%', '%EXTRACT_DIR%'); Write-Host '[SUCCESS] 解压完成'; } catch { Write-Host '[ERROR] PowerShell解压失败:' $_.Exception.Message; exit 1; }}"
    
    if %ERRORLEVEL% neq 0 (
        echo [WARNING] PowerShell解压失败，需要手动解压
        echo [INFO] 请手动解压文件: %DOWNLOAD_FILE%
        echo [INFO] 解压到目录: %EXTRACT_DIR%
        goto :manual_extract
    )
)

REM 验证解压结果
echo [STEP] 验证CEF109解压结果...

REM 查找解压后的CEF目录
set "CEF109_BINARY_DIR="
for /d %%d in ("%EXTRACT_DIR%\cef_binary_*_%CEF109_PLATFORM%") do (
    set "CEF109_BINARY_DIR=%%d"
)

if "%CEF109_BINARY_DIR%"=="" (
    echo [ERROR] 未找到解压后的CEF109目录
    echo [INFO] 请检查解压过程是否成功
    goto :error
)

echo [INFO] 找到CEF109目录: %CEF109_BINARY_DIR%

REM 验证关键文件
set "CEF109_INCLUDE_DIR=%CEF109_BINARY_DIR%\include"
set "CEF109_RELEASE_DIR=%CEF109_BINARY_DIR%\Release"

if not exist "%CEF109_INCLUDE_DIR%\cef_version.h" (
    echo [ERROR] 关键头文件缺失: cef_version.h
    goto :error
)

if not exist "%CEF109_RELEASE_DIR%\libcef.lib" (
    echo [WARNING] libcef.lib未找到，可能需要重新下载
)

echo [SUCCESS] CEF 109.1.18 安装完成！
echo.
echo [INFO] 安装摘要:
echo   - CEF版本: %CEF109_VERSION%
echo   - 平台: %CEF109_PLATFORM%
echo   - 安装路径: %CEF109_BINARY_DIR%
echo   - 头文件: %CEF109_INCLUDE_DIR%
echo   - 库文件: %CEF109_RELEASE_DIR%
echo.
echo [INFO] 使用CEF109测试构建:
echo   mkdir build-cef109 ^&^& cd build-cef109
echo   cmake .. -DUSE_CEF109=ON
echo   cmake --build . --config Release
echo.
goto :end

:manual_extract
echo [INFO] 手动解压指导:
echo   1. 使用7-Zip或WinRAR等工具
echo   2. 解压文件: %DOWNLOAD_FILE%
echo   3. 解压到: %EXTRACT_DIR%
echo   4. 确保解压后有cef_binary_*目录
goto :end

:error
echo [ERROR] CEF109安装失败！
echo [INFO] 请检查错误信息并重试
exit /b 1

:end
echo [INFO] CEF109下载脚本完成
pause