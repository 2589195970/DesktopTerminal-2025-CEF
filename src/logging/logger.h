#ifndef LOGGER_H
#define LOGGER_H

#include <QObject>
#include <QString>
#include <QDateTime>
#include <QMap>
#include <QList>
#include <QTimer>

class QWidget;

// 日志级别枚举
enum LogLevel { 
    L_DEBUG, 
    L_INFO, 
    L_WARNING, 
    L_ERROR 
};

// 日志条目结构
struct LogEntry {
    QDateTime timestamp;
    QString category;
    QString message;
    QString filename;
};

/**
 * @brief 日志管理器类
 * 
 * 提供分类日志记录功能，支持缓冲写入和自动刷新
 * 完全保持与原项目相同的接口和功能
 */
class Logger : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief 获取单例实例
     */
    static Logger& instance();

    /**
     * @brief 设置日志级别
     */
    void setLogLevel(LogLevel level);

    /**
     * @brief 获取当前日志级别
     */
    LogLevel getLogLevel() const;

    /**
     * @brief 确保日志目录存在
     */
    bool ensureLogDirectoryExists();

    /**
     * @brief 记录日志事件
     * @param category 事件分类
     * @param message 日志消息
     * @param filename 日志文件名（默认app.log）
     * @param level 日志级别（默认INFO）
     */
    void logEvent(const QString &category, const QString &message,
                  const QString &filename = "app.log", LogLevel level = L_INFO);

    /**
     * @brief 刷新指定文件的日志缓冲区
     */
    void flushLogBuffer(const QString &filename);

    /**
     * @brief 刷新所有日志缓冲区
     */
    void flushAllLogBuffers();

    // 便捷的日志记录方法（与原项目完全相同）
    void appEvent(const QString &msg, LogLevel lv = L_INFO);
    void configEvent(const QString &msg, LogLevel lv = L_INFO);
    void hotkeyEvent(const QString &msg);
    void logStartup(const QString &path);
    void errorEvent(const QString &msg, LogLevel lv = L_ERROR);
    void systemEvent(const QString &msg, LogLevel lv = L_INFO);

    // UI交互方法（与原项目完全相同）
    void showMessage(QWidget *parent, const QString &title, const QString &message);
    void showCriticalError(QWidget *parent, const QString &title, const QString &message);
    bool getPassword(QWidget *parent, const QString &title, const QString &label, QString &password);

    /**
     * @brief 收集系统信息用于诊断
     */
    void collectSystemInfo();

    /**
     * @brief 关闭日志系统
     */
    void shutdown();

private:
    Logger();
    ~Logger();
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

private slots:
    void onTimerTimeout();

private:
    static const int LOG_BUFFER_SIZE = 10;
    
    LogLevel m_logLevel;
    QMap<QString, QList<LogEntry>> m_logBuffer;
    QTimer* m_flushTimer;
};

#endif // LOGGER_H