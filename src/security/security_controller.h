#ifndef SECURITY_CONTROLLER_H
#define SECURITY_CONTROLLER_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QUrl>

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
     * @brief 处理安全违规行为
     * @param violation 违规类型
     * @param description 违规描述
     */
    void handleSecurityViolation(SecurityViolationType violation, const QString& description);

    /**
     * @brief 获取违规统计信息
     */
    int getViolationCount() const;

    /**
     * @brief 重置违规计数
     */
    void resetViolationCount();

public slots:
    // 保留占位以兼容旧信号槽，无实际逻辑
    void performSecurityCheck();

signals:
    /**
     * @brief 检测到安全违规时发出信号
     */
    void securityViolationDetected(SecurityViolationType type, const QString& description);

private:
    QString getViolationDescription(SecurityViolationType type);
    void logSecurityEvent(const QString& event, const QString& details);

private:
    Logger* m_logger;
    ConfigManager* m_configManager;

    int m_violationCount;
};

#endif // SECURITY_CONTROLLER_H
