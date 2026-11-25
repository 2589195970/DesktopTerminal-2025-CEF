#include "terminal_core.h"
#include "registration_monitor.h"

#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QTemporaryDir>
#include <QtTest>

using namespace terminal;

class TerminalServiceTests : public QObject {
    Q_OBJECT

private slots:
    void requestsUseContract();
    void parseRegisterResponses();
    void parseHeartbeatResponses();
    void parseExamSeatDisplay();
    void deduplicateAndExpireCommands();
    void acknowledgeOnlySentResults();
    void buildMagicPacket();
    void calculateDirectedBroadcast();
    void readWindowsConfig();
    void validateServerUrl_data();
    void validateServerUrl();
    void editServerSettings();
    void persistState();
    void monitorRegistrationLifecycle();
    void monitorPreservesFailureAfterRestart();
    void monitorRotatesAndExpiresLogs();
};

void TerminalServiceTests::requestsUseContract()
{
    const QUrl base(QStringLiteral("https://exam.example.org/um/"));
    QCOMPARE(Protocol::registerUrl(base), QUrl(QStringLiteral("https://exam.example.org/um/api/terminal/v1/register")));
    QCOMPARE(Protocol::heartbeatUrl(base), QUrl(QStringLiteral("https://exam.example.org/um/api/terminal/v1/heartbeat")));
    const QUrl httpBase(QStringLiteral("http://exam.example.org:8080/um/"));
    QCOMPARE(Protocol::registerUrl(httpBase),
             QUrl(QStringLiteral("http://exam.example.org:8080/um/api/terminal/v1/register")));
    QCOMPARE(Protocol::heartbeatUrl(httpBase),
             QUrl(QStringLiteral("http://exam.example.org:8080/um/api/terminal/v1/heartbeat")));
    const NetworkIdentity network{QHostAddress(QStringLiteral("192.168.124.37")), 24, QStringLiteral("AA:BB:CC:DD:EE:FF")};
    const QJsonObject registration = Protocol::registerRequest(10003, QStringLiteral("token"), QStringLiteral("PC01"),
                                                               network, QStringLiteral("1.0.0"));
    QCOMPARE(registration, (QJsonObject{{"slid", 10003}, {"regToken", "token"}, {"computerName", "PC01"},
                                       {"localIp", "192.168.124.37"}, {"version", "1.0.0"}}));
    const QJsonArray results{CommandResult{QStringLiteral("a"), true, QStringLiteral("已接受")}.toJson()};
    const QJsonObject heartbeat = Protocol::heartbeatRequest(10003, QStringLiteral("123456789012"),
                                                             QStringLiteral("PC01"), network, true, results);
    QCOMPARE(heartbeat, (QJsonObject{{"slid", 10003}, {"terminalCode", "123456789012"}, {"computerName", "PC01"},
                                    {"localIp", "192.168.124.37"}, {"prefixLength", 24}, {"mac", "AA:BB:CC:DD:EE:FF"},
                                    {"appRunning", true}, {"results", results}}));
}

void TerminalServiceTests::parseRegisterResponses()
{
    const auto assigned = Protocol::parseRegister(200, R"({"code":10200,"result":{"terminalCode":"012345678901"}})");
    QVERIFY(assigned.valid);
    QCOMPARE(assigned.terminalCode, QStringLiteral("012345678901"));

    const auto unassigned = Protocol::parseRegister(200, R"({"code":10200,"result":{"terminalCode":null}})");
    QVERIFY(unassigned.valid);
    QVERIFY(unassigned.terminalCode.isEmpty());

    const auto error = Protocol::parseRegister(200, R"({"code":10500,"message":"register error","result":null})");
    QVERIFY(!error.valid);
    QCOMPARE(error.error, QStringLiteral("register error"));
    QVERIFY(!Protocol::parseRegister(503, R"({"code":10200,"result":{"terminalCode":"012345678901"}})").valid);
    QVERIFY(!Protocol::parseRegister(200, R"({"code":10200,"result":{}})").valid);
}

