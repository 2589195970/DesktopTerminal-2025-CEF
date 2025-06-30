#ifndef EXCEPTIONS_H
#define EXCEPTIONS_H

#include <exception>
#include <QString>

/**
 * @brief 应用程序基础异常类
 * 
 * 所有应用程序特定异常的基类
 */
class ApplicationException : public std::exception
{
public:
    explicit ApplicationException(const QString& message, const QString& category = "General");
    virtual ~ApplicationException() noexcept = default;

    const char* what() const noexcept override;
    QString getMessage() const;
    QString getCategory() const;
    virtual bool isRecoverable() const;
    virtual QString getRecoveryHint() const;

protected:
    QString m_message;
    QString m_category;
    mutable QByteArray m_whatBuffer;
};

/**
 * @brief CEF相关异常
 */
class CEFException : public ApplicationException
{
public:
    explicit CEFException(const QString& message);
    bool isRecoverable() const override;
    QString getRecoveryHint() const override;
};

/**
 * @brief 配置相关异常
 */
class ConfigException : public ApplicationException
{
public:
    explicit ConfigException(const QString& message, const QString& configPath = "");
    bool isRecoverable() const override;
    QString getRecoveryHint() const override;
    QString getConfigPath() const;

private:
    QString m_configPath;
};

/**
 * @brief 系统兼容性异常
 */
class CompatibilityException : public ApplicationException
{
public:
    explicit CompatibilityException(const QString& message, const QString& systemInfo = "");
    bool isRecoverable() const override;
    QString getRecoveryHint() const override;
    QString getSystemInfo() const;

private:
    QString m_systemInfo;
};

/**
 * @brief 日志系统异常
 */
class LoggingException : public ApplicationException
{
public:
    explicit LoggingException(const QString& message);
    bool isRecoverable() const override;
    QString getRecoveryHint() const override;
};

/**
 * @brief 安全相关异常
 */
class SecurityException : public ApplicationException
{
public:
    explicit SecurityException(const QString& message, const QString& violationType = "");
    bool isRecoverable() const override;
    QString getRecoveryHint() const override;
    QString getViolationType() const;

private:
    QString m_violationType;
};

/**
 * @brief 资源相关异常
 */
class ResourceException : public ApplicationException
{
public:
    explicit ResourceException(const QString& message, const QString& resourcePath = "");
    bool isRecoverable() const override;
    QString getRecoveryHint() const override;
    QString getResourcePath() const;

private:
    QString m_resourcePath;
};

/**
 * @brief 网络相关异常
 */
class NetworkException : public ApplicationException
{
public:
    explicit NetworkException(const QString& message, int errorCode = 0);
    bool isRecoverable() const override;
    QString getRecoveryHint() const override;
    int getErrorCode() const;

private:
    int m_errorCode;
};

#endif // EXCEPTIONS_H