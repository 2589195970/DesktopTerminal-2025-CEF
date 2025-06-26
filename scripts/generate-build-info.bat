@echo off
setlocal enabledelayedexpansion

REM 参数检查
if "%~1"=="" (
    echo 错误: 未提供输出路径
    exit /b 1
)

set "OUTPUT_PATH=%~1"
set "ARCHITECTURE=%~2"
set "PLATFORM=%~3"
set "CEF_VERSION=%~4"
set "QT_VERSION=%~5"
set "BUILD_TYPE=%~6"
set "COMMIT_SHA=%~7"

REM 获取当前UTC时间
for /f "tokens=*" %%i in ('powershell -Command "Get-Date -Format 'yyyy-MM-dd HH:mm:ss UTC'"') do set "BUILD_DATE=%%i"

REM 生成构建信息文件
echo DesktopTerminal-CEF Build Information > "%OUTPUT_PATH%"
echo ==================================== >> "%OUTPUT_PATH%"
echo Architecture: %ARCHITECTURE% >> "%OUTPUT_PATH%"
echo Platform: %PLATFORM% >> "%OUTPUT_PATH%"
echo CEF Version: %CEF_VERSION% >> "%OUTPUT_PATH%"
echo Qt Version: %QT_VERSION% >> "%OUTPUT_PATH%"
echo Build Type: %BUILD_TYPE% >> "%OUTPUT_PATH%"
echo Build Date: %BUILD_DATE% >> "%OUTPUT_PATH%"
echo Commit: %COMMIT_SHA% >> "%OUTPUT_PATH%"

echo ✅ 构建信息文件已生成: %OUTPUT_PATH% 