#include "service_runtime.h"
#include "seat_display_store.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QHostInfo>
#include <QJsonDocument>
#include <QMetaEnum>
#include <QRandomGenerator>
#include <QSysInfo>

#include <algorithm>
#include <utility>

namespace terminal {
namespace {

constexpr int RegisterIntervalMinMs = 20 * 1000;
constexpr int RegisterJitterMs = 5 * 1000;
constexpr int RegisterRetryMinMs = 30 * 1000;
constexpr int RegisterRetryMaxMs = 60 * 1000;
constexpr int HeartbeatIntervalMs = 30 * 1000;
constexpr int RequestTimeoutMs = 10 * 1000;
constexpr int StatusRefreshMs = 15 * 1000;
constexpr int ServiceLogCleanupMs = 60 * 60 * 1000;

RegistrationProblem networkProblem(QNetworkReply::NetworkError error, int status)
{
    if (status >= 100 && status <= 599 && status != 200) {
        return RegistrationProblem::Http;
    }
    switch (error) {
    case QNetworkReply::NoError: return RegistrationProblem::None;
    case QNetworkReply::HostNotFoundError: return RegistrationProblem::Dns;
    case QNetworkReply::TimeoutError: return RegistrationProblem::Timeout;
    case QNetworkReply::SslHandshakeFailedError: return RegistrationProblem::Tls;
    default: return RegistrationProblem::Connection;
    }
}

} // namespace

void writeServiceLog(const QString &directory, const QString &message)
{
    QString line = message.left(2048);
    line.replace('\r', ' ');
    line.replace('\n', ' ');
    RegistrationMonitor::appendServiceLog(directory,
        (QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs) + ' ' + line + '\n').toUtf8());
    if (QCoreApplication::arguments().contains(QStringLiteral("--console"))) {
        qInfo().noquote() << line;
    }
}

ServiceRuntime::ServiceRuntime(const QString &directory, const Config &config, const State &state,
                               RegistrationMonitor &monitor, WaitForStop waitForStop)
    : m_directory(directory), m_statePath(QDir(directory).filePath(QStringLiteral("state.json"))), m_config(config),
      m_state(state), m_monitor(monitor), m_waitForStop(std::move(waitForStop)), m_computerName(QSysInfo::machineHostName())
{
    m_tick.setSingleShot(true);
    m_tick.setTimerType(Qt::PreciseTimer);
    m_requestTimeout.setSingleShot(true);
    m_requestTimeout.setTimerType(Qt::PreciseTimer);
    connect(&m_tick, &QTimer::timeout, this, &ServiceRuntime::sendNextRequest);
    connect(&m_requestTimeout, &QTimer::timeout, this, &ServiceRuntime::requestTimedOut);
    connect(&m_statusRefresh, &QTimer::timeout, this, [this] { m_monitor.publish(); });
    connect(&m_logCleanup, &QTimer::timeout, this, [this] { RegistrationMonitor::pruneServiceLog(m_directory); });
}

ServiceRuntime::~ServiceRuntime()
{
    if (m_lookupId >= 0) {
        QHostInfo::abortHostLookup(m_lookupId);
    }
    if (m_worker) {
        m_worker->wait();
        delete m_worker;
        updateCommandResult(m_activeResult);
    }
    if (m_powerCommand) {
        updateCommandResult({m_powerCommand->id, false, QStringLiteral("终端服务正在停止")});
    }
    saveState();
}

void ServiceRuntime::start()
{
    log(QStringLiteral("终端服务已启动"));
    if (m_config.serverUrl.scheme() == QStringLiteral("https")
            && !m_http.supportedSchemes().contains(QStringLiteral("https"))) {
        log(QStringLiteral("Qt HTTPS 运行时不可用，请检查安装目录中的 OpenSSL DLL"));
    }
    if (m_state.regToken.isEmpty()) {
        m_state.ensureRegToken();
        m_stateDirty = true;
    }
    m_statusRefresh.start(StatusRefreshMs);
    m_logCleanup.start(ServiceLogCleanupMs);
    m_tick.start(0);
}

void ServiceRuntime::sendNextRequest()
{
    if (m_waitForStop(0) || m_request != Request::None || m_powerInitiated) {
        return;
    }
    m_requestElapsed.start();
    const bool registering = m_state.terminalCode.isEmpty();
    m_monitor.beginRequest(registering);
    if (!saveState() && registering) {
        reportRequestFailure(true, RegistrationProblem::State);
        scheduleRegisterRetry();
        return;
    }

    m_request = registering ? Request::Register : Request::Heartbeat;
    m_requestTimeout.start(RequestTimeoutMs);
    const QString host = m_config.serverUrl.host();
    const QHostAddress literal(host);
    if (!literal.isNull()) {
        postRequest({literal});
        return;
    }
    m_lookupId = QHostInfo::lookupHost(host, this, [this](const QHostInfo &info) {
        if (info.lookupId() != m_lookupId) {
            return;
        }
        m_lookupId = -1;
        if (info.error() != QHostInfo::NoError) {
            finishRequest(0, {}, RegistrationProblem::Dns);
            return;
        }
        postRequest(info.addresses());
    });
}

