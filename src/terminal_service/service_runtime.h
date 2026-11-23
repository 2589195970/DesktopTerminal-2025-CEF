#pragma once

#include "platform_win.h"

#include <QElapsedTimer>
#include <QNetworkAccessManager>
#include <QQueue>
#include <QThread>
#include <QTimer>

#include <optional>

namespace terminal {

void writeServiceLog(const QString &directory, const QString &message);

class ServiceRuntime : public QObject {
public:
    ServiceRuntime(const QString &directory, const Config &config, const State &state, WaitForStop waitForStop);
    ~ServiceRuntime() override;

    void start();

private:
    enum class Request { None, Register, Heartbeat };

    void sendNextRequest();
    void postRequest(const QList<QHostAddress> &addresses);
    void requestTimedOut();
    void finishRequest(int status, const QByteArray &body, const QString &transportError);
    void processQueuedCommands();
    void finishWorker();
    void finishPowerCommand();
    bool saveState();
    void schedule(int intervalMs);
    void logResult(const CommandResult &result) const;
    void log(const QString &message) const;

    QString m_directory;
    QString m_statePath;
    Config m_config;
    State m_state;
    WaitForStop m_waitForStop;
    bool m_stateDirty = false;
    QString m_computerName;
    QNetworkAccessManager m_http;
    QTimer m_tick;
    QTimer m_requestTimeout;
    QElapsedTimer m_requestElapsed;
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
};

} // namespace terminal
