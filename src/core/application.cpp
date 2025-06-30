#include "application.h"
#include "cef_manager.h"
#include "secure_browser.h"
#include "system_detector.h"
#include "compatibility_manager.h"
#include "common_utils.h"
#ifdef Q_OS_WIN
#include "windows_privilege_manager.h"
#endif
#include "../logging/logger.h"
#include "../config/config_manager.h"

#include <QDir>
#include <QStandardPaths>
#include <QMessageBox>
#include <QSysInfo>
#include <QVersionNumber>
#include <QTimer>
#include <QProcess>
#include <QFileInfo>

#ifdef Q_OS_WIN
#include <windows.h>
#include <versionhelpers.h>
#endif

// 静态成员初始化
Application::ArchType Application::s_architecture = Application::ArchType::Unknown;
Application::PlatformType Application::s_platform = Application::PlatformType::Unknown;
Application::CompatibilityLevel Application::s_compatibility = Application::CompatibilityLevel::Unknown;
QString Application::s_systemDescription;
bool Application::s_systemInfoDetected = false;

Application::Application(int &argc, char **argv)
    : QApplication(argc, argv)
    , m_logger(nullptr)
    , m_configManager(nullptr)
    , m_initialized(false)
    , m_shutdownRequested(false)
{
    // 设置应用程序信息
    setApplicationName("DesktopTerminal-CEF");
    setApplicationDisplayName("智多分机考桌面端 (CEF版)");
    setApplicationVersion("1.0.0");
    setOrganizationName("智多分");
    setOrganizationDomain("sdzdf.com");

    // 设置退出策略
    setQuitOnLastWindowClosed(false);

    // 创建管理器实例（使用智能指针）
    m_systemDetector = std::make_unique<SystemDetector>();
    m_compatibilityManager = std::make_unique<CompatibilityManager>(this);
#ifdef Q_OS_WIN
    m_windowsPrivilegeManager = std::make_unique<WindowsPrivilegeManager>(this);
#endif

    // 检测系统信息
    m_systemDetector->detectSystemInfo();

    // 连接退出信号
    connect(this, &QApplication::aboutToQuit, this, &Application::onAboutToQuit);
}

Application::~Application()
{
    shutdown();
}

bool Application::initialize()
{
    if (m_initialized) {
        return true;
    }

    // 1. 初始化日志系统
    if (!initializeLogging()) {
        return false;
    }

    m_logger->appEvent("应用程序开始初始化...");
    logSystemInfo();

    // 2. 检查系统要求
    if (!checkSystemRequirements()) {
        CommonUtils::showErrorDialog(nullptr, "系统要求不满足", 
            getCompatibilityReport() + "\n\n应用程序将退出。", m_logger);
        return false;
    }

    // 3. 应用兼容性设置
    applyCompatibilitySettings();

#ifdef Q_OS_WIN
    // 3.1 Windows 7系统特别需要检查VC++运行时
    if (isWindows7SP1() && m_windowsPrivilegeManager) {
        m_windowsPrivilegeManager->setLogger(m_logger);
        m_windowsPrivilegeManager->setConfigManager(m_configManager);
        
        WindowsPrivilegeManager::Result result = m_windowsPrivilegeManager->checkAndHandleVCRuntime();
        if (result == WindowsPrivilegeManager::Result::Failed) {
            // VC++运行时检查失败，但不阻止程序继续运行
            if (m_logger) {
                m_logger->appEvent("VC++运行时检查失败，但程序将继续运行");
            }
        }
    }
#endif

    // 4. 初始化配置管理
    if (!initializeConfiguration()) {
        return CommonUtils::logErrorAndReturnFalse(m_logger, "配置初始化失败");
    }

    // 5. 初始化CEF
    if (!initializeCEF()) {
        return CommonUtils::logErrorAndReturnFalse(m_logger, "CEF初始化失败");
    }

    m_initialized = true;
    m_logger->appEvent("应用程序初始化完成");
    return true;
}

bool Application::startMainWindow()
{
    if (!m_initialized) {
        return false;
    }

    if (!createMainWindow()) {
        return CommonUtils::logErrorAndReturnFalse(m_logger, "主窗口创建失败");
    }

    m_logger->appEvent("主窗口启动成功");
    return true;
}