void ServiceRuntime::postRequest(const QList<QHostAddress> &addresses)
{
    m_network = findNetworkIdentity(addresses);
    m_monitor.setLocalIp(m_network.localIp.toString());
    const bool registering = m_request == Request::Register;
    const QUrl url = registering ? Protocol::registerUrl(m_config.serverUrl) : Protocol::heartbeatUrl(m_config.serverUrl);
    m_sentResults = registering ? QJsonArray() : m_state.resultsForHeartbeat(m_activeCommandId);
    const QJsonObject payload = registering
            ? Protocol::registerRequest(m_config.slid, m_state.regToken, m_computerName, m_network,
                                        QCoreApplication::applicationVersion())
            : Protocol::heartbeatRequest(m_config.slid, m_state.terminalCode, m_computerName, m_network,
                                         appIsRunning(), m_sentResults);

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
    m_reply = m_http.post(request, QJsonDocument(payload).toJson(QJsonDocument::Compact));
    connect(m_reply, &QNetworkReply::finished, this, [this] {
        QNetworkReply *reply = m_reply;
        m_reply = nullptr;
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const auto error = reply->error();
        const auto problem = networkProblem(error, status);
        const QByteArray body = reply->readAll();
        reply->deleteLater();
        finishRequest(status, body, problem, error);
    });
}

void ServiceRuntime::requestTimedOut()
{
    if (m_lookupId >= 0) {
        QHostInfo::abortHostLookup(m_lookupId);
        m_lookupId = -1;
    }
    if (m_reply) {
        disconnect(m_reply, nullptr, this, nullptr);
        m_reply->abort();
        m_reply->deleteLater();
        m_reply = nullptr;
    }
    finishRequest(0, {}, RegistrationProblem::Timeout);
}

void ServiceRuntime::finishRequest(int status, const QByteArray &body, RegistrationProblem transportProblem,
                                   QNetworkReply::NetworkError networkError)
{
    m_requestTimeout.stop();
    const Request completed = m_request;
    m_request = Request::None;
    const int protocolStatus = transportProblem == RegistrationProblem::None ? status : 0;
    const auto problem = transportProblem == RegistrationProblem::None ? RegistrationProblem::Response : transportProblem;
    if (completed == Request::Register) {
        const RegisterResponse response = Protocol::parseRegister(protocolStatus, body);
        if (!response.valid) {
            reportRequestFailure(true, problem, status, networkError);
            scheduleRegisterRetry();
            return;
        }
        if (response.terminalCode.isEmpty()) {
            m_monitor.registered(false);
            m_registerRetryMs = 0;
            schedule(RegisterIntervalMinMs + QRandomGenerator::global()->bounded(RegisterJitterMs + 1));
            return;
        }
        m_state.terminalCode = response.terminalCode;
        m_stateDirty = true;
        if (!saveState()) {
            m_state.terminalCode.clear();
            reportRequestFailure(true, RegistrationProblem::State);
            scheduleRegisterRetry();
            return;
        }
        m_registerRetryMs = 0;
        log(QStringLiteral("已保存终端码，开始发送心跳"));
        m_monitor.registered(true);
        schedule(0);
        return;
    }

    const HeartbeatResponse response = Protocol::parseHeartbeat(protocolStatus, body);
    if (response.confirmed && !m_sentResults.isEmpty()) {
        m_state.acknowledge(m_sentResults, true);
        m_stateDirty = true;
    }
    m_sentResults = {};
    if (!response.valid) {
        reportRequestFailure(false, problem, status, networkError);
        schedule(HeartbeatIntervalMs);
    } else if (!response.codeValid) {
        publishExamSeatDisplay({{QStringLiteral("state"), QStringLiteral("SYNC")}}, m_waitForStop);
        m_commands.clear();
        QString error;
        if (!SeatDisplayStore(m_directory).clearAssignment(error)) {
            log(error);
            reportRequestFailure(false, RegistrationProblem::State);
            saveState();
            schedule(HeartbeatIntervalMs);
            return;
        }
        m_state.terminalCode.clear();
        m_stateDirty = true;
        log(QStringLiteral("终端码已失效，重新注册"));
        m_monitor.codeInvalid();
        schedule(0);
    } else {
        if (response.seatNumber) {
            QString error;
            if (!SeatDisplayStore(m_directory).applyAssignment(*response.seatNumber, error)) {
                log(error);
            }
        }
        auto seat = response.seatNumber;
        if (seat && (seat->contains(m_state.terminalCode) || seat->contains(m_state.regToken))) {
            seat = QString();
        }
        m_monitor.heartbeatReceived(seat);
        publishExamSeatDisplay(response.examSeatDisplay, m_waitForStop);
        for (const Command &command : response.commands) {
            m_commands.enqueue(command);
        }
        schedule(appIsRunning() ? 5000 : HeartbeatIntervalMs + QRandomGenerator::global()->bounded(5001));
    }
    saveState();
    if (m_powerCommand) {
        finishPowerCommand();
    }
    processQueuedCommands();
}

