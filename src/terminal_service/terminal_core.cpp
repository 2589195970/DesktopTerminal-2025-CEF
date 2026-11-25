#include "terminal_core.h"

#include <QFile>
#include <QHash>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSet>
#include <QTextStream>
#include <QUuid>

#include <algorithm>

namespace terminal {
namespace {

constexpr int MaxProcessedCommands = 200;
constexpr qint64 MaxCommandAgeMs = 10 * 60 * 1000;

bool readTimestamp(const QJsonValue &value, qint64 &timestamp)
{
    // JSON 时间戳须能以整数毫秒精确表示。
    const double number = value.toDouble(-1);
    if (number < 0 || number > 9007199254740991.0) {
        return false;
    }
    timestamp = static_cast<qint64>(number);
    return number == static_cast<double>(timestamp);
}

QUrl endpoint(QUrl url, const QString &method)
{
    QString path = url.path(QUrl::FullyEncoded);
    while (path.endsWith('/')) {
        path.chop(1);
    }
    url.setPath(path + QStringLiteral("/api/terminal/v1/") + method, QUrl::StrictMode);
    return url;
}

struct Envelope {
    bool confirmed = false;
    QJsonValue result;
    QString error;
};

Envelope parseEnvelope(int httpStatus, const QByteArray &body)
{
    if (httpStatus != 200) {
        return {false, {}, QStringLiteral("HTTP %1").arg(httpStatus)};
    }
    const QJsonDocument document = QJsonDocument::fromJson(body);
    if (!document.isObject()) {
        return {false, {}, QStringLiteral("响应不是有效的 JSON 对象")};
    }
    const QJsonObject object = document.object();
    if (object.value(QStringLiteral("code")).toDouble() != 10200) {
        return {false, {}, object.value(QStringLiteral("message")).toString(QStringLiteral("服务器响应失败"))};
    }
    return {true, object.value(QStringLiteral("result")), {}};
}

} // namespace

bool Config::load(const QString &path, Config &config, QString &error)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        error = file.errorString();
        return false;
    }
    // Windows INI 中的反斜杠是路径和账户名的一部分，按原文读取。
    QTextStream ini(&file);
    ini.setCodec("UTF-8");
    QHash<QString, QString> values;
    QString section;
    while (!ini.atEnd()) {
        const QString line = ini.readLine().trimmed();
        if (line.isEmpty() || line.startsWith(';') || line.startsWith('#')) {
            continue;
        }
        if (line.startsWith('[') && line.endsWith(']')) {
            section = line.mid(1, line.size() - 2).trimmed().toLower();
            continue;
        }
        const int separator = line.indexOf('=');
        if (separator > 0) {
            const QString key = line.left(separator).trimmed().toLower();
            QString value = line.mid(separator + 1).trimmed();
            if (value.size() >= 2 && value.startsWith('"') && value.endsWith('"')) {
                value = value.mid(1, value.size() - 2);
            }
            values.insert(section + '/' + key, value);
        }
    }
    Config loaded;
    loaded.serverUrl = QUrl(values.value(QStringLiteral("server/url")), QUrl::StrictMode);
    loaded.slid = values.value(QStringLiteral("server/slid")).toInt();
    loaded.appPath = values.value(QStringLiteral("app/path"));
    loaded.examAccount = values.value(QStringLiteral("app/examaccount"));
    if (ini.status() != QTextStream::Ok || file.error() != QFileDevice::NoError) {
        error = QStringLiteral("读取 service.ini 失败");
        return false;
    }
    const QString scheme = loaded.serverUrl.scheme();
    const bool hasSupportedScheme = scheme.compare(QStringLiteral("http"), Qt::CaseInsensitive) == 0
            || scheme.compare(QStringLiteral("https"), Qt::CaseInsensitive) == 0;
    if (!loaded.serverUrl.isValid() || loaded.serverUrl.host().isEmpty()
            || !hasSupportedScheme || !loaded.serverUrl.userInfo().isEmpty() || loaded.serverUrl.hasQuery()
            || loaded.serverUrl.hasFragment() || loaded.slid <= 0) {
        error = QStringLiteral("service.ini 需要有效的 HTTP 或 HTTPS 服务器地址和学校实例标识");
        return false;
    }
    config = loaded;
    return true;
}

