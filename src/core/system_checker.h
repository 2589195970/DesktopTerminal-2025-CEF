#ifndef SYSTEM_CHECKER_H
#define SYSTEM_CHECKER_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QNetworkInformation>
#include <QNetworkInterface>

class Logger;
class ConfigManager;

/**
 * @brief 系统检测器 - LoadingDialog的核心检测引擎
 * 
 * 负责启动时的各种系统检测，包括：
 * - 系统兼容性检测（Qt版本、OpenGL驱动、操作系统）
 * - 网络连接检测（连接状态、质量、目标可达性）
 * - CEF依赖完整性检查（文件存在、版本兼容、完整性）
 * - 配置和权限验证（文件权限、管理员权限、磁盘空间）
 */
class SystemChecker : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief 检测结果级别
     */
    enum CheckLevel {
        LEVEL_OK = 0,        // 正常，无问题
        LEVEL_WARNING = 1,   // 警告，可继续运行但可能影响性能
        LEVEL_ERROR = 2,     // 错误，严重问题需要用户处理
        LEVEL_FATAL = 3      // 致命错误，无法继续运行
    };

    /**
     * @brief 检测项目类型
     */
    enum CheckType {
        CHECK_SYSTEM_COMPATIBILITY,  // 系统兼容性
        CHECK_NETWORK_CONNECTION,    // 网络连接
        CHECK_CEF_DEPENDENCIES,      // CEF依赖
        CHECK_CONFIG_PERMISSIONS,    // 配置和权限
        CHECK_PRELOAD_COMPONENTS     // 预加载组件
    };

    /**
     * @brief 检测结果结构
     */
    struct CheckResult {
        CheckType type;
        CheckLevel level;
        QString title;
        QString message;
        QString solution;
        QStringList details;
        bool canRetry;
        bool autoFixable;
        
        CheckResult() : type(CHECK_SYSTEM_COMPATIBILITY), level(LEVEL_OK), canRetry(false), autoFixable(false) {}
    };

    explicit SystemChecker(QObject *parent = nullptr);
    ~SystemChecker();

    /**
     * @brief 开始全面系统检测
     */
    void startSystemCheck();

    /**
     * @brief 检查特定项目
     */
    void checkSpecificItem(CheckType type);

    /**
     * @brief 获取最后的检测结果
     */
    QList<CheckResult> getLastResults() const { return m_results; }

    /**
     * @brief 检查是否有致命错误
     */
    bool hasFatalErrors() const;

    /**
     * @brief 获取致命错误列表
     */
    QList<CheckResult> getFatalErrors() const;

    /**
     * @brief 尝试自动修复可修复的问题
     */
    void attemptAutoFix();

public slots:
    /**
     * @brief 重试特定检测项目
     */
    void retryCheck(CheckType type);

signals:
    /**
     * @brief 检测进度更新
     * @param current 当前检测项
     * @param total 总检测项数
     * @param message 当前状态消息
     */
    void checkProgress(int current, int total, const QString& message);

    /**
     * @brief 单个检测项完成
     * @param result 检测结果
     */
    void checkItemCompleted(const SystemChecker::CheckResult& result);

    /**
     * @brief 所有检测完成
     * @param success 是否可以继续启动
     * @param results 所有检测结果
     */
    void checkCompleted(bool success, const QList<SystemChecker::CheckResult>& results);

    /**
     * @brief 自动修复完成
     * @param fixed 修复的问题数量
     */
    void autoFixCompleted(int fixed);

private slots:
    void onNetworkCheckTimeout();

private:
    // 具体检测方法
    CheckResult checkSystemCompatibility();
    CheckResult checkQtCompatibility();
    CheckResult checkOpenGLSupport();
    CheckResult checkOperatingSystem();
    CheckResult checkMemoryResources();

    CheckResult checkNetworkConnection();
    CheckResult checkBasicConnectivity();
    CheckResult checkNetworkQuality();
    CheckResult checkTargetReachability();

    CheckResult checkCEFDependencies();
    CheckResult checkCEFFiles();
    CheckResult checkCEFVersionCompatibility();
    CheckResult checkCEFIntegrity();

    CheckResult checkConfigPermissions();
    CheckResult checkConfigFileIntegrity();
    CheckResult checkFilePermissions();
    CheckResult checkDiskSpace();

    CheckResult preloadComponents();

    // 工具方法
    QString formatFileSize(qint64 bytes);
    bool isAdministrator();
    qint64 getAvailableDiskSpace(const QString& path);
    QString getSystemDescription();

private:
    Logger* m_logger;
    ConfigManager* m_configManager;
    QList<CheckResult> m_results;
    QTimer* m_networkTimeout;
    QNetworkInformation* m_networkInfo;
    
    int m_currentCheck;
    int m_totalChecks;
    bool m_checkInProgress;
};

Q_DECLARE_METATYPE(SystemChecker::CheckResult)

#endif // SYSTEM_CHECKER_H