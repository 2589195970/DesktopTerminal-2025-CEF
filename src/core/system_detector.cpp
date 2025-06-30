#include "system_detector.h"
#include <QSysInfo>
#include <QVersionNumber>
#include <QCoreApplication>

#ifdef Q_OS_WIN
#include <windows.h>
#include <versionhelpers.h>
#endif

SystemDetector::SystemDetector()
{
    // 初始化系统信息结构体
    m_systemInfo.architecture = ArchType::Unknown;
    m_systemInfo.platform = PlatformType::Unknown;
    m_systemInfo.compatibility = CompatibilityLevel::Unknown;
    m_systemInfo.isDetected = false;
}

SystemDetector::SystemInfo SystemDetector::detectSystemInfo(bool forceRedetect)
{
    if (m_systemInfo.isDetected && !forceRedetect) {
        return m_systemInfo;
    }

    // 检测平台
    detectPlatform();
    
    // 检测架构
    detectArchitecture();
    
    // 检测兼容性级别
    detectCompatibilityLevel();
    
    // 构建系统描述
    buildSystemDescription();
    
    m_systemInfo.isDetected = true;
    return m_systemInfo;
}

const SystemDetector::SystemInfo& SystemDetector::getSystemInfo() const
{
    return m_systemInfo;
}

SystemDetector::ArchType SystemDetector::getSystemArchitecture()
{
    if (!m_systemInfo.isDetected) {
        detectSystemInfo();
    }
    return m_systemInfo.architecture;
}

SystemDetector::PlatformType SystemDetector::getSystemPlatform()
{
    if (!m_systemInfo.isDetected) {
        detectSystemInfo();
    }
    return m_systemInfo.platform;
}

SystemDetector::CompatibilityLevel SystemDetector::getCompatibilityLevel()
{
    if (!m_systemInfo.isDetected) {
        detectSystemInfo();
    }
    return m_systemInfo.compatibility;
}

QString SystemDetector::getSystemDescription()
{
    if (!m_systemInfo.isDetected) {
        detectSystemInfo();
    }
    return m_systemInfo.description;
}

bool SystemDetector::isWindows7SP1()
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

bool SystemDetector::is32BitSystem()
{
    return getSystemArchitecture() == ArchType::X86_32;
}

QString SystemDetector::getCEFVersionForPlatform()
{
    if (is32BitSystem()) {
        return "75.1.16+g16a67c4+chromium-75.0.3770.100";
    } else {
        return "118.6.8+g1e19f4c+chromium-118.0.5993.119";
    }
}

bool SystemDetector::checkSystemRequirements()
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

bool SystemDetector::checkCEFCompatibility()
{
    // 对于32位系统，确认使用CEF 75
    if (is32BitSystem()) {
        return true; // CEF 75支持所有32位系统
    }

    // 对于64位系统，检查是否支持较新的CEF
    CompatibilityLevel level = getCompatibilityLevel();
    return level != CompatibilityLevel::Unknown;
}

QString SystemDetector::getCompatibilityReport()
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

#ifdef Q_OS_WIN
bool SystemDetector::checkWindowsVersion()
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

bool SystemDetector::checkWindowsAPI()
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
        return hasCreateFile;
    }

    return true;
}

bool SystemDetector::isRunningAsAdministrator()
{
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
}

bool SystemDetector::checkVCRuntimeInstalled()
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
    
    return false;
}
#endif

void SystemDetector::detectPlatform()
{
    // 检测平台
#ifdef Q_OS_WIN
    m_systemInfo.platform = PlatformType::Windows;
#elif defined(Q_OS_MAC)
    m_systemInfo.platform = PlatformType::MacOS;
#elif defined(Q_OS_LINUX)
    m_systemInfo.platform = PlatformType::Linux;
#else
    m_systemInfo.platform = PlatformType::Unknown;
#endif
}

void SystemDetector::detectArchitecture()
{
    // 检测架构
    if (sizeof(void*) == 8) {
        QString arch = QSysInfo::currentCpuArchitecture();
        if (arch.contains("arm", Qt::CaseInsensitive)) {
            m_systemInfo.architecture = ArchType::ARM64;
        } else {
            m_systemInfo.architecture = ArchType::X86_64;
        }
    } else {
        m_systemInfo.architecture = ArchType::X86_32;
    }
    
    m_systemInfo.cpuArchitecture = QSysInfo::currentCpuArchitecture();
}

void SystemDetector::detectCompatibilityLevel()
{
    // 检测兼容性级别
    QString version = QSysInfo::productVersion();
    
    if (m_systemInfo.platform == PlatformType::Windows) {
        QVersionNumber winVersion = QVersionNumber::fromString(version);
        
        if (winVersion.majorVersion() < 6 || 
            (winVersion.majorVersion() == 6 && winVersion.minorVersion() < 1)) {
            // Windows Vista或更早版本
            m_systemInfo.compatibility = CompatibilityLevel::Unknown;
        } else if (winVersion.majorVersion() == 6 && winVersion.minorVersion() == 1) {
            // Windows 7
            m_systemInfo.compatibility = CompatibilityLevel::LegacySystem;
        } else if (winVersion.majorVersion() == 6 || winVersion.majorVersion() == 10) {
            // Windows 8/8.1/10
            m_systemInfo.compatibility = CompatibilityLevel::ModernSystem;
        } else {
            // Windows 11+
            m_systemInfo.compatibility = CompatibilityLevel::OptimalSystem;
        }
    } else if (m_systemInfo.platform == PlatformType::MacOS) {
        QVersionNumber macVersion = QVersionNumber::fromString(version);
        
        if (macVersion < QVersionNumber(10, 14)) {
            m_systemInfo.compatibility = CompatibilityLevel::LegacySystem;
        } else if (macVersion < QVersionNumber(12, 0)) {
            m_systemInfo.compatibility = CompatibilityLevel::ModernSystem;
        } else {
            m_systemInfo.compatibility = CompatibilityLevel::OptimalSystem;
        }
    } else {
        m_systemInfo.compatibility = CompatibilityLevel::ModernSystem; // Linux一般都是现代系统
    }
    
    m_systemInfo.osVersion = version;
}

void SystemDetector::buildSystemDescription()
{
    // 构建系统描述
    m_systemInfo.description = QString("%1 %2 (%3)")
        .arg(QSysInfo::prettyProductName())
        .arg(m_systemInfo.cpuArchitecture)
        .arg(m_systemInfo.architecture == ArchType::X86_32 ? "32位" : "64位");
}