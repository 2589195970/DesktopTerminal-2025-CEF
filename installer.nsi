; NSIS 安装脚本 - DesktopTerminal-CEF版本（请以 UTF-8 无 BOM 保存）
Unicode true

; 包含必要的头文件
!include "MUI2.nsh"
!include "LogicLib.nsh"
!include "FileFunc.nsh"
!include "x64.nsh"
!include "WordFunc.nsh"
!include "WinVer.nsh"
!include "nsDialogs.nsh"

; ─────────────────────────────────────────────
; 常量定义
; ─────────────────────────────────────────────
!define APPNAME        "智多分机考桌面端"
!define COMPANYNAME    "山东智多分"
!define DESCRIPTION    "教育考试场景专用安全浏览器"
!searchparse /file "src\app_version.h" `#define DESKTOP_TERMINAL_VERSION "` APP_VERSION `"`
!define TERMINAL_APP   "DesktopTerminal-CEF.exe"
!define TERMINAL_SERVICE "UMTerminalService"
!define TERMINAL_SERVICE_EXE "DesktopTerminalService.exe"
!define TERMINAL_STATUS_EXE "DesktopTerminalStatus.exe"
!define TERMINAL_STATUS_RUN_KEY "Software\Microsoft\Windows\CurrentVersion\Run"
!define ERROR_NOT_LOGGED_ON 1245
!define TERMINAL_SERVICE_DISPLAY "UM 考场终端服务"
!define TERMINAL_SERVICE_OPTIONS `binPath= "\$\"$INSTDIR\${TERMINAL_SERVICE_EXE}\$\"" start= auto obj= LocalSystem DisplayName= "${TERMINAL_SERVICE_DISPLAY}"`

; CEF版本信息（将在构建时由GitHub Actions设置）
!ifndef CEF_VERSION
  !define CEF_VERSION "118"
!endif

; 架构：默认为 x64，可在命令行 -DARCH=x86 覆盖
!ifndef ARCH
  !define ARCH "x64"
!endif

!if "${ARCH}" == "x86"
  !define VC_MIN_VERSION "14.29.30157.0"
  !define INSTALL_DIR "$PROGRAMFILES32\${COMPANYNAME}\${APPNAME}"
  !define OUTPUT_FILE "Output\DesktopTerminal-CEF-v${APP_VERSION}-${ARCH}-cef${CEF_VERSION}-win7-setup.exe"
!else
  !define VC_MIN_VERSION "14.44.35211.0"
  !define INSTALL_DIR "$PROGRAMFILES64\${COMPANYNAME}\${APPNAME}"
  !define OUTPUT_FILE "Output\DesktopTerminal-CEF-v${APP_VERSION}-${ARCH}-cef${CEF_VERSION}-setup.exe"
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
Page custom ConfigPage ConfigPageLeave
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

; 必须在页面设置之后添加语言设置
!insertmacro MUI_LANGUAGE "SimpChinese"

; 语言字符串定义
LangString MSG_InstallDone ${LANG_SIMPCHINESE} "安装完成！桌面快捷方式已创建。"
LangString MSG_UninstallDone ${LANG_SIMPCHINESE} "卸载完成，相关文件已全部移除。"

Var /GLOBAL VerificationErrors
Var /GLOBAL VCRuntimeReady
Var /GLOBAL VCRuntimeRebootRequired
Var /GLOBAL ConfigURL
Var /GLOBAL ConfigPassword
Var /GLOBAL ConfigApiBaseUrl
Var /GLOBAL RequirePassword
Var /GLOBAL CefProcessMode
Var /GLOBAL TerminalURL
Var /GLOBAL TerminalSlid
Var /GLOBAL SampleMode
Var /GLOBAL TerminalDataDir
Var /GLOBAL TerminalServiceResult
Var /GLOBAL TerminalServiceReady
Var /GLOBAL Dialog
Var /GLOBAL URLText
Var /GLOBAL PasswordText
Var /GLOBAL ApiBaseUrlText
Var /GLOBAL RequirePasswordCheckbox
Var /GLOBAL CefProcessModeAutoRadio
Var /GLOBAL CefProcessModeSingleRadio
Var /GLOBAL CefProcessModeMultiRadio
Var /GLOBAL TerminalURLText
Var /GLOBAL TerminalSlidText
Var /GLOBAL SampleModeCheckbox
Var /GLOBAL ConfigPatchResult
Var /GLOBAL OpenSslRuntimeReady

; ─────────────────────────────────────────────
; 安装前检查
; ─────────────────────────────────────────────
Function .onInit
    ; 检查管理员权限
    UserInfo::GetAccountType
    Pop $R0
    ${If} $R0 != "admin"
        MessageBox MB_OK|MB_ICONSTOP "此程序需要管理员权限才能安装，请右键选择'以管理员身份运行'。" /SD IDOK
        SetErrorLevel 740 ; ERROR_ELEVATION_REQUIRED
        Quit
    ${EndIf}
    
    ; 检查系统架构
    ${If} "${ARCH}" == "x64"
        ${IfNot} ${RunningX64}
            MessageBox MB_OK|MB_ICONSTOP "此安装包仅适用于64位Windows系统。" /SD IDOK
            Quit
        ${EndIf}
    ${EndIf}
    
    ; 检查操作系统版本（要求Windows 7或更高版本）
    ${IfNot} ${AtLeastWin7}
        MessageBox MB_OK|MB_ICONSTOP "不支持的操作系统版本，需要Windows 7或更高版本。" /SD IDOK
        Quit
    ${EndIf}
    
    ; 交互安装和静默安装共用考试端默认配置。
    StrCpy $ConfigPassword "sdzdf.com"
    StrCpy $RequirePassword "1"
    StrCpy $CefProcessMode "multi"

    ReadINIStr $TerminalURL "$EXEDIR\school-config.ini" "school" "terminalUrl"
    ReadINIStr $TerminalSlid "$EXEDIR\school-config.ini" "school" "slid"
    ReadINIStr $ConfigURL "$EXEDIR\school-config.ini" "school" "examUrl"

    ${GetParameters} $R0
    ; 以空格分隔参数，保留 URL 中的斜杠。
    ${GetOptions} " $R0" " /TERMINAL_URL=" $R1
    ${IfNot} ${Errors}
        StrCpy $TerminalURL $R1
    ${EndIf}
    ${GetOptions} " $R0" " /SLID=" $R1
    ${IfNot} ${Errors}
        StrCpy $TerminalSlid $R1
    ${EndIf}
    ${GetOptions} $R0 "/SAMPLE=" $R1
    StrCpy $SampleMode "0"
    ${If} $R1 == "1"
        StrCpy $SampleMode "1"
    ${EndIf}

    Push $TerminalURL
    Call TrimSchoolConfigValue
    Pop $TerminalURL
    Push $TerminalSlid
    Call TrimSchoolConfigValue
    Pop $TerminalSlid
    Push $ConfigURL
    Call TrimSchoolConfigValue
    Pop $ConfigURL
