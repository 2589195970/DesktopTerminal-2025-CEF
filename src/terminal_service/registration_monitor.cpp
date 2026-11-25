#include "registration_monitor.h"
#include "seat_display_store.h"

#include <QBuffer>
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QHostAddress>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QSysInfo>
#include <QUuid>

#include <algorithm>
#include <limits>

#ifdef Q_OS_WIN
#include "win_handle.h"
#endif

namespace terminal {
namespace {

constexpr int SchemaVersion = 1;
constexpr int MaxFileCount = 5;
constexpr qint64 MaxFileSize = 5 * 1024 * 1024;
constexpr int MaxEventSize = 1024;
constexpr int MaxStatusSize = 8192;
constexpr int RetentionDays = 7;
constexpr qint64 RetentionMs = RetentionDays * 24 * 60 * 60 * 1000LL;
constexpr qint64 CleanupInterval = 60 * 60 * 1000;
constexpr qint64 MaxStatusAge = 45 * 1000;
constexpr qint64 MaxHeartbeatAge = 90 * 1000;

QString statusPath(const QString &directory)
{
    return QDir(directory).filePath(QStringLiteral("status.json"));
}

QString eventLogPath(const QString &directory)
{
    return QDir(directory).filePath(QStringLiteral("registration.log"));
}

QString serviceLogPath(const QString &directory)
{
    return QDir(directory).filePath(QStringLiteral("logs/terminal-service.log"));
}

QString archivePath(const QString &path, int index)
{
    return index == 0 ? path : path + QStringLiteral(".%1").arg(index);
}

QByteArray readFile(const QString &path, int limit)
{
#ifdef Q_OS_WIN
    const std::wstring nativePath = QDir::toNativeSeparators(path).toStdWString();
    const WinHandle handle(CreateFileW(nativePath.c_str(), GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
        FILE_FLAG_OPEN_REPARSE_POINT, nullptr));
    BY_HANDLE_FILE_INFORMATION info{};
    if (!handle || !GetFileInformationByHandle(handle.get(), &info)
            || (info.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT))
            || info.nFileSizeHigh != 0 || info.nFileSizeLow > static_cast<DWORD>(limit)) {
        return {};
    }
    QByteArray bytes(static_cast<int>(info.nFileSizeLow), Qt::Uninitialized);
    DWORD count = 0;
    if (!bytes.isEmpty() && !ReadFile(handle.get(), bytes.data(), info.nFileSizeLow, &count, nullptr)) {
        return {};
    }
    bytes.truncate(static_cast<int>(count));
    return bytes;
#else
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly) || file.size() > limit) {
        return {};
    }
    return file.read(limit);
#endif
}

void pruneLogs(const QString &path, const QDateTime &now)
{
    const auto earliest = now.addDays(-RetentionDays);
    for (int index = 0; index < MaxFileCount; ++index) {
        const QFileInfo file(archivePath(path, index));
        if (file.isFile() && !file.isSymLink()
                && (file.lastModified() < earliest || file.size() > MaxFileSize)) {
            QFile::remove(file.filePath());
        }
    }
}

bool appendLog(const QString &path, const QByteArray &line)
{
    const auto now = QDateTime::currentDateTimeUtc();
    const QFileInfo active(path);
    if (active.isSymLink() || (active.exists() && !active.isFile()) || line.size() > MaxFileSize) {
        return false;
    }
    if (active.exists() && (active.lastModified().toUTC().date() != now.date()
                            || active.size() + line.size() > MaxFileSize)) {
        pruneLogs(path, now);
        const QString oldest = archivePath(path, MaxFileCount - 1);
        if (QFile::exists(oldest) && !QFile::remove(oldest)) {
            return false;
        }
        for (int index = MaxFileCount - 2; index >= 0; --index) {
            const QString source = archivePath(path, index);
            if (QFile::exists(source) && !QFile::rename(source, archivePath(path, index + 1))) {
                return false;
            }
        }
    }
    QFile file(path);
    return file.open(QIODevice::WriteOnly | QIODevice::Append) && file.write(line) == line.size() && file.flush();
}

qint64 readTime(const QJsonValue &value)
{
    const double number = value.toDouble();
    if (number <= 0 || number > 9007199254740991.0) {
        return 0;
    }
    const auto integer = static_cast<qint64>(number);
    return static_cast<double>(integer) == number ? integer : 0;
}

