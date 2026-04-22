#ifndef APPLICATION_H
#define APPLICATION_H

#include <QApplication>
#include <QString>
#include <QVersionNumber>
#include <QThread>
#include <QLockFile>

#include "../security/keyboard_filter.h"
#include "../cef/cef_app_impl.h"

class CEFManager;
class SecureBrowser;
class Logger;
class ConfigManager;
class NetworkChecker;
#ifdef Q_OS_WIN
class WindowsKeyBlocker;
#endif

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

    Application(int &argc, char **argv, int originalArgc, char **originalArgv);
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

    void setSharedCEFApp(CefRefPtr<CEFApp> cefApp);
    CefRefPtr<CEFApp> sharedCEFApp() const { return m_sharedCEFApp; }
    int getOriginalArgc() const { return m_originalArgc; }
    char** getOriginalArgv() const { return m_originalArgv; }

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

    /**
     * @brief 初始化Windows安全控制（键盘钩子等）
     * @return 总是返回true（软失败设计）
     *
     * @note 返回值语义说明：
     * - 本函数采用"软失败"设计理念
     * - 无论钩子是否安装成功，都返回true，不阻止程序启动
     * - 钩子安装失败时，会启动后台自动恢复机制
     * - 这确保了即使安全控制暂时不可用，考试程序也能正常启动
     *
     * @details 实现策略：
     * - 尝试安装Windows键盘钩子
     * - 成功：记录日志，返回true
     * - 失败：记录错误码，启动后台恢复，返回true
     * - 不等待后台恢复完成，避免阻塞启动流程
     *
     * @see WindowsKeyBlocker::install() 了解后台恢复机制详情
     */
    bool initializeWindowsSecurityControls();
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

    // 单实例锁文件
    QLockFile* m_lockFile;
    bool m_lockAcquired;

#ifdef Q_OS_WIN
    // Windows键拦截器
    WindowsKeyBlocker* m_windowsKeyBlocker;
#endif

    // 系统信息
    static ArchType s_architecture;
    static PlatformType s_platform;
    static CompatibilityLevel s_compatibility;
    static QString s_systemDescription;
    static bool s_systemInfoDetected;

    bool m_initialized;
    bool m_shutdownRequested;
    CefRefPtr<CEFApp> m_sharedCEFApp;
    int m_originalArgc;
    char** m_originalArgv;
};

#endif // APPLICATION_H
