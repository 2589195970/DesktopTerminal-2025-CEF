#include "exceptions.h"

// ApplicationException 实现
ApplicationException::ApplicationException(const QString& message, const QString& category)
    : m_message(message), m_category(category)
{
}

const char* ApplicationException::what() const noexcept
{
    m_whatBuffer = QString("[%1] %2").arg(m_category, m_message).toUtf8();
    return m_whatBuffer.constData();
}

QString ApplicationException::getMessage() const
{
    return m_message;
}

QString ApplicationException::getCategory() const
{
    return m_category;
}

bool ApplicationException::isRecoverable() const
{
    return true; // 默认认为可以恢复
}

QString ApplicationException::getRecoveryHint() const
{
    return "请重试操作或联系技术支持";
}

// CEFException 实现
CEFException::CEFException(const QString& message)
    : ApplicationException(message, "CEF")
{
}

bool CEFException::isRecoverable() const
{
    return true; // CEF异常通常可以通过重新初始化恢复
}

QString CEFException::getRecoveryHint() const
{
    return "请尝试重新启动应用程序。如果问题持续存在，可能需要重新安装CEF组件。";
}

// ConfigException 实现
ConfigException::ConfigException(const QString& message, const QString& configPath)
    : ApplicationException(message, "Config"), m_configPath(configPath)
{
}

bool ConfigException::isRecoverable() const
{
    return true; // 配置异常通常可以通过修复配置文件恢复
}

QString ConfigException::getRecoveryHint() const
{
    QString hint = "请检查配置文件格式是否正确";
    if (!m_configPath.isEmpty()) {
        hint += QString("：%1").arg(m_configPath);
    }
    hint += "。您可以删除配置文件以使用默认设置，或参考配置示例文件。";
    return hint;
}

QString ConfigException::getConfigPath() const
{
    return m_configPath;
}

// CompatibilityException 实现
CompatibilityException::CompatibilityException(const QString& message, const QString& systemInfo)
    : ApplicationException(message, "Compatibility"), m_systemInfo(systemInfo)
{
}

bool CompatibilityException::isRecoverable() const
{
    return false; // 兼容性问题通常不能在运行时恢复
}

QString CompatibilityException::getRecoveryHint() const
{
    QString hint = "您的系统可能不满足运行要求";
    if (!m_systemInfo.isEmpty()) {
        hint += QString("：%1").arg(m_systemInfo);
    }
    hint += "。请检查系统要求或尝试在兼容模式下运行。";
    return hint;
}

QString CompatibilityException::getSystemInfo() const
{
    return m_systemInfo;
}

// LoggingException 实现
LoggingException::LoggingException(const QString& message)
    : ApplicationException(message, "Logging")
{
}

bool LoggingException::isRecoverable() const
{
    return true; // 日志异常不应影响核心功能
}

QString LoggingException::getRecoveryHint() const
{
    return "日志功能异常不会影响程序主要功能。请检查磁盘空间和文件权限。";
}

// SecurityException 实现
SecurityException::SecurityException(const QString& message, const QString& violationType)
    : ApplicationException(message, "Security"), m_violationType(violationType)
{
}

bool SecurityException::isRecoverable() const
{
    return false; // 安全异常通常不可恢复，需要终止操作
}

QString SecurityException::getRecoveryHint() const
{
    QString hint = "检测到安全违规";
    if (!m_violationType.isEmpty()) {
        hint += QString("（%1）").arg(m_violationType);
    }
    hint += "。为了系统安全，相关操作已被阻止。";
    return hint;
}

QString SecurityException::getViolationType() const
{
    return m_violationType;
}

// ResourceException 实现
ResourceException::ResourceException(const QString& message, const QString& resourcePath)
    : ApplicationException(message, "Resource"), m_resourcePath(resourcePath)
{
}

bool ResourceException::isRecoverable() const
{
    return true; // 资源异常可能可以通过重试或替代资源恢复
}

QString ResourceException::getRecoveryHint() const
{
    QString hint = "资源访问失败";
    if (!m_resourcePath.isEmpty()) {
        hint += QString("：%1").arg(m_resourcePath);
    }
    hint += "。请检查文件是否存在和权限设置，或尝试重新安装应用程序。";
    return hint;
}

QString ResourceException::getResourcePath() const
{
    return m_resourcePath;
}

// NetworkException 实现
NetworkException::NetworkException(const QString& message, int errorCode)
    : ApplicationException(message, "Network"), m_errorCode(errorCode)
{
}

bool NetworkException::isRecoverable() const
{
    return true; // 网络异常通常可以通过重试恢复
}

QString NetworkException::getRecoveryHint() const
{
    QString hint = "网络连接异常";
    if (m_errorCode != 0) {
        hint += QString("（错误代码：%1）").arg(m_errorCode);
    }
    hint += "。请检查网络连接，稍后重试。";
    return hint;
}

int NetworkException::getErrorCode() const
{
    return m_errorCode;
}