QJsonObject eventJson(const RegistrationLogEntry &entry)
{
    return {{QStringLiteral("id"), entry.id}, {QStringLiteral("event"), static_cast<int>(entry.event)},
            {QStringLiteral("problem"), static_cast<int>(entry.problem)}, {QStringLiteral("http"), entry.httpStatus},
            {QStringLiteral("firstAt"), entry.firstAt}, {QStringLiteral("lastAt"), entry.lastAt},
            {QStringLiteral("count"), entry.count}};
}

std::optional<RegistrationLogEntry> parseEvent(const QJsonObject &json)
{
    RegistrationLogEntry entry;
    entry.id = json.value(QStringLiteral("id")).toString();
    const int event = json.value(QStringLiteral("event")).toInt(-1);
    const int problem = json.value(QStringLiteral("problem")).toInt(-1);
    entry.httpStatus = json.value(QStringLiteral("http")).toInt();
    entry.firstAt = readTime(json.value(QStringLiteral("firstAt")));
    entry.lastAt = readTime(json.value(QStringLiteral("lastAt")));
    entry.count = json.value(QStringLiteral("count")).toInt();
    if (QUuid(entry.id).isNull() || event < 0 || event > static_cast<int>(RegistrationEvent::HeartbeatRecovered)
            || problem < 0 || problem > static_cast<int>(RegistrationProblem::Response)
            || entry.firstAt == 0 || entry.lastAt < entry.firstAt || entry.count < 1
            || entry.httpStatus < 0 || entry.httpStatus > 599) {
        return std::nullopt;
    }
    entry.event = static_cast<RegistrationEvent>(event);
    entry.problem = static_cast<RegistrationProblem>(problem);
    return entry;
}

RegistrationLogEntry makeEvent(RegistrationEvent event, RegistrationProblem problem, int httpStatus, qint64 now)
{
    return {QUuid::createUuid().toString(QUuid::WithoutBraces), event, problem, httpStatus, now, now, 1};
}

} // namespace

bool RegistrationLogEntry::isFailure() const
{
    return event == RegistrationEvent::StartupFailed || event == RegistrationEvent::RegistrationFailed
            || event == RegistrationEvent::HeartbeatFailed || event == RegistrationEvent::CodeInvalid;
}

bool RegistrationStatus::isCurrent(qint64 servicePid, qint64 now) const
{
    return pid > 0 && pid == servicePid && updatedAt > 0 && now >= updatedAt && now - updatedAt <= MaxStatusAge;
}

bool RegistrationStatus::heartbeatIsCurrent(qint64 now) const
{
    return lastHeartbeat > 0 && now >= lastHeartbeat && now - lastHeartbeat <= MaxHeartbeatAge;
}

RegistrationMonitor::RegistrationMonitor(const QString &directory)
    : m_directory(directory)
{
    m_status.computerName = QSysInfo::machineHostName();
    m_status.version = QCoreApplication::applicationVersion();
    m_status.pid = QCoreApplication::applicationPid();
}

void RegistrationMonitor::start()
{
    if (m_directory.isEmpty()) {
        return;
    }
    const QString path = eventLogPath(m_directory);
    const auto lastLog = readFile(path, MaxFileSize);
    if (!lastLog.isEmpty() && !lastLog.endsWith('\n')) {
        appendLog(path, "\n");
    }
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (const auto last = readStatus(m_directory); last && last->recentFailure
            && last->recentFailure->lastAt >= now - RetentionMs
            && (last->phase == RegistrationPhase::RegistrationFailed || last->phase == RegistrationPhase::HeartbeatFailed)) {
        append(*last->recentFailure);
    }
    record(RegistrationEvent::Started);
    publish();
}

void RegistrationMonitor::setSchoolInstance(int instance)
{
    m_status.schoolInstance = instance;
}

void RegistrationMonitor::setLocalIp(const QString &ip)
{
    m_status.localIp = !QHostAddress(ip).isNull() ? ip : QString();
}