void TerminalServiceTests::parseExamSeatDisplay()
{
    const auto waiting = Protocol::parseHeartbeat(200, R"({"code":10200,"result":{"codeValid":true,"commands":[],"seatNumber":"07","examSeatDisplay":{"state":"WAITING","sessionName":" 上午第一场 ","seatNumber":"002","releaseId":"room-1","validForSeconds":20}}})");
    QVERIFY(waiting.valid);
    QCOMPARE(waiting.seatNumber.value_or(QString()), QStringLiteral("07"));
    QCOMPARE(waiting.examSeatDisplay.value("state").toString(), QStringLiteral("WAITING"));
    QCOMPARE(waiting.examSeatDisplay.value("seatNumber").toString(), QStringLiteral("002"));
    QCOMPARE(waiting.examSeatDisplay.value("sessionName").toString(), QStringLiteral("上午第一场"));

    const auto opened = Protocol::parseHeartbeat(200, R"({"code":10200,"result":{"codeValid":true,"commands":[],"examSeatDisplay":{"state":"LOGIN_OPEN","seatNumber":"002","releaseId":"room-1","validForSeconds":20}}})");
    QCOMPARE(opened.examSeatDisplay.value("state").toString(), QStringLiteral("LOGIN_OPEN"));
    QVERIFY(!opened.examSeatDisplay.contains("sessionName"));

    const auto stale = Protocol::parseHeartbeat(200, R"({"code":10200,"result":{"codeValid":true,"commands":[],"examSeatDisplay":{"state":"LOGIN_OPEN","seatNumber":"002","releaseId":"room-1","validForSeconds":0}}})");
    QCOMPARE(stale.examSeatDisplay.value("state").toString(), QStringLiteral("SYNC"));

    const auto unused = Protocol::parseHeartbeat(200, R"({"code":10200,"result":{"codeValid":true,"commands":[],"examSeatDisplay":{"state":"UNUSED","releaseId":"room-1","validForSeconds":20}}})");
    QCOMPARE(unused.examSeatDisplay.value("state").toString(), QStringLiteral("UNUSED"));
    QVERIFY(!unused.examSeatDisplay.contains("seatNumber"));

    const auto ordinary = Protocol::parseHeartbeat(200, R"({"code":10200,"result":{"codeValid":true,"commands":[]}})");
    QVERIFY(!ordinary.seatNumber);
    QVERIFY(ordinary.examSeatDisplay.isEmpty());
}

void TerminalServiceTests::parseHeartbeatResponses()
{
    const QByteArray success = R"({"code":10200,"result":{"codeValid":true,"commands":[
        {"id":"first","type":"WAKE_RELAY","issuedAt":1763800000000,"macs":["AA:BB:CC:DD:EE:FF"]},
        {"id":"second","type":"REBOOT","issuedAt":1763800000001,"macs":null}]}})";
    const auto heartbeat = Protocol::parseHeartbeat(200, success);
    QVERIFY(heartbeat.confirmed);
    QVERIFY(heartbeat.valid);
    QVERIFY(heartbeat.codeValid);
    QCOMPARE(heartbeat.commands.size(), 2);
    QCOMPARE(heartbeat.commands.at(0).id, QStringLiteral("first"));
    QCOMPARE(heartbeat.commands.at(0).type, QStringLiteral("WAKE_RELAY"));
    QCOMPARE(heartbeat.commands.at(0).issuedAt, 1763800000000LL);
    QCOMPARE(heartbeat.commands.at(0).macs, QStringList{QStringLiteral("AA:BB:CC:DD:EE:FF")});
    QVERIFY(heartbeat.commands.at(1).isPowerCommand());
    QVERIFY(heartbeat.commands.at(1).macs.isEmpty());

    const auto invalidCode = Protocol::parseHeartbeat(200, R"({"code":10200,"result":{"codeValid":false,"commands":[]}})");
    QVERIFY(invalidCode.confirmed);
    QVERIFY(invalidCode.valid);
    QVERIFY(!invalidCode.codeValid);
    QVERIFY(invalidCode.commands.isEmpty());

    const auto failure = Protocol::parseHeartbeat(200, R"({"code":10500,"message":"heartbeat error","result":null})");
    QVERIFY(!failure.confirmed);
    QVERIFY(!failure.valid);
    QCOMPARE(failure.error, QStringLiteral("heartbeat error"));
    QVERIFY(!Protocol::parseHeartbeat(502, success).confirmed);
    QVERIFY(!Protocol::parseHeartbeat(200, R"({"code":10200,"result":{"codeValid":"true"}})").valid);
}

