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