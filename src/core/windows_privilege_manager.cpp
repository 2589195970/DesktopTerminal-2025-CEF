#include "windows_privilege_manager.h"
#include "common_utils.h"
#include "../logging/logger.h"
#include "../config/config_manager.h"

#include <QMessageBox>
#include <QProcess>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QCoreApplication>
#include <QSysInfo>
#include <QVersionNumber>

#ifdef Q_OS_WIN
#include <windows.h>
#include <versionhelpers.h>
#include <shellapi.h>
#endif

WindowsPrivilegeManager::WindowsPrivilegeManager(QObject* parent)
    : QObject(parent)
    , m_logger(nullptr)
    , m_configManager(nullptr)
    , m_vcRuntimeChecked(false)
    , m_vcRuntimeInstalled(false)
{
}

void WindowsPrivilegeManager::setLogger(Logger* logger)
{
    m_logger = logger;
}

void WindowsPrivilegeManager::setConfigManager(ConfigManager* configManager)
{
    m_configManager = configManager;
}

bool WindowsPrivilegeManager::isRunningAsAdministrator()
{
#ifdef Q_OS_WIN
    BOOL isAdmin = FALSE;
    PSID adminGroup = nullptr;
    SID_IDENTIFIER_AUTHORITY ntAuthority = SECURITY_NT_AUTHORITY;
    
    // 创建管理员组的SID
    if (AllocateAndInitializeSid(
        &ntAuthority,
        2,
        SECURITY_BUILTIN_DOMAIN_RID,
        DOMAIN_ALIAS_RID_ADMINS,
        0, 0, 0, 0, 0, 0,
        &adminGroup)) {
        
        // 检查当前用户是否属于管理员组
        if (!CheckTokenMembership(nullptr, adminGroup, &isAdmin)) {
            isAdmin = FALSE;
        }
        
        FreeSid(adminGroup);
    }
    
    return isAdmin == TRUE;
#else
    return false; // 非Windows系统不需要管理员权限概念
#endif
}

bool WindowsPrivilegeManager::isVCRuntimeInstalled() const
{
#ifdef Q_OS_WIN
    if (m_vcRuntimeChecked) {
        return m_vcRuntimeInstalled;
    }

    // 检查VC++ 2015-2022 Redistributable是否已安装
    if (checkVCRuntimeInRegistry("SOFTWARE\\Microsoft\\VisualStudio\\14.0\\VC\\Runtimes\\x86")) {
        const_cast<WindowsPrivilegeManager*>(this)->m_vcRuntimeInstalled = true;
        const_cast<WindowsPrivilegeManager*>(this)->m_vcRuntimeChecked = true;
        return true;
    }
    
    // 检查VC++ 2013 Redistributable (x86) - 作为备选
    if (checkVCRuntimeInRegistry("SOFTWARE\\Microsoft\\VisualStudio\\12.0\\VC\\Runtimes\\x86")) {
        const_cast<WindowsPrivilegeManager*>(this)->m_vcRuntimeInstalled = true;
        const_cast<WindowsPrivilegeManager*>(this)->m_vcRuntimeChecked = true;
        return true;
    }
    
    // 检查系统DLL文件
    bool dllsAvailable = checkVCRuntimeDLLs();
    const_cast<WindowsPrivilegeManager*>(this)->m_vcRuntimeInstalled = dllsAvailable;
    const_cast<WindowsPrivilegeManager*>(this)->m_vcRuntimeChecked = true;
    
    return dllsAvailable;
#else
    return true; // 非Windows系统不需要VC++运行时
#endif
}