void TerminalServiceTests::deduplicateAndExpireCommands()
{
    State state;
    constexpr qint64 now = 1763800000000LL;
    const Command command{QStringLiteral("first"), QStringLiteral("START_APP"), now - 600000, {}};
    QCOMPARE(state.decide(command, now), CommandDecision::Execute);
    QCOMPARE(state.decide(command, now + 1), CommandDecision::Expired);
    state.record({command.id, true, QStringLiteral("完成")}, now);
    const QJsonArray firstReport = state.resultsForHeartbeat();
    state.acknowledge(firstReport, true);
    QCOMPARE(state.decide(command, now), CommandDecision::Duplicate);
    QCOMPARE(state.decide(command, now + 1), CommandDecision::Duplicate);
    for (int n = 0; n < 200; ++n) {
        state.record({QString::number(n), true, {}}, now + n + 1);
    }
    QCOMPARE(state.processedCommandIds.size(), 200);
    QCOMPARE(state.processedCommandIds.first().id, QStringLiteral("0"));
    QCOMPARE(state.processedCommandIds.last().processedAt, now + 200);
    QCOMPARE(state.decide(command, now + 1), CommandDecision::Expired);
}

void TerminalServiceTests::acknowledgeOnlySentResults()
{
    State state;
    state.record({QStringLiteral("first"), true, QStringLiteral("已接受")}, 1);
    const QJsonArray sent = state.resultsForHeartbeat();
    state.record({QStringLiteral("second"), false, QStringLiteral("指令执行中断")}, 2);
    QCOMPARE(state.resultsForHeartbeat(QStringLiteral("second")), sent);
    const auto invalidHttp = Protocol::parseHeartbeat(503, R"({"code":10200,"result":{"codeValid":true,"commands":[]}})");
    const auto invalidApi = Protocol::parseHeartbeat(200, R"({"code":10500,"message":"failed"})");
    for (const auto &response : {invalidHttp, invalidApi}) {
        state.acknowledge(sent, response.confirmed);
        QCOMPARE(state.pendingResults.size(), 2);
    }
    const auto confirmed = Protocol::parseHeartbeat(200, R"({"code":10200,"result":{"codeValid":true,"commands":[]}})");
    state.acknowledge(sent, confirmed.confirmed);
    QCOMPARE(state.pendingResults.size(), 1);
    QCOMPARE(state.pendingResults.first().commandId, QStringLiteral("second"));
}

void TerminalServiceTests::buildMagicPacket()
{
    const QByteArray packet = magicPacket(QStringLiteral("aa:BB:cC:dD:eE:FF"));
    QCOMPARE(packet.size(), 102);
    QCOMPARE(packet.left(6), QByteArray::fromHex("ffffffffffff"));
    for (int index = 0; index < 16; ++index) {
        QCOMPARE(packet.mid(6 + index * 6, 6), QByteArray::fromHex("aabbccddeeff"));
    }
    QVERIFY(magicPacket(QStringLiteral("AA:BB:CC:DD:EE:GG")).isEmpty());
}

void TerminalServiceTests::calculateDirectedBroadcast()
{
    const QHostAddress ip(QStringLiteral("192.168.124.37"));
    QCOMPARE(broadcastAddress(ip, 24), QHostAddress(QStringLiteral("192.168.124.255")));
    QCOMPARE(broadcastAddress(ip, 23), QHostAddress(QStringLiteral("192.168.125.255")));
    QCOMPARE(broadcastAddress(ip, 16), QHostAddress(QStringLiteral("192.168.255.255")));
    QCOMPARE(broadcastAddress(ip, 32), ip);
    QVERIFY(broadcastAddress(ip, 33).isNull());
}

void TerminalServiceTests::readWindowsConfig()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("service.ini"));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    const QByteArray ini = QStringLiteral("[server]\r\nurl=https://exam.example.org\r\nslid=10003\r\n"
                                          "[app]\r\npath=C:\\软件\\terminal.exe\r\n").toUtf8();
    QCOMPARE(file.write(ini), static_cast<qint64>(ini.size()));
    file.close();
    Config config;
    QString error;
    QVERIFY2(Config::load(path, config, error), qPrintable(error));
    QCOMPARE(config.serverUrl, QUrl(QStringLiteral("https://exam.example.org")));
    QCOMPARE(config.slid, 10003);
    QCOMPARE(config.appPath, QStringLiteral("C:\\软件\\terminal.exe"));

    const QByteArray legacyOptions = "examAccount=School\\Exam01\r\n[mode]\r\nsample=1\r\n";
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Append));
    QCOMPARE(file.write(legacyOptions), static_cast<qint64>(legacyOptions.size()));
    file.close();
    Config legacyConfig;
    QVERIFY2(Config::load(path, legacyConfig, error), qPrintable(error));
    QCOMPARE(legacyConfig.serverUrl, config.serverUrl);
    QCOMPARE(legacyConfig.slid, config.slid);
    QCOMPARE(legacyConfig.appPath, config.appPath);
}

