#ifndef CEF_MANAGER_H
#define CEF_MANAGER_H

#include <QObject>
#include <QString>
#include <QStringList>

#include "../cef/cef_app_impl.h"
#include "../cef/cef_client_impl.h"

class Application;
class Logger;
class ConfigManager;

/**
 * @brief CEF生命周期管理器
 * 
 * 负责CEF的初始化、配置和关闭，特别针对32位系统和Windows 7 SP1进行了优化
 */
class CEFManager : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief CEF进程模式
     */
    enum class ProcessMode {
        SingleProcess,  // 单进程模式（适合32位系统）
        MultiProcess    // 多进程模式（适合64位系统）
    };

    /**
     * @brief CEF内存配置级别
     */
    enum class MemoryProfile {
        Minimal,    // 最小内存使用（32位系统）
        Balanced,   // 平衡模式（一般64位系统）
        Performance // 性能模式（高端64位系统）
    };

    explicit CEFManager(Application* app, QObject* parent = nullptr);
    ~CEFManager();

    /**
     * @brief 初始化CEF
     * @return 成功返回true
     */
    bool initialize();

    /**
     * @brief 关闭CEF
     */
    void shutdown();

    /**
     * @brief 获取CEF是否已初始化
     */
    bool isInitialized() const { return m_initialized; }

    /**
     * @brief 创建浏览器实例
     * @param parentWidget 父窗口句柄
     * @param url 初始URL
     * @return CEF浏览器句柄
     */
    int createBrowser(void* parentWidget, const QString& url);

    /**
     * @brief 获取当前进程模式
     */
    ProcessMode getProcessMode() const { return m_processMode; }

    /**
     * @brief 获取当前内存配置
     */
    MemoryProfile getMemoryProfile() const { return m_memoryProfile; }

    /**
     * @brief 执行CEF消息循环（如果需要）
     */
    void doMessageLoopWork();

    /**
     * @brief 通知URL退出事件（由CEFClient调用）
     * @param url 触发退出的URL
     */
    void notifyUrlExitTriggered(const QString& url);

    /**
     * @brief 显示开发者工具
     * @param browserId 浏览器ID
     * @return 成功返回true
     */
    bool showDevTools(int browserId);

    /**
     * @brief 关闭开发者工具
     * @param browserId 浏览器ID
     * @return 成功返回true
     */
    bool closeDevTools(int browserId);

    // 静态配置方法
    static ProcessMode selectOptimalProcessMode();
    static MemoryProfile selectOptimalMemoryProfile();
    static QStringList buildCEFCommandLine();
    static QString getCEFCachePath();
    static QString getCEFLogPath();

private:
    // 初始化步骤
    bool initializeCEFSettings();
    bool initializeCEFApp();
    bool initializeCEFContext();

    // 配置构建
    void buildCEFSettings(CefSettings& settings);
    void buildCommandLineArgs(CefRefPtr<CefCommandLine> commandLine);
    void applyCachingPolicy(CefSettings& settings);
    void applySecuritySettings(CefSettings& settings);

    // 平台特定配置
#ifdef Q_OS_WIN
    void applyWindowsSettings(CefSettings& settings);
    void applyWindows7Optimizations(CefSettings& settings);
#elif defined(Q_OS_MAC)
    void applyMacOSSettings(CefSettings& settings);
#elif defined(Q_OS_LINUX)
    void applyLinuxSettings(CefSettings& settings);
#endif

    // 内存优化
    void applyMemoryOptimizations(CefSettings& settings);
    void apply32BitOptimizations(CefSettings& settings);
    void configureProcessLimits(CefSettings& settings);

    // 错误处理
    void handleInitializationError(const QString& error);
    bool verifyCEFInstallation();
    bool checkCEFDependencies();
    void checkOptionalFiles(const QStringList& optionalFiles, const QString& cefDir);
    QString checkCrashpadStatus();
    
    // 端口管理
    int findAvailablePort(int startPort);

signals:
    /**
     * @brief URL检测到退出触发器时发出此信号
     * @param url 触发退出的URL
     */
    void urlExitTriggered(const QString& url);

    /**
     * @brief 初始化进度更新信号
     * @param progress 进度百分比 (0-100)
     * @param message 当前步骤描述
     */
    void initializationProgress(int progress, const QString& message);

    /**
     * @brief 初始化完成信号
     * @param success 是否成功
     * @param errorMessage 错误信息（如果失败）
     */
    void initializationFinished(bool success, const QString& errorMessage);

private slots:
    void onApplicationShutdown();

private:
    Application* m_application;
    Logger* m_logger;
    ConfigManager* m_configManager;

    bool m_initialized;
    bool m_shutdownRequested;
    ProcessMode m_processMode;
    MemoryProfile m_memoryProfile;

    CefRefPtr<CefApp> m_cefApp;
    CefRefPtr<class CEFClient> m_cefClient; // CEF客户端实例（用于开发者工具管理）
    QString m_cefPath;
    QString m_cachePath;
    QString m_logPath;

    // 配置参数
    int m_maxRenderProcessCount;
    int m_cacheSizeMB;
    bool m_hardwareAccelerationEnabled;
    bool m_webSecurityEnabled;
    QString m_userAgent;
};

#endif // CEF_MANAGER_H