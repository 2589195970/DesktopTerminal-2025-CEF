@echo off
setlocal enabledelayedexpansion

echo ===========================================
echo CEF文件验证测试脚本 - 修复版本
echo ===========================================
echo.

echo "=== 构建和CEF文件验证阶段 ==="
echo "检查构建产物和CEF文件完整性..."

REM 首先检查是否在正确的目录
echo "当前工作目录: %CD%"

REM 检查多个可能的位置 - 使用标准批处理语法
set "FOUND_EXE=false"

REM 检查第一个位置: build\bin\Release\DesktopTerminal-CEF.exe
set "EXE_PATH1=build\bin\Release\DesktopTerminal-CEF.exe"
if exist "%EXE_PATH1%" (
  echo "[FOUND] 找到可执行文件: %EXE_PATH1%"
  for %%f in ("%EXE_PATH1%") do echo "  文件大小: %%~zf bytes"
  set "FOUND_EXE=true"
  goto :found_exe
) else (
  echo "[NOT FOUND] 未找到: %EXE_PATH1%"
)

REM 检查第二个位置: build\Release\DesktopTerminal-CEF.exe
set "EXE_PATH2=build\Release\DesktopTerminal-CEF.exe"
if exist "%EXE_PATH2%" (
  echo "[FOUND] 找到可执行文件: %EXE_PATH2%"
  for %%f in ("%EXE_PATH2%") do echo "  文件大小: %%~zf bytes"
  set "FOUND_EXE=true"
  goto :found_exe
) else (
  echo "[NOT FOUND] 未找到: %EXE_PATH2%"
)

REM 检查第三个位置: bin\Release\DesktopTerminal-CEF.exe
set "EXE_PATH3=bin\Release\DesktopTerminal-CEF.exe"
if exist "%EXE_PATH3%" (
  echo "[FOUND] 找到可执行文件: %EXE_PATH3%"
  for %%f in ("%EXE_PATH3%") do echo "  文件大小: %%~zf bytes"
  set "FOUND_EXE=true"
  goto :found_exe
) else (
  echo "[NOT FOUND] 未找到: %EXE_PATH3%"
)

:found_exe
if "%FOUND_EXE%"=="true" (
  echo "[OK] 主程序文件找到，开始CEF文件完整性检查..."
  
  REM 检查CEF关键文件是否存在
  set "CEF_FILES_OK=true"
  set "BUILD_DIR="
  
  REM 确定实际的构建目录（按优先级检查已知路径）
  if exist "build\bin\Release" (
    set "BUILD_DIR=build\bin\Release"
    echo "[INFO] 使用标准Release构建目录"
  ) else (
    if exist "build\Release" (
      set "BUILD_DIR=build\Release"
      echo "[INFO] 使用简化Release构建目录"
    ) else (
      if exist "build\bin\Debug" (
        set "BUILD_DIR=build\bin\Debug"
        echo "[INFO] 使用Debug构建目录"
      ) else (
        echo "[WARNING] 未找到标准构建目录，尝试动态检测..."
        for /d %%d in ("build\bin\*") do (
          if exist "%%d\DesktopTerminal-CEF.exe" (
            set "BUILD_DIR=%%d"
            echo "[INFO] 动态检测到构建目录: %%d"
            goto :build_dir_found
          )
        )
      )
    )
  )
  :build_dir_found
  
  REM 验证BUILD_DIR是否正确设置
  if "%BUILD_DIR%"=="" (
    echo "[ERROR] 无法确定构建目录，终止CEF验证"
    echo "[INFO] 可用的构建目录结构："
    if exist "build" (
      dir "build" /b
      if exist "build\bin" (
        echo "build\bin目录内容："
        dir "build\bin" /b
      )
    )
    set "CEF_FILES_OK=false"
    goto :skip_cef_check
  ) else (
    echo "使用构建目录: %BUILD_DIR%"
  )
  
  REM 检查关键CEF文件
  set "CEF_CORE_FILES=libcef.dll cef.pak"
  for %%f in (%CEF_CORE_FILES%) do (
    if exist "%BUILD_DIR%\%%f" (
      echo "[OK] CEF核心文件存在: %%f"
      for %%s in ("%BUILD_DIR%\%%f") do echo "   大小: %%~zs bytes"
    ) else (
      echo "[ERROR] CEF核心文件缺失: %%f"
      set "CEF_FILES_OK=false"
    )
  )
  
  REM 检查locales目录 - 修复版本：简化检查，避免find命令语法错误
  if exist "%BUILD_DIR%\locales" (
    echo "[OK] CEF本地化文件存在"
    echo "   locales目录已验证"
  ) else (
    echo "[ERROR] CEF本地化文件缺失"
    set "CEF_FILES_OK=false"
  )
  
  REM 简化的文件存在性验证（避免复杂的数学运算）- 修复版本
  echo "[INFO] 验证关键文件存在性..."
  if defined BUILD_DIR (
    echo "[INFO] 检查主要文件..."
    
    REM 检查主程序文件
    if exist "%BUILD_DIR%\DesktopTerminal-CEF.exe" (
      for %%f in ("%BUILD_DIR%\DesktopTerminal-CEF.exe") do (
        echo "  [OK] 主程序: %%~zf bytes"
      )
    ) else (
      echo "  [ERROR] 主程序文件缺失"
      set "CEF_FILES_OK=false"
    )
    
    REM 检查CEF核心DLL
    if exist "%BUILD_DIR%\libcef.dll" (
      for %%f in ("%BUILD_DIR%\libcef.dll") do (
        echo "  [OK] CEF核心库: %%~zf bytes"
      )
    ) else (
      echo "  [WARNING] CEF核心库未找到（可能在构建后复制）"
    )
    
    echo "[INFO] 核心文件验证完成"
  ) else (
    echo "[WARNING] 构建目录未确定，跳过文件验证"
  )
  
  :skip_cef_check
  if "%CEF_FILES_OK%"=="true" (
    echo "[SUCCESS] 构建和CEF文件验证成功！"
    echo "[INFO] 验证摘要："
    echo "   - 主程序: OK"
    echo "   - CEF核心文件: OK"
    echo "   - 本地化文件: OK"
    echo "   - 文件验证: 已完成"
    echo "[OK] CEF文件验证通过"
  ) else (
    echo "[ERROR] CEF文件验证失败"
    echo "--- CEF文件诊断信息 ---"
    echo "构建目录: %BUILD_DIR%"
    echo "目录内容："
    dir "%BUILD_DIR%" /b
    echo "DLL文件："
    dir "%BUILD_DIR%\*.dll" /b 2>nul || echo "未找到DLL文件"
    echo "PAK文件："
    dir "%BUILD_DIR%\*.pak" /b 2>nul || echo "未找到PAK文件"
    echo "[ERROR] CEF验证失败"
    exit /b 1
  )
) else (
  echo "[ERROR] 构建验证失败 - 未找到可执行文件"
  echo "--- 详细诊断 ---"
  echo "构建目录完整结构:"
  if exist "build" (
    dir build /s /b | findstr /i "\.exe$" || echo "未找到任何exe文件"
    echo "--- bin目录内容 ---"
    if exist "build\bin" (
      dir "build\bin" /s /b
    ) else (
      echo "build\bin目录不存在"
    )
  ) else (
    echo "build目录不存在！"
  )
  echo "[ERROR] 主程序验证失败"
  exit /b 1
)

echo.
echo "=== CEF验证测试完成 ==="
pause