void TerminalServiceTests::validateServerUrl_data()
{
    QTest::addColumn<QString>("url");
    QTest::addColumn<bool>("valid");
    QTest::newRow("HTTP") << QStringLiteral("http://exam.example.org:8080/um/") << true;
    QTest::newRow("HTTPS") << QStringLiteral("https://exam.example.org") << true;
    QTest::newRow("混合大小写 HTTP") << QStringLiteral("HtTp://exam.example.org") << true;
    QTest::newRow("混合大小写 HTTPS") << QStringLiteral("HtTpS://exam.example.org") << true;
    QTest::newRow("FTP") << QStringLiteral("ftp://exam.example.org") << false;
    QTest::newRow("缺少主机名") << QStringLiteral("http:///um") << false;
}

void TerminalServiceTests::validateServerUrl()
{
    QFETCH(QString, url);
    QFETCH(bool, valid);
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QFile file(dir.filePath(QStringLiteral("service.ini")));
    QVERIFY(file.open(QIODevice::WriteOnly));
    const QByteArray ini = QStringLiteral("[server]\r\nurl=%1\r\nslid=10003\r\n").arg(url).toUtf8();
    QCOMPARE(file.write(ini), static_cast<qint64>(ini.size()));
    file.close();

    Config config;
    QString error;
    QCOMPARE(Config::load(file.fileName(), config, error), valid);
    if (valid) {
        QCOMPARE(config.serverUrl, QUrl(url));
    }
}

void TerminalServiceTests::editServerSettings()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QFile file(dir.filePath(QStringLiteral("service.ini")));
    QVERIFY(file.open(QIODevice::WriteOnly));
    const QByteArray otherSettings = QStringLiteral("[app]\r\npath=\"C:\\软件\\terminal.exe\"\r\n; 安装路径\r\n"
                                                    "[mode]\r\nsample=1\r\n").toUtf8();
    const QByteArray ini = "[server]\r\nurl=http://old.example.org\r\n[SERVER]\r\nURL=invalid\r\n" + otherSettings;
    QCOMPARE(file.write(ini), static_cast<qint64>(ini.size()));
    file.close();

    QString url;
    QString school;
    QString error;
    QVERIFY2(Config::loadServerSettings(file.fileName(), url, school, error), qPrintable(error));
    QCOMPARE(url, QStringLiteral("invalid"));
    QVERIFY(school.isEmpty());

    QVERIFY(!Config::saveServerSettings(file.fileName(), QStringLiteral("ftp://example.org"), QStringLiteral("1"), error));
    QVERIFY(!Config::saveServerSettings(file.fileName(), QStringLiteral("https://example.org"), QStringLiteral("0"), error));
    QVERIFY(!Config::saveServerSettings(file.fileName(), QStringLiteral("https://example.org/path\nurl=http://localhost"),
                                        QStringLiteral("1"), error));
    QVERIFY(file.open(QIODevice::ReadOnly));
    QCOMPARE(file.readAll(), ini);
    file.close();

    const QString validUrl = QStringLiteral("https://exam.example.org:8443/um");
    QVERIFY2(Config::saveServerSettings(file.fileName(), validUrl, QStringLiteral("10003"), error), qPrintable(error));
    Config config;
    QVERIFY2(Config::load(file.fileName(), config, error), qPrintable(error));
    QCOMPARE(config.serverUrl, QUrl(validUrl));
    QCOMPARE(config.slid, 10003);
    QCOMPARE(config.appPath, QStringLiteral("C:\\软件\\terminal.exe"));
    QVERIFY(file.open(QIODevice::ReadOnly));
    QVERIFY(file.readAll().endsWith(otherSettings));
}