void Application::shutdown()
{
    if (m_shutdownRequested) {
        return;
    }

    m_shutdownRequested = true;

    if (m_logger) {
        m_logger->appEvent("应用程序开始关闭...");
    }

    // 关闭主窗口（智能指针自动管理内存）
    if (m_mainWindow) {
        m_mainWindow->close();
        m_mainWindow.reset();  // 显式重置智能指针
    }

    // 关闭CEF（智能指针自动管理内存）
    if (m_cefManager) {
        m_cefManager->shutdown();
        m_cefManager.reset();  // 显式重置智能指针
    }

    // 清理管理器（智能指针自动管理内存）
    m_systemDetector.reset();
    m_compatibilityManager.reset();

    // 关闭日志系统
    if (m_logger) {
        m_logger->appEvent("应用程序关闭完成");
        m_logger->shutdown();
        // 注意：不要delete单例对象，它们有自己的生命周期管理
        m_logger = nullptr;
    }

    // 清理配置管理器引用
    if (m_configManager) {
        // 注意：不要delete单例对象，它们有自己的生命周期管理
        m_configManager = nullptr;
    }
}

Application::ArchType Application::getSystemArchitecture()
{
    if (!s_systemInfoDetected) {
        Application::detectSystemInfoStatic();
    }
    return s_architecture;
}

Application::PlatformType Application::getSystemPlatform()
{
    if (!s_systemInfoDetected) {
        Application::detectSystemInfoStatic();
    }
    return s_platform;
}

Application::CompatibilityLevel Application::getCompatibilityLevel()
{
    if (!s_systemInfoDetected) {
        Application::detectSystemInfoStatic();
    }
    return s_compatibility;
}

QString Application::getSystemDescription()
{
    if (!s_systemInfoDetected) {
        Application::detectSystemInfoStatic();
    }
    return s_systemDescription;
}

bool Application::isWindows7SP1()
{
#ifdef Q_OS_WIN
    QString version = QSysInfo::productVersion();
    QVersionNumber winVersion = QVersionNumber::fromString(version);
    
    // Windows 7 = 6.1
    if (winVersion.majorVersion() == 6 && winVersion.minorVersion() == 1) {
        // 检查是否为SP1
        OSVERSIONINFOEX osvi;
        ZeroMemory(&osvi, sizeof(OSVERSIONINFOEX));
        osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
        
        if (GetVersionEx((OSVERSIONINFO*)&osvi)) {
            return osvi.wServicePackMajor >= 1;
        }
        
        return true; // 假设是SP1
    }
#endif
    return false;
}

bool Application::is32BitSystem()
{
    return getSystemArchitecture() == ArchType::X86_32;
}

QString Application::getCEFVersionForPlatform()
{
    if (is32BitSystem()) {
        return "75.1.16+g16a67c4+chromium-75.0.3770.100";
    } else {
        return "118.6.8+g1e19f4c+chromium-118.0.5993.119";
    }
}

bool Application::checkSystemRequirements()
{
    // 检查操作系统
    PlatformType platform = getSystemPlatform();
    if (platform == PlatformType::Unknown) {
        return false;
    }

#ifdef Q_OS_WIN
    // Windows特定检查
    if (!checkWindowsVersion()) {
        return false;
    }
    
    if (!checkWindowsAPI()) {
        return false;
    }
#endif

    // 检查CEF兼容性
    return checkCEFCompatibility();
}

bool Application::checkCEFCompatibility()
{
    // 对于32位系统，确认使用CEF 75
    if (is32BitSystem()) {
        return true; // CEF 75支持所有32位系统
    }

    // 对于64位系统，检查是否支持较新的CEF
    CompatibilityLevel level = getCompatibilityLevel();
    return level != CompatibilityLevel::Unknown;
}

QString Application::getCompatibilityReport()
{
    QString report;
    report += "系统兼容性报告:\n";
    report += "================\n";
    report += QString("系统描述: %1\n").arg(getSystemDescription());
    report += QString("架构: %1\n").arg(is32BitSystem() ? "32位" : "64位");
    report += QString("CEF版本: %1\n").arg(getCEFVersionForPlatform());
    
    CompatibilityLevel level = getCompatibilityLevel();
    switch (level) {
        case CompatibilityLevel::LegacySystem:
            report += "兼容性级别: 传统系统 (需要特殊优化)\n";
            break;
        case CompatibilityLevel::ModernSystem:
            report += "兼容性级别: 现代系统 (完全支持)\n";
            break;
        case CompatibilityLevel::OptimalSystem:
            report += "兼容性级别: 最优系统 (所有功能)\n";
            break;
        default:
            report += "兼容性级别: 未知 (可能不兼容)\n";
            break;
    }

    if (!checkSystemRequirements()) {
        report += "\n⚠️ 警告: 系统要求检查失败\n";
        
        if (isWindows7SP1() && is32BitSystem()) {
            report += "建议:\n";
            report += "- 确保安装了所有Windows更新\n";
            report += "- 安装Visual C++ 2019-2022运行时\n";
            report += "- 确保有足够的内存空间 (至少2GB)\n";
        }
    }

    return report;
}