bool Command::isPowerCommand() const
{
    return type == QStringLiteral("REBOOT") || type == QStringLiteral("SHUTDOWN");
}

QJsonObject CommandResult::toJson() const
{
    return {{QStringLiteral("commandId"), commandId}, {QStringLiteral("success"), success},
            {QStringLiteral("message"), message}};
}

bool State::load(const QString &path, State &state, QString &error)
{
    QFile file(path);
    if (!file.exists()) {
        state = {};
        return true;
    }
    if (!file.open(QIODevice::ReadOnly)) {
        error = file.errorString();
        return false;
    }
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
    const QJsonObject object = document.object();
    if (!document.isObject() || !object.value(QStringLiteral("regToken")).isString()
            || !object.value(QStringLiteral("terminalCode")).isString()
            || !object.value(QStringLiteral("processedCommandIds")).isArray()
            || !object.value(QStringLiteral("pendingResults")).isArray()) {
        error = QStringLiteral("state.json 格式错误");
        return false;
    }

    State loaded;
    loaded.regToken = object.value(QStringLiteral("regToken")).toString();
    loaded.terminalCode = object.value(QStringLiteral("terminalCode")).toString();
    const QJsonArray processed = object.value(QStringLiteral("processedCommandIds")).toArray();
    const int first = std::max(0, processed.size() - MaxProcessedCommands);
    for (int index = first; index < processed.size(); ++index) {
        const QJsonObject item = processed.at(index).toObject();
        ProcessedCommand command;
        command.id = item.value(QStringLiteral("id")).toString();
        if (command.id.isEmpty() || !readTimestamp(item.value(QStringLiteral("processedAt")), command.processedAt)) {
            error = QStringLiteral("state.json 的已处理指令格式错误");
            return false;
        }
        loaded.processedCommandIds.append(command);
    }
    const QJsonArray pending = object.value(QStringLiteral("pendingResults")).toArray();
    for (const QJsonValue &value : pending) {
        const QJsonObject item = value.toObject();
        CommandResult result;
        result.commandId = item.value(QStringLiteral("commandId")).toString();
        if (result.commandId.isEmpty() || !item.value(QStringLiteral("success")).isBool()
                || !item.value(QStringLiteral("message")).isString()) {
            error = QStringLiteral("state.json 的待回报结果格式错误");
            return false;
        }
        result.success = item.value(QStringLiteral("success")).toBool();
        result.message = item.value(QStringLiteral("message")).toString();
        loaded.pendingResults.append(result);
    }
    state = loaded;
    return true;
}

bool State::save(const QString &path, QString &error) const
{
    QJsonArray processed;
    for (const ProcessedCommand &command : processedCommandIds) {
        processed.append(QJsonObject{{QStringLiteral("id"), command.id},
                                     {QStringLiteral("processedAt"), static_cast<double>(command.processedAt)}});
    }
    const QJsonObject object{{QStringLiteral("regToken"), regToken},
                             {QStringLiteral("terminalCode"), terminalCode},
                             {QStringLiteral("processedCommandIds"), processed},
                             {QStringLiteral("pendingResults"), resultsForHeartbeat()}};
    const QByteArray bytes = QJsonDocument(object).toJson(QJsonDocument::Compact);
    QSaveFile file(path);
    file.setDirectWriteFallback(false);
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit()) {
        error = file.errorString();
        return false;
    }
    return true;
}

void State::ensureRegToken()
{
    if (regToken.isEmpty()) {
        regToken = QUuid::createUuid().toString(QUuid::WithoutBraces).toLower();
    }
}

CommandDecision State::decide(const Command &command, qint64 now) const
{
    for (const ProcessedCommand &processed : processedCommandIds) {
        if (processed.id == command.id) {
            return CommandDecision::Duplicate;
        }
    }
    for (const CommandResult &result : pendingResults) {
        if (result.commandId == command.id) {
            return CommandDecision::Duplicate;
        }
    }
    return command.issuedAt < now - MaxCommandAgeMs ? CommandDecision::Expired : CommandDecision::Execute;
}