void TerminalServiceTests::persistState()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("state.json"));
    State state;
    QString error;
    QVERIFY2(State::load(path, state, error), qPrintable(error));
    QVERIFY(state.regToken.isEmpty());
    state.ensureRegToken();
    static const QRegularExpression uuid(QStringLiteral("\\A[0-9a-f]{8}(?:-[0-9a-f]{4}){3}-[0-9a-f]{12}\\z"));
    QVERIFY(uuid.match(state.regToken).hasMatch());
    state.terminalCode = QStringLiteral("012345678901");
    state.record({QStringLiteral("first"), false, QStringLiteral("指令已过期")}, 1763800000000LL);
    QVERIFY2(state.save(path, error), qPrintable(error));

    State loaded;
    QVERIFY2(State::load(path, loaded, error), qPrintable(error));
    loaded.ensureRegToken();
    QCOMPARE(loaded.regToken, state.regToken);
    QCOMPARE(loaded.terminalCode, state.terminalCode);
    QCOMPARE(loaded.processedCommandIds.size(), 1);
    QCOMPARE(loaded.processedCommandIds.first().id, QStringLiteral("first"));
    QCOMPARE(loaded.processedCommandIds.first().processedAt, 1763800000000LL);
    QCOMPARE(loaded.resultsForHeartbeat(), state.resultsForHeartbeat());

    loaded.terminalCode.clear();
    loaded.acknowledge(loaded.resultsForHeartbeat(), true);
    QVERIFY2(loaded.save(path, error), qPrintable(error));
    QVERIFY2(State::load(path, loaded, error), qPrintable(error));
    QVERIFY(loaded.terminalCode.isEmpty());
    QVERIFY(loaded.pendingResults.isEmpty());
    QCOMPARE(loaded.processedCommandIds.size(), 1);
}

void TerminalServiceTests::monitorRegistrationLifecycle()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    RegistrationMonitor monitor(directory.path());
    monitor.start();
    monitor.setSchoolInstance(12);
    monitor.setLocalIp(QStringLiteral("192.168.1.23"));
    monitor.beginRequest(true);
    const auto registering = RegistrationMonitor::readStatus(directory.path());
    QVERIFY(registering);
    QCOMPARE(registering->phase, RegistrationPhase::Registering);
    QCOMPARE(registering->schoolInstance, 12);
    QCOMPARE(registering->localIp, QStringLiteral("192.168.1.23"));

    monitor.registered(false);
    monitor.setNextAttempt(20000);
    QCOMPARE(RegistrationMonitor::readStatus(directory.path())->phase, RegistrationPhase::WaitingForBinding);
    monitor.registered(false);
    monitor.registered(true);
    monitor.setNextAttempt(0);
    const auto bound = RegistrationMonitor::readStatus(directory.path());
    QVERIFY(bound);
    QCOMPARE(bound->phase, RegistrationPhase::WaitingForHeartbeat);
    QVERIFY(!bound->heartbeatIsCurrent(QDateTime::currentMSecsSinceEpoch()));

    monitor.heartbeatReceived(QStringLiteral("023"));
    monitor.setNextAttempt(30000);
    const auto online = RegistrationMonitor::readStatus(directory.path());
    QVERIFY(online);
    QCOMPARE(online->phase, RegistrationPhase::Online);
    QCOMPARE(online->seat, QStringLiteral("023"));
    QVERIFY(online->isCurrent(online->pid, online->updatedAt));
    QVERIFY(!online->isCurrent(online->pid + 1, online->updatedAt));
    QVERIFY(!online->isCurrent(online->pid, online->updatedAt + 46000));
    QVERIFY(online->heartbeatIsCurrent(online->lastHeartbeat + 90000));
    QVERIFY(!online->heartbeatIsCurrent(online->lastHeartbeat + 90001));

    QVERIFY(monitor.failed(false, RegistrationProblem::Timeout));
    QVERIFY(!monitor.failed(false, RegistrationProblem::Timeout));
    monitor.setNextAttempt(30000);
    const auto failed = RegistrationMonitor::readStatus(directory.path());
    QVERIFY(failed && failed->recentFailure);
    QCOMPARE(failed->phase, RegistrationPhase::HeartbeatFailed);
    QCOMPARE(failed->recentFailure->count, 2);
    QVERIFY(failed->nextAttempt > failed->updatedAt);

    monitor.heartbeatReceived(std::nullopt);
    monitor.publish();
    monitor.codeInvalid();
    monitor.publish();
    const auto unbound = RegistrationMonitor::readStatus(directory.path());
    QVERIFY(unbound);
    QCOMPARE(unbound->phase, RegistrationPhase::Registering);
    QVERIFY(unbound->seat.isEmpty());

    const auto logs = RegistrationMonitor::readLogs(directory.path(), 0);
    QCOMPARE(logs.size(), 7);
    int failures = 0;
    int recoveries = 0;
    for (const auto &event : logs) {
        if (event.event == RegistrationEvent::HeartbeatFailed) {
            QCOMPARE(event.count, 2);
            ++failures;
        }
        if (event.event == RegistrationEvent::HeartbeatRecovered) {
            QCOMPARE(event.count, 2);
            ++recoveries;
        }
    }
    QCOMPARE(failures, 1);
    QCOMPARE(recoveries, 1);

    QFile snapshot(directory.filePath(QStringLiteral("status.json")));
    QVERIFY(snapshot.open(QIODevice::ReadOnly));
    const auto fields = QJsonDocument::fromJson(snapshot.readAll()).object();
    QVERIFY(!fields.contains(QStringLiteral("regToken")));
    QVERIFY(!fields.contains(QStringLiteral("terminalCode")));
}

