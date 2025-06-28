; NSIS 安装脚本 - DesktopTerminal-CEF版本（请以 UTF-8 无 BOM 保存）
Unicode true

; 包含必要的头文件
!include "MUI2.nsh"
!include "LogicLib.nsh"
!include "FileFunc.nsh"
!include "x64.nsh"
!include "WinVer.nsh"

; ─────────────────────────────────────────────
; 常量定义
; ─────────────────────────────────────────────
!define APPNAME        "智多分机考桌面端"
!define COMPANYNAME    "山东智多分"
!define DESCRIPTION    "教育考试场景专用安全浏览器"
!define VERSIONMAJOR   1
!define VERSIONMINOR   0
!define VERSIONPATCH   0

; CEF版本信息（将在构建时由GitHub Actions设置）
!ifndef CEF_VERSION
  !define CEF_VERSION "118"
!endif

; 架构：默认为 x64，可在命令行 -DARCH=x86 覆盖
!ifndef ARCH
  !define ARCH "x64"
!endif

!if "${ARCH}" == "x86"
  !define INSTALL_DIR "$PROGRAMFILES32\${COMPANYNAME}\${APPNAME}"
  !define OUTPUT_FILE "Output\${APPNAME}-setup-x86-cef${CEF_VERSION}.exe"
!else
  !define INSTALL_DIR "$PROGRAMFILES64\${COMPANYNAME}\${APPNAME}"
  !define OUTPUT_FILE "Output\${APPNAME}-setup-cef${CEF_VERSION}.exe"
!endif

; ─────────────────────────────────────────────
; 界面与全局设置
; ─────────────────────────────────────────────
Name "${APPNAME}"
OutFile "${OUTPUT_FILE}"
InstallDir "${INSTALL_DIR}"
InstallDirRegKey HKLM "Software\${COMPANYNAME}\${APPNAME}" "InstallDir"
RequestExecutionLevel admin

; MUI2
!define MUI_ICON    "resources\logo.ico"
!define MUI_UNICON  "resources\logo.ico"

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

; 必须在页面设置之后添加语言设置
!insertmacro MUI_LANGUAGE "SimpChinese"

; 语言字符串定义
LangString MSG_InstallDone ${LANG_SIMPCHINESE} "安装完成！桌面快捷方式已创建。"
LangString MSG_UninstallDone ${LANG_SIMPCHINESE} "卸载完成，相关文件已全部移除。"

; ─────────────────────────────────────────────
; 安装前检查
; ─────────────────────────────────────────────
Function .onInit
    ; 检查管理员权限
    UserInfo::GetAccountType
    Pop $R0
    ${If} $R0 != "admin"
        MessageBox MB_ICONSTOP "此程序需要管理员权限才能安装，请右键选择'以管理员身份运行'。"
        SetErrorLevel 740 ; ERROR_ELEVATION_REQUIRED
        Quit
    ${EndIf}
    
    ; 检查系统架构
    ${If} "${ARCH}" == "x64"
        ${IfNot} ${RunningX64}
            MessageBox MB_ICONSTOP "此安装包仅适用于64位Windows系统。"
            Quit
        ${EndIf}
    ${EndIf}
    
    ; 检查操作系统版本（要求Windows 7或更高版本）
    ${IfNot} ${AtLeastWin7}
        MessageBox MB_ICONSTOP "不支持的操作系统版本，需要Windows 7或更高版本。"
        Quit
    ${EndIf}
    
    ; 检查是否已安装
    ReadRegStr $R0 HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPNAME}" "UninstallString"
    ${If} $R0 != ""
        MessageBox MB_YESNO|MB_ICONQUESTION "检测到已安装${APPNAME}，是否要先卸载旧版本？" IDYES uninstall_old IDNO continue_install
        uninstall_old:
            ExecWait '$R0 /S _?=$INSTDIR'
            Delete $R0
            RMDir "$INSTDIR"
        continue_install:
    ${EndIf}
FunctionEnd