FunctionEnd

Function TrimSchoolConfigValue
    Exch $0
    Push $1
    ${Do}
        StrCpy $1 $0 1
        ${If} $1 != " "
        ${AndIf} $1 != "$\t"
            ${ExitDo}
        ${EndIf}
        StrCpy $0 $0 "" 1
    ${Loop}
    ${Do}
        StrCpy $1 $0 1 -1
        ${If} $1 != " "
        ${AndIf} $1 != "$\t"
            ${ExitDo}
        ${EndIf}
        StrCpy $0 $0 -1
    ${Loop}
    Pop $1
    Exch $0
FunctionEnd

; ─────────────────────────────────────────────
; 配置输入页面
; ─────────────────────────────────────────────
Function ConfigPage
    nsDialogs::Create 1018
    Pop $Dialog

    ${NSD_CreateLabel} 0 0 100% 12u "终端注册优先使用后端 API 地址，留空时使用备用终端管理地址。"
    Pop $0

    ${NSD_CreateLabel} 0 18u 35% 12u "考试系统 URL"
    Pop $0
    ${NSD_CreateText} 36% 16u 64% 13u "$ConfigURL"
    Pop $URLText

    ${NSD_CreateLabel} 0 33u 35% 12u "退出密码"
    Pop $0
    ${NSD_CreatePassword} 36% 31u 64% 13u "$ConfigPassword"
    Pop $PasswordText

    ${NSD_CreateLabel} 0 48u 35% 12u "后端 API 地址（可选）"
    Pop $0
    ${NSD_CreateText} 36% 46u 64% 13u "$ConfigApiBaseUrl"
    Pop $ApiBaseUrlText

    ${NSD_CreateLabel} 0 63u 35% 12u "备用终端管理地址"
    Pop $0
    ${NSD_CreateText} 36% 61u 64% 13u "$TerminalURL"
    Pop $TerminalURLText

    ${NSD_CreateLabel} 0 78u 35% 12u "学校实例号（slid）"
    Pop $0
    ${NSD_CreateText} 36% 76u 64% 13u "$TerminalSlid"
    Pop $TerminalSlidText

    ${NSD_CreateLabel} 0 92u 24% 12u "运行模式"
    Pop $0
    ${NSD_CreateRadioButton} 25% 91u 25% 12u "智能模式"
    Pop $CefProcessModeAutoRadio
    ${NSD_CreateRadioButton} 50% 91u 25% 12u "普通版本"
    Pop $CefProcessModeSingleRadio
    ${NSD_CreateRadioButton} 75% 91u 25% 12u "高性能版本"
    Pop $CefProcessModeMultiRadio
    ${If} $CefProcessMode == "auto"
        ${NSD_Check} $CefProcessModeAutoRadio
    ${ElseIf} $CefProcessMode == "single"
        ${NSD_Check} $CefProcessModeSingleRadio
    ${Else}
        ${NSD_Check} $CefProcessModeMultiRadio
    ${EndIf}

    ${NSD_CreateCheckbox} 0 103u 100% 11u "敏感操作需要密码验证（F10 退出等）"
    Pop $RequirePasswordCheckbox
    ${NSD_SetState} $RequirePasswordCheckbox $RequirePassword

    ${NSD_CreateCheckbox} 0 114u 100% 11u "样机模式（只安装，不启动服务）"
    Pop $SampleModeCheckbox
    ${NSD_SetState} $SampleModeCheckbox $SampleMode

    nsDialogs::Show
FunctionEnd

Function ConfigPageLeave
    ${NSD_GetText} $URLText $ConfigURL
    ${NSD_GetText} $PasswordText $ConfigPassword
    ${NSD_GetText} $ApiBaseUrlText $ConfigApiBaseUrl
    Push $ConfigApiBaseUrl
    Call TrimSchoolConfigValue
    Pop $ConfigApiBaseUrl
    ${NSD_GetText} $TerminalURLText $TerminalURL
    ${NSD_GetText} $TerminalSlidText $TerminalSlid
    ${NSD_GetState} $SampleModeCheckbox $SampleMode
    ${NSD_GetState} $RequirePasswordCheckbox $RequirePassword
    ${NSD_GetState} $CefProcessModeSingleRadio $0
    ${If} $0 == 1
        StrCpy $CefProcessMode "single"
    ${Else}
        ${NSD_GetState} $CefProcessModeMultiRadio $0
        ${If} $0 == 1
            StrCpy $CefProcessMode "multi"
        ${Else}
            StrCpy $CefProcessMode "auto"
        ${EndIf}
    ${EndIf}
FunctionEnd

; ─────────────────────────────────────────────
; 日志宏：同时写 DetailPrint 和文件
; ─────────────────────────────────────────────
Var /GLOBAL InstallLogHandle

!macro LOG_INIT
    CreateDirectory "$INSTDIR"
    FileOpen $InstallLogHandle "$INSTDIR\install.log" w
    FileWrite $InstallLogHandle "=== Install Log ===$\r$\n"
    FileWrite $InstallLogHandle "Time: $EXEDIR$\r$\n"
    FileWrite $InstallLogHandle "InstDir: $INSTDIR$\r$\n"
    FileWrite $InstallLogHandle "Arch: ${ARCH}$\r$\n"
    FileWrite $InstallLogHandle "===$\r$\n"
!macroend

!macro LOG_CLOSE
    ${If} $InstallLogHandle != ""
        FileWrite $InstallLogHandle "=== Install Complete ===$\r$\n"
        FileClose $InstallLogHandle
    ${EndIf}
!macroend

!macro LOG_MSG _MSG
    DetailPrint "${_MSG}"
    ${If} $InstallLogHandle != ""
        FileWrite $InstallLogHandle "${_MSG}$\r$\n"
    ${EndIf}
!macroend

!define Log "!insertmacro LOG_MSG"

!macro EXEC_TERMINAL_COMMAND _COMMAND
    nsExec::ExecToLog /TIMEOUT=40000 `${_COMMAND}`
    Pop $TerminalServiceResult
!macroend

