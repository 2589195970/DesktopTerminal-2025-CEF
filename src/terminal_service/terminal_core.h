#pragma once

#include <QByteArray>
#include <QHostAddress>
#include <QJsonArray>
#include <QJsonObject>
#include <QStringList>
#include <QUrl>
#include <QVector>

namespace terminal {

struct Config {
    QUrl serverUrl;
    int slid = 0;
    QString appPath;
    QString examAccount;
    bool sample = false;

    static bool load(const QString &path, Config &config, QString &error);
};

struct Command {
    QString id;
    QString type;
    qint64 issuedAt = 0;
    QStringList macs;

    bool isPowerCommand() const;
};

struct CommandResult {
    QString commandId;
    bool success = false;
    QString message;

    QJsonObject toJson() const;
};

struct ProcessedCommand {
    QString id;
    qint64 processedAt = 0;
};

enum class CommandDecision { Execute, Duplicate, Expired };

struct State {
    QString regToken;
    QString terminalCode;
    QVector<ProcessedCommand> processedCommandIds;
    QVector<CommandResult> pendingResults;

    static bool load(const QString &path, State &state, QString &error);
    bool save(const QString &path, QString &error) const;
    void ensureRegToken();
    CommandDecision decide(const Command &command, qint64 now) const;
    void record(const CommandResult &result, qint64 now);
    void updateResult(const CommandResult &result);
    QJsonArray resultsForHeartbeat(const QString &activeCommandId = {}) const;
    void acknowledge(const QJsonArray &sentResults, bool confirmed);
};

struct NetworkIdentity {
    QHostAddress localIp;
    int prefixLength = 0;
    QString mac;
};

struct RegisterResponse {
    bool valid = false;
    QString terminalCode;
    QString error;
};

struct HeartbeatResponse {
    bool confirmed = false;
    bool valid = false;
    bool codeValid = false;
    QVector<Command> commands;
    QString error;
};

class Protocol {
public:
    static QUrl registerUrl(const QUrl &baseUrl);
    static QUrl heartbeatUrl(const QUrl &baseUrl);
    static QJsonObject registerRequest(int slid, const QString &regToken, const QString &computerName,
                                       const NetworkIdentity &network, const QString &version);
    static QJsonObject heartbeatRequest(int slid, const QString &terminalCode, const QString &computerName,
                                        const NetworkIdentity &network, bool appRunning,
                                        const QJsonArray &results);
    static RegisterResponse parseRegister(int httpStatus, const QByteArray &body);
    static HeartbeatResponse parseHeartbeat(int httpStatus, const QByteArray &body);
};

QByteArray magicPacket(const QString &mac);
QHostAddress broadcastAddress(const QHostAddress &localIp, int prefixLength);
bool examAccountMatches(const QString &examAccount, const QString &user, const QString &domain);

} // namespace terminal
