#ifndef SYSTEM_DETECTOR_H
#define SYSTEM_DETECTOR_H

#include <QString>
#include <QVersionNumber>

/**
 * @brief 系统检测器类
 * 
 * 专门负责系统信息检测和兼容性判断
 * 从Application类中分离出来以遵循单一职责原则
 */
class SystemDetector
{
public:
    /**
     * @brief 系统架构类型
     */
    enum class ArchType {
        Unknown,
        X86_32,     // 32位 x86
        X86_64,     // 64位 x86_64
        ARM64       // ARM64（仅macOS）
    };

    /**
     * @brief 系统平台类型
     */
    enum class PlatformType {
        Unknown,
        Windows,
        MacOS,
        Linux
    };

    /**
     * @brief 系统兼容性级别
     */
    enum class CompatibilityLevel {
        Unknown,
        LegacySystem,   // Windows 7 SP1等老系统
        ModernSystem,   // Windows 10+, macOS 10.14+等现代系统
        OptimalSystem   // 最新系统，支持所有功能
    };

    /**
     * @brief 系统信息结构体
     */
    struct SystemInfo {
        ArchType architecture;
        PlatformType platform;
        CompatibilityLevel compatibility;
        QString description;
        QString osVersion;
        QString cpuArchitecture;
        bool isDetected;
    };

    SystemDetector();
    ~SystemDetector() = default;

    /**
     * @brief 检测系统信息
     * @param forceRedetect 是否强制重新检测
     * @return 系统信息结构体
     */
    SystemInfo detectSystemInfo(bool forceRedetect = false);

    /**
     * @brief 获取已检测的系统信息
     */
    const SystemInfo& getSystemInfo() const;

    // 便捷方法
    ArchType getSystemArchitecture();
    PlatformType getSystemPlatform();
    CompatibilityLevel getCompatibilityLevel();
    QString getSystemDescription();
    bool isWindows7SP1();
    bool is32BitSystem();
    QString getCEFVersionForPlatform();

    /**
     * @brief 系统要求检查
     */
    bool checkSystemRequirements();
    bool checkCEFCompatibility();
    QString getCompatibilityReport();

    // Windows特定检查
#ifdef Q_OS_WIN
    bool checkWindowsVersion();
    bool checkWindowsAPI();
    bool isRunningAsAdministrator();
    bool checkVCRuntimeInstalled();
#endif

private:
    void detectPlatform();
    void detectArchitecture();
    void detectCompatibilityLevel();
    void buildSystemDescription();

    SystemInfo m_systemInfo;
};

#endif // SYSTEM_DETECTOR_H