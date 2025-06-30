#include "async_log_writer.h"
#include <QDir>
#include <QStandardPaths>
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QMutexLocker>

AsyncLogWriter::AsyncLogWriter(QObject* parent)
    : QThread(parent)
    , m_stopRequested(false)
    , m_maxBufferSize(DEFAULT_MAX_BUFFER_SIZE)
    , m_flushTimer(nullptr)
    , m_flushInterval(DEFAULT_FLUSH_INTERVAL)
    , m_enableBuffering(true)
    , m_consecutiveErrors(0)
{
    // 初始化统计信息
    m_stats.totalEntriesWritten = 0;
    m_stats.totalBytesWritten = 0;
    m_stats.queueSize = 0;
    m_stats.droppedEntries = 0;
    m_stats.averageWriteTime = 0.0;
    
    // 创建刷新定时器（在主线程中创建）
    m_flushTimer = new QTimer();
    m_flushTimer->setInterval(m_flushInterval);
    m_flushTimer->moveToThread(this);
    connect(m_flushTimer, &QTimer::timeout, this, &AsyncLogWriter::onFlushTimer);
}

AsyncLogWriter::~AsyncLogWriter()
{
    stopWriter();
    if (m_flushTimer) {
        delete m_flushTimer;
    }
}

void AsyncLogWriter::addLogEntry(const AsyncLogEntry& entry)
{
    QMutexLocker locker(&m_queueMutex);
    
    // 检查缓冲区是否已满
    if (m_logQueue.size() >= m_maxBufferSize) {
        // 丢弃最旧的条目
        m_logQueue.dequeue();
        m_stats.droppedEntries++;
        emit bufferOverflow(entry.filename);
    }
    
    m_logQueue.enqueue(entry);
    m_stats.queueSize = m_logQueue.size();
    
    // 唤醒写入线程
    m_queueCondition.wakeOne();
}

void AsyncLogWriter::setMaxBufferSize(int maxSize)
{
    QMutexLocker locker(&m_queueMutex);
    m_maxBufferSize = maxSize;
}

void AsyncLogWriter::setFlushInterval(int intervalMs)
{
    m_flushInterval = intervalMs;
    if (m_flushTimer) {
        m_flushTimer->setInterval(intervalMs);
    }
}

void AsyncLogWriter::flushAll()
{
    // 通过添加特殊的刷新指令来触发刷新
    AsyncLogEntry flushEntry;
    flushEntry.category = "__FLUSH_ALL__";
    addLogEntry(flushEntry);
}

void AsyncLogWriter::flushFile(const QString& filename)
{
    AsyncLogEntry flushEntry;
    flushEntry.category = "__FLUSH_FILE__";
    flushEntry.filename = filename;
    addLogEntry(flushEntry);
}

void AsyncLogWriter::stopWriter()
{
    if (!isRunning()) {
        return;
    }
    
    m_stopRequested = true;
    
    // 唤醒线程
    m_queueCondition.wakeAll();
    
    // 等待线程结束
    if (!wait(5000)) {
        terminate();
        wait(1000);
    }
}

AsyncLogWriter::WriteStats AsyncLogWriter::getWriteStats() const
{
    QMutexLocker locker(&m_statsMutex);
    WriteStats stats = m_stats;
    stats.queueSize = m_logQueue.size();
    return stats;
}

void AsyncLogWriter::resetStats()
{
    QMutexLocker locker(&m_statsMutex);
    m_stats.totalEntriesWritten = 0;
    m_stats.totalBytesWritten = 0;
    m_stats.droppedEntries = 0;
    m_stats.averageWriteTime = 0.0;
    m_writeTimeHistory.clear();
}

void AsyncLogWriter::run()
{
    // 启动刷新定时器
    m_flushTimer->start();
    
    while (!m_stopRequested) {
        processLogQueue();
        
        // 如果队列为空，等待新条目
        if (m_logQueue.isEmpty()) {
            QMutexLocker locker(&m_queueMutex);
            if (!m_stopRequested && m_logQueue.isEmpty()) {
                m_queueCondition.wait(&m_queueMutex, 1000); // 最多等待1秒
            }
        }
    }
    
    // 处理剩余的日志条目
    processLogQueue();
    
    // 关闭所有文件
    closeAllFiles();
    
    m_flushTimer->stop();
}