void Application::onAboutToQuit()
{
    shutdown();
}

bool Application::initializeLogging()
{
    try {
        m_logger = &Logger::instance();
        m_logger->ensureLogDirectoryExists();
        
#ifdef QT_DEBUG
        m_logger->setLogLevel(L_DEBUG);
#else
        m_logger->setLogLevel(L_INFO);
#endif

        return true;
    } catch (...) {
        return false;
    }
}

bool Application::initializeConfiguration()
{
    try {
        m_configManager = &ConfigManager::instance();
        
        if (!m_configManager->loadConfig()) {
            QString defaultPath = QDir(applicationDirPath()).filePath("config.json");
            
            if (m_configManager->createDefaultConfig(defaultPath) && 
                m_configManager->loadConfig(defaultPath)) {
                
                QMessageBox::information(nullptr, "配置文件", 
                    QString("已生成默认配置文件：\n%1\n请修改后重新启动。").arg(defaultPath));
                return false;
            } else {
                return CommonUtils::logErrorAndReturnFalse(m_logger, "无法创建或加载配置文件");
            }
        }

        m_logger->logStartup(m_configManager->getActualConfigPath());
        return true;
    } catch (...) {
        return CommonUtils::logErrorAndReturnFalse(m_logger, "配置管理器初始化异常");
    }
}

bool Application::initializeCEF()
{
    return CommonUtils::safeExecute([this]() {
        m_cefManager = std::make_unique<CEFManager>(this);
        return m_cefManager->initialize();
    }, "CEF管理器初始化异常", m_logger);
}

bool Application::createMainWindow()
{
    return CommonUtils::safeExecute([this]() {
        m_mainWindow = std::make_unique<SecureBrowser>(m_cefManager.get());
        m_mainWindow->show();
        return true;
    }, "主窗口创建异常", m_logger);
}

void Application::detectSystemInfoStatic()
{
    if (s_systemInfoDetected) {
        return;
    }

    // 检测平台
#ifdef Q_OS_WIN
    s_platform = PlatformType::Windows;
#elif defined(Q_OS_MAC)
    s_platform = PlatformType::MacOS;
#elif defined(Q_OS_LINUX)
    s_platform = PlatformType::Linux;
#else
    s_platform = PlatformType::Unknown;
#endif

    // 检测架构
    if (sizeof(void*) == 8) {
        QString arch = QSysInfo::currentCpuArchitecture();
        if (arch.contains("arm", Qt::CaseInsensitive)) {
            s_architecture = ArchType::ARM64;
        } else {
            s_architecture = ArchType::X86_64;
        }
    } else {
        s_architecture = ArchType::X86_32;
    }

    // 检测兼容性级别
    QString product = QSysInfo::productType();
    QString version = QSysInfo::productVersion();
    
    if (s_platform == PlatformType::Windows) {
        QVersionNumber winVersion = QVersionNumber::fromString(version);
        
        if (winVersion.majorVersion() < 6 || 
            (winVersion.majorVersion() == 6 && winVersion.minorVersion() < 1)) {
            // Windows Vista或更早版本
            s_compatibility = CompatibilityLevel::Unknown;
        } else if (winVersion.majorVersion() == 6 && winVersion.minorVersion() == 1) {
            // Windows 7
            s_compatibility = CompatibilityLevel::LegacySystem;
        } else if (winVersion.majorVersion() == 6 || winVersion.majorVersion() == 10) {
            // Windows 8/8.1/10
            s_compatibility = CompatibilityLevel::ModernSystem;
        } else {
            // Windows 11+
            s_compatibility = CompatibilityLevel::OptimalSystem;
        }
    } else if (s_platform == PlatformType::MacOS) {
        QVersionNumber macVersion = QVersionNumber::fromString(version);
        
        if (macVersion < QVersionNumber(10, 14)) {
            s_compatibility = CompatibilityLevel::LegacySystem;
        } else if (macVersion < QVersionNumber(12, 0)) {
            s_compatibility = CompatibilityLevel::ModernSystem;
        } else {
            s_compatibility = CompatibilityLevel::OptimalSystem;
        }
    } else {
        s_compatibility = CompatibilityLevel::ModernSystem; // Linux一般都是现代系统
    }

    // 构建系统描述
    s_systemDescription = QString("%1 %2 (%3)")
        .arg(QSysInfo::prettyProductName())
        .arg(QSysInfo::currentCpuArchitecture())
        .arg(s_architecture == ArchType::X86_32 ? "32位" : "64位");

    s_systemInfoDetected = true;
}