QString WindowsPrivilegeManager::getVCRuntimeInstallerPath() const
{
    if (!m_configManager) {
        return QString();
    }

    // 获取配置中的安装包文件名
    QString installerFileName = m_configManager->getVCRuntimeInstallerFileName();
    
    // 获取应用程序目录
    QString appDir = QCoreApplication::applicationDirPath();
    
    // 检查resources目录中的安装包
    QStringList possiblePaths = {
        appDir + "/resources/" + installerFileName,
        appDir + "/" + installerFileName,
        appDir + "/../resources/" + installerFileName
    };
    
    for (const QString& path : possiblePaths) {
        QFileInfo fileInfo(path);
        if (fileInfo.exists() && fileInfo.isFile() && fileInfo.isReadable()) {
            return QDir::toNativeSeparators(fileInfo.absoluteFilePath());
        }
    }
    
    return QString(); // 未找到安装包
}

WindowsPrivilegeManager::VCRuntimeResult WindowsPrivilegeManager::installVCRuntime(bool showPrompt)
{
#ifdef Q_OS_WIN
    if (m_logger) {
        m_logger->appEvent("开始VC++运行时安装流程");
    }

    // 检查是否有管理员权限
    if (!isRunningAsAdministrator()) {
        QString message = "安装VC++ Redistributable需要管理员权限";
        if (m_logger) {
            m_logger->errorEvent(message);
        }
        return VCRuntimeResult(Result::Failed, message);
    }
    
    // 获取安装包路径
    QString installerPath = getVCRuntimeInstallerPath();
    if (installerPath.isEmpty()) {
        QString message = "未找到VC++ Redistributable安装包";
        if (m_logger) {
            m_logger->errorEvent(message);
        }
        return VCRuntimeResult(Result::Failed, message);
    }
    
    // 显示用户确认对话框（如果需要）
    if (showPrompt && !showVCRuntimePrompt()) {
        QString message = "用户取消了VC++ Redistributable安装";
        if (m_logger) {
            m_logger->appEvent(message);
        }
        return VCRuntimeResult(Result::NotRequired, message);
    }
    
    // 执行安装
    return executeVCRuntimeInstallation(installerPath);
#else
    return VCRuntimeResult(Result::NotSupported, "VC++运行时安装仅在Windows系统上支持");
#endif
}

WindowsPrivilegeManager::Result WindowsPrivilegeManager::checkAndHandleVCRuntime()
{
#ifdef Q_OS_WIN
    if (!m_configManager) {
        return Result::Failed;
    }

    // 检查配置是否启用了VC++运行时检查
    if (!m_configManager->isVCRuntimeAutoInstallEnabled()) {
        if (m_logger) {
            m_logger->appEvent("VC++运行时自动安装功能已在配置中禁用");
        }
        return Result::NotRequired;
    }

    // 检查是否已安装
    if (isVCRuntimeInstalled()) {
        if (m_logger) {
            m_logger->appEvent("VC++ Redistributable检查通过");
        }
        emit vcRuntimeStatusChanged(true);
        return Result::Success;
    }

    if (m_logger) {
        m_logger->appEvent("检测到缺少VC++ Redistributable");
    }
    
    // 如果有管理员权限且找到安装包，尝试自动安装
    if (isRunningAsAdministrator() && !getVCRuntimeInstallerPath().isEmpty()) {
        bool showPrompt = m_configManager->isVCRuntimePromptEnabled();
        VCRuntimeResult result = installVCRuntime(showPrompt);
        
        if (result.result == Result::Success) {
            emit vcRuntimeStatusChanged(true);
            return Result::Success;
        } else {
            return Result::Failed;
        }
    } else {
        // 没有管理员权限或没有安装包，仅提示
        QString message = "检测到系统可能缺少Microsoft Visual C++ Redistributable。\n\n";
        
        if (!isRunningAsAdministrator()) {
            message += "建议以管理员权限重新运行程序以自动安装。\n\n";
        }
        
        if (getVCRuntimeInstallerPath().isEmpty()) {
            message += "或者请手动下载并安装Microsoft Visual C++ Redistributable。\n\n";
        }
        
        message += "程序将尝试继续运行，但可能会遇到问题。";
        
        QMessageBox::information(nullptr, "运行时检查", message);
        if (m_logger) {
            m_logger->appEvent("VC++运行时缺失，但无法自动安装");
        }
        
        emit vcRuntimeStatusChanged(false);
        return Result::Failed;
    }
#else
    return Result::NotSupported;
#endif
}

