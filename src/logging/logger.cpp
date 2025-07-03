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
#include <QProcess>

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
#include <QTextCodec>
#else
#include <QStringConverter>
#endif

// 平台特定的头文件
#ifdef Q_OS_WIN
#include <windows.h>
#include <psapi.h>
#include <pdh.h>
#include <pdhmsg.h>
#elif defined(Q_OS_MAC)
#include <mach/mach.h>
#include <mach/vm_statistics.h>
#include <mach/mach_types.h>
#include <mach/mach_init.h>
#include <mach/mach_host.h>
#include <sys/types.h>
#include <sys/sysctl.h>
#include <sys/resource.h>
#elif defined(Q_OS_LINUX)
#include <unistd.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
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
    , m_performanceTimer(nullptr)
{
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    QTextCodec::setCodecForLocale(QTextCodec::codecForName("UTF-8"));
#endif

    // 创建定时器用于自动刷新日志缓冲区
    m_flushTimer = new QTimer(this);
    connect(m_flushTimer, &QTimer::timeout, this, &Logger::onTimerTimeout);
    m_flushTimer->start(5000); // 每5秒自动刷新一次

    // 创建定时器用于性能监控
    m_performanceTimer = new QTimer(this);
    connect(m_performanceTimer, &QTimer::timeout, this, &Logger::onPerformanceTimerTimeout);
    // 注意：性能监控需要手动启动，不在构造函数中自动开始
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

void Logger::exitEvent(const QString &msg)
{
    logEvent("退出事件", msg, "exit.log", L_INFO);
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
    
    if (m_performanceTimer) {
        m_performanceTimer->stop();
        m_performanceTimer = nullptr;
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

// ===== 性能监控功能实现 =====

void Logger::performanceEvent(const PerformanceMetrics &metrics)
{
    QString message = QString(
        "CPU系统: %1% | CPU进程: %2% | "
        "内存总量: %3MB | 内存已用: %4MB | 进程内存: %5MB | "
        "磁盘读: %6KB | 磁盘写: %7KB | "
        "网络接收: %8KB | 网络发送: %9KB | "
        "句柄数: %10 | 线程数: %11"
    ).arg(metrics.cpuUsageSystem, 0, 'f', 1)
     .arg(metrics.cpuUsageProcess, 0, 'f', 1)
     .arg(metrics.memoryPhysicalTotal)
     .arg(metrics.memoryPhysicalUsed)
     .arg(metrics.memoryProcessUsed)
     .arg(metrics.diskReadBytes / 1024)
     .arg(metrics.diskWriteBytes / 1024)
     .arg(metrics.networkRecvBytes / 1024)
     .arg(metrics.networkSentBytes / 1024)
     .arg(metrics.processHandles)
     .arg(metrics.processThreads);

    logEvent("性能监控", message, "performance.log", L_INFO);
}

void Logger::startPerformanceMonitoring()
{
    if (m_performanceTimer && !m_performanceTimer->isActive()) {
        appEvent("启动性能监控，间隔30秒");
        m_performanceTimer->start(30000); // 30秒间隔
    }
}

void Logger::stopPerformanceMonitoring()
{
    if (m_performanceTimer && m_performanceTimer->isActive()) {
        m_performanceTimer->stop();
        appEvent("停止性能监控");
    }
}

void Logger::onPerformanceTimerTimeout()
{
    PerformanceMetrics metrics = collectPerformanceMetrics();
    performanceEvent(metrics);
}

PerformanceMetrics Logger::collectPerformanceMetrics()
{
    PerformanceMetrics metrics;
    metrics.timestamp = QDateTime::currentDateTime();

    // 初始化所有字段为默认值
    metrics.cpuUsageSystem = 0.0;
    metrics.cpuUsageProcess = 0.0;
    metrics.memoryPhysicalTotal = 0;
    metrics.memoryPhysicalUsed = 0;
    metrics.memoryProcessUsed = 0;
    metrics.diskReadBytes = 0;
    metrics.diskWriteBytes = 0;
    metrics.networkRecvBytes = 0;
    metrics.networkSentBytes = 0;
    metrics.processHandles = 0;
    metrics.processThreads = 0;

#ifdef Q_OS_WIN
    return collectWindowsPerformanceMetrics();
#elif defined(Q_OS_MAC)
    return collectMacOSPerformanceMetrics();
#elif defined(Q_OS_LINUX)
    return collectLinuxPerformanceMetrics();
#else
    errorEvent("不支持的平台，无法收集性能指标");
    return metrics;
#endif
}

#ifdef Q_OS_WIN
PerformanceMetrics Logger::collectWindowsPerformanceMetrics()
{
    PerformanceMetrics metrics;
    metrics.timestamp = QDateTime::currentDateTime();

    // 获取进程句柄
    HANDLE currentProcess = GetCurrentProcess();
    
    // 获取内存信息
    MEMORYSTATUSEX memStatus;
    memStatus.dwLength = sizeof(memStatus);
    if (GlobalMemoryStatusEx(&memStatus)) {
        metrics.memoryPhysicalTotal = memStatus.ullTotalPhys / (1024 * 1024); // 转换为MB
        metrics.memoryPhysicalUsed = (memStatus.ullTotalPhys - memStatus.ullAvailPhys) / (1024 * 1024);
    }

    // 获取进程内存信息
    PROCESS_MEMORY_COUNTERS pmc;
    if (GetProcessMemoryInfo(currentProcess, &pmc, sizeof(pmc))) {
        metrics.memoryProcessUsed = pmc.WorkingSetSize / (1024 * 1024); // 转换为MB
    }

    // 获取CPU使用率（简化版本，使用GetProcessTimes）
    FILETIME idleTime, kernelTime, userTime;
    FILETIME processCreationTime, processExitTime, processKernelTime, processUserTime;
    
    if (GetSystemTimes(&idleTime, &kernelTime, &userTime)) {
        // 计算系统CPU使用率的逻辑在这里会比较复杂，简化处理
        // 实际实现中需要保存上次的时间并计算差值
        static ULONGLONG lastKernelTime = 0, lastUserTime = 0, lastIdleTime = 0;
        
        ULONGLONG currentKernelTime = ((ULONGLONG)kernelTime.dwHighDateTime << 32) | kernelTime.dwLowDateTime;
        ULONGLONG currentUserTime = ((ULONGLONG)userTime.dwHighDateTime << 32) | userTime.dwLowDateTime;
        ULONGLONG currentIdleTime = ((ULONGLONG)idleTime.dwHighDateTime << 32) | idleTime.dwLowDateTime;
        
        if (lastKernelTime != 0) {
            ULONGLONG kernelDiff = currentKernelTime - lastKernelTime;
            ULONGLONG userDiff = currentUserTime - lastUserTime;
            ULONGLONG idleDiff = currentIdleTime - lastIdleTime;
            ULONGLONG totalDiff = kernelDiff + userDiff;
            
            if (totalDiff > 0) {
                metrics.cpuUsageSystem = ((double)(totalDiff - idleDiff) / totalDiff) * 100.0;
            }
        }
        
        lastKernelTime = currentKernelTime;
        lastUserTime = currentUserTime;
        lastIdleTime = currentIdleTime;
    }

    // 获取进程CPU时间
    if (GetProcessTimes(currentProcess, &processCreationTime, &processExitTime, &processKernelTime, &processUserTime)) {
        // 简化的进程CPU计算，实际需要计算时间差
        static ULONGLONG lastProcessKernelTime = 0, lastProcessUserTime = 0;
        
        ULONGLONG currentProcessKernelTime = ((ULONGLONG)processKernelTime.dwHighDateTime << 32) | processKernelTime.dwLowDateTime;
        ULONGLONG currentProcessUserTime = ((ULONGLONG)processUserTime.dwHighDateTime << 32) | processUserTime.dwLowDateTime;
        
        if (lastProcessKernelTime != 0) {
            // 这里需要更复杂的计算来得到实际的CPU使用率百分比
            // 为了简化，设置一个估算值
            metrics.cpuUsageProcess = 0.1; // 临时值
        }
        
        lastProcessKernelTime = currentProcessKernelTime;
        lastProcessUserTime = currentProcessUserTime;
    }

    // 获取磁盘I/O信息（简化版本）
    IO_COUNTERS ioCounters;
    if (GetProcessIoCounters(currentProcess, &ioCounters)) {
        metrics.diskReadBytes = ioCounters.ReadTransferCount;
        metrics.diskWriteBytes = ioCounters.WriteTransferCount;
    }

    // 获取进程信息
    DWORD processId = GetCurrentProcessId();
    
    // 获取句柄数
    DWORD handleCount = 0;
    if (GetProcessHandleCount(currentProcess, &handleCount)) {
        metrics.processHandles = static_cast<int>(handleCount);
    }

    // 获取线程数（通过CreateToolhelp32Snapshot，这里简化）
    metrics.processThreads = 1; // 简化实现

    // 网络统计信息（简化，实际需要使用GetIfTable2或WMI）
    metrics.networkRecvBytes = 0;
    metrics.networkSentBytes = 0;

    return metrics;
}
#endif

#ifdef Q_OS_MAC
PerformanceMetrics Logger::collectMacOSPerformanceMetrics()
{
    PerformanceMetrics metrics;
    metrics.timestamp = QDateTime::currentDateTime();

    // 获取系统内存信息
    vm_size_t page_size;
    vm_statistics64_data_t vm_stat;
    mach_msg_type_number_t host_size = sizeof(vm_statistics64_data_t) / sizeof(natural_t);
    
    host_page_size(mach_host_self(), &page_size);
    if (host_statistics64(mach_host_self(), HOST_VM_INFO64, (host_info64_t)&vm_stat, &host_size) == KERN_SUCCESS) {
        uint64_t total_memory = (vm_stat.free_count + vm_stat.active_count + vm_stat.inactive_count + vm_stat.wire_count) * page_size;
        uint64_t used_memory = (vm_stat.active_count + vm_stat.inactive_count + vm_stat.wire_count) * page_size;
        
        metrics.memoryPhysicalTotal = total_memory / (1024 * 1024); // 转换为MB
        metrics.memoryPhysicalUsed = used_memory / (1024 * 1024);
    }

    // 获取当前进程的内存信息
    task_t current_task = mach_task_self();
    mach_task_basic_info_data_t task_basic_info;
    mach_msg_type_number_t task_info_count = MACH_TASK_BASIC_INFO_COUNT;
    
    if (task_info(current_task, MACH_TASK_BASIC_INFO, (task_info_t)&task_basic_info, &task_info_count) == KERN_SUCCESS) {
        metrics.memoryProcessUsed = task_basic_info.resident_size / (1024 * 1024); // 转换为MB
    }

    // 获取CPU信息
    host_cpu_load_info_data_t cpu_load;
    mach_msg_type_number_t cpu_load_count = HOST_CPU_LOAD_INFO_COUNT;
    
    if (host_statistics(mach_host_self(), HOST_CPU_LOAD_INFO, (host_info_t)&cpu_load, &cpu_load_count) == KERN_SUCCESS) {
        static natural_t last_user = 0, last_system = 0, last_idle = 0, last_nice = 0;
        
        natural_t user_diff = cpu_load.cpu_ticks[CPU_STATE_USER] - last_user;
        natural_t system_diff = cpu_load.cpu_ticks[CPU_STATE_SYSTEM] - last_system;
        natural_t idle_diff = cpu_load.cpu_ticks[CPU_STATE_IDLE] - last_idle;
        natural_t nice_diff = cpu_load.cpu_ticks[CPU_STATE_NICE] - last_nice;
        
        natural_t total_diff = user_diff + system_diff + idle_diff + nice_diff;
        
        if (total_diff > 0 && last_user != 0) {
            metrics.cpuUsageSystem = ((double)(user_diff + system_diff + nice_diff) / total_diff) * 100.0;
        }
        
        last_user = cpu_load.cpu_ticks[CPU_STATE_USER];
        last_system = cpu_load.cpu_ticks[CPU_STATE_SYSTEM];
        last_idle = cpu_load.cpu_ticks[CPU_STATE_IDLE];
        last_nice = cpu_load.cpu_ticks[CPU_STATE_NICE];
    }

    // 获取进程资源使用情况
    struct rusage usage;
    if (getrusage(RUSAGE_SELF, &usage) == 0) {
        // 计算进程CPU使用率（简化）
        static long last_user_time = 0, last_sys_time = 0;
        
        long current_user_time = usage.ru_utime.tv_sec * 1000000 + usage.ru_utime.tv_usec;
        long current_sys_time = usage.ru_stime.tv_sec * 1000000 + usage.ru_stime.tv_usec;
        
        if (last_user_time != 0) {
            // 简化的进程CPU计算
            metrics.cpuUsageProcess = 0.1; // 临时值，实际需要更复杂的计算
        }
        
        last_user_time = current_user_time;
        last_sys_time = current_sys_time;
    }

    // 磁盘和网络I/O信息（macOS需要使用更复杂的API，这里简化）
    metrics.diskReadBytes = 0;
    metrics.diskWriteBytes = 0;
    metrics.networkRecvBytes = 0;
    metrics.networkSentBytes = 0;

    // 进程句柄和线程数（简化）
    metrics.processHandles = 0; // macOS没有Windows意义上的句柄概念
    metrics.processThreads = 1; // 简化实现

    return metrics;
}
#endif

#ifdef Q_OS_LINUX
PerformanceMetrics Logger::collectLinuxPerformanceMetrics()
{
    PerformanceMetrics metrics;
    metrics.timestamp = QDateTime::currentDateTime();

    // 读取系统内存信息 /proc/meminfo
    QFile memInfoFile("/proc/meminfo");
    if (memInfoFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream stream(&memInfoFile);
        QString line;
        
        while (!stream.atEnd()) {
            line = stream.readLine();
            if (line.startsWith("MemTotal:")) {
                metrics.memoryPhysicalTotal = line.split(QRegExp("\\s+"))[1].toULongLong() / 1024; // 转换为MB
            } else if (line.startsWith("MemAvailable:")) {
                quint64 available = line.split(QRegExp("\\s+"))[1].toULongLong() / 1024;
                metrics.memoryPhysicalUsed = metrics.memoryPhysicalTotal - available;
            }
        }
        memInfoFile.close();
    }

    // 读取进程内存信息 /proc/self/status
    QFile statusFile("/proc/self/status");
    if (statusFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream stream(&statusFile);
        QString line;
        
        while (!stream.atEnd()) {
            line = stream.readLine();
            if (line.startsWith("VmRSS:")) {
                metrics.memoryProcessUsed = line.split(QRegExp("\\s+"))[1].toULongLong() / 1024; // 转换为MB
            } else if (line.startsWith("Threads:")) {
                metrics.processThreads = line.split(QRegExp("\\s+"))[1].toInt();
            }
        }
        statusFile.close();
    }

    // 读取系统CPU信息 /proc/stat
    QFile statFile("/proc/stat");
    if (statFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream stream(&statFile);
        QString line = stream.readLine(); // 第一行是总CPU统计
        
        if (line.startsWith("cpu ")) {
            QStringList values = line.split(QRegExp("\\s+"));
            if (values.size() >= 8) {
                static quint64 lastUser = 0, lastNice = 0, lastSystem = 0, lastIdle = 0;
                static quint64 lastIowait = 0, lastIrq = 0, lastSoftirq = 0;
                
                quint64 user = values[1].toULongLong();
                quint64 nice = values[2].toULongLong();
                quint64 system = values[3].toULongLong();
                quint64 idle = values[4].toULongLong();
                quint64 iowait = values[5].toULongLong();
                quint64 irq = values[6].toULongLong();
                quint64 softirq = values[7].toULongLong();
                
                if (lastUser != 0) {
                    quint64 userDiff = user - lastUser;
                    quint64 niceDiff = nice - lastNice;
                    quint64 systemDiff = system - lastSystem;
                    quint64 idleDiff = idle - lastIdle;
                    quint64 iowaitDiff = iowait - lastIowait;
                    quint64 irqDiff = irq - lastIrq;
                    quint64 softirqDiff = softirq - lastSoftirq;
                    
                    quint64 totalDiff = userDiff + niceDiff + systemDiff + idleDiff + iowaitDiff + irqDiff + softirqDiff;
                    quint64 activeDiff = userDiff + niceDiff + systemDiff + irqDiff + softirqDiff;
                    
                    if (totalDiff > 0) {
                        metrics.cpuUsageSystem = ((double)activeDiff / totalDiff) * 100.0;
                    }
                }
                
                lastUser = user; lastNice = nice; lastSystem = system; lastIdle = idle;
                lastIowait = iowait; lastIrq = irq; lastSoftirq = softirq;
            }
        }
        statFile.close();
    }

    // 读取进程CPU信息 /proc/self/stat
    QFile procStatFile("/proc/self/stat");
    if (procStatFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream stream(&procStatFile);
        QString line = stream.readLine();
        QStringList values = line.split(" ");
        
        if (values.size() >= 22) {
            static quint64 lastProcessUtime = 0, lastProcessStime = 0;
            
            quint64 utime = values[13].toULongLong(); // 用户态时间
            quint64 stime = values[14].toULongLong(); // 内核态时间
            
            if (lastProcessUtime != 0) {
                // 简化的进程CPU计算
                metrics.cpuUsageProcess = 0.1; // 临时值，实际需要更复杂的计算
            }
            
            lastProcessUtime = utime;
            lastProcessStime = stime;
        }
        procStatFile.close();
    }

    // 读取进程I/O信息 /proc/self/io
    QFile ioFile("/proc/self/io");
    if (ioFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream stream(&ioFile);
        QString line;
        
        while (!stream.atEnd()) {
            line = stream.readLine();
            if (line.startsWith("read_bytes:")) {
                metrics.diskReadBytes = line.split(":")[1].trimmed().toULongLong();
            } else if (line.startsWith("write_bytes:")) {
                metrics.diskWriteBytes = line.split(":")[1].trimmed().toULongLong();
            }
        }
        ioFile.close();
    }

    // 读取网络信息 /proc/net/dev
    QFile netFile("/proc/net/dev");
    if (netFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream stream(&netFile);
        stream.readLine(); // 跳过标题行
        stream.readLine(); // 跳过第二行
        
        quint64 totalRecv = 0, totalSent = 0;
        QString line;
        
        while (!stream.atEnd()) {
            line = stream.readLine();
            if (!line.contains("lo:")) { // 跳过本地回环接口
                QStringList parts = line.split(QRegExp("\\s+"));
                if (parts.size() >= 10) {
                    totalRecv += parts[1].toULongLong(); // 接收字节数
                    totalSent += parts[9].toULongLong(); // 发送字节数
                }
            }
        }
        
        metrics.networkRecvBytes = totalRecv;
        metrics.networkSentBytes = totalSent;
        netFile.close();
    }

    // 进程句柄数（在Linux中类似于文件描述符数）
    QDir fdDir("/proc/self/fd");
    metrics.processHandles = fdDir.count() - 2; // 减去 . 和 ..

    return metrics;
}
#endif