!macro LOG_TERMINAL_RESULT _NAME
    ${If} $TerminalServiceResult == 0
        ${Log} "${_NAME} succeeded (0)"
    ${Else}
        ${Log} "${_NAME} failed ($TerminalServiceResult)"
    ${EndIf}
!macroend

!macro TERMINAL_COMMAND _NAME _COMMAND
    !insertmacro EXEC_TERMINAL_COMMAND `${_COMMAND}`
    !insertmacro LOG_TERMINAL_RESULT "${_NAME}"
!macroend

!define TerminalCommand "!insertmacro TERMINAL_COMMAND"

!macro WAIT_TERMINAL_SERVICE _ACTION _STATUS _SECONDS
    ${TerminalCommand} "wait for ${TERMINAL_SERVICE} to ${_ACTION}" `"$SYSDIR\WindowsPowerShell\v1.0\powershell.exe" -NoProfile -NonInteractive -Command "$$ErrorActionPreference = 'Stop'; (Get-Service -Name '${TERMINAL_SERVICE}').WaitForStatus('${_STATUS}', [TimeSpan]::FromSeconds(${_SECONDS}))"`
!macroend

!macro STOP_TERMINAL_SERVICE
    !insertmacro EXEC_TERMINAL_COMMAND `"$SYSDIR\sc.exe" stop ${TERMINAL_SERVICE}`
    ${If} $TerminalServiceResult == 1060
        ${Log} "sc stop: service does not exist (1060)"
    ${ElseIf} $TerminalServiceResult == 1062
        ${Log} "sc stop: service is already stopped (1062)"
    ${Else}
        !insertmacro LOG_TERMINAL_RESULT "sc stop"
        !insertmacro WAIT_TERMINAL_SERVICE "stop" "Stopped" 30
    ${EndIf}
!macroend

!macro GET_TERMINAL_DATA_DIR
    SetShellVarContext all
    StrCpy $TerminalDataDir "$APPDATA\UMTerminal"
    SetShellVarContext current
!macroend

!macro TERMINAL_ACL _NAME _OPTIONS
    ${TerminalCommand} "${_NAME}" `"$SYSDIR\icacls.exe" "$TerminalDataDir" /L ${_OPTIONS}`
    ${If} $TerminalServiceResult != 0
        Return
    ${EndIf}
!macroend

!macro CHECK_VC_DLL _DLL
    ${GetFileVersion} "$0\${_DLL}" $3
    ${If} ${Errors}
        ${Log} "VC++ ${ARCH}: ${_DLL} is missing or its version cannot be read"
        StrCpy $VCRuntimeReady "0"
    ${Else}
        ${Log} "VC++ ${ARCH}: ${_DLL} version $3, required ${VC_MIN_VERSION}"
        ${VersionCompare} "$3" "${VC_MIN_VERSION}" $1
        ${If} $1 == 2
            StrCpy $VCRuntimeReady "0"
        ${EndIf}
    ${EndIf}
!macroend

Function CheckVCRuntime
    ; 安装器为 32 位进程。Sysnative 访问 64 位系统库。
    !if "${ARCH}" == "x64"
        StrCpy $0 "$WINDIR\Sysnative"
    !else
        StrCpy $0 "$SYSDIR"
    !endif
    StrCpy $VCRuntimeReady "1"
    !insertmacro CHECK_VC_DLL "msvcp140.dll"
    !insertmacro CHECK_VC_DLL "vcruntime140.dll"
    !if "${ARCH}" == "x64"
        !insertmacro CHECK_VC_DLL "vcruntime140_1.dll"
    !endif
FunctionEnd

!macro RUN_VC_REDIST _MODE
    ${Log} "VC++ ${ARCH}: running bundled redistributable (/${_MODE})"
    ClearErrors
    ExecWait '"$PLUGINSDIR\VC_redist.${ARCH}.exe" /${_MODE} /quiet /norestart /log "$INSTDIR\vc-redist-${_MODE}.log"' $R0
    ${If} ${Errors}
        StrCpy $R0 "launch failed"
    ${EndIf}
    ${Log} "VC++ ${ARCH}: /${_MODE} result $R0"
!macroend

Function EnsureVCRuntime
    StrCpy $VCRuntimeRebootRequired "0"
    Call CheckVCRuntime
    ${If} $VCRuntimeReady == "1"
        Return
    ${EndIf}

    ; 从安装包解压到私有临时目录，再运行匹配架构的 Microsoft 安装程序。
    InitPluginsDir
    SetOutPath "$PLUGINSDIR"
    File "resources\dependencies\VC_redist.${ARCH}.exe"
    SetOutPath "$INSTDIR"
    !insertmacro RUN_VC_REDIST "install"

    ${If} $R0 == 0
        Call CheckVCRuntime
        ${If} $VCRuntimeReady == "1"
            Return
        ${EndIf}
        ; 已注册的同版本运行库可能需要修复丢失的文件。
        !insertmacro RUN_VC_REDIST "repair"
    ${EndIf}
    ${If} $R0 == 0
    ${OrIf} $R0 == 1638
        Call CheckVCRuntime
        ${If} $VCRuntimeReady == "1"
            Return
        ${EndIf}
    ${ElseIf} $R0 == 3010
    ${OrIf} $R0 == 1641
        StrCpy $VCRuntimeRebootRequired "1"
        SetRebootFlag true
        ${Log} "VC++ ${ARCH}: Windows restart required"
        Return
    ${EndIf}

    ${Log} "VC++ ${ARCH}: prerequisite check failed"
    FileClose $InstallLogHandle
    MessageBox MB_OK|MB_ICONSTOP "VC++ ${ARCH} 运行库安装或验证失败（返回：$R0）。$\n请查看安装目录中的 install.log 和 vc-redist-*.log。" /SD IDOK
    SetErrorLevel 1
    Abort
FunctionEnd

!macro TERMINAL_STATUS_AUTOSTART _ACTION
    ${If} ${RunningX64}
        SetRegView 64
    ${EndIf}
    ClearErrors
    !if "${_ACTION}" == "install"
        WriteRegStr HKLM "${TERMINAL_STATUS_RUN_KEY}" "UMTerminalStatus" '"$INSTDIR\${TERMINAL_STATUS_EXE}" --tray'
    !else
        DeleteRegValue HKLM "${TERMINAL_STATUS_RUN_KEY}" "UMTerminalStatus"
    !endif
    SetRegView 32
!macroend

