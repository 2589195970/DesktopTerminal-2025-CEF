#include "logger.h"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QSysInfo>
#include <QMessageBox>
#include <QInputDialog>
#include <QLineEdit>
#include <QWidget>
#include <QMutexLocker>
#include <QRegExp>

#ifdef Q_OS_WIN
#include <windows.h>
#include <psapi.h>
#include <pdh.h>
#include <pdhmsg.h>
#include <tlhelp32.h>
#pragma comment(lib, "pdh.lib")
#elif defined(Q_OS_LINUX)
#include <unistd.h>
#include <sys/times.h>
#include <sys/sysinfo.h>
#include <sys/statvfs.h>
#elif defined(Q_OS_MAC)
#include <sys/types.h>
#include <sys/sysctl.h>
#include <mach/mach.h>
#include <mach/processor_info.h>
#include <mach/mach_host.h>
#endif

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
#include <QTextCodec>
#else
#include <QStringConverter>
#endif

Logger& Logger::instance()
{
    static Logger logger;
    return logger;
}

Logger::Logger()
    : QObject(nullptr)
    , m_logLevel(L_INFO)
    , m_flushTimer(nullptr)
    , m_nextTimerId(1)
    , m_applicationStartTime(QDateTime::currentDateTime())
{
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    QTextCodec::setCodecForLocale(QTextCodec::codecForName("UTF-8"));
#endif

    // 创建定时器用于自动刷新日志缓冲区
    m_flushTimer = new QTimer(this);
    connect(m_flushTimer, &QTimer::timeout, this, &Logger::onTimerTimeout);
    m_flushTimer->start(5000); // 每5秒自动刷新一次
}

Logger::~Logger()
{
    shutdown();
}

void Logger::setLogLevel(LogLevel level)
{
    m_logLevel = level;
}

LogLevel Logger::getLogLevel() const
{
    return m_logLevel;
}

bool Logger::ensureLogDirectoryExists()
{
    QString logDir = QCoreApplication::applicationDirPath() + "/log";
    QDir dir(logDir);
    if (!dir.exists()) {
        return dir.mkpath(".");
    }
    return true;
}

void Logger::logEvent(const QString &category, const QString &message, const QString &filename, LogLevel level)
{
    if (level < m_logLevel) {
        return;
    }

    LogEntry entry;
    entry.timestamp = QDateTime::currentDateTime();
    entry.category = category;
    entry.message = message;
    entry.filename = filename;

    m_logBuffer[filename].append(entry);

    // 如果缓冲区达到限制或者是警告/错误级别，立即刷新
    if (m_logBuffer[filename].size() >= LOG_BUFFER_SIZE || level >= L_WARNING) {
        flushLogBuffer(filename);
    }
}

void Logger::flushLogBuffer(const QString &filename)
{
    if (m_logBuffer[filename].isEmpty()) {
        return;
    }

    if (!ensureLogDirectoryExists()) {
        return;
    }

    QString logPath = QCoreApplication::applicationDirPath() + "/log/" + filename;
    QFile file(logPath);
    
    if (file.open(QIODevice::Append | QIODevice::Text)) {
        QTextStream out(&file);
        
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
        out.setCodec("UTF-8");
#else
        out.setEncoding(QStringConverter::Utf8);
#endif

        // 兼容C++11/14编译器，不使用std::as_const
        const auto& logBuffer = m_logBuffer[filename];
        for (const LogEntry &entry : logBuffer) {
            out << entry.timestamp.toString("yyyy-MM-dd hh:mm:ss")
                << " | " << entry.category << " | " << entry.message << "\n";
        }
        
        file.close();
        m_logBuffer[filename].clear();
    }
}

void Logger::flushAllLogBuffers()
{
    const QStringList keys = m_logBuffer.keys();
    for (const QString &key : keys) {
        flushLogBuffer(key);
    }
}

void Logger::appEvent(const QString &msg, LogLevel lv)
{
    logEvent("应用程序", msg, "app.log", lv);
}

void Logger::configEvent(const QString &msg, LogLevel lv)
{
    logEvent("配置文件", msg, "config.log", lv);
}

void Logger::hotkeyEvent(const QString &msg)
{
    logEvent("热键退出尝试", msg, "exit.log", L_INFO);
}

void Logger::exitEvent(const QString &msg, LogLevel lv)
{
    logEvent("程序退出", msg, "exit.log", lv);
}

void Logger::logStartup(const QString &path)
{
    logEvent("启动", QString("程序启动成功，使用配置文件: %1").arg(path), "startup.log", L_INFO);
}

void Logger::errorEvent(const QString &msg, LogLevel lv)
{
    logEvent("错误", msg, "error.log", lv);
}

void Logger::systemEvent(const QString &msg, LogLevel lv)
{
    logEvent("系统信息", msg, "system.log", lv);
}

void Logger::showMessage(QWidget *parent, const QString &title, const QString &message)
{
    QMessageBox::warning(parent, title, message);
}

