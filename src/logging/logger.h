#ifndef LOGGER_H
#define LOGGER_H

#include <QObject>
#include <QString>
#include <QDateTime>
#include <QMap>
#include <QList>
#include <QTimer>
#include <QElapsedTimer>
#include <QMutex>
#include <QThread>
// QNetworkAccessManager和QNetworkReply已移除以避免Qt Network模块依赖

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

// 性能计时器结构
struct PerformanceTimer {
    QString operationName;
    QElapsedTimer timer;
    QDateTime startTime;
};

// 性能指标结构
struct PerformanceMetric {
    QString name;
    double value;
    QString unit;
    QDateTime timestamp;
};

// 运行时性能快照结构
struct RuntimePerformanceSnapshot {
    QDateTime timestamp;
    double cpuUsage;                    // CPU使用率 (%)
    double memoryUsageMB;               // 内存使用 (MB)
    double memoryUsagePercent;          // 内存使用率 (%)
    qint64 networkLatency;              // 网络延迟 (ms)
    bool networkConnected;              // 网络连接状态
    int activeThreadCount;              // 活跃线程数
    double diskUsagePercent;            // 磁盘使用率 (%)
    qint64 uptimeSeconds;               // 运行时长 (秒)
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
     * @brief 记录系统信息
     */
    void logSystemInfo();

    /**
     * @brief 关闭日志系统
     */
    void shutdown();

    // 性能监控方法
    /**
     * @brief 开始性能计时
     * @param operationName 操作名称
     * @return 计时器ID
     */
    int startPerformanceTimer(const QString &operationName);

    /**
     * @brief 结束性能计时并记录
     * @param timerId 计时器ID
     * @param additionalInfo 附加信息
     */
    void endPerformanceTimer(int timerId, const QString &additionalInfo = QString());

    /**
     * @brief 记录性能指标
     * @param metricName 指标名称
     * @param value 指标值
     * @param unit 单位
     */
    void logPerformanceMetric(const QString &metricName, double value, const QString &unit = "ms");

    /**
     * @brief 记录内存使用情况
     */
    void logMemoryUsage();

    /**
     * @brief 记录应用启动时间
     * @param startTime 启动开始时间
     */
    void logApplicationStartTime(const QDateTime &startTime);

    /**
     * @brief 记录页面加载性能
     * @param url 页面URL
     * @param loadTime 加载时间（毫秒）
     */
    void logPageLoadPerformance(const QString &url, qint64 loadTime);

    /**
     * @brief 记录CEF初始化性能
     * @param initTime 初始化时间（毫秒）
     * @param success 是否成功
     */
    void logCEFInitPerformance(qint64 initTime, bool success);

    /**
     * @brief 生成性能报告
     */
    void generatePerformanceReport();

    // 运行时性能监控方法
    /**
     * @brief 启动运行时性能监控
     * @param intervalMs 监控间隔（毫秒）
     */
    void startRuntimePerformanceMonitoring(int intervalMs = 30000);

    /**
     * @brief 停止运行时性能监控
     */
    void stopRuntimePerformanceMonitoring();

    /**
     * @brief 记录CPU使用率
     */
    void logCPUUsage();

    /**
     * @brief 记录网络状态（简化版本，不依赖Qt Network模块）
     */
    void logNetworkStatus();

    /**
     * @brief 记录磁盘使用情况
     */
    void logDiskUsage();

    /**
     * @brief 记录用户交互响应时间
     * @param interactionType 交互类型
     * @param responseTime 响应时间（毫秒）
     */
    void logUserInteractionPerformance(const QString &interactionType, qint64 responseTime);

    /**
     * @brief 记录CEF进程状态
     */
    void logCEFProcessStatus();

    /**
     * @brief 获取系统资源趋势分析
     */
    void generateResourceTrendAnalysis();

    /**
     * @brief 设置性能监控配置
     * @param enabled 是否启用
     * @param interval 监控间隔（毫秒）
     * @param enableCPU 是否监控CPU
     * @param enableMemory 是否监控内存
     * @param enableNetwork 是否监控网络
     * @param enableDisk 是否监控磁盘
     */
    void configurePerformanceMonitoring(bool enabled, int interval = 30000, 
                                       bool enableCPU = true, bool enableMemory = true, 
                                       bool enableNetwork = true, bool enableDisk = true);

private:
    Logger();
    ~Logger();
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

private slots:
    void onTimerTimeout();
    void onRuntimePerformanceCheck();
    // onNetworkLatencyCheckFinished已移除以避免Qt Network模块依赖

private:
    static const int LOG_BUFFER_SIZE = 10;
    
    LogLevel m_logLevel;
    QMap<QString, QList<LogEntry>> m_logBuffer;
    QTimer* m_flushTimer;

    // 性能监控相关成员
    QMap<int, PerformanceTimer> m_performanceTimers;
    QList<PerformanceMetric> m_performanceMetrics;
    int m_nextTimerId;
    QMutex m_performanceMutex;
    QDateTime m_applicationStartTime;
    
    // 运行时性能监控
    QTimer* m_runtimePerformanceTimer;
    QList<RuntimePerformanceSnapshot> m_runtimeSnapshots;
    QMutex m_runtimeMutex;
    
    // 网络延迟检测功能已移除以避免Qt Network模块依赖
    // QNetworkAccessManager* m_networkManager;
    // QElapsedTimer m_networkLatencyTimer;
    
    // 性能监控配置
    bool m_performanceMonitoringEnabled;
    int m_performanceMonitoringInterval;
    bool m_monitorCPU;
    bool m_monitorMemory;
    bool m_monitorNetwork;
    bool m_monitorDisk;
    
    // 系统资源历史数据（最多保留24小时数据）
    static const int MAX_RUNTIME_SNAPSHOTS = 2880; // 24小时 * 60分钟 * 2（每30秒一次）
    
    // 私有辅助方法
    double getCurrentCPUUsage();
    double getCurrentMemoryUsagePercent();
    qint64 getSystemUptimeSeconds();
    double getDiskUsagePercent();
    void cleanupOldSnapshots();
};

#endif // LOGGER_H