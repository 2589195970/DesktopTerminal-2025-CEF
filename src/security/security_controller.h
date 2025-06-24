#ifndef SECURITY_CONTROLLER_H
#define SECURITY_CONTROLLER_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <QRegExp>
#include <QTimer>

class Logger;
class ConfigManager;

/**
 * @brief 安全控制器类
 * 
 * 负责URL访问控制、安全策略执行和违规检测
 * 与原项目的安全控制逻辑保持一致
 */
class SecurityController : public QObject
{
    Q_OBJECT

public:
    enum SecurityViolationType {
        UnauthorizedURL,
        ForbiddenKeyboard,
        WindowManipulation,
        ProcessViolation,
        Unknown
    };

    explicit SecurityController(QObject *parent = nullptr);
    ~SecurityController();

    /**
     * @brief 初始化安全控制器
     */
    bool initialize();

    /**
     * @brief 检查URL是否被允许访问
     * @param url 待检查的URL
     * @return 允许访问返回true
     */
    bool isUrlAllowed(const QUrl& url);

    /**
     * @brief 检查URL是否需要特殊处理
     * @param url 待检查的URL
     * @return 需要特殊处理返回true
     */
    bool requiresSpecialHandling(const QUrl& url);

    /**
     * @brief 处理安全违规行为
     * @param violation 违规类型
     * @param description 违规描述
     */
    void handleSecurityViolation(SecurityViolationType violation, const QString& description);

    /**
     * @brief 检查是否为安全的外部URL
     * @param url 待检查的URL
     * @return 安全返回true
     */
    bool isSafeExternalUrl(const QUrl& url);

    /**
     * @brief 获取违规统计信息
     */
    int getViolationCount() const;

    /**
     * @brief 重置违规计数
     */
    void resetViolationCount();

    /**
     * @brief 启用/禁用严格安全模式
     */
    void setStrictMode(bool enabled);
    bool isStrictModeEnabled() const;

    /**
     * @brief 更新安全策略
     */
    void updateSecurityPolicy();

public slots:
    /**
     * @brief 处理定时安全检查
     */
    void performSecurityCheck();

signals:
    /**
     * @brief 检测到安全违规时发出信号
     */
    void securityViolationDetected(SecurityViolationType type, const QString& description);

    /**
     * @brief URL被阻止时发出信号
     */
    void urlBlocked(const QUrl& url, const QString& reason);

private:
    void initializeUrlFilters();
    void initializeSecurityTimer();
    void loadSecurityRules();
    bool matchesPattern(const QString& text, const QString& pattern);
    QString getViolationDescription(SecurityViolationType type);
    void logSecurityEvent(const QString& event, const QString& details);

private:
    Logger* m_logger;
    ConfigManager* m_configManager;
    
    // URL控制
    QStringList m_allowedDomains;
    QStringList m_blockedDomains;
    QStringList m_allowedUrlPatterns;
    QStringList m_blockedUrlPatterns;
    QString m_baseDomain;
    
    // 安全状态
    bool m_strictMode;
    bool m_urlFilterEnabled;
    int m_violationCount;
    
    // 定时器
    QTimer* m_securityTimer;
    
    // 统计信息
    int m_totalUrlChecks;
    int m_blockedUrlCount;
};

#endif // SECURITY_CONTROLLER_H