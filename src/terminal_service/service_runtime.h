#pragma once

#include "platform_win.h"
#include "registration_monitor.h"

#include <QElapsedTimer>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QQueue>
#include <QThread>
#include <QTimer>

#include <optional>

namespace terminal {

void writeServiceLog(const QString &directory, const QString &message);

class ServiceRuntime : public QObject {
public:
    ServiceRuntime(const QString &directory, const Config &config, const State &state,
                   RegistrationMonitor &monitor, WaitForStop waitForStop);
    ~ServiceRuntime() override;

    void start();
    void refreshNow();

private:
    enum class Request { None, Register, Heartbeat };

    void sendNextRequest();
    void postRequest(const QList<QHostAddress> &addresses);
    void requestTimedOut();
    void finishRequest(int status, const QByteArray &body, RegistrationProblem transportProblem,
                       QNetworkReply::NetworkError networkError = QNetworkReply::NoError);
    void reportRequestFailure(bool registering, RegistrationProblem problem, int status = 0,
                              QNetworkReply::NetworkError networkError = QNetworkReply::NoError);
    void processQueuedCommands();
    void finishWorker();
    void finishPowerCommand();
    void updateCommandResult(const CommandResult &result);
    bool saveState();
    void schedule(int intervalMs);
    void scheduleRegisterRetry();
    void watchApp();
    void setExamSeatDisplay(const QJsonObject &display);
    void deliverExamSeatDisplay();
    void logResult(const CommandResult &result) const;
    void log(const QString &message) const;

    QString m_directory;
    QString m_statePath;
    Config m_config;
    State m_state;
    RegistrationMonitor &m_monitor;
    WaitForStop m_waitForStop;
    bool m_stateDirty = false;
    QString m_computerName;
    QNetworkAccessManager m_http;
    QTimer m_tick;
    QTimer m_requestTimeout;
    QTimer m_statusRefresh;
    QTimer m_logCleanup;
    QTimer m_appWatch;
    QElapsedTimer m_requestElapsed;
    int m_registerRetryMs = 0;
    Request m_request = Request::None;
    int m_lookupId = -1;
    QNetworkReply *m_reply = nullptr;
    NetworkIdentity m_network;
    QJsonArray m_sentResults;
    QQueue<Command> m_commands;
    std::optional<Command> m_powerCommand;
    bool m_powerInitiated = false;
    QThread *m_worker = nullptr;
    QString m_activeCommandId;
    CommandResult m_activeResult;
    bool m_appRunning = false;
    QJsonObject m_examSeatDisplay;
    QElapsedTimer m_examSeatDisplayAge;
    bool m_examSeatDisplayPending = false;
};

} // namespace terminal