bool WindowsPrivilegeManager::requestAdministratorPrivileges()
{
#ifdef Q_OS_WIN
    if (isRunningAsAdministrator()) {
        return true; // 已经具有管理员权限
    }

    // 尝试重新启动应用程序并请求管理员权限
    QString program = QCoreApplication::applicationFilePath();
    QStringList arguments = QCoreApplication::arguments();
    arguments.removeFirst(); // 移除程序名称

    // 使用ShellExecute以管理员权限启动
    QString programNative = QDir::toNativeSeparators(program);
    QString argumentsString = arguments.join(" ");

    HINSTANCE result = ShellExecuteA(
        nullptr,
        "runas",
        programNative.toLocal8Bit().constData(),
        argumentsString.toLocal8Bit().constData(),
        nullptr,
        SW_SHOWNORMAL
    );

    return reinterpret_cast<int>(result) > 32;
#else
    return false;
#endif
}

bool WindowsPrivilegeManager::checkWindowsAPIAvailability()
{
#ifdef Q_OS_WIN
    // 检查关键API可用性
    HMODULE kernel32 = GetModuleHandleA("kernel32.dll");
    if (!kernel32) {
        return false;
    }

    // 对于Windows 7，检查是否有必要的API
    if (isWindows7OrLater()) {
        // Windows 7 SP1应该有这些基础API
        bool hasCreateFile = (GetProcAddress(kernel32, "CreateFileA") != nullptr);
        bool hasSetFileInfo = (GetProcAddress(kernel32, "SetFileInformationByHandle") != nullptr);
        
        return hasCreateFile; // SetFileInformationByHandle在Windows 7可能不存在，但CreateFile是必须的
    }

    return true;
#else
    return false;
#endif
}

QString WindowsPrivilegeManager::getWindowsVersionInfo()
{
#ifdef Q_OS_WIN
    return QString("%1 %2")
        .arg(QSysInfo::prettyProductName())
        .arg(QSysInfo::currentCpuArchitecture());
#else
    return "非Windows系统";
#endif
}

bool WindowsPrivilegeManager::isWindows7OrLater()
{
#ifdef Q_OS_WIN
    QString version = QSysInfo::productVersion();
    QVersionNumber winVersion = QVersionNumber::fromString(version);
    
    // 支持Windows 7 SP1及以上版本
    return !(winVersion.majorVersion() < 6 || 
            (winVersion.majorVersion() == 6 && winVersion.minorVersion() < 1));
#else
    return false;
#endif
}

bool WindowsPrivilegeManager::checkVCRuntimeInRegistry(const QString& registryPath) const
{
#ifdef Q_OS_WIN
    HKEY hKey;
    LONG result = RegOpenKeyExA(HKEY_LOCAL_MACHINE,
        registryPath.toLocal8Bit().constData(),
        0, KEY_READ, &hKey);
    
    if (result == ERROR_SUCCESS) {
        DWORD installed = 0;
        DWORD dataSize = sizeof(DWORD);
        DWORD valueType;
        
        result = RegQueryValueExA(hKey, "Installed", NULL, &valueType, 
                                 (LPBYTE)&installed, &dataSize);
        RegCloseKey(hKey);
        
        return (result == ERROR_SUCCESS && installed == 1);
    }
    
    return false;
#else
    return false;
#endif
}

bool WindowsPrivilegeManager::checkVCRuntimeDLLs() const
{
#ifdef Q_OS_WIN
    QString systemDir = QString::fromLocal8Bit(qgetenv("SystemRoot")) + "\\System32\\";
    QStringList requiredDlls = {
        "api-ms-win-crt-runtime-l1-1-0.dll",
        "vcruntime140.dll",
        "msvcp140.dll"
    };
    
    for (const QString& dll : requiredDlls) {
        if (!QFile::exists(systemDir + dll)) {
            return false;
        }
    }
    
    return true;
#else
    return false;
#endif
}