void Application::applyCompatibilitySettings()
{
    if (m_compatibilityManager && m_systemDetector && m_logger) {
        SystemDetector::SystemInfo systemInfo = m_systemDetector->getSystemInfo();
        m_compatibilityManager->applyCompatibilitySettings(systemInfo, m_logger);
    }
}

void Application::logSystemInfo()
{
    if (!m_logger) {
        return;
    }

    m_logger->systemEvent(QString("系统信息: %1").arg(getSystemDescription()));
    m_logger->systemEvent(QString("Qt版本: %1").arg(QT_VERSION_STR));
    m_logger->systemEvent(QString("应用程序架构: %1").arg(is32BitSystem() ? "32位" : "64位"));
    m_logger->systemEvent(QString("CEF目标版本: %1").arg(getCEFVersionForPlatform()));
    
    QString compatStr;
    switch (s_compatibility) {
        case CompatibilityLevel::LegacySystem: compatStr = "传统系统"; break;
        case CompatibilityLevel::ModernSystem: compatStr = "现代系统"; break;
        case CompatibilityLevel::OptimalSystem: compatStr = "最优系统"; break;
        default: compatStr = "未知"; break;
    }
    m_logger->systemEvent(QString("兼容性级别: %1").arg(compatStr));

    if (isWindows7SP1()) {
        m_logger->systemEvent("检测到Windows 7 SP1，已启用兼容模式");
    }
}

#ifdef Q_OS_WIN
bool Application::checkWindowsVersion()
{
    QString version = QSysInfo::productVersion();
    QVersionNumber winVersion = QVersionNumber::fromString(version);
    
    // 支持Windows 7 SP1及以上版本
    if (winVersion.majorVersion() < 6 || 
        (winVersion.majorVersion() == 6 && winVersion.minorVersion() < 1)) {
        return false;
    }

    return true;
}

bool Application::checkWindowsAPI()
{
    // 检查关键API可用性
    HMODULE kernel32 = GetModuleHandleA("kernel32.dll");
    if (!kernel32) {
        return false;
    }

    // 对于Windows 7，检查是否有必要的API
    if (isWindows7SP1()) {
        // Windows 7 SP1应该有这些基础API
        bool hasCreateFile = (GetProcAddress(kernel32, "CreateFileA") != nullptr);
        bool hasSetFileInfo = (GetProcAddress(kernel32, "SetFileInformationByHandle") != nullptr);
        
        return hasCreateFile; // SetFileInformationByHandle在Windows 7可能不存在，但CreateFile是必须的
    }

    return true;
}

// 注意：以下方法已被抽象到 WindowsPrivilegeManager 类中
// 这些方法保留仅为向后兼容，实际功能委托给 WindowsPrivilegeManager

bool Application::isRunningAsAdministrator()
{
#ifdef Q_OS_WIN
    return WindowsPrivilegeManager::isRunningAsAdministrator();
#else
    return false;
#endif
}