void State::record(const CommandResult &result, qint64 now)
{
    if (processedCommandIds.size() == MaxProcessedCommands) {
        processedCommandIds.removeFirst();
    }
    processedCommandIds.append({result.commandId, now});
    pendingResults.append(result);
}

void State::updateResult(const CommandResult &result)
{
    for (CommandResult &pending : pendingResults) {
        if (pending.commandId == result.commandId) {
            pending = result;
            return;
        }
    }
    pendingResults.append(result);
}

QJsonArray State::resultsForHeartbeat(const QString &activeCommandId) const
{
    QJsonArray results;
    for (const CommandResult &pending : pendingResults) {
        if (pending.commandId != activeCommandId) {
            results.append(pending.toJson());
        }
    }
    return results;
}

void State::acknowledge(const QJsonArray &sentResults, bool confirmed)
{
    if (!confirmed) {
        return;
    }
    QSet<QString> ids;
    for (const QJsonValue &value : sentResults) {
        ids.insert(value.toObject().value(QStringLiteral("commandId")).toString());
    }
    pendingResults.erase(std::remove_if(pendingResults.begin(), pendingResults.end(),
                                       [&ids](const CommandResult &result) { return ids.contains(result.commandId); }),
                         pendingResults.end());
}

QUrl Protocol::registerUrl(const QUrl &baseUrl)
{
    return endpoint(baseUrl, QStringLiteral("register"));
}

QUrl Protocol::heartbeatUrl(const QUrl &baseUrl)
{
    return endpoint(baseUrl, QStringLiteral("heartbeat"));
}

QJsonObject Protocol::registerRequest(int slid, const QString &regToken, const QString &computerName,
                                      const NetworkIdentity &network, const QString &version)
{
    return {{QStringLiteral("slid"), slid}, {QStringLiteral("regToken"), regToken},
            {QStringLiteral("computerName"), computerName}, {QStringLiteral("localIp"), network.localIp.toString()},
            {QStringLiteral("version"), version}};
}

QJsonObject Protocol::heartbeatRequest(int slid, const QString &terminalCode, const QString &computerName,
                                       const NetworkIdentity &network, bool appRunning, const QJsonArray &results)
{
    return {{QStringLiteral("slid"), slid}, {QStringLiteral("terminalCode"), terminalCode},
            {QStringLiteral("computerName"), computerName}, {QStringLiteral("localIp"), network.localIp.toString()},
            {QStringLiteral("prefixLength"), network.prefixLength}, {QStringLiteral("mac"), network.mac},
            {QStringLiteral("appRunning"), appRunning}, {QStringLiteral("results"), results}};
}

RegisterResponse Protocol::parseRegister(int httpStatus, const QByteArray &body)
{
    const Envelope envelope = parseEnvelope(httpStatus, body);
    if (!envelope.confirmed) {
        return {false, {}, envelope.error};
    }
    const QJsonValue code = envelope.result.toObject().value(QStringLiteral("terminalCode"));
    static const QRegularExpression format(QStringLiteral("\\A[0-9]{12}\\z"));
    if (!code.isNull() && (!code.isString() || !format.match(code.toString()).hasMatch())) {
        return {false, {}, QStringLiteral("注册响应的终端码格式错误")};
    }
    return {true, code.toString(), {}};
}