void Logger::showCriticalError(QWidget *parent, const QString &title, const QString &message)
{
    QMessageBox::critical(parent, title, message);
    errorEvent(QString("%1: %2").arg(title).arg(message), L_ERROR);
}

bool Logger::getPassword(QWidget *parent, const QString &title, const QString &label, QString &password)
{
    bool ok;
    password = QInputDialog::getText(parent, title, label, QLineEdit::Password, "", &ok);
    return ok;
}

void Logger::collectSystemInfo()
{
    systemEvent(QString("Qt版本: %1").arg(QT_VERSION_STR));
    systemEvent(QString("操作系统: %1 %2").arg(QSysInfo::productType()).arg(QSysInfo::productVersion()));
    systemEvent(QString("系统架构: %1").arg(QSysInfo::currentCpuArchitecture()));
    systemEvent(QString("内核版本: %1 %2").arg(QSysInfo::kernelType()).arg(QSysInfo::kernelVersion()));
    systemEvent(QString("机器主机名: %1").arg(QSysInfo::machineHostName()));
    
    // GPU和显卡信息
    systemEvent(QString("OpenGL环境变量: QT_OPENGL=%1").arg(qgetenv("QT_OPENGL").constData()));
    systemEvent(QString("CEF标志: %1").arg(qgetenv("CEF_CHROMIUM_FLAGS").constData()));
    
    // 内存信息
    systemEvent("程序启动完成，系统信息已收集");
}

void Logger::shutdown()
{
    flushAllLogBuffers();
    
    if (m_flushTimer) {
        m_flushTimer->stop();
        m_flushTimer = nullptr;
    }
}

void Logger::onTimerTimeout()
{
    flushAllLogBuffers();
}

void Logger::logSystemInfo()
{
    systemEvent("=== 系统信息记录 ===");
    systemEvent(QString("操作系统: %1").arg(QSysInfo::prettyProductName()));
    systemEvent(QString("架构: %1").arg(QSysInfo::currentCpuArchitecture()));
    systemEvent(QString("内核类型: %1").arg(QSysInfo::kernelType()));
    systemEvent(QString("内核版本: %1").arg(QSysInfo::kernelVersion()));
    systemEvent(QString("主机名: %1").arg(QSysInfo::machineHostName()));
    systemEvent(QString("Qt版本: %1").arg(qVersion()));
    systemEvent("=== 系统信息记录完成 ===");
}

// 性能监控方法实现
int Logger::startPerformanceTimer(const QString &operationName)
{
    QMutexLocker locker(&m_performanceMutex);
    
    PerformanceTimer timer;
    timer.operationName = operationName;
    timer.startTime = QDateTime::currentDateTime();
    timer.timer.start();
    
    int timerId = m_nextTimerId++;
    m_performanceTimers[timerId] = timer;
    
    logEvent("性能监控", QString("开始计时: %1 (ID: %2)").arg(operationName).arg(timerId), "performance.log", L_DEBUG);
    
    return timerId;
}

void Logger::endPerformanceTimer(int timerId, const QString &additionalInfo)
{
    QMutexLocker locker(&m_performanceMutex);
    
    if (!m_performanceTimers.contains(timerId)) {
        logEvent("性能监控", QString("无效的计时器ID: %1").arg(timerId), "performance.log", L_WARNING);
        return;
    }
    
    PerformanceTimer timer = m_performanceTimers.take(timerId);
    qint64 elapsedMs = timer.timer.elapsed();
    
    QString message = QString("操作完成: %1, 耗时: %2ms").arg(timer.operationName).arg(elapsedMs);
    if (!additionalInfo.isEmpty()) {
        message += QString(", 附加信息: %1").arg(additionalInfo);
    }
    
    logEvent("性能监控", message, "performance.log", L_INFO);
    
    // 记录性能指标
    PerformanceMetric metric;
    metric.name = timer.operationName;
    metric.value = elapsedMs;
    metric.unit = "ms";
    metric.timestamp = QDateTime::currentDateTime();
    m_performanceMetrics.append(metric);
}

void Logger::logPerformanceMetric(const QString &metricName, double value, const QString &unit)
{
    QMutexLocker locker(&m_performanceMutex);
    
    PerformanceMetric metric;
    metric.name = metricName;
    metric.value = value;
    metric.unit = unit;
    metric.timestamp = QDateTime::currentDateTime();
    m_performanceMetrics.append(metric);
    
    QString message = QString("性能指标: %1 = %2 %3").arg(metricName).arg(value).arg(unit);
    logEvent("性能监控", message, "performance.log", L_INFO);
}

