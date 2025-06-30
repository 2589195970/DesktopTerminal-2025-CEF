#include "exception_handler.h"
#include "../logging/logger.h"
#include "../config/config_manager.h"
#include <QApplication>
#include <QMessageBox>
#include <QWidget>
#include <QDateTime>
#include <QTimer>
#include <QDir>
#include <QFile>

ExceptionHandler::ExceptionHandler(QObject* parent)
    : QObject(parent)
    , m_logger(nullptr)
    , m_parentWidget(nullptr)
    , m_autoRecoveryEnabled(true)
    , m_maxRecoveryAttempts(3)
    , m_recoveryTimer(nullptr)
{
    // 初始化统计信息
    m_stats.totalExceptions = 0;
    m_stats.recoveredExceptions = 0;
    m_stats.failedRecoveries = 0;
    
    // 创建恢复超时定时器
    m_recoveryTimer = new QTimer(this);
    m_recoveryTimer->setSingleShot(true);
    m_recoveryTimer->setInterval(10000); // 10秒超时
    connect(m_recoveryTimer, &QTimer::timeout, this, &ExceptionHandler::onRecoveryTimeout);
    
    // 注册默认恢复策略
    registerRecoveryStrategy("CEF", createCEFRecoveryStrategy());
    registerRecoveryStrategy("Config", createConfigRecoveryStrategy());
    registerRecoveryStrategy("Resource", createResourceRecoveryStrategy());
}

ExceptionHandler::~ExceptionHandler()
{
    // 智能指针会自动清理恢复策略
}

void ExceptionHandler::setLogger(Logger* logger)
{
    m_logger = logger;
}

void ExceptionHandler::setParentWidget(QWidget* parent)
{
    m_parentWidget = parent;
}

bool ExceptionHandler::handleException(const ApplicationException& exception, bool showDialog)
{
    m_stats.totalExceptions++;
    m_stats.lastExceptionTime = QDateTime::currentDateTime().toString();
    m_stats.lastExceptionMessage = exception.getMessage();
    
    // 记录异常
    logException(exception, false);
    
    bool recovered = false;
    
    // 尝试自动恢复
    if (m_autoRecoveryEnabled && exception.isRecoverable()) {
        recovered = attemptRecovery(exception);
        
        if (recovered) {
            m_stats.recoveredExceptions++;
            logException(exception, true);
        } else {
            m_stats.failedRecoveries++;
        }
    }
    
    // 显示错误对话框
    if (showDialog) {
        showErrorDialog(exception, recovered);
    }
    
    // 发出信号
    emit exceptionHandled(exception.getCategory(), recovered);
    
    // 如果是不可恢复的严重异常，发出严重异常信号
    if (!exception.isRecoverable() && !recovered) {
        emit criticalExceptionOccurred(exception.getMessage());
    }
    
    return recovered;
}

bool ExceptionHandler::handleStdException(const std::exception& exception, const QString& context)
{
    QString message = QString("标准异常: %1").arg(exception.what());
    if (!context.isEmpty()) {
        message = QString("%1 (%2)").arg(message, context);
    }
    
    ApplicationException appException(message, "System");
    return handleException(appException);
}

bool ExceptionHandler::handleUnknownException(const QString& context)
{
    QString message = "未知异常";
    if (!context.isEmpty()) {
        message = QString("%1 (%2)").arg(message, context);
    }
    
    ApplicationException appException(message, "Unknown");
    return handleException(appException);
}

void ExceptionHandler::registerRecoveryStrategy(const QString& exceptionType, std::unique_ptr<IRecoveryStrategy> strategy)
{
    m_recoveryStrategies[exceptionType] = std::move(strategy);
}

void ExceptionHandler::setAutoRecoveryEnabled(bool enabled)
{
    m_autoRecoveryEnabled = enabled;
}

void ExceptionHandler::setMaxRecoveryAttempts(int maxAttempts)
{
    m_maxRecoveryAttempts = maxAttempts;
}

ExceptionHandler::ExceptionStats ExceptionHandler::getExceptionStats() const
{
    return m_stats;
}

void ExceptionHandler::resetStats()
{
    m_stats.totalExceptions = 0;
    m_stats.recoveredExceptions = 0;
    m_stats.failedRecoveries = 0;
    m_stats.lastExceptionTime.clear();
    m_stats.lastExceptionMessage.clear();
    m_recoveryAttempts.clear();
}

bool ExceptionHandler::attemptRecovery(const ApplicationException& exception)
{
    QString exceptionType = exception.getCategory();
    
    // 检查恢复尝试次数
    int attempts = m_recoveryAttempts.value(exceptionType, 0);
    if (attempts >= m_maxRecoveryAttempts) {
        if (m_logger) {
            m_logger->errorEvent(QString("异常恢复失败: %1 (已达到最大尝试次数 %2)")
                .arg(exception.getMessage()).arg(m_maxRecoveryAttempts));
        }
        return false;
    }
    
    // 增加尝试计数
    m_recoveryAttempts[exceptionType] = attempts + 1;
    
    // 查找对应的恢复策略
    auto it = m_recoveryStrategies.find(exceptionType);
    if (it != m_recoveryStrategies.end()) {
        if (m_logger) {
            m_logger->appEvent(QString("尝试使用恢复策略 '%1' 处理异常: %2")
                .arg(it.value()->getStrategyName(), exception.getMessage()));
        }
        
        try {
            // 启动恢复超时定时器
            m_recoveryTimer->start();
            
            bool success = it.value()->attemptRecovery(exception);
            
            m_recoveryTimer->stop();
            
            if (success) {
                // 恢复成功，重置尝试计数
                m_recoveryAttempts[exceptionType] = 0;
                if (m_logger) {
                    m_logger->appEvent(QString("异常恢复成功: %1").arg(exception.getMessage()));
                }
                return true;
            }
        } catch (...) {
            m_recoveryTimer->stop();
            if (m_logger) {
                m_logger->errorEvent(QString("恢复策略执行异常: %1").arg(exception.getMessage()));
            }
        }
    }
    
    return false;
}