void AsyncLogWriter::onFlushTimer()
{
    // 刷新所有文件缓冲区
    for (auto it = m_fileBuffers.begin(); it != m_fileBuffers.end(); ++it) {
        if (!it.value().isEmpty()) {
            flushFileBuffer(it.key());
        }
    }
    
    // 更新统计信息
    updateStats();
}

void AsyncLogWriter::processLogQueue()
{
    QElapsedTimer timer;
    
    while (!m_logQueue.isEmpty() && !m_stopRequested) {
        AsyncLogEntry entry;
        
        {
            QMutexLocker locker(&m_queueMutex);
            if (m_logQueue.isEmpty()) {
                break;
            }
            entry = m_logQueue.dequeue();
            m_stats.queueSize = m_logQueue.size();
        }
        
        // 处理特殊指令
        if (entry.category == "__FLUSH_ALL__") {
            for (const QString& filename : m_fileBuffers.keys()) {
                flushFileBuffer(filename);
            }
            continue;
        } else if (entry.category == "__FLUSH_FILE__") {
            flushFileBuffer(entry.filename);
            continue;
        }
        
        // 写入日志条目
        timer.start();
        writeLogEntry(entry);
        double writeTime = timer.elapsed();
        
        // 更新写入时间历史
        {
            QMutexLocker locker(&m_statsMutex);
            m_writeTimeHistory.enqueue(writeTime);
            if (m_writeTimeHistory.size() > MAX_WRITE_TIME_HISTORY) {
                m_writeTimeHistory.dequeue();
            }
            
            // 计算平均写入时间
            double sum = 0.0;
            for (double time : m_writeTimeHistory) {
                sum += time;
            }
            m_stats.averageWriteTime = sum / m_writeTimeHistory.size();
        }
    }
}

void AsyncLogWriter::writeLogEntry(const AsyncLogEntry& entry)
{
    if (m_enableBuffering) {
        // 添加到文件缓冲区
        m_fileBuffers[entry.filename].enqueue(entry);
        
        // 检查是否需要刷新缓冲区
        if (m_fileBuffers[entry.filename].size() >= 10) { // 每10条刷新一次
            flushFileBuffer(entry.filename);
        }
    } else {
        // 直接写入文件
        if (ensureFileOpen(entry.filename)) {
            QString formattedEntry = formatLogEntry(entry);
            QTextStream* stream = m_fileStreams.value(entry.filename);
            if (stream) {
                *stream << formattedEntry << Qt::endl;
                stream->flush();
                
                QMutexLocker locker(&m_statsMutex);
                m_stats.totalEntriesWritten++;
                m_stats.totalBytesWritten += formattedEntry.toUtf8().size();
                m_stats.lastWriteTime = QDateTime::currentDateTime().toString();
                m_consecutiveErrors = 0; // 重置错误计数
            }
        }
    }
}

bool AsyncLogWriter::ensureFileOpen(const QString& filename)
{
    if (m_openFiles.contains(filename)) {
        return true;
    }
    
    // 确保日志目录存在
    QString logDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/log";
    QDir dir;
    if (!dir.exists(logDir)) {
        dir.mkpath(logDir);
    }
    
    QString fullPath = logDir + "/" + filename;
    
    QFile* file = new QFile(fullPath);
    if (!file->open(QIODevice::WriteOnly | QIODevice::Append)) {
        delete file;
        m_consecutiveErrors++;
        emit writeError(filename, QString("无法打开文件: %1").arg(file->errorString()));
        return false;
    }
    
    QTextStream* stream = new QTextStream(file);
    stream->setCodec("UTF-8");
    
    m_openFiles[filename] = file;
    m_fileStreams[filename] = stream;
    
    return true;
}

void AsyncLogWriter::closeAllFiles()
{
    // 先刷新所有缓冲区
    for (const QString& filename : m_fileBuffers.keys()) {
        flushFileBuffer(filename);
    }
    
    // 关闭所有文件
    for (auto it = m_fileStreams.begin(); it != m_fileStreams.end(); ++it) {
        delete it.value();
    }
    m_fileStreams.clear();
    
    for (auto it = m_openFiles.begin(); it != m_openFiles.end(); ++it) {
        it.value()->close();
        delete it.value();
    }
    m_openFiles.clear();
}