void TerminalServiceTests::monitorPreservesFailureAfterRestart()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    {
        RegistrationMonitor monitor(directory.path());
        monitor.start();
        monitor.beginRequest(true);
        QVERIFY(monitor.failed(true, RegistrationProblem::Dns));
        QVERIFY(!monitor.failed(true, RegistrationProblem::Dns));
        monitor.setNextAttempt(30000);
    }
    QFile interrupted(directory.filePath(QStringLiteral("registration.log")));
    QVERIFY(interrupted.open(QIODevice::WriteOnly | QIODevice::Append));
    QCOMPARE(interrupted.write("{\"id\":"), qint64(6));
    interrupted.close();

    RegistrationMonitor monitor(directory.path());
    monitor.start();
    monitor.startupFailed(RegistrationProblem::Configuration);
    const auto logs = RegistrationMonitor::readLogs(directory.path(), 0);
    QCOMPARE(logs.size(), 4);
    for (const auto &event : logs) {
        if (event.event == RegistrationEvent::RegistrationFailed) {
            QCOMPARE(event.problem, RegistrationProblem::Dns);
            QCOMPARE(event.count, 2);
        }
    }
}

void TerminalServiceTests::monitorRotatesAndExpiresLogs()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString logPath = directory.filePath(QStringLiteral("registration.log"));
    QFile oldLog(logPath + QStringLiteral(".1"));
    QVERIFY(oldLog.open(QIODevice::WriteOnly));
    QVERIFY(oldLog.setFileTime(QDateTime::currentDateTimeUtc().addDays(-8), QFileDevice::FileModificationTime));
    oldLog.close();

    RegistrationMonitor monitor(directory.path());
    monitor.start();
    QVERIFY(!oldLog.exists());

    QFile active(logPath);
    QVERIFY(active.open(QIODevice::ReadWrite));
    QVERIFY(active.setFileTime(QDateTime::currentDateTimeUtc().addDays(-1), QFileDevice::FileModificationTime));
    active.close();
    monitor.stop();
    QVERIFY(oldLog.exists());

    QVERIFY(active.open(QIODevice::ReadWrite));
    QVERIFY(active.resize(5 * 1024 * 1024));
    active.close();
    for (int index = 0; index < 5; ++index) {
        monitor.stop();
        if (index < 4) {
            QVERIFY(active.open(QIODevice::ReadWrite));
            QVERIFY(active.resize(5 * 1024 * 1024));
            active.close();
        }
    }
    QCOMPARE(QDir(directory.path()).entryList({QStringLiteral("registration.log*")}, QDir::Files).size(), 5);
    QVERIFY(QFileInfo(logPath).size() < 5 * 1024 * 1024);
    QVERIFY(!QFile::exists(logPath + QStringLiteral(".5")));
    QVERIFY(RegistrationMonitor::readLogs(directory.path(), QDateTime::currentMSecsSinceEpoch() + 60000).isEmpty());
}

QTEST_GUILESS_MAIN(TerminalServiceTests)
#include "terminal_service_tests.moc"