void RegistrationMonitor::beginRequest(bool registering)
{
    m_status.nextAttempt = 0;
    if (m_status.phase == RegistrationPhase::Starting) {
        m_status.phase = registering ? RegistrationPhase::Registering : RegistrationPhase::WaitingForHeartbeat;
        publish();
    } else if (m_failureActive) {
        publish();
    }
}

void RegistrationMonitor::registered(bool bound)
{
    endFailure(true);
    m_status.lastRegistration = QDateTime::currentMSecsSinceEpoch();
    m_status.phase = bound ? RegistrationPhase::WaitingForHeartbeat : RegistrationPhase::WaitingForBinding;
    if (bound || !m_waitingLogged) {
        record(bound ? RegistrationEvent::Bound : RegistrationEvent::WaitingForBinding);
    }
    m_waitingLogged = !bound;
}

void RegistrationMonitor::heartbeatReceived(const std::optional<QString> &seat)
{
    endFailure(true);
    m_status.lastHeartbeat = QDateTime::currentMSecsSinceEpoch();
    m_status.phase = RegistrationPhase::Online;
    if (seat) {
        m_status.seat = SeatDisplayStore::isValidNumber(*seat) ? *seat : QString();
    }
    if (!m_heartbeatConfirmed) {
        record(RegistrationEvent::FirstHeartbeat);
        m_heartbeatConfirmed = true;
    }
}

void RegistrationMonitor::codeInvalid()
{
    endFailure(false);
    m_heartbeatConfirmed = false;
    m_waitingLogged = false;
    m_status.seat.clear();
    m_status.nextAttempt = 0;
    m_status.phase = RegistrationPhase::Registering;
    record(RegistrationEvent::CodeInvalid);
}

bool RegistrationMonitor::failed(bool registering, RegistrationProblem problem, int httpStatus)
{
    m_status.phase = registering ? RegistrationPhase::RegistrationFailed : RegistrationPhase::HeartbeatFailed;
    const auto type = registering ? RegistrationEvent::RegistrationFailed : RegistrationEvent::HeartbeatFailed;
    const auto now = QDateTime::currentMSecsSinceEpoch();
    if (m_failureActive && m_status.recentFailure->event == type && m_status.recentFailure->problem == problem
            && m_status.recentFailure->httpStatus == httpStatus) {
        auto &failure = *m_status.recentFailure;
        if (failure.count < std::numeric_limits<int>::max()) {
            ++failure.count;
        }
        failure.lastAt = std::max(now, failure.lastAt);
        return false;
    }
    endFailure(false);
    m_status.recentFailure = makeEvent(type, problem, httpStatus, now);
    m_failureActive = true;
    append(*m_status.recentFailure);
    return true;
}

void RegistrationMonitor::startupFailed(RegistrationProblem problem)
{
    m_status.phase = RegistrationPhase::StartupFailed;
    m_status.recentFailure = makeEvent(RegistrationEvent::StartupFailed, problem, 0, QDateTime::currentMSecsSinceEpoch());
    append(*m_status.recentFailure);
    publish();
}

void RegistrationMonitor::stop()
{
    endFailure(false);
    m_status.phase = RegistrationPhase::Stopped;
    m_status.nextAttempt = 0;
    record(RegistrationEvent::Stopped);
    publish();
}

void RegistrationMonitor::setNextAttempt(int delayMs)
{
    m_status.nextAttempt = QDateTime::currentMSecsSinceEpoch() + delayMs;
    publish();
}

void RegistrationMonitor::publish()
{
    if (m_directory.isEmpty()) {
        return;
    }
    m_status.updatedAt = QDateTime::currentMSecsSinceEpoch();
    if (m_status.updatedAt >= m_nextCleanup) {
        const auto now = QDateTime::fromMSecsSinceEpoch(m_status.updatedAt, Qt::UTC);
        pruneLogs(eventLogPath(m_directory), now);
        m_nextCleanup = m_status.updatedAt + CleanupInterval;
    }
    if (!m_failureActive && m_status.recentFailure && m_status.recentFailure->lastAt < m_status.updatedAt - RetentionMs) {
        m_status.recentFailure.reset();
    }
    QJsonObject json{{QStringLiteral("schema"), SchemaVersion}, {QStringLiteral("pid"), m_status.pid},
        {QStringLiteral("updatedAt"), m_status.updatedAt}, {QStringLiteral("phase"), static_cast<int>(m_status.phase)},
        {QStringLiteral("computer"), m_status.computerName}, {QStringLiteral("ip"), m_status.localIp},
        {QStringLiteral("instance"), m_status.schoolInstance}, {QStringLiteral("version"), m_status.version},
        {QStringLiteral("seat"), m_status.seat}, {QStringLiteral("lastRegistration"), m_status.lastRegistration},
        {QStringLiteral("lastHeartbeat"), m_status.lastHeartbeat}, {QStringLiteral("nextAttempt"), m_status.nextAttempt}};
    if (m_status.recentFailure) {
        json.insert(QStringLiteral("recentFailure"), eventJson(*m_status.recentFailure));
    }
    QSaveFile file(statusPath(m_directory));
    const auto bytes = QJsonDocument(json).toJson(QJsonDocument::Compact);
    if (file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size()) {
        file.commit();
    }
}