Function InstallTerminalService
    StrCpy $TerminalServiceReady "0"
    !insertmacro GET_TERMINAL_DATA_DIR
    ClearErrors
    CreateDirectory "$TerminalDataDir"
    ${If} ${Errors}
        ${Log} "Failed to create $TerminalDataDir"
        Return
    ${EndIf}

    ; 拒绝将服务配置写到重解析目录。
    System::Call 'Kernel32::GetFileAttributesW(w "$TerminalDataDir") i.r0'
    IntOp $1 $0 & 0x400
    ${If} $1 != 0
        ${Log} "Invalid directory for terminal service data: $TerminalDataDir"
        Return
    ${EndIf}

    !insertmacro TERMINAL_ACL "icacls terminal directory" `/inheritance:r /grant:r "*S-1-5-18:(OI)(CI)F" "*S-1-5-32-544:(OI)(CI)F"`
    !insertmacro TERMINAL_ACL "icacls terminal directory owner" `/setowner "*S-1-5-32-544"`

    ; 使用 UTF-8 写入完整 INI，保留安装路径中的中文。
    InitPluginsDir
    SetOutPath "$PLUGINSDIR"
    File "scripts\write-terminal-service-config.ps1"
    SetOutPath "$INSTDIR"
    StrCpy $0 $TerminalURL
    ${If} $ConfigApiBaseUrl != ""
        StrCpy $0 $ConfigApiBaseUrl
    ${EndIf}
    System::Call 'Kernel32::SetEnvironmentVariableW(w "DTCEF_TERMINAL_URL", w r0)'
    StrCpy $0 $TerminalSlid
    System::Call 'Kernel32::SetEnvironmentVariableW(w "DTCEF_TERMINAL_SLID", w r0)'
    ${TerminalCommand} "write service.ini" `"$SYSDIR\WindowsPowerShell\v1.0\powershell.exe" -NoProfile -NonInteractive -ExecutionPolicy Bypass -File "$PLUGINSDIR\write-terminal-service-config.ps1" -DataDirectory "$TerminalDataDir" -AppPath "$INSTDIR\${TERMINAL_APP}"`
    System::Call 'Kernel32::SetEnvironmentVariableW(w "DTCEF_TERMINAL_URL", p 0)'
    System::Call 'Kernel32::SetEnvironmentVariableW(w "DTCEF_TERMINAL_SLID", p 0)'
    ${If} $TerminalServiceResult != 0
        Return
    ${EndIf}

    ${IfNot} ${FileExists} "$INSTDIR\${TERMINAL_SERVICE_EXE}"
        ${Log} "Service executable missing: $INSTDIR\${TERMINAL_SERVICE_EXE}"
        Return
    ${EndIf}

    !insertmacro EXEC_TERMINAL_COMMAND `"$SYSDIR\sc.exe" query ${TERMINAL_SERVICE}`
    ${If} $TerminalServiceResult == 1060
        ${Log} "sc query: service does not exist (1060)"
        ${TerminalCommand} "sc create" `"$SYSDIR\sc.exe" create ${TERMINAL_SERVICE} ${TERMINAL_SERVICE_OPTIONS}`
    ${Else}
        !insertmacro LOG_TERMINAL_RESULT "sc query"
        ${If} $TerminalServiceResult != 0
            Return
        ${EndIf}
        ${TerminalCommand} "sc config" `"$SYSDIR\sc.exe" config ${TERMINAL_SERVICE} ${TERMINAL_SERVICE_OPTIONS}`
    ${EndIf}
    ${If} $TerminalServiceResult != 0
        Return
    ${EndIf}

    ${TerminalCommand} "sc failure" `"$SYSDIR\sc.exe" failure ${TERMINAL_SERVICE} reset= 86400 actions= restart/60000/restart/60000/restart/300000`
    ${TerminalCommand} "sc description" `"$SYSDIR\sc.exe" description ${TERMINAL_SERVICE} "考场终端注册、状态上报及考试端管理"`
    ${If} $SampleMode == "1"
        ${Log} "Terminal service start skipped in sample mode"
    ${ElseIf} $VCRuntimeRebootRequired == "1"
        ${Log} "Terminal service will start after Windows restart"
    ${Else}
        ${TerminalCommand} "sc start" `"$SYSDIR\sc.exe" start ${TERMINAL_SERVICE}`
        ${If} $TerminalServiceResult == 0
            !insertmacro WAIT_TERMINAL_SERVICE "run" "Running" 10
        ${EndIf}
        ${If} $TerminalServiceResult != 0
            Return
        ${EndIf}
    ${EndIf}
    StrCpy $TerminalServiceReady "1"
FunctionEnd

