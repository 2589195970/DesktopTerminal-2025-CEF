#ifndef APPLICATION_H
#define APPLICATION_H

#include <QApplication>
#include <QString>
#include <QVersionNumber>
#include <QThread>

class CEFManager;
class SecureBrowser;
class Logger;
class ConfigManager;
class LoadingDialog;
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
     * @brief 异步初始化应用程序（带加载对话框）
     * @return 成功返回true，失败返回false
     */
    bool initializeAsync();

    /**
     * @brief 启动主界面
     * @return 成功返回true，失败返回false
     */
    bool startMainWindow();

    /**
     * @brief 关闭应用程序
     */
    void shutdown();

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

private slots:
    void onAboutToQuit();
    
    // 异步初始化相关槽函数
    void onNetworkCheckCompleted(int status, const QString& details);
    void onCEFInitializationFinished();
    void onLoadingDialogRetryRequested();
    void onLoadingDialogCancelRequested();

private:
    // 初始化步骤
    bool initializeLogging();
    bool initializeConfiguration();
    bool initializeCEF();
    bool createMainWindow();
    
    // 异步初始化步骤
    void startAsyncInitialization();
    void performNetworkCheck();
    void performCEFInitialization();
    void handleInitializationSuccess();
    void handleInitializationFailure(const QString& error, const QString& details = QString());

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
    
    // 异步初始化相关
    LoadingDialog* m_loadingDialog;
    NetworkChecker* m_networkChecker;
    QThread* m_initializationThread;

    // 系统信息
    static ArchType s_architecture;
    static PlatformType s_platform;
    static CompatibilityLevel s_compatibility;
    static QString s_systemDescription;
    static bool s_systemInfoDetected;

    bool m_initialized;
    bool m_shutdownRequested;
    bool m_asyncInitialization;
};

#endif // APPLICATION_H