WindowsPrivilegeManager::VCRuntimeResult WindowsPrivilegeManager::executeVCRuntimeInstallation(const QString& installerPath)
{
#ifdef Q_OS_WIN
    if (m_logger) {
        m_logger->appEvent(QString("开始安装VC++ Redistributable: %1").arg(installerPath));
    }
    
    emit operationProgress("安装VC++ Redistributable", 0);
    
    // 使用QProcess执行静默安装
    QProcess process;
    process.setProgram(installerPath);
    
    // 静默安装参数
    QStringList arguments;
    arguments << "/quiet" << "/norestart";
    process.setArguments(arguments);
    
    // 设置超时时间（5分钟）
    process.start();
    
    if (!process.waitForStarted(30000)) { // 30秒启动超时
        QString message = "VC++ Redistributable安装程序启动失败";
        if (m_logger) {
            m_logger->errorEvent(message);
        }
        return VCRuntimeResult(Result::Failed, message);
    }
    
    emit operationProgress("安装VC++ Redistributable", 50);
    
    if (!process.waitForFinished(300000)) { // 5分钟完成超时
        QString message = "VC++ Redistributable安装超时";
        if (m_logger) {
            m_logger->errorEvent(message);
        }
        process.kill();
        return VCRuntimeResult(Result::Failed, message);
    }
    
    emit operationProgress("安装VC++ Redistributable", 100);
    
    int exitCode = process.exitCode();
    QString output = QString::fromLocal8Bit(process.readAllStandardOutput());
    QString errorOutput = QString::fromLocal8Bit(process.readAllStandardError());
    
    if (exitCode == 0) {
        QString message = "VC++ Redistributable安装成功";
        if (m_logger) {
            m_logger->appEvent(message);
        }
        // 重新检查安装状态
        m_vcRuntimeChecked = false;
        return VCRuntimeResult(Result::Success, message, exitCode);
    } else if (exitCode == 1638) {
        // 1638表示已经安装了更新版本
        QString message = "VC++ Redistributable已存在更新版本，跳过安装";
        if (m_logger) {
            m_logger->appEvent(message);
        }
        return VCRuntimeResult(Result::Success, message, exitCode);
    } else if (exitCode == 3010) {
        // 3010表示需要重启
        QString message = "VC++ Redistributable安装成功，建议重启系统";
        if (m_logger) {
            m_logger->appEvent(message);
        }
        return VCRuntimeResult(Result::Success, message, exitCode);
    } else {
        QString message = QString("VC++ Redistributable安装失败，退出代码: %1").arg(exitCode);
        if (m_logger) {
            m_logger->errorEvent(message);
            if (!output.isEmpty()) {
                m_logger->errorEvent(QString("标准输出: %1").arg(output));
            }
            if (!errorOutput.isEmpty()) {
                m_logger->errorEvent(QString("错误输出: %1").arg(errorOutput));
            }
        }
        return VCRuntimeResult(Result::Failed, message, exitCode);
    }
#else
    return VCRuntimeResult(Result::NotSupported, "VC++运行时安装仅在Windows系统上支持");
#endif
}

bool WindowsPrivilegeManager::showVCRuntimePrompt()
{
#ifdef Q_OS_WIN
    if (!m_configManager || !m_configManager->isVCRuntimePromptEnabled()) {
        return true; // 如果配置为静默安装，直接返回同意
    }

    QMessageBox::StandardButton reply = QMessageBox::question(
        nullptr,
        "运行时依赖",
        "检测到系统缺少Microsoft Visual C++ Redistributable运行时库。\n\n"
        "这是运行程序所必需的组件。是否现在自动安装？\n\n"
        "注意：安装过程可能需要几分钟时间。",
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::Yes
    );

    return reply == QMessageBox::Yes;
#else
    return false;
#endif
}