bool Application::checkVCRuntimeInstalled()
{
    // 检查VC++ 2015-2022 Redistributable是否已安装
    // 检查32位版本的注册表项
    HKEY hKey;
    LONG result;
    
    // 首先检查VC++ 2015-2022 Redistributable (x86)
    result = RegOpenKeyExA(HKEY_LOCAL_MACHINE,
        "SOFTWARE\\Microsoft\\VisualStudio\\14.0\\VC\\Runtimes\\x86",
        0, KEY_READ, &hKey);
    
    if (result == ERROR_SUCCESS) {
        DWORD installed = 0;
        DWORD dataSize = sizeof(DWORD);
        DWORD valueType;
        
        result = RegQueryValueExA(hKey, "Installed", NULL, &valueType, 
                                 (LPBYTE)&installed, &dataSize);
        RegCloseKey(hKey);
        
        if (result == ERROR_SUCCESS && installed == 1) {
            return true;
        }
    }
    
    // 检查VC++ 2013 Redistributable (x86) - 作为备选
    result = RegOpenKeyExA(HKEY_LOCAL_MACHINE,
        "SOFTWARE\\Microsoft\\VisualStudio\\12.0\\VC\\Runtimes\\x86",
        0, KEY_READ, &hKey);
    
    if (result == ERROR_SUCCESS) {
        DWORD installed = 0;
        DWORD dataSize = sizeof(DWORD);
        DWORD valueType;
        
        result = RegQueryValueExA(hKey, "Installed", NULL, &valueType, 
                                 (LPBYTE)&installed, &dataSize);
        RegCloseKey(hKey);
        
        if (result == ERROR_SUCCESS && installed == 1) {
            return true;
        }
    }
    
    // 检查Windows\System32中的关键DLL文件
    QString systemDir = QString::fromLocal8Bit(qgetenv("SystemRoot")) + "\\System32\\";
    QStringList requiredDlls = {
        "api-ms-win-crt-runtime-l1-1-0.dll",
        "vcruntime140.dll",
        "msvcp140.dll"
    };
    
    bool allDllsExist = true;
    for (const QString& dll : requiredDlls) {
        if (!QFile::exists(systemDir + dll)) {
            allDllsExist = false;
            break;
        }
    }
    
    return allDllsExist;
}

QString Application::getVCRuntimeInstallerPath()
{
    // 获取配置中的安装包文件名
    ConfigManager& configManager = ConfigManager::instance();
    QString installerFileName = configManager.getVCRuntimeInstallerFileName();
    
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

bool Application::installVCRuntime()
{
    Logger& logger = Logger::instance();
    
    // 检查是否有管理员权限
    if (!isRunningAsAdministrator()) {
        logger.errorEvent("安装VC++ Redistributable需要管理员权限");
        return false;
    }
    
    // 获取安装包路径
    QString installerPath = getVCRuntimeInstallerPath();
    if (installerPath.isEmpty()) {
        logger.errorEvent("未找到VC++ Redistributable安装包");
        return false;
    }
    
    logger.appEvent(QString("开始安装VC++ Redistributable: %1").arg(installerPath));
    
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
        logger.errorEvent("VC++ Redistributable安装程序启动失败");
        return false;
    }
    
    if (!process.waitForFinished(300000)) { // 5分钟完成超时
        logger.errorEvent("VC++ Redistributable安装超时");
        process.kill();
        return false;
    }
    
    int exitCode = process.exitCode();
    QString output = QString::fromLocal8Bit(process.readAllStandardOutput());
    QString errorOutput = QString::fromLocal8Bit(process.readAllStandardError());
    
    if (exitCode == 0) {
        logger.appEvent("VC++ Redistributable安装成功");
        return true;
    } else if (exitCode == 1638) {
        // 1638表示已经安装了更新版本
        logger.appEvent("VC++ Redistributable已存在更新版本，跳过安装");
        return true;
    } else if (exitCode == 3010) {
        // 3010表示需要重启
        logger.appEvent("VC++ Redistributable安装成功，建议重启系统");
        return true;
    } else {
        logger.errorEvent(QString("VC++ Redistributable安装失败，退出代码: %1").arg(exitCode));
        if (!output.isEmpty()) {
            logger.errorEvent(QString("标准输出: %1").arg(output));
        }
        if (!errorOutput.isEmpty()) {
            logger.errorEvent(QString("错误输出: %1").arg(errorOutput));
        }
        return false;
    }
}

void Application::applyWindows7Optimizations()
{
    // Windows 7特定优化
    if (m_logger) {
        m_logger->appEvent("应用Windows 7优化设置");
    }

    // 禁用一些可能导致问题的特性
    setAttribute(Qt::AA_DisableWindowContextHelpButton, true);
    setAttribute(Qt::AA_DontCreateNativeWidgetSiblings, true);
    
    // 强制软件渲染以避免GPU兼容性问题
    if (is32BitSystem()) {
        qputenv("QT_OPENGL", "software");
        qputenv("QT_ANGLE_PLATFORM", "d3d9");
    }
}
#endif