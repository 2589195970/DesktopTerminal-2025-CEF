#ifndef APPLICATION_H
#define APPLICATION_H

#include <QApplication>
#include <QString>
#include <QVersionNumber>
#include <QThread>

#include "../security/keyboard_filter.h"

class CEFManager;
class SecureBrowser;
class Logger;
class ConfigManager;
class NetworkChecker;

/**
 * @brief 主应用程序类
 * 
 * 负责应用程序初始化、系统兼容性检测和生命周期管理
 * 特别针对Windows 7 SP1 32位系统进行了优化
 */
class Application : public QApplication
{
    Q_OBJECT

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

    Application(int &argc, char **argv);
    ~Application();

    /**
     * @brief 初始化应用程序（同步版本）
     * @return 成功返回true，失败返回false
     */
    bool initialize();


    /**
     * @brief 启动主界面
     * @return 成功返回true，失败返回false
     */
    bool startMainWindow();

    /**
     * @brief 关闭应用程序
     */
    void shutdown();

    /**
     * @brief 获取主窗口指针
     * @return SecureBrowser指针，如果未创建则返回nullptr
     */
    SecureBrowser* getMainWindow() const;

    // 系统信息获取
    static ArchType getSystemArchitecture();
    static PlatformType getSystemPlatform();
    static CompatibilityLevel getCompatibilityLevel();
    static QString getSystemDescription();
    static bool isWindows7SP1();
    static bool is32BitSystem();
    static QString getCEFVersionForPlatform();

    // 兼容性检查
    static bool checkSystemRequirements();
    static bool checkCEFCompatibility();
    static QString getCompatibilityReport();

signals:
    // 初始化进度信号
    void initializationProgress(const QString& status);
    void initializationError(const QString& error);
    void initializationCompleted();

private slots:
    void onAboutToQuit();
    

private:
    // 初始化步骤
    bool initializeLogging();
    bool initializeConfiguration();
    bool initializeCEF();
    bool checkNetworkConnection();
    bool createMainWindow();
    

    // 系统检测
    void detectSystemInfo();
    static void detectSystemInfoStatic();  // 静态版本，用于静态方法调用
    void applyCompatibilitySettings();
    void logSystemInfo();

    // Windows特定功能
#ifdef Q_OS_WIN
    static bool checkWindowsVersion();
    static bool checkWindowsAPI();
    void applyWindows7Optimizations();
#endif

private:
    CEFManager* m_cefManager;
    SecureBrowser* m_mainWindow;
    Logger* m_logger;
    ConfigManager* m_configManager;
    
    // 网络检测器
    NetworkChecker* m_networkChecker;

    // 键盘过滤器
    KeyboardFilter* m_keyboardFilter;

    // 系统信息
    static ArchType s_architecture;
    static PlatformType s_platform;
    static CompatibilityLevel s_compatibility;
    static QString s_systemDescription;
    static bool s_systemInfoDetected;

    bool m_initialized;
    bool m_shutdownRequested;
};

#endif // APPLICATION_H