void AsyncLogWriter::flushFileBuffer(const QString& filename)
{
    if (!m_fileBuffers.contains(filename) || m_fileBuffers[filename].isEmpty()) {
        return;
    }
    
    if (!ensureFileOpen(filename)) {
        return;
    }
    
    QTextStream* stream = m_fileStreams.value(filename);
    if (!stream) {
        return;
    }
    
    QQueue<AsyncLogEntry>& buffer = m_fileBuffers[filename];
    int entriesWritten = 0;
    qint64 bytesWritten = 0;
    
    while (!buffer.isEmpty()) {
        AsyncLogEntry entry = buffer.dequeue();
        QString formattedEntry = formatLogEntry(entry);
        *stream << formattedEntry << Qt::endl;
        
        entriesWritten++;
        bytesWritten += formattedEntry.toUtf8().size();
    }
    
    stream->flush();
    
    // 更新统计信息
    {
        QMutexLocker locker(&m_statsMutex);
        m_stats.totalEntriesWritten += entriesWritten;
        m_stats.totalBytesWritten += bytesWritten;
        m_stats.lastWriteTime = QDateTime::currentDateTime().toString();
        m_consecutiveErrors = 0; // 重置错误计数
    }
}

void AsyncLogWriter::checkBufferLimits()
{
    // 检查是否有缓冲区过大
    for (auto it = m_fileBuffers.begin(); it != m_fileBuffers.end(); ++it) {
        if (it.value().size() > 50) { // 如果单个文件缓冲区超过50条
            flushFileBuffer(it.key());
        }
    }
}

QString AsyncLogWriter::formatLogEntry(const AsyncLogEntry& entry)
{
    return QString("[%1] [%2] %3")
        .arg(entry.timestamp.toString("yyyy-MM-dd hh:mm:ss.zzz"))
        .arg(entry.category)
        .arg(entry.message);
}

void AsyncLogWriter::updateStats()
{
    QMutexLocker locker(&m_statsMutex);
    emit statsUpdated(m_stats);
}

void AsyncLogWriter::cleanupOldFiles()
{
    // 清理超过7天的日志文件
    QString logDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/log";
    QDir dir(logDir);
    
    QFileInfoList files = dir.entryInfoList(QStringList() << "*.log", QDir::Files);
    QDateTime cutoffTime = QDateTime::currentDateTime().addDays(-7);
    
    for (const QFileInfo& fileInfo : files) {
        if (fileInfo.lastModified() < cutoffTime) {
            QFile::remove(fileInfo.absoluteFilePath());
        }
    }
}

// AsyncLogManager 实现
AsyncLogManager& AsyncLogManager::instance()
{
    static AsyncLogManager manager;
    return manager;
}

AsyncLogManager::AsyncLogManager(QObject* parent)
    : QObject(parent), m_writer(nullptr), m_initialized(false)
{
}

AsyncLogManager::~AsyncLogManager()
{
    shutdown();
}

bool AsyncLogManager::initialize()
{
    QMutexLocker locker(&m_initMutex);
    
    if (m_initialized) {
        return true;
    }
    
    m_writer = new AsyncLogWriter(this);
    m_writer->start();
    
    m_initialized = true;
    return true;
}

void AsyncLogManager::shutdown()
{
    QMutexLocker locker(&m_initMutex);
    
    if (!m_initialized) {
        return;
    }
    
    if (m_writer) {
        m_writer->stopWriter();
        delete m_writer;
        m_writer = nullptr;
    }
    
    m_initialized = false;
}

AsyncLogWriter* AsyncLogManager::getWriter()
{
    if (!m_initialized) {
        initialize();
    }
    return m_writer;
}

void AsyncLogManager::configure(int maxBufferSize, int flushIntervalMs)
{
    if (m_writer) {
        m_writer->setMaxBufferSize(maxBufferSize);
        m_writer->setFlushInterval(flushIntervalMs);
    }
}

bool AsyncLogManager::isInitialized() const
{
    return m_initialized;
}