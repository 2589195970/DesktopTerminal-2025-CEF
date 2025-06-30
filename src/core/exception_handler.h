#ifndef EXCEPTION_HANDLER_H
#define EXCEPTION_HANDLER_H

#include "exceptions.h"
#include <QObject>
#include <QString>
#include <QTimer>
#include <functional>

class Logger;
class QWidget;

/**
 * @brief 异常恢复策略接口
 */
class IRecoveryStrategy
{
public:
    virtual ~IRecoveryStrategy() = default;
    virtual bool attemptRecovery(const ApplicationException& exception) = 0;
    virtual QString getStrategyName() const = 0;
};

/**
 * @brief 异常处理器类
 * 
 * 提供统一的异常处理和恢复机制
 */
class ExceptionHandler : public QObject
{
    Q_OBJECT

public:
    explicit ExceptionHandler(QObject* parent = nullptr);
    ~ExceptionHandler();

    /**
     * @brief 设置日志器
     */
    void setLogger(Logger* logger);

    /**
     * @brief 设置父窗口（用于显示错误对话框）
     */
    void setParentWidget(QWidget* parent);

    /**
     * @brief 处理异常
     * @param exception 异常对象
     * @param showDialog 是否显示错误对话框
     * @return 是否成功处理和恢复
     */
    bool handleException(const ApplicationException& exception, bool showDialog = true);

    /**
     * @brief 处理std::exception
     */
    bool handleStdException(const std::exception& exception, const QString& context = "");

    /**
     * @brief 处理未知异常
     */
    bool handleUnknownException(const QString& context = "");

    /**
     * @brief 注册恢复策略
     */
    void registerRecoveryStrategy(const QString& exceptionType, std::unique_ptr<IRecoveryStrategy> strategy);

    /**
     * @brief 启用/禁用自动恢复
     */
    void setAutoRecoveryEnabled(bool enabled);

    /**
     * @brief 设置最大恢复尝试次数
     */
    void setMaxRecoveryAttempts(int maxAttempts);

    /**
     * @brief 获取异常统计信息
     */
    struct ExceptionStats {
        int totalExceptions;
        int recoveredExceptions;
        int failedRecoveries;
        QString lastExceptionTime;
        QString lastExceptionMessage;
    };
    ExceptionStats getExceptionStats() const;

    /**
     * @brief 重置异常统计
     */
    void resetStats();

signals:
    /**
     * @brief 异常处理完成信号
     */
    void exceptionHandled(const QString& exceptionType, bool recovered);

    /**
     * @brief 严重异常信号（建议应用程序退出）
     */
    void criticalExceptionOccurred(const QString& message);

private slots:
    void onRecoveryTimeout();

private:
    // 恢复策略
    bool attemptRecovery(const ApplicationException& exception);
    void logException(const ApplicationException& exception, bool recovered);
    void showErrorDialog(const ApplicationException& exception, bool recovered);

    // 内置恢复策略
    std::unique_ptr<IRecoveryStrategy> createCEFRecoveryStrategy();
    std::unique_ptr<IRecoveryStrategy> createConfigRecoveryStrategy();
    std::unique_ptr<IRecoveryStrategy> createResourceRecoveryStrategy();

private:
    Logger* m_logger;
    QWidget* m_parentWidget;
    bool m_autoRecoveryEnabled;
    int m_maxRecoveryAttempts;
    
    // 恢复策略映射
    QMap<QString, std::unique_ptr<IRecoveryStrategy>> m_recoveryStrategies;
    
    // 恢复尝试计数
    QMap<QString, int> m_recoveryAttempts;
    
    // 统计信息
    ExceptionStats m_stats;
    
    // 恢复超时定时器
    QTimer* m_recoveryTimer;
    std::function<void()> m_currentRecoveryAction;
};

/**
 * @brief CEF恢复策略
 */
class CEFRecoveryStrategy : public IRecoveryStrategy
{
public:
    bool attemptRecovery(const ApplicationException& exception) override;
    QString getStrategyName() const override;
};

/**
 * @brief 配置恢复策略
 */
class ConfigRecoveryStrategy : public IRecoveryStrategy
{
public:
    bool attemptRecovery(const ApplicationException& exception) override;
    QString getStrategyName() const override;
};

/**
 * @brief 资源恢复策略
 */
class ResourceRecoveryStrategy : public IRecoveryStrategy
{
public:
    bool attemptRecovery(const ApplicationException& exception) override;
    QString getStrategyName() const override;
};

/**
 * @brief 全局异常处理宏
 */
#define TRY_CATCH_LOG(logger, action) \
    try { \
        action; \
    } catch (const ApplicationException& e) { \
        if (logger) logger->errorEvent(QString("异常: %1").arg(e.getMessage())); \
        throw; \
    } catch (const std::exception& e) { \
        if (logger) logger->errorEvent(QString("标准异常: %1").arg(e.what())); \
        throw; \
    } catch (...) { \
        if (logger) logger->errorEvent("未知异常"); \
        throw; \
    }

#define SAFE_EXECUTE(handler, action) \
    try { \
        action; \
    } catch (const ApplicationException& e) { \
        if (handler) handler->handleException(e); \
    } catch (const std::exception& e) { \
        if (handler) handler->handleStdException(e); \
    } catch (...) { \
        if (handler) handler->handleUnknownException(); \
    }

#endif // EXCEPTION_HANDLER_H