std::optional<RegistrationStatus> RegistrationMonitor::readStatus(const QString &directory)
{
    if (directory.isEmpty()) {
        return std::nullopt;
    }
    const QByteArray bytes = readFile(statusPath(directory), MaxStatusSize);
    const QJsonObject json = QJsonDocument::fromJson(bytes).object();
    const int phase = json.value(QStringLiteral("phase")).toInt(-1);
    RegistrationStatus status;
    status.pid = readTime(json.value(QStringLiteral("pid")));
    status.updatedAt = readTime(json.value(QStringLiteral("updatedAt")));
    if (json.value(QStringLiteral("schema")).toInt() != SchemaVersion
            || phase < 0 || phase > static_cast<int>(RegistrationPhase::StartupFailed)
            || status.pid <= 0 || status.updatedAt <= 0) {
        return std::nullopt;
    }
    status.phase = static_cast<RegistrationPhase>(phase);
    status.computerName = json.value(QStringLiteral("computer")).toString().left(255);
    const QString ip = json.value(QStringLiteral("ip")).toString();
    status.localIp = QHostAddress(ip).isNull() ? QString() : ip;
    status.schoolInstance = json.value(QStringLiteral("instance")).toInt();
    status.version = json.value(QStringLiteral("version")).toString().left(32);
    const QString seat = json.value(QStringLiteral("seat")).toString();
    if (SeatDisplayStore::isValidNumber(seat)) {
        status.seat = seat;
    }
    status.lastRegistration = readTime(json.value(QStringLiteral("lastRegistration")));
    status.lastHeartbeat = readTime(json.value(QStringLiteral("lastHeartbeat")));
    status.nextAttempt = readTime(json.value(QStringLiteral("nextAttempt")));
    status.recentFailure = parseEvent(json.value(QStringLiteral("recentFailure")).toObject());
    return status;
}

QList<RegistrationLogEntry> RegistrationMonitor::readLogs(const QString &directory, qint64 since)
{
    if (directory.isEmpty()) {
        return {};
    }
    since = std::max(since, QDateTime::currentMSecsSinceEpoch() - RetentionMs);
    QHash<QString, RegistrationLogEntry> entries;
    const auto add = [&entries, since](const RegistrationLogEntry &entry) {
        auto previous = entries.find(entry.id);
        if (entry.lastAt >= since && (previous == entries.end() || previous->count < entry.count)) {
            entries.insert(entry.id, entry);
        }
    };
    for (int index = 0; index < MaxFileCount; ++index) {
        auto bytes = readFile(archivePath(eventLogPath(directory), index), MaxFileSize);
        if (bytes.isEmpty()) {
            continue;
        }
        QBuffer file(&bytes);
        file.open(QIODevice::ReadOnly);
        while (!file.atEnd()) {
            const auto line = file.readLine(MaxEventSize + 1);
            if (line.isEmpty()) {
                break;
            }
            if (!line.endsWith('\n')) {
                // 只有完整记录才能进入导出。跳过截断写入和超长行。
                while (!file.atEnd()) {
                    const auto tail = file.readLine(MaxEventSize + 1);
                    if (tail.isEmpty() || tail.endsWith('\n')) {
                        break;
                    }
                }
                continue;
            }
            if (const auto event = parseEvent(QJsonDocument::fromJson(line).object())) {
                add(*event);
            }
        }
    }
    if (const auto snapshot = readStatus(directory); snapshot && snapshot->recentFailure) {
        add(*snapshot->recentFailure);
    }
    auto result = entries.values();
    std::sort(result.begin(), result.end(), [](const auto &a, const auto &b) {
        if (a.lastAt != b.lastAt) {
            return a.lastAt > b.lastAt;
        }
        if (a.firstAt != b.firstAt) {
            return a.firstAt > b.firstAt;
        }
        return a.event > b.event;
    });
    return result;
}