HeartbeatResponse Protocol::parseHeartbeat(int httpStatus, const QByteArray &body)
{
    const Envelope envelope = parseEnvelope(httpStatus, body);
    if (!envelope.confirmed) {
        return {false, false, false, {}, envelope.error};
    }
    const QJsonObject result = envelope.result.toObject();
    const QJsonValue codeValid = result.value(QStringLiteral("codeValid"));
    if (!codeValid.isBool()) {
        return {true, false, false, {}, QStringLiteral("心跳响应缺少有效的 codeValid")};
    }
    if (!codeValid.toBool()) {
        return {true, true, false, {}, {}};
    }
    const QJsonValue commands = result.value(QStringLiteral("commands"));
    if (!commands.isArray()) {
        return {true, false, true, {}, QStringLiteral("心跳响应缺少有效的指令列表")};
    }
    HeartbeatResponse response{true, true, true, {}, {}};
    const QJsonValue seatNumber = result.value(QStringLiteral("seatNumber"));
    if (seatNumber.isString()) {
        response.seatNumber = seatNumber.toString().trimmed();
    }
    const QJsonValue displayValue = result.value(QStringLiteral("examSeatDisplay"));
    if (!displayValue.isUndefined() && !displayValue.isNull()) {
        const QJsonObject display = displayValue.toObject();
        const QString state = display.value(QStringLiteral("state")).toString();
        const QString seatNumber = display.value(QStringLiteral("seatNumber")).toString();
        const QString releaseId = display.value(QStringLiteral("releaseId")).toString();
        const int seconds = display.value(QStringLiteral("validForSeconds")).toInt();
        static const QRegularExpression numberPattern(QStringLiteral("\\A[0-9]{1,32}\\z"));
        if ((state == QStringLiteral("UNUSED") || state == QStringLiteral("WAITING")
                || state == QStringLiteral("LOGIN_OPEN"))
                && (state == QStringLiteral("UNUSED") || numberPattern.match(seatNumber).hasMatch())
                && !releaseId.isEmpty() && releaseId.size() <= 36 && seconds > 0 && seconds <= 90) {
            response.examSeatDisplay = {{QStringLiteral("state"), state},
                    {QStringLiteral("releaseId"), releaseId},
                    {QStringLiteral("validForSeconds"), seconds}};
            const QString sessionName = display.value(QStringLiteral("sessionName")).toString().trimmed();
            if (!sessionName.isEmpty()) {
                response.examSeatDisplay.insert(QStringLiteral("sessionName"), sessionName);
            }
            if (state != QStringLiteral("UNUSED")) {
                response.examSeatDisplay.insert(QStringLiteral("seatNumber"), seatNumber);
            }
        } else {
            response.examSeatDisplay = {{QStringLiteral("state"), QStringLiteral("SYNC")}};
        }
    }
    const QJsonArray items = commands.toArray();
    for (const QJsonValue &value : items) {
        const QJsonObject item = value.toObject();
        Command command;
        command.id = item.value(QStringLiteral("id")).toString();
        command.type = item.value(QStringLiteral("type")).toString();
        if (command.id.isEmpty() || !readTimestamp(item.value(QStringLiteral("issuedAt")), command.issuedAt)) {
            continue;
        }
        const QJsonArray macs = item.value(QStringLiteral("macs")).toArray();
        for (const QJsonValue &mac : macs) {
            if (mac.isString()) {
                command.macs.append(mac.toString());
            }
        }
        response.commands.append(command);
    }
    return response;
}

QByteArray magicPacket(const QString &mac)
{
    static const QRegularExpression format(QStringLiteral("\\A[0-9A-Fa-f]{2}(?::[0-9A-Fa-f]{2}){5}\\z"));
    if (!format.match(mac).hasMatch()) {
        return {};
    }
    QByteArray digits = mac.toLatin1();
    digits.replace(":", "");
    return QByteArray(6, '\xff') + QByteArray::fromHex(digits).repeated(16);
}

QHostAddress broadcastAddress(const QHostAddress &localIp, int prefixLength)
{
    if (localIp.protocol() != QAbstractSocket::IPv4Protocol || prefixLength < 0 || prefixLength > 32) {
        return {};
    }
    const quint32 hostMask = prefixLength == 32 ? 0 : (0xffffffffu >> prefixLength);
    return QHostAddress(localIp.toIPv4Address() | hostMask);
}

bool examAccountMatches(const QString &examAccount, const QString &user, const QString &domain)
{
    if (user.isEmpty()) {
        return false;
    }
    if (examAccount.isEmpty()) {
        return true;
    }
    const QString loggedIn = examAccount.contains('\\') ? domain + '\\' + user : user;
    return examAccount.compare(loggedIn, Qt::CaseInsensitive) == 0;
}

} // namespace terminal