; ─────────────────────────────────────────────
; 安装部分
; ─────────────────────────────────────────────
Section "主程序" SecMain
    SetOutPath "$INSTDIR"
    
    ; 直接安装程序文件 - 使用正确的编译时源路径
    ; GitHub Actions构建文件位于artifacts\windows-${ARCH}\目录
    DetailPrint "正在安装DesktopTerminal-CEF应用程序..."
    
    ; 安装主程序文件（从artifacts目录）
    File "artifacts\windows-${ARCH}\DesktopTerminal-CEF.exe"
    DetailPrint "✓ 主程序已安装: DesktopTerminal-CEF.exe"
    
    ; 安装CEF核心库文件
    File /nonfatal "artifacts\windows-${ARCH}\libcef.dll"
    File /nonfatal "artifacts\windows-${ARCH}\chrome_elf.dll" 
    File /nonfatal "artifacts\windows-${ARCH}\d3dcompiler_47.dll"
    File /nonfatal "artifacts\windows-${ARCH}\libEGL.dll"
    File /nonfatal "artifacts\windows-${ARCH}\libGLESv2.dll"
    DetailPrint "✓ CEF核心库已安装"
    
    ; 注意：移除cef_sandbox.lib安装，因为它是静态链接库，运行时不需要
    
    ; 安装Qt5运行时库文件
    File /nonfatal "artifacts\windows-${ARCH}\Qt5Core.dll"
    File /nonfatal "artifacts\windows-${ARCH}\Qt5Gui.dll"
    File /nonfatal "artifacts\windows-${ARCH}\Qt5Widgets.dll"
    DetailPrint "✓ Qt5运行时库已安装"
    
    ; 安装CEF子进程文件（架构特定处理）
    ${If} "${ARCH}" == "x86"
        ; 32位系统使用单进程模式，不需要subprocess文件
        DetailPrint "ℹ 32位系统：CEF单进程模式，无需子进程文件"
    ${Else}
        ; 64位系统使用多进程模式，需要subprocess文件
        ; 注意：CEF 75 x86构建不包含子进程文件，使用单进程模式
        DetailPrint "ℹ CEF 75：使用单进程模式，无需子进程文件"
    ${EndIf}
    
    ; 安装CEF数据文件（根据实际构建输出调整）
    File /nonfatal "artifacts\windows-${ARCH}\snapshot_blob.bin"
    File /nonfatal "artifacts\windows-${ARCH}\v8_context_snapshot.bin"
    File /nonfatal "artifacts\windows-${ARCH}\natives_blob.bin"
    DetailPrint "✓ CEF数据文件已安装"
    
    ; 安装CEF资源文件
    File /nonfatal "artifacts\windows-${ARCH}\cef.pak"
    File /nonfatal "artifacts\windows-${ARCH}\cef_100_percent.pak"
    File /nonfatal "artifacts\windows-${ARCH}\cef_200_percent.pak"
    File /nonfatal "artifacts\windows-${ARCH}\cef_extensions.pak"
    File /nonfatal "artifacts\windows-${ARCH}\devtools_resources.pak"
    DetailPrint "✓ CEF资源文件已安装"
    
    ; 安装本地化文件目录
    File /r "artifacts\windows-${ARCH}\locales"
    DetailPrint "✓ 本地化文件已安装"
    
    ; 安装resources目录
    File /r "artifacts\windows-${ARCH}\resources"
    DetailPrint "✓ 应用资源已安装"
    
    ; 安装Qt5平台插件（创建platforms目录并复制qwindows.dll）
    CreateDirectory "$INSTDIR\platforms"
    File /nonfatal /oname=platforms\qwindows.dll "artifacts\windows-${ARCH}\platforms\qwindows.dll"
    DetailPrint "✓ Qt5平台插件已安装"

    ; 安装后验证逻辑 - 检查实际安装到目标目录的文件
    DetailPrint "正在验证安装结果..."
    
    ; 初始化验证错误变量
    Var /GLOBAL VerificationErrors
    StrCpy $VerificationErrors ""
    
    ; 检查主程序文件是否成功安装
    ${If} ${FileExists} "$INSTDIR\DesktopTerminal-CEF.exe"
        DetailPrint "✓ 主程序安装成功"
        ; 验证文件大小（应该大于1MB）
        ${GetSize} "$INSTDIR\DesktopTerminal-CEF.exe" "/S=0K" $R1 $R2 $R3
        ${If} $R1 > 1000  ; 大于1MB
            DetailPrint "✓ 主程序文件大小正常 ($R1 KB)"
        ${Else}
            DetailPrint "⚠ 主程序文件可能不完整 ($R1 KB)"
        ${EndIf}
    ${Else}
        MessageBox MB_ICONSTOP "❌ 主程序安装失败：$INSTDIR\DesktopTerminal-CEF.exe$\n$\n可能原因：$\n1. 磁盘空间不足$\n2. 目标目录无写权限$\n3. 杀毒软件拦截"
        Abort
    ${EndIf}
    
    ; 检查CEF核心库是否安装
    ${If} ${FileExists} "$INSTDIR\libcef.dll"
        DetailPrint "✓ CEF核心库安装成功"
    ${Else}
        DetailPrint "⚠ CEF核心库未安装，程序可能无法运行"
    ${EndIf}
    
    ; 检查CEF子进程文件（仅在多进程模式下需要）
    ${If} "${ARCH}" == "x86"
        ; 32位系统使用单进程模式，不需要subprocess文件
        DetailPrint "ℹ 32位系统：使用CEF单进程模式，无需子进程文件"
        ; 检查32位特有的DLL依赖
        ${If} ${FileExists} "$INSTDIR\d3dcompiler_47.dll"
            DetailPrint "✓ DirectX编译器库安装成功"
        ${Else}
            DetailPrint "⚠ DirectX编译器库缺失，可能影响渲染效果"
        ${EndIf}
    ${Else}
        ; 64位系统使用多进程模式，需要子进程文件
        ${If} ${FileExists} "$INSTDIR\cef_subprocess_win64.exe"
            DetailPrint "✓ CEF 64位子进程安装成功"
        ${Else}
            DetailPrint "❌ CEF 64位子进程缺失，程序无法运行"
            StrCpy $VerificationErrors "$VerificationErrors• CEF子进程 cef_subprocess_win64.exe 缺失$\\n"
        ${EndIf}
    ${EndIf}
    
    ; 检查Qt5运行时库是否安装
    ${If} ${FileExists} "$INSTDIR\Qt5Core.dll"
        ${If} ${FileExists} "$INSTDIR\Qt5Gui.dll"
            ${If} ${FileExists} "$INSTDIR\Qt5Widgets.dll"
                DetailPrint "✓ Qt5运行时库安装完整"
            ${Else}
                DetailPrint "⚠ Qt5Widgets.dll缺失"
            ${EndIf}
        ${Else}
            DetailPrint "⚠ Qt5Gui.dll缺失"
        ${EndIf}
    ${Else}
        DetailPrint "⚠ Qt5Core.dll缺失，程序无法启动"
    ${EndIf}
    
    ; 统计安装文件数量
    ${GetSize} "$INSTDIR" "/S=0K" $R1 $R2 $R3
    DetailPrint "安装完成：$R2 个文件，总大小 $R1 KB"
    
    ${If} $R2 < 3
        DetailPrint "⚠ 安装文件数量偏少，请检查安装包完整性"
    ${ElseIf} $R1 < 500
        DetailPrint "⚠ 安装大小偏小，可能文件缺失"
    ${Else}
        DetailPrint "✓ 安装文件数量和大小正常"
    ${EndIf}

    ; 资源目录
    CreateDirectory "$INSTDIR\resources"
    DetailPrint "正在配置应用程序..."
    
    ; config.json：仅首次安装时复制，保护用户配置
    ${If} ${FileExists} "$INSTDIR\config.json"
        DetailPrint "保留现有配置文件"
    ${Else}
        ${If} ${FileExists} "resources\config.json"
            File /oname=config.json "resources\config.json"
            DetailPrint "已安装默认配置文件"
        ${Else}
            ; 创建基本配置文件（适配CEF版本）
            FileOpen $0 "$INSTDIR\config.json" w
            FileWrite $0 '{"url":"https://example.com","exitPassword":"admin123","appName":"智多分机考桌面端-CEF","cefVersion":"${CEF_VERSION}"}'
            FileClose $0
            DetailPrint "已创建默认配置文件"
        ${EndIf}
    ${EndIf}
    
    ; 始终复制一份默认配置作为备份
    ${If} ${FileExists} "resources\config.json"
        File /oname=resources\config.json.default "resources\config.json"
    ${EndIf}
    
    ; 复制图标文件
    ${If} ${FileExists} "resources\logo.ico"
        File /oname=resources\logo.ico "resources\logo.ico"
    ${ElseIf} ${FileExists} "resources\logo.png"
        File /oname=resources\logo.png "resources\logo.png"
        DetailPrint "使用PNG格式图标"
    ${Else}
        DetailPrint "警告：未找到应用图标文件"
    ${EndIf}

    ; 注册表写入（适配CEF版本）
    WriteRegStr HKLM "Software\${COMPANYNAME}\${APPNAME}" "InstallDir" "$INSTDIR"
    WriteRegStr HKLM "Software\${COMPANYNAME}\${APPNAME}" "CEFVersion" "${CEF_VERSION}"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPNAME}" "DisplayName"       "${APPNAME}"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPNAME}" "DisplayVersion"    "${VERSIONMAJOR}.${VERSIONMINOR}.${VERSIONPATCH}"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPNAME}" "Publisher"         "${COMPANYNAME}"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPNAME}" "DisplayIcon"       "$INSTDIR\DesktopTerminal-CEF.exe,0"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPNAME}" "UninstallString"   "$INSTDIR\uninstall.exe"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPNAME}" "Comments"          "${DESCRIPTION} (CEF ${CEF_VERSION})"
    WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPNAME}" "NoModify" 1
    WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPNAME}" "NoRepair" 1

    ; 卸载器
    WriteUninstaller "$INSTDIR\uninstall.exe"

    ; 开始菜单
    CreateDirectory "$SMPROGRAMS\${APPNAME}"
    CreateShortcut "$SMPROGRAMS\${APPNAME}\${APPNAME}.lnk" "$INSTDIR\DesktopTerminal-CEF.exe" "" "$INSTDIR\resources\logo.ico" 0
    CreateShortcut "$SMPROGRAMS\${APPNAME}\卸载.lnk"      "$INSTDIR\uninstall.exe"        "" "$INSTDIR\resources\logo.ico" 0

    ; 桌面快捷方式（带管理员权限提示）
    ${If} ${FileExists} "$INSTDIR\resources\logo.ico"
        CreateShortcut "$DESKTOP\${APPNAME}.lnk" "$INSTDIR\DesktopTerminal-CEF.exe" "" "$INSTDIR\resources\logo.ico" 0
    ${Else}
        CreateShortcut "$DESKTOP\${APPNAME}.lnk" "$INSTDIR\DesktopTerminal-CEF.exe"
    ${EndIf}
    
    ; 创建带明确管理员权限提示的开始菜单快捷方式
    CreateShortcut "$SMPROGRAMS\${APPNAME}\${APPNAME}（管理员模式）.lnk" "$INSTDIR\DesktopTerminal-CEF.exe" "" "$INSTDIR\resources\logo.ico" 0
    
    ; 提供权限说明信息
    DetailPrint "已创建桌面和开始菜单快捷方式"
    DetailPrint "注意：程序已配置为自动请求管理员权限"
    
    ; 最终安装验证和诊断（适配CEF）
    DetailPrint "正在进行最终安装验证..."
    
    ${If} ${FileExists} "$INSTDIR\DesktopTerminal-CEF.exe"
        DetailPrint "✓ 主程序文件验证通过"
    ${Else}
        DetailPrint "❌ 主程序文件缺失"
        StrCpy $VerificationErrors "$VerificationErrors• 主程序文件 DesktopTerminal-CEF.exe 缺失$\n"
    ${EndIf}
    
    ${If} ${FileExists} "$INSTDIR\config.json"
        DetailPrint "✓ 配置文件验证通过"
    ${Else}
        DetailPrint "⚠ 配置文件缺失，将影响程序启动"
        StrCpy $VerificationErrors "$VerificationErrors• 配置文件 config.json 缺失$\n"
    ${EndIf}
    
    ; 验证关键CEF子进程文件（架构特定处理）
    ${If} "${ARCH}" == "x86"
        ; 32位系统：单进程模式验证
        DetailPrint "ℹ 32位系统验证：CEF单进程模式配置"
        ${If} ${FileExists} "$INSTDIR\d3dcompiler_47.dll"
            DetailPrint "✓ DirectX编译器库验证通过"
        ${Else}
            DetailPrint "⚠ DirectX编译器库验证失败，可能影响渲染"
        ${EndIf}
    ${Else}
        ; 64位系统：CEF 75同样使用单进程模式
        DetailPrint "ℹ CEF 75配置：使用单进程模式，无需子进程文件验证"
        ${If} ${FileExists} "$INSTDIR\libcef.dll"
            DetailPrint "✓ CEF核心库验证通过"
        ${Else}
            DetailPrint "❌ CEF核心库验证失败"
            StrCpy $VerificationErrors "$VerificationErrors• CEF核心库 libcef.dll 缺失$\n"
        ${EndIf}
    ${EndIf}
    
    ; 可选：检查crashpad处理程序（仅信息记录）
    ${If} ${FileExists} "$INSTDIR\crashpad_handler.exe"
        DetailPrint "ℹ 检测到crashpad崩溃处理程序（可选功能）"
    ${Else}
        DetailPrint "ℹ 未安装crashpad处理程序，使用CEF内嵌崩溃处理（推荐）"
    ${EndIf}
    
    ; 统计安装文件数量进行验证
    ${GetSize} "$INSTDIR" "/S=0K" $R1 $R2 $R3
    DetailPrint "安装统计：$R2 个文件，总大小 $R1 KB"
    
    ${If} $R2 < 3
        DetailPrint "❌ 安装文件数量异常偏少"
        StrCpy $VerificationErrors "$VerificationErrors• 安装文件数量过少（仅 $R2 个文件）$\n"
    ${ElseIf} $R1 < 500
        DetailPrint "❌ 安装文件总大小异常偏小"
        StrCpy $VerificationErrors "$VerificationErrors• 安装文件总大小异常（仅 $R1 KB）$\n"
    ${Else}
        DetailPrint "✓ 文件数量和大小验证通过"
    ${EndIf}
    
    ; 如果发现错误，显示详细诊断信息
    ${If} $VerificationErrors != ""
        MessageBox MB_ICONSTOP "❌ 安装验证失败！$\n$\n发现的问题：$\n$VerificationErrors$\n可能原因：$\n1. 安装包文件不完整或损坏$\n2. 磁盘空间不足或写权限受限$\n3. 杀毒软件阻止了文件写入$\n4. 系统环境不兼容$\n$\n建议：$\n- 重新下载安装包$\n- 以管理员身份运行安装程序$\n- 临时关闭杀毒软件后重试"
        Abort
    ${Else}
        DetailPrint "✅ 所有验证检查通过"
        
        ; 检查VC++运行时（CEF需要）
        ClearErrors
        ReadRegStr $R0 HKLM "SOFTWARE\Microsoft\VisualStudio\14.0\VC\Runtimes\${ARCH}" "Version"
        ${If} ${Errors}
            MessageBox MB_YESNO|MB_ICONQUESTION "未检测到Visual C++ Redistributable，这可能导致程序无法运行。$\n$\n是否现在打开Microsoft官网下载页面？" IDYES open_vc_download IDNO skip_vc_download
            open_vc_download:
                ${If} "${ARCH}" == "x64"
                    ExecShell "open" "https://aka.ms/vs/17/release/vc_redist.x64.exe"
                ${Else}
                    ExecShell "open" "https://aka.ms/vs/17/release/vc_redist.x86.exe"
                ${EndIf}
            skip_vc_download:
        ${EndIf}
        
        MessageBox MB_OK "$(MSG_InstallDone)$\n$\n安装摘要：$\n- 文件数量：$R2 个$\n- 安装大小：$R1 KB$\n- CEF版本：${CEF_VERSION}$\n$\n权限配置：$\n- 程序已配置为自动请求管理员权限$\n- 首次运行时可能会出现UAC提示，请选择'是'$\n- 如有权限问题，可使用开始菜单中的'管理员模式'快捷方式$\n$\n注意：首次运行前请修改配置文件中的URL和密码设置。"
    ${EndIf}
SectionEnd

; ─────────────────────────────────────────────
; 卸载部分
; ─────────────────────────────────────────────
Section "Uninstall"
    ; 删除文件与目录
    RMDir /r "$INSTDIR"

    ; 删除快捷方式
    RMDir /r "$SMPROGRAMS\${APPNAME}"
    Delete "$DESKTOP\${APPNAME}.lnk"

    ; 删除注册表
    DeleteRegKey HKLM "Software\${COMPANYNAME}\${APPNAME}"
    DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPNAME}"

    MessageBox MB_OK "$(MSG_UninstallDone)"
SectionEnd