void RegistrationMonitor::pruneServiceLog(const QString &directory)
{
    pruneLogs(serviceLogPath(directory), QDateTime::currentDateTimeUtc());
}

bool RegistrationMonitor::appendServiceLog(const QString &directory, const QByteArray &line)
{
    return appendLog(serviceLogPath(directory), line);
}

void RegistrationMonitor::record(RegistrationEvent event, RegistrationProblem problem)
{
    if (!m_directory.isEmpty()) {
        append(makeEvent(event, problem, 0, QDateTime::currentMSecsSinceEpoch()));
    }
}

void RegistrationMonitor::endFailure(bool recovered)
{
    if (!m_failureActive) {
        return;
    }
    if (m_status.recentFailure->count > 1) {
        append(*m_status.recentFailure);
    }
    if (recovered && !m_directory.isEmpty()) {
        const auto recovery = m_status.recentFailure->event == RegistrationEvent::RegistrationFailed
                ? RegistrationEvent::RegistrationRecovered : RegistrationEvent::HeartbeatRecovered;
        auto event = makeEvent(recovery, m_status.recentFailure->problem,
                               m_status.recentFailure->httpStatus, QDateTime::currentMSecsSinceEpoch());
        event.firstAt = std::min(event.lastAt, m_status.recentFailure->firstAt);
        event.count = m_status.recentFailure->count;
        append(event);
    }
    m_failureActive = false;
}

bool RegistrationMonitor::append(const RegistrationLogEntry &event) const
{
    return !m_directory.isEmpty()
            && appendLog(eventLogPath(m_directory), QJsonDocument(eventJson(event)).toJson(QJsonDocument::Compact) + '\n');
}

QString registrationProblemText(RegistrationProblem problem, int httpStatus)
{
    switch (problem) {
    case RegistrationProblem::Configuration: return QStringLiteral("服务配置无法读取或无效");
    case RegistrationProblem::State: return QStringLiteral("本机注册状态无法读取或保存");
    case RegistrationProblem::Dns: return QStringLiteral("无法解析学校服务器地址");
    case RegistrationProblem::Timeout: return QStringLiteral("连接学校服务器超时");
    case RegistrationProblem::Tls: return QStringLiteral("与学校服务器的 HTTPS 握手失败");
    case RegistrationProblem::Connection: return QStringLiteral("无法连接学校服务器");
    case RegistrationProblem::Http: return QStringLiteral("学校服务器返回 HTTP %1").arg(httpStatus);
    case RegistrationProblem::Response: return QStringLiteral("学校服务器拒绝请求或响应格式异常");
    case RegistrationProblem::None: return {};
    }
    return {};
}

QString registrationEventText(RegistrationEvent event)
{
    switch (event) {
    case RegistrationEvent::Started: return QStringLiteral("服务启动");
    case RegistrationEvent::Stopped: return QStringLiteral("服务已停止");
    case RegistrationEvent::StartupFailed: return QStringLiteral("服务启动失败");
    case RegistrationEvent::WaitingForBinding: return QStringLiteral("待绑定");
    case RegistrationEvent::Bound: return QStringLiteral("绑定成功");
    case RegistrationEvent::FirstHeartbeat: return QStringLiteral("首次心跳成功");
    case RegistrationEvent::CodeInvalid: return QStringLiteral("绑定已失效");
    case RegistrationEvent::RegistrationFailed: return QStringLiteral("注册失败");
    case RegistrationEvent::HeartbeatFailed: return QStringLiteral("心跳失败");
    case RegistrationEvent::RegistrationRecovered: return QStringLiteral("注册恢复");
    case RegistrationEvent::HeartbeatRecovered: return QStringLiteral("心跳恢复");
    }
    return {};
}

} // namespace terminal
