#ifndef ASYNC_LOG_WRITER_H
#define ASYNC_LOG_WRITER_H

#include <QThread>
#include <QMutex>
#include <QWaitCondition>
#include <QQueue>
#include <QDateTime>
#include <QMap>
#include <QTimer>
#include <QFile>
#include <QTextStream>
#include <atomic>

/**
 * @brief 异步日志条目结构
 */
struct AsyncLogEntry {
    QDateTime timestamp;
    QString category;
    QString message;
    QString filename;
    int level;
    
    AsyncLogEntry() : level(0) {}
    AsyncLogEntry(const QDateTime& ts, const QString& cat, const QString& msg, 
                  const QString& file, int lv)
        : timestamp(ts), category(cat), message(msg), filename(file), level(lv) {}
};

/**
 * @brief 异步日志写入器类
 * 
 * 在独立线程中处理日志写入，避免阻塞主线程
 */
class AsyncLogWriter : public QThread
{
    Q_OBJECT

public:
    explicit AsyncLogWriter(QObject* parent = nullptr);
    ~AsyncLogWriter();

    /**
     * @brief 添加日志条目到写入队列
     * @param entry 日志条目
     */
    void addLogEntry(const AsyncLogEntry& entry);

    /**
     * @brief 设置缓冲区大小限制
     * @param maxSize 最大缓冲区大小
     */
    void setMaxBufferSize(int maxSize);

    /**
     * @brief 设置刷新间隔
     * @param intervalMs 刷新间隔（毫秒）
     */
    void setFlushInterval(int intervalMs);

    /**
     * @brief 立即刷新所有缓冲区
     */
    void flushAll();

    /**
     * @brief 刷新指定文件的缓冲区
     * @param filename 文件名
     */
    void flushFile(const QString& filename);

    /**
     * @brief 停止日志写入器
     */
    void stopWriter();

    /**
     * @brief 获取统计信息
     */
    struct WriteStats {
        qint64 totalEntriesWritten;
        qint64 totalBytesWritten;
        qint64 queueSize;
        qint64 droppedEntries;
        double averageWriteTime;
        QString lastWriteTime;
    };
    WriteStats getWriteStats() const;

    /**
     * @brief 重置统计信息
     */
    void resetStats();

signals:
    /**
     * @brief 缓冲区满时发出的信号
     */
    void bufferOverflow(const QString& filename);

    /**
     * @brief 写入错误信号
     */
    void writeError(const QString& filename, const QString& error);

    /**
     * @brief 统计信息更新信号
     */
    void statsUpdated(const WriteStats& stats);

protected:
    void run() override;

private slots:
    void onFlushTimer();

private:
    // 写入方法
    void processLogQueue();
    void writeLogEntry(const AsyncLogEntry& entry);
    bool ensureFileOpen(const QString& filename);
    void closeAllFiles();
    
    // 缓冲管理
    void flushFileBuffer(const QString& filename);
    void checkBufferLimits();
    QString formatLogEntry(const AsyncLogEntry& entry);
    
    // 性能优化
    void updateStats();
    void cleanupOldFiles();

private:
    // 线程同步
    mutable QMutex m_queueMutex;
    QWaitCondition m_queueCondition;
    std::atomic<bool> m_stopRequested;
    
    // 日志队列
    QQueue<AsyncLogEntry> m_logQueue;
    int m_maxBufferSize;
    
    // 文件管理
    QMap<QString, QFile*> m_openFiles;
    QMap<QString, QTextStream*> m_fileStreams;
    QMap<QString, QQueue<AsyncLogEntry>> m_fileBuffers;
    
    // 刷新定时器
    QTimer* m_flushTimer;
    int m_flushInterval;
    
    // 统计信息
    mutable QMutex m_statsMutex;
    WriteStats m_stats;
    QDateTime m_lastStatsUpdate;
    QQueue<double> m_writeTimeHistory; // 用于计算平均写入时间
    
    // 性能优化配置
    static const int DEFAULT_MAX_BUFFER_SIZE = 1000;
    static const int DEFAULT_FLUSH_INTERVAL = 1000; // 1秒
    static const int MAX_WRITE_TIME_HISTORY = 100;  // 保留最近100次写入时间
    static const int FILE_CLEANUP_INTERVAL = 3600;  // 1小时清理一次旧文件
    
    // 缓冲区管理
    bool m_enableBuffering;
    QMap<QString, int> m_fileBufferSizes;
    
    // 错误处理
    int m_consecutiveErrors;
    static const int MAX_CONSECUTIVE_ERRORS = 10;
};

/**
 * @brief 异步日志管理器
 * 
 * 管理异步日志写入器的生命周期和配置
 */
class AsyncLogManager : public QObject
{
    Q_OBJECT

public:
    static AsyncLogManager& instance();
    
    /**
     * @brief 初始化异步日志系统
     */
    bool initialize();
    
    /**
     * @brief 关闭异步日志系统
     */
    void shutdown();
    
    /**
     * @brief 获取日志写入器
     */
    AsyncLogWriter* getWriter();
    
    /**
     * @brief 配置异步日志系统
     */
    void configure(int maxBufferSize, int flushIntervalMs);
    
    /**
     * @brief 检查是否已初始化
     */
    bool isInitialized() const;

private:
    explicit AsyncLogManager(QObject* parent = nullptr);
    ~AsyncLogManager();
    
    AsyncLogWriter* m_writer;
    bool m_initialized;
    QMutex m_initMutex;
};

#endif // ASYNC_LOG_WRITER_H