void Logger::logMemoryUsage()
{
#ifdef Q_OS_WIN
    // Windows内存使用情况
    PROCESS_MEMORY_COUNTERS pmc;
    if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) {
        double workingSetMB = pmc.WorkingSetSize / (1024.0 * 1024.0);
        double pageFileUsageMB = pmc.PagefileUsage / (1024.0 * 1024.0);
        
        logPerformanceMetric("内存-工作集", workingSetMB, "MB");
        logPerformanceMetric("内存-页面文件使用", pageFileUsageMB, "MB");
        
        QString message = QString("内存使用: 工作集=%1MB, 页面文件=%2MB")
            .arg(workingSetMB, 0, 'f', 2)
            .arg(pageFileUsageMB, 0, 'f', 2);
        logEvent("性能监控", message, "performance.log", L_INFO);
    }
#else
    // Linux/macOS: 使用/proc/self/status（Linux）或其他方法
    QFile statusFile("/proc/self/status");
    if (statusFile.open(QIODevice::ReadOnly)) {
        QTextStream stream(&statusFile);
        QString line;
        double vmSizeMB = 0, vmRssMB = 0;
        
        while (stream.readLineInto(&line)) {
            if (line.startsWith("VmSize:")) {
                QStringList parts = line.split(QRegExp("\\s+"));
                if (parts.size() >= 2) {
                    vmSizeMB = parts[1].toDouble() / 1024.0; // KB to MB
                }
            } else if (line.startsWith("VmRSS:")) {
                QStringList parts = line.split(QRegExp("\\s+"));
                if (parts.size() >= 2) {
                    vmRssMB = parts[1].toDouble() / 1024.0; // KB to MB
                }
            }
        }
        
        if (vmSizeMB > 0 || vmRssMB > 0) {
            logPerformanceMetric("内存-虚拟内存", vmSizeMB, "MB");
            logPerformanceMetric("内存-物理内存", vmRssMB, "MB");
            
            QString message = QString("内存使用: 虚拟内存=%1MB, 物理内存=%2MB")
                .arg(vmSizeMB, 0, 'f', 2)
                .arg(vmRssMB, 0, 'f', 2);
            logEvent("性能监控", message, "performance.log", L_INFO);
        }
    } else {
        logEvent("性能监控", "无法读取内存使用信息", "performance.log", L_WARNING);
    }
#endif
}

void Logger::logApplicationStartTime(const QDateTime &startTime)
{
    QDateTime currentTime = QDateTime::currentDateTime();
    qint64 startupMs = startTime.msecsTo(currentTime);
    
    logPerformanceMetric("应用启动时间", startupMs, "ms");
    
    QString message = QString("应用启动完成: 耗时 %1ms").arg(startupMs);
    logEvent("性能监控", message, "performance.log", L_INFO);
}

void Logger::logPageLoadPerformance(const QString &url, qint64 loadTime)
{
    logPerformanceMetric("页面加载时间", loadTime, "ms");
    
    QString message = QString("页面加载: %1, 耗时: %2ms").arg(url).arg(loadTime);
    logEvent("性能监控", message, "performance.log", L_INFO);
}

void Logger::logCEFInitPerformance(qint64 initTime, bool success)
{
    logPerformanceMetric("CEF初始化时间", initTime, "ms");
    
    QString message = QString("CEF初始化: %1, 耗时: %2ms")
        .arg(success ? "成功" : "失败")
        .arg(initTime);
    logEvent("性能监控", message, "performance.log", success ? L_INFO : L_ERROR);
}

void Logger::generatePerformanceReport()
{
    QMutexLocker locker(&m_performanceMutex);
    
    logEvent("性能监控", "=== 性能报告生成开始 ===", "performance.log", L_INFO);
    
    // 统计不同类型的性能指标
    QMap<QString, QList<double>> metricsByName;
    
    for (const PerformanceMetric &metric : m_performanceMetrics) {
        metricsByName[metric.name].append(metric.value);
    }
    
    // 生成统计报告
    for (auto it = metricsByName.begin(); it != metricsByName.end(); ++it) {
        const QString &name = it.key();
        const QList<double> &values = it.value();
        
        if (values.isEmpty()) continue;
        
        // 计算统计信息
        double sum = 0;
        double min = values.first();
        double max = values.first();
        
        for (double value : values) {
            sum += value;
            min = qMin(min, value);
            max = qMax(max, value);
        }
        
        double avg = sum / values.size();
        
        QString report = QString("指标统计: %1 - 次数:%2, 平均:%3ms, 最小:%4ms, 最大:%5ms")
            .arg(name)
            .arg(values.size())
            .arg(avg, 0, 'f', 2)
            .arg(min, 0, 'f', 2)
            .arg(max, 0, 'f', 2);
            
        logEvent("性能监控", report, "performance.log", L_INFO);
    }
    
    // 应用运行时长
    qint64 runtimeMs = m_applicationStartTime.msecsTo(QDateTime::currentDateTime());
    logEvent("性能监控", QString("应用运行时长: %1ms").arg(runtimeMs), "performance.log", L_INFO);
    
    logEvent("性能监控", "=== 性能报告生成完成 ===", "performance.log", L_INFO);
}