; ─────────────────────────────────────────────
; 安装部分
; ─────────────────────────────────────────────
Section "主程序" SecMain
    SetOutPath "$INSTDIR"
    !insertmacro LOG_INIT
    StrCpy $VerificationErrors ""

    Call EnsureVCRuntime
    SetOutPath "$INSTDIR"

    ; 覆盖安装先停止服务，以便替换服务程序并保留终端状态。
    !insertmacro STOP_TERMINAL_SERVICE
    
    ; 直接安装程序文件 - 使用正确的编译时源路径
    ExecWait '"$SYSDIR\taskkill.exe" /F /IM ${TERMINAL_STATUS_EXE}' $R0

    ; GitHub Actions构建文件位于artifacts\windows-${ARCH}\目录
    ${Log} "正在安装DesktopTerminal-CEF应用程序..."
    
    ; 安装主程序文件（从artifacts目录）
    File "artifacts\windows-${ARCH}\${TERMINAL_APP}"
    ${Log} "✓ 主程序已安装: DesktopTerminal-CEF.exe"

    ClearErrors
    SetOverwrite try
    File "artifacts\windows-${ARCH}\${TERMINAL_SERVICE_EXE}"
    SetOverwrite lastused
    ${If} ${Errors}
        ${Log} "Failed to replace ${TERMINAL_SERVICE_EXE}"
    ${EndIf}
    
    ClearErrors
    File "artifacts\windows-${ARCH}\${TERMINAL_STATUS_EXE}"
    ${If} ${Errors}
        ${Log} "Failed to replace ${TERMINAL_STATUS_EXE}"
        StrCpy $VerificationErrors "$VerificationErrors- ${TERMINAL_STATUS_EXE} 安装失败$\n"
    ${EndIf}

    File "scripts\diagnose-terminal-service.cmd"
    File "scripts\diagnose-terminal-service.ps1"
    SetOutPath "$INSTDIR\终端注册"
    File "scripts\start-terminal-service.bat"
    File "scripts\stop-terminal-service.bat"
    File "scripts\restart-terminal-service.bat"
    File "scripts\start-terminal-status.bat"
    File "scripts\stop-terminal-status.bat"
    File "scripts\restart-terminal-status.bat"
    File "scripts\control-terminal-status.ps1"
    SetOutPath "$INSTDIR"

    ; 安装CEF核心库文件
    File /nonfatal "artifacts\windows-${ARCH}\libcef.dll"
    File /nonfatal "artifacts\windows-${ARCH}\chrome_elf.dll" 
    File /nonfatal "artifacts\windows-${ARCH}\d3dcompiler_47.dll"
    File /nonfatal "artifacts\windows-${ARCH}\libEGL.dll"
    File /nonfatal "artifacts\windows-${ARCH}\libGLESv2.dll"
    ${Log} "✓ CEF核心库已安装"
    
    ; 注意：移除cef_sandbox.lib安装，因为它是静态链接库，运行时不需要
    
    ; 安装Qt5运行时库文件
    File /nonfatal "artifacts\windows-${ARCH}\Qt5Core.dll"
    File /nonfatal "artifacts\windows-${ARCH}\Qt5Gui.dll"
    File /nonfatal "artifacts\windows-${ARCH}\Qt5Widgets.dll"
    File /nonfatal "artifacts\windows-${ARCH}\Qt5Network.dll"
    File /nonfatal "artifacts\windows-${ARCH}\Qt5Concurrent.dll"
    File /nonfatal "artifacts\windows-${ARCH}\libssl-1_1-x64.dll"
    File /nonfatal "artifacts\windows-${ARCH}\libcrypto-1_1-x64.dll"
    File /nonfatal "artifacts\windows-${ARCH}\libssl-1_1.dll"
    File /nonfatal "artifacts\windows-${ARCH}\libcrypto-1_1.dll"
    ${Log} "✓ Qt5运行时库已安装"
    
    ; 安装CEF数据文件（根据实际构建输出调整）
    File /nonfatal "artifacts\windows-${ARCH}\icudtl.dat"
    File /nonfatal "artifacts\windows-${ARCH}\snapshot_blob.bin"
    File /nonfatal "artifacts\windows-${ARCH}\v8_context_snapshot.bin"
    File /nonfatal "artifacts\windows-${ARCH}\natives_blob.bin"
    ${Log} "✓ CEF数据文件已安装"
    
    ; 安装CEF资源文件
    File /nonfatal "artifacts\windows-${ARCH}\cef.pak"
    File /nonfatal "artifacts\windows-${ARCH}\cef_100_percent.pak"
    File /nonfatal "artifacts\windows-${ARCH}\cef_200_percent.pak"
    File /nonfatal "artifacts\windows-${ARCH}\cef_extensions.pak"
    File /nonfatal "artifacts\windows-${ARCH}\devtools_resources.pak"
    ${Log} "✓ CEF资源文件已安装"
    
    ; 安装本地化文件目录
    File /r "artifacts\windows-${ARCH}\locales"
    ${Log} "✓ 本地化文件已安装"

    ; 先复制构建产物中的resources目录（如果存在）
    ${If} ${FileExists} "artifacts\windows-${ARCH}\resources"
        File /r "artifacts\windows-${ARCH}\resources"
        ${Log} "✓ 构建产物resources已安装"
    ${EndIf}

    ; 安装Qt5平台插件（创建platforms目录并复制qwindows.dll）
    ; 此文件是Qt GUI应用启动的硬性依赖，缺失则程序无法运行
    CreateDirectory "$INSTDIR\platforms"
    File /oname=platforms\qwindows.dll "artifacts\windows-${ARCH}\platforms\qwindows.dll"
    ${Log} "platforms\qwindows.dll installed"

    ; 安装后验证逻辑 - 检查实际安装到目标目录的文件
    ${Log} "正在验证安装结果..."
    
    ; 检查主程序文件是否成功安装
    ${If} ${FileExists} "$INSTDIR\DesktopTerminal-CEF.exe"
        ${Log} "✓ 主程序安装成功"
        ; 验证文件大小（应该大于1MB）
        ${GetSize} "$INSTDIR\DesktopTerminal-CEF.exe" "/S=0K" $R1 $R2 $R3
        ${If} $R1 > 1000  ; 大于1MB
            ${Log} "✓ 主程序文件大小正常 ($R1 KB)"
        ${Else}
            ${Log} "⚠ 主程序文件可能不完整 ($R1 KB)"
        ${EndIf}
    ${Else}
        MessageBox MB_OK|MB_ICONSTOP "❌ 主程序安装失败：$INSTDIR\DesktopTerminal-CEF.exe$\n$\n可能原因：$\n1. 磁盘空间不足$\n2. 目标目录无写权限$\n3. 杀毒软件拦截" /SD IDOK
        Abort
    ${EndIf}
    
    ; 检查CEF核心库是否安装
    ${If} ${FileExists} "$INSTDIR\libcef.dll"
        ${Log} "✓ CEF核心库安装成功"
    ${Else}
        ${Log} "⚠ CEF核心库未安装，程序可能无法运行"
    ${EndIf}
    
    ; CEF 75 单进程模式：验证架构特定的依赖文件
    ${If} "${ARCH}" == "x86"
        ${If} ${FileExists} "$INSTDIR\d3dcompiler_47.dll"
            ${Log} "✓ DirectX编译器库安装成功"
        ${Else}
            ${Log} "⚠ DirectX编译器库缺失，可能影响渲染效果"
        ${EndIf}
    ${Else}
        ${If} ${FileExists} "$INSTDIR\libcef.dll"
            ${Log} "✓ CEF核心库验证通过"
        ${Else}
            ${Log} "❌ CEF核心库缺失"
            StrCpy $VerificationErrors "$VerificationErrors• CEF核心库 libcef.dll 缺失$\n"
        ${EndIf}
    ${EndIf}
    
    ; 检查Qt5运行时库是否安装
    ${If} ${FileExists} "$INSTDIR\Qt5Core.dll"
        ${If} ${FileExists} "$INSTDIR\Qt5Gui.dll"
            ${If} ${FileExists} "$INSTDIR\Qt5Widgets.dll"
                ${If} ${FileExists} "$INSTDIR\Qt5Network.dll"
                    ${If} ${FileExists} "$INSTDIR\Qt5Concurrent.dll"
                        ${Log} "Qt5 runtime DLLs verified"
                    ${Else}
                        ${Log} "WARNING: Qt5Concurrent.dll missing"
                        StrCpy $VerificationErrors "$VerificationErrors- Qt5Concurrent.dll 缺失$\n"
                    ${EndIf}
                ${Else}
                    ${Log} "WARNING: Qt5Network.dll missing"
                    StrCpy $VerificationErrors "$VerificationErrors- Qt5Network.dll 缺失，Qt网络与HTTPS不可用$\n"
                ${EndIf}
            ${Else}
                ${Log} "WARNING: Qt5Widgets.dll missing"
                StrCpy $VerificationErrors "$VerificationErrors- Qt5Widgets.dll 缺失$\n"
            ${EndIf}
        ${Else}
            ${Log} "WARNING: Qt5Gui.dll missing"
            StrCpy $VerificationErrors "$VerificationErrors- Qt5Gui.dll 缺失$\n"
        ${EndIf}
    ${Else}
        ${Log} "WARNING: Qt5Core.dll missing"
        StrCpy $VerificationErrors "$VerificationErrors- Qt5Core.dll 缺失$\n"
    ${EndIf}

    ; 检查Qt HTTPS所需OpenSSL运行时库
    StrCpy $OpenSslRuntimeReady "false"
    ${If} "${ARCH}" == "x86"
        ${If} ${FileExists} "$INSTDIR\libssl-1_1.dll"
            ${If} ${FileExists} "$INSTDIR\libcrypto-1_1.dll"
                StrCpy $OpenSslRuntimeReady "true"
            ${EndIf}
        ${EndIf}
    ${Else}
        ${If} ${FileExists} "$INSTDIR\libssl-1_1-x64.dll"
            ${If} ${FileExists} "$INSTDIR\libcrypto-1_1-x64.dll"
                StrCpy $OpenSslRuntimeReady "true"
            ${EndIf}
        ${EndIf}
        ${If} $OpenSslRuntimeReady != "true"
            ${If} ${FileExists} "$INSTDIR\libssl-1_1.dll"
                ${If} ${FileExists} "$INSTDIR\libcrypto-1_1.dll"
                    StrCpy $OpenSslRuntimeReady "true"
                ${EndIf}
            ${EndIf}
        ${EndIf}
    ${EndIf}

    ${If} $OpenSslRuntimeReady == "true"
        ${Log} "OpenSSL runtime verified"
    ${Else}
        ${Log} "WARNING: OpenSSL runtime missing or incomplete"
        ${If} "${ARCH}" == "x86"
            StrCpy $VerificationErrors "$VerificationErrors- OpenSSL运行时 libssl-1_1.dll / libcrypto-1_1.dll 缺失，HTTPS认证不可用$\n"
        ${Else}
            StrCpy $VerificationErrors "$VerificationErrors- OpenSSL运行时 libssl/libcrypto 成对DLL缺失，HTTPS认证不可用$\n"
        ${EndIf}
    ${EndIf}
    
    ; 检查Qt5平台插件（缺失会导致"no Qt platform plugin"致命错误）
    ${If} ${FileExists} "$INSTDIR\platforms\qwindows.dll"
        ${Log} "platforms\qwindows.dll verified"
    ${Else}
        ${Log} "CRITICAL: platforms\qwindows.dll missing - app will not start"
        StrCpy $VerificationErrors "$VerificationErrors platforms\qwindows.dll missing$\n"
    ${EndIf}
    
    ; 统计安装文件数量
    ${GetSize} "$INSTDIR" "/S=0K" $R1 $R2 $R3
    ${Log} "安装完成：$R2 个文件，总大小 $R1 KB"
    
    ${If} $R2 < 3
        ${Log} "⚠ 安装文件数量偏少，请检查安装包完整性"
    ${ElseIf} $R1 < 500
        ${Log} "⚠ 安装大小偏小，可能文件缺失"
    ${Else}
        ${Log} "✓ 安装文件数量和大小正常"
    ${EndIf}

    ; 资源目录
    CreateDirectory "$INSTDIR\resources"
    ${Log} "正在配置应用程序..."

    ; 复制 config.json 模板（UTF-8，包含中文 appName，不做任何修改）
    SetOutPath "$INSTDIR\resources"
    File /oname=config.json "resources\config.json"
    ${Log} "config.json template copied"

    ; 先用 PowerShell 直接补丁 config.json，保证安装后可见配置与运行配置一致。
    ; 同时由脚本生成 config.override.ini，继续保留程序启动阶段的覆盖兜底逻辑。
    ${If} $RequirePassword == 1
        StrCpy $R9 "true"
    ${Else}
        StrCpy $R9 "false"
    ${EndIf}

    InitPluginsDir
    SetOutPath "$PLUGINSDIR"
    File /oname=patch-install-config.ps1 "scripts\patch-install-config.ps1"
    SetOutPath "$INSTDIR"

    StrCpy $0 "$ConfigURL"
    System::Call 'Kernel32::SetEnvironmentVariableW(w "DTCEF_CONFIG_URL", w r0)i.r1'
    StrCpy $0 "$ConfigPassword"
    System::Call 'Kernel32::SetEnvironmentVariableW(w "DTCEF_CONFIG_PASSWORD", w r0)i.r1'
    StrCpy $0 "$ConfigApiBaseUrl"
    System::Call 'Kernel32::SetEnvironmentVariableW(w "DTCEF_CONFIG_API_BASE_URL", w r0)i.r1'
    StrCpy $0 "$R9"
    System::Call 'Kernel32::SetEnvironmentVariableW(w "DTCEF_CONFIG_REQUIRE_PASSWORD", w r0)i.r1'
    StrCpy $0 "$CefProcessMode"
    System::Call 'Kernel32::SetEnvironmentVariableW(w "DTCEF_CONFIG_PROCESS_MODE", w r0)i.r1'
    nsExec::ExecToLog '"$SYSDIR\WindowsPowerShell\v1.0\powershell.exe" -NoProfile -NonInteractive -ExecutionPolicy Bypass -File "$PLUGINSDIR\patch-install-config.ps1" -ConfigPath "$INSTDIR\resources\config.json" -OverridePath "$INSTDIR\resources\config.override.ini" -LogPath "$INSTDIR\resources\install-config-patch.log"'
    Pop $ConfigPatchResult
    System::Call 'Kernel32::SetEnvironmentVariableW(w "DTCEF_CONFIG_URL", w "")i.r1'
    System::Call 'Kernel32::SetEnvironmentVariableW(w "DTCEF_CONFIG_PASSWORD", w "")i.r1'
    System::Call 'Kernel32::SetEnvironmentVariableW(w "DTCEF_CONFIG_API_BASE_URL", w "")i.r1'
    System::Call 'Kernel32::SetEnvironmentVariableW(w "DTCEF_CONFIG_REQUIRE_PASSWORD", w "")i.r1'
    System::Call 'Kernel32::SetEnvironmentVariableW(w "DTCEF_CONFIG_PROCESS_MODE", w "")i.r1'

    ${If} $ConfigPatchResult == 0
        ${Log} "config.json patched and config.override.ini written"
    ${Else}
        ${Log} "WARNING: config.json patch failed ($ConfigPatchResult), writing config.override.ini fallback"
        FileOpen $0 "$INSTDIR\resources\config.override.ini" w
        FileWrite $0 "[override]$\r$\n"
        FileWrite $0 "url=$ConfigURL$\r$\n"
        FileWrite $0 "exitPassword=$ConfigPassword$\r$\n"
        FileWrite $0 "apiBaseUrl=$ConfigApiBaseUrl$\r$\n"
        FileWrite $0 "sensitiveOperationRequirePassword=$R9$\r$\n"
        ${If} $CefProcessMode != "auto"
            FileWrite $0 "cefProcessMode=$CefProcessMode$\r$\n"
        ${EndIf}
        FileClose $0
        ${Log} "config.override.ini fallback written: url=$ConfigURL, api=$ConfigApiBaseUrl, cefProcessMode=$CefProcessMode"
        MessageBox MB_OK|MB_ICONEXCLAMATION "安装器未能直接更新 config.json，已写入 config.override.ini 兜底配置。程序启动仍会按安装页填写的配置运行。请查看安装目录中的 install.log 和 resources\install-config-patch.log（补丁脚本启动后生成）。" /SD IDOK
    ${EndIf}
    
    ; 复制资源文件到resources目录
    SetOutPath "$INSTDIR\resources"
    File "resources\logo.ico"
    File /nonfatal "resources\logo.png"
    File /nonfatal "artifacts\windows-${ARCH}\resources\app.manifest"
    File /nonfatal "resources\loading_animation.css"
    ${Log} "✓ 应用资源文件已安装"

    ; 复制离线运行时安装包
    CreateDirectory "$INSTDIR\resources\dependencies"
    SetOutPath "$INSTDIR\resources\dependencies"
    File "resources\dependencies\VC_redist.x86.exe"
    File "resources\dependencies\VC_redist.x64.exe"
    File /nonfatal "resources\dependencies\manifest.json"
    ${Log} "✓ 依赖安装包已复制"

    ; 恢复输出路径到主目录
    SetOutPath "$INSTDIR"

    ; 注册表写入（适配CEF版本）
    WriteRegStr HKLM "Software\${COMPANYNAME}\${APPNAME}" "InstallDir" "$INSTDIR"
    WriteRegStr HKLM "Software\${COMPANYNAME}\${APPNAME}" "CEFVersion" "${CEF_VERSION}"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPNAME}" "DisplayName"       "${APPNAME}"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPNAME}" "DisplayVersion"    "${APP_VERSION}"
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
    ${Log} "已创建桌面和开始菜单快捷方式"
    ${Log} "注意：程序已配置为自动请求管理员权限"
    
    ; 最终安装验证和诊断（适配CEF）
    ${Log} "正在进行最终安装验证..."
    
    ${If} ${FileExists} "$INSTDIR\DesktopTerminal-CEF.exe"
        ${Log} "✓ 主程序文件验证通过"
    ${Else}
        ${Log} "❌ 主程序文件缺失"
        StrCpy $VerificationErrors "$VerificationErrors• 主程序文件 DesktopTerminal-CEF.exe 缺失$\n"
    ${EndIf}
    
    ${If} ${FileExists} "$INSTDIR\resources\config.json"
        ${Log} "✓ 配置文件验证通过"
    ${Else}
        ${Log} "⚠ 配置文件缺失，将影响程序启动"
        StrCpy $VerificationErrors "$VerificationErrors• 配置文件 config.json 缺失$\n"
    ${EndIf}

    ${If} ${FileExists} "$INSTDIR\resources\config.override.ini"
        ${Log} "config.override.ini verified"
    ${Else}
        ${Log} "WARNING: config.override.ini missing, app will use config.json only"
    ${EndIf}
    
    ; 验证关键CEF数据文件
    ${If} ${FileExists} "$INSTDIR\icudtl.dat"
        ${Log} "✓ CEF ICU数据文件验证通过"
    ${Else}
        ${Log} "❌ CEF ICU数据文件缺失，程序无法启动"
        StrCpy $VerificationErrors "$VerificationErrors• CEF ICU数据文件 icudtl.dat 缺失$\n"
    ${EndIf}
    
    ; 验证关键CEF子进程文件（架构特定处理）
    ${If} "${ARCH}" == "x86"
        ; 32位系统：单进程模式验证
        ${Log} "ℹ 32位系统验证：CEF单进程模式配置"
        ${If} ${FileExists} "$INSTDIR\d3dcompiler_47.dll"
            ${Log} "✓ DirectX编译器库验证通过"
        ${Else}
            ${Log} "⚠ DirectX编译器库验证失败，可能影响渲染"
        ${EndIf}
    ${Else}
        ; 64位系统：CEF 75验证
        ${Log} "ℹ CEF 75配置：核心库验证"
        ${If} ${FileExists} "$INSTDIR\libcef.dll"
            ${Log} "✓ CEF核心库验证通过"
        ${Else}
            ${Log} "❌ CEF核心库验证失败"
            StrCpy $VerificationErrors "$VerificationErrors• CEF核心库 libcef.dll 缺失$\n"
        ${EndIf}
    ${EndIf}
    
    ; 可选：检查crashpad处理程序（仅信息记录）
    ${If} ${FileExists} "$INSTDIR\crashpad_handler.exe"
        ${Log} "✓ 检测到crashpad崩溃处理程序"
    ${Else}
        ${Log} "✓ 使用CEF内嵌崩溃处理（正常）"
    ${EndIf}
    
    ; 统计安装文件数量进行验证
    ${GetSize} "$INSTDIR" "/S=0K" $R1 $R2 $R3
    ${Log} "安装统计：$R2 个文件，总大小 $R1 KB"
    
    ${If} $R2 < 3
        ${Log} "❌ 安装文件数量异常偏少"
        StrCpy $VerificationErrors "$VerificationErrors• 安装文件数量过少（仅 $R2 个文件）$\n"
    ${ElseIf} $R1 < 500
        ${Log} "❌ 安装文件总大小异常偏小"
        StrCpy $VerificationErrors "$VerificationErrors• 安装文件总大小异常（仅 $R1 KB）$\n"
    ${Else}
        ${Log} "✓ 文件数量和大小验证通过"
    ${EndIf}
    
    ; 如果发现错误，显示详细诊断信息
    ${If} $VerificationErrors != ""
        MessageBox MB_OK|MB_ICONSTOP "❌ 安装验证失败！$\n$\n发现的问题：$\n$VerificationErrors$\n可能原因：$\n1. 安装包文件不完整或损坏$\n2. 磁盘空间不足或写权限受限$\n3. 杀毒软件阻止了文件写入$\n4. 系统环境不兼容$\n$\n建议：$\n- 重新下载安装包$\n- 以管理员身份运行安装程序$\n- 临时关闭杀毒软件后重试" /SD IDOK
        Abort
    ${Else}
        ${Log} "✅ 所有验证检查通过"
        
        !insertmacro TERMINAL_STATUS_AUTOSTART "install"
        ${If} ${Errors}
            ${Log} "Failed to configure terminal status autostart"
            FileClose $InstallLogHandle
            MessageBox MB_OK|MB_ICONSTOP "无法配置终端注册状态登录启动项。" /SD IDOK
            SetErrorLevel 1
            Abort
        ${EndIf}
        SetShellVarContext all
        ClearErrors
        CreateDirectory "$SMPROGRAMS\${APPNAME}"
        CreateShortcut "$SMPROGRAMS\${APPNAME}\终端注册状态.lnk" "$INSTDIR\${TERMINAL_STATUS_EXE}" "" "$INSTDIR\resources\logo.ico" 0
        SetShellVarContext current
        ${If} ${Errors}
            ${Log} "Failed to create terminal status Start menu shortcut"
            FileClose $InstallLogHandle
            MessageBox MB_OK|MB_ICONSTOP "无法创建「终端注册状态」开始菜单入口。" /SD IDOK
            SetErrorLevel 1
            Abort
        ${EndIf}
        Call InstallTerminalService
        ${If} $TerminalServiceReady != "1"
            ${Log} "Terminal service setup failed"
            FileClose $InstallLogHandle
            MessageBox MB_OK|MB_ICONSTOP "终端服务配置或启动失败。请在安装目录中以管理员身份运行 diagnose-terminal-service.cmd 生成诊断报告。" /SD IDOK
            SetErrorLevel 1
            Abort
        ${EndIf}
        ${If} $VCRuntimeRebootRequired == "1"
            ${Log} "Terminal status tray will start at Windows logon after restart"
        ${Else}
            !insertmacro EXEC_TERMINAL_COMMAND `"$INSTDIR\${TERMINAL_STATUS_EXE}" --start-for-desktop-user`
            ${If} $TerminalServiceResult == ${ERROR_NOT_LOGGED_ON}
                ${Log} "No desktop in the installer session; terminal status tray will start at Windows logon"
            ${Else}
                !insertmacro LOG_TERMINAL_RESULT "Start terminal status tray"
            ${EndIf}
        ${EndIf}
        MessageBox MB_OK "$(MSG_InstallDone)$\n$\n安装摘要：$\n- 文件数量：$R2 个$\n- 安装大小：$R1 KB$\n- CEF版本：${CEF_VERSION}$\n$\n权限配置：$\n- 程序已配置为自动请求管理员权限$\n- 首次运行时可能会出现UAC提示，请选择'是'$\n- 如有权限问题，可使用开始菜单中的'管理员模式'快捷方式$\n$\n配置说明：$\n- 考试URL与退出密码可在安装时修改$\n- 后端 API 地址留空时，考试端从前端 Config.json 发现 API 地址，终端服务使用备用终端管理地址$\n- 桌面端认证密钥已预置，需与后端TD_XTCS.DESKTOP_AUTH_KEY一致" /SD IDOK
    ${EndIf}
    !insertmacro LOG_CLOSE
    ${If} $VCRuntimeRebootRequired == "1"
        SetErrorLevel 3010
    ${EndIf}