void ServiceRuntime::reportRequestFailure(bool registering, RegistrationProblem problem, int status,
                                          QNetworkReply::NetworkError networkError)
{
    const int httpStatus = problem == RegistrationProblem::Http ? status : 0;
    if (m_monitor.failed(registering, problem, httpStatus)) {
        QString detail = registrationProblemText(problem, httpStatus);
        if (networkError != QNetworkReply::NoError) {
            const char *name = QMetaEnum::fromType<QNetworkReply::NetworkError>().valueToKey(networkError);
            detail += QStringLiteral("（Qt 网络错误 %1 %2）")
                    .arg(networkError).arg(QString::fromLatin1(name));
        }
        log(QStringLiteral("%1：%2").arg(
            registering ? QStringLiteral("终端注册失败") : QStringLiteral("心跳失败"),
            detail));
    }
}

void ServiceRuntime::processQueuedCommands()
{
    while (!m_commands.isEmpty() && !m_worker && !m_powerCommand && !m_powerInitiated
            && !m_waitForStop(0) && m_request == Request::None) {
        const Command command = m_commands.head();
        const qint64 now = QDateTime::currentMSecsSinceEpoch();
        const CommandDecision decision = m_state.decide(command, now);
        if (decision == CommandDecision::Duplicate) {
            m_commands.dequeue();
            continue;
        }
        const bool expired = decision == CommandDecision::Expired;
        const bool power = !expired && command.isPowerCommand();
        const CommandResult initialResult{command.id, power, expired ? QStringLiteral("指令已过期")
                : power ? QStringLiteral("已接受") : QStringLiteral("指令执行中断")};
        // 先保存指令 ID，避免服务重启后重复执行已触发的系统操作。
        const State previous = m_state;
        const bool previousDirty = m_stateDirty;
        m_state.record(initialResult, now);
        m_stateDirty = true;
        if (!saveState()) {
            m_state = previous;
            m_stateDirty = previousDirty;
            return;
        }
        m_commands.dequeue();

        if (expired || power) {
            logResult(initialResult);
            if (power) {
                m_powerCommand = command;
                m_tick.stop();
                sendNextRequest();
            }
            continue;
        }

        m_activeCommandId = command.id;
        m_activeResult = initialResult;
        const NetworkIdentity network = m_network;
        m_worker = QThread::create([this, command, network] {
            m_activeResult = executeAppOrWake(command, m_config, network, m_waitForStop);
        });
        connect(m_worker, &QThread::finished, this, &ServiceRuntime::finishWorker);
        m_worker->start();
    }
}

void ServiceRuntime::finishWorker()
{
    m_worker->wait();
    m_worker->deleteLater();
    m_worker = nullptr;
    updateCommandResult(m_activeResult);
    m_activeResult = {};
    m_activeCommandId.clear();
    saveState();
    processQueuedCommands();
}

void ServiceRuntime::finishPowerCommand()
{
    const Command command = *m_powerCommand;
    if (m_waitForStop(0)) {
        return;
    }
    m_powerCommand.reset();
    // 补发心跳已经结束，服务端不可用时也执行电源操作。
    const unsigned long error = initiatePowerChange(command.type == QStringLiteral("REBOOT"));
    if (error == 0) {
        m_powerInitiated = true;
        m_tick.stop();
        m_commands.clear();
        return;
    }
    updateCommandResult({command.id, false, windowsFailure(QStringLiteral("电源操作失败"), error)});
    saveState();
}

void ServiceRuntime::updateCommandResult(const CommandResult &result)
{
    m_state.updateResult(result);
    m_stateDirty = true;
    logResult(result);
}

bool ServiceRuntime::saveState()
{
    if (!m_stateDirty) {
        return true;
    }
    QString error;
    if (!m_state.save(m_statePath, error)) {
        log(QStringLiteral("保存 state.json 失败：%1").arg(error));
        return false;
    }
    m_stateDirty = false;
    return true;
}

void ServiceRuntime::schedule(int intervalMs)
{
    const int delay = static_cast<int>(std::max<qint64>(0, intervalMs - m_requestElapsed.elapsed()));
    m_tick.start(delay);
    m_monitor.setNextAttempt(delay);
}

void ServiceRuntime::scheduleRegisterRetry()
{
    m_registerRetryMs = std::clamp(m_registerRetryMs * 2, RegisterRetryMinMs, RegisterRetryMaxMs);
    schedule(m_registerRetryMs - QRandomGenerator::global()->bounded(RegisterJitterMs + 1));
}

void ServiceRuntime::logResult(const CommandResult &result) const
{
    log(QStringLiteral("指令 %1 %2：%3").arg(result.commandId,
            result.success ? QStringLiteral("成功") : QStringLiteral("失败"), result.message));
}

void ServiceRuntime::log(const QString &message) const
{
    QString safe = message;
    for (const QString &credential : {m_state.regToken, m_state.terminalCode}) {
        if (!credential.isEmpty()) {
            safe.replace(credential, QStringLiteral("[已隐藏]"));
        }
    }
    writeServiceLog(m_directory, safe);
}

} // namespace terminal