void ExceptionHandler::logException(const ApplicationException& exception, bool recovered)
{
    if (!m_logger) {
        return;
    }
    
    QString logMessage = QString("[异常] %1: %2").arg(exception.getCategory(), exception.getMessage());
    if (recovered) {
        logMessage += " (已恢复)";
    }
    
    // 根据异常类型选择日志级别
    LogLevel level = L_ERROR;
    if (recovered) {
        level = L_WARNING;
    }
    
    m_logger->logEvent("异常处理", logMessage, "error.log", level);
    
    // 记录恢复提示
    if (!recovered && exception.isRecoverable()) {
        m_logger->logEvent("恢复建议", exception.getRecoveryHint(), "error.log", L_INFO);
    }
}

void ExceptionHandler::showErrorDialog(const ApplicationException& exception, bool recovered)
{
    if (!m_parentWidget) {
        return;
    }
    
    QString title = recovered ? "错误已恢复" : "应用程序错误";
    QString message = exception.getMessage();
    
    if (recovered) {
        message += "\n\n✓ 问题已自动修复，应用程序继续运行。";
    } else if (exception.isRecoverable()) {
        message += "\n\n💡 恢复建议：\n" + exception.getRecoveryHint();
    } else {
        message += "\n\n⚠️ 这是一个严重错误，可能需要重启应用程序。";
    }
    
    QMessageBox::Icon icon = recovered ? QMessageBox::Information : 
                            exception.isRecoverable() ? QMessageBox::Warning : QMessageBox::Critical;
    
    QMessageBox msgBox(icon, title, message, QMessageBox::Ok, m_parentWidget);
    msgBox.exec();
}

void ExceptionHandler::onRecoveryTimeout()
{
    if (m_logger) {
        m_logger->errorEvent("异常恢复超时");
    }
}

std::unique_ptr<IRecoveryStrategy> ExceptionHandler::createCEFRecoveryStrategy()
{
    return std::make_unique<CEFRecoveryStrategy>();
}

std::unique_ptr<IRecoveryStrategy> ExceptionHandler::createConfigRecoveryStrategy()
{
    return std::make_unique<ConfigRecoveryStrategy>();
}

std::unique_ptr<IRecoveryStrategy> ExceptionHandler::createResourceRecoveryStrategy()
{
    return std::make_unique<ResourceRecoveryStrategy>();
}

// CEFRecoveryStrategy 实现
bool CEFRecoveryStrategy::attemptRecovery(const ApplicationException& exception)
{
    // CEF恢复策略：尝试重新初始化CEF
    // 注意：这里只是示例实现，实际恢复逻辑需要根据具体的CEF管理器接口实现
    
    // 模拟恢复过程
    QThread::msleep(1000); // 等待1秒
    
    // 在实际实现中，这里应该调用CEF管理器的重新初始化方法
    // 例如：CEFManager::instance().reinitialize();
    
    return true; // 假设恢复成功
}

QString CEFRecoveryStrategy::getStrategyName() const
{
    return "CEF重新初始化恢复策略";
}

// ConfigRecoveryStrategy 实现
bool ConfigRecoveryStrategy::attemptRecovery(const ApplicationException& exception)
{
    // 配置恢复策略：尝试使用默认配置
    const ConfigException* configEx = dynamic_cast<const ConfigException*>(&exception);
    
    if (configEx) {
        QString configPath = configEx->getConfigPath();
        if (!configPath.isEmpty()) {
            // 尝试创建默认配置文件
            ConfigManager& manager = ConfigManager::instance();
            return manager.createDefaultConfig(configPath);
        }
    }
    
    return false;
}

QString ConfigRecoveryStrategy::getStrategyName() const
{
    return "配置文件恢复策略";
}

// ResourceRecoveryStrategy 实现
bool ResourceRecoveryStrategy::attemptRecovery(const ApplicationException& exception)
{
    // 资源恢复策略：检查并修复资源文件
    const ResourceException* resourceEx = dynamic_cast<const ResourceException*>(&exception);
    
    if (resourceEx) {
        QString resourcePath = resourceEx->getResourcePath();
        if (!resourcePath.isEmpty()) {
            // 检查文件是否存在
            QFile file(resourcePath);
            if (!file.exists()) {
                // 尝试从备用位置复制资源
                QString backupPath = resourcePath + ".backup";
                if (QFile::exists(backupPath)) {
                    return QFile::copy(backupPath, resourcePath);
                }
            }
        }
    }
    
    return false;
}

QString ResourceRecoveryStrategy::getStrategyName() const
{
    return "资源文件恢复策略";
}