SectionEnd

; ─────────────────────────────────────────────
; 卸载部分
; ─────────────────────────────────────────────
Section "Uninstall"
    StrCpy $InstallLogHandle ""
    !insertmacro STOP_TERMINAL_SERVICE
    ${TerminalCommand} "sc delete" `"$SYSDIR\sc.exe" delete ${TERMINAL_SERVICE}`

    !insertmacro GET_TERMINAL_DATA_DIR
    System::Call 'Kernel32::GetFileAttributesW(w "$TerminalDataDir") i.r0'
    IntOp $1 $0 & 0x400
    ${If} $1 == 0
        ; 卸载时清除本机绑定状态，空目录直接删除。
        Delete "$TerminalDataDir\service.ini"
        Delete "$TerminalDataDir\state.json"
        RMDir "$TerminalDataDir"
    ${EndIf}

    !insertmacro TERMINAL_STATUS_AUTOSTART "uninstall"
    ExecWait '"$SYSDIR\taskkill.exe" /F /IM ${TERMINAL_STATUS_EXE}' $R0
    SetShellVarContext all
    Delete "$SMPROGRAMS\${APPNAME}\终端注册状态.lnk"
    RMDir "$SMPROGRAMS\${APPNAME}"
    StrCpy $0 "$APPDATA\UMTerminalStatus"
    SetShellVarContext current
    System::Call 'Kernel32::GetFileAttributesW(w "$0") i.r1'
    IntOp $1 $1 & 0x400
    ${If} $1 == 0
        Delete "$0\status.json"
        Delete "$0\registration.log"
        Delete "$0\registration.log.1"
        Delete "$0\registration.log.2"
        Delete "$0\registration.log.3"
        Delete "$0\registration.log.4"
        RMDir "$0"
    ${EndIf}

    ; 删除安装目录
    Delete /REBOOTOK "$INSTDIR\${TERMINAL_STATUS_EXE}"
    Delete /REBOOTOK "$INSTDIR\${TERMINAL_SERVICE_EXE}"
    RMDir /r "$INSTDIR"

    ; 删除快捷方式
    RMDir /r "$SMPROGRAMS\${APPNAME}"
    Delete "$DESKTOP\${APPNAME}.lnk"

    ; 删除注册表
    DeleteRegKey HKLM "Software\${COMPANYNAME}\${APPNAME}"
    DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPNAME}"

    MessageBox MB_OK "$(MSG_UninstallDone)" /SD IDOK
SectionEnd
