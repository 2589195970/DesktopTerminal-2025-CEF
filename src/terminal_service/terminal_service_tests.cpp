#include "terminal_core.h"

#include <QFile>
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
    void matchExamAccount();
    void readWindowsConfig();
    void validateServerUrl_data();
    void validateServerUrl();
    void persistState();
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
    const auto waiting = Protocol::parseHeartbeat(200, R"({"code":10200,"result":{"codeValid":true,"commands":[],"examSeatDisplay":{"state":"WAITING","seatNumber":"002","releaseId":"room-1","validForSeconds":20}}})");
    QVERIFY(waiting.valid);
    QCOMPARE(waiting.examSeatDisplay.value("state").toString(), QStringLiteral("WAITING"));
    QCOMPARE(waiting.examSeatDisplay.value("seatNumber").toString(), QStringLiteral("002"));

    const auto opened = Protocol::parseHeartbeat(200, R"({"code":10200,"result":{"codeValid":true,"commands":[],"examSeatDisplay":{"state":"LOGIN_OPEN","seatNumber":"002","releaseId":"room-1","validForSeconds":20}}})");
    QCOMPARE(opened.examSeatDisplay.value("state").toString(), QStringLiteral("LOGIN_OPEN"));

    const auto stale = Protocol::parseHeartbeat(200, R"({"code":10200,"result":{"codeValid":true,"commands":[],"examSeatDisplay":{"state":"LOGIN_OPEN","seatNumber":"002","releaseId":"room-1","validForSeconds":0}}})");
    QCOMPARE(stale.examSeatDisplay.value("state").toString(), QStringLiteral("SYNC"));

    const auto unused = Protocol::parseHeartbeat(200, R"({"code":10200,"result":{"codeValid":true,"commands":[],"examSeatDisplay":{"state":"UNUSED","releaseId":"room-1","validForSeconds":20}}})");
    QCOMPARE(unused.examSeatDisplay.value("state").toString(), QStringLiteral("UNUSED"));
    QVERIFY(!unused.examSeatDisplay.contains("seatNumber"));

    const auto ordinary = Protocol::parseHeartbeat(200, R"({"code":10200,"result":{"codeValid":true,"commands":[]}})");
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

void TerminalServiceTests::matchExamAccount()
{
    QVERIFY(examAccountMatches(QStringLiteral("SCHOOL\\EXAM01"), QStringLiteral("exam01"), QStringLiteral("School")));
    QVERIFY(examAccountMatches(QStringLiteral("Exam01"), QStringLiteral("exam01"), QStringLiteral("School")));
    QVERIFY(examAccountMatches({}, QStringLiteral("exam01"), {}));
    QVERIFY(!examAccountMatches(QStringLiteral("OTHER\\exam01"), QStringLiteral("exam01"), QStringLiteral("School")));
    QVERIFY(!examAccountMatches(QStringLiteral("exam02"), QStringLiteral("exam01"), QStringLiteral("School")));
    QVERIFY(!examAccountMatches({}, {}, {}));
}

void TerminalServiceTests::readWindowsConfig()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("service.ini"));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    const QByteArray ini = QStringLiteral("[server]\r\nurl=https://exam.example.org\r\nslid=10003\r\n"
                                          "[app]\r\npath=C:\\软件\\terminal.exe\r\nexamAccount=School\\考试账户\r\n").toUtf8();
    QCOMPARE(file.write(ini), static_cast<qint64>(ini.size()));
    file.close();
    Config config;
    QString error;
    QVERIFY2(Config::load(path, config, error), qPrintable(error));
    QCOMPARE(config.serverUrl, QUrl(QStringLiteral("https://exam.example.org")));
    QCOMPARE(config.slid, 10003);
    QCOMPARE(config.appPath, QStringLiteral("C:\\软件\\terminal.exe"));
    QCOMPARE(config.examAccount, QStringLiteral("School\\考试账户"));

    const QByteArray legacyMode = "[mode]\r\nsample=1\r\n";
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Append));
    QCOMPARE(file.write(legacyMode), static_cast<qint64>(legacyMode.size()));
    file.close();
    Config legacyConfig;
    QVERIFY2(Config::load(path, legacyConfig, error), qPrintable(error));
    QCOMPARE(legacyConfig.serverUrl, config.serverUrl);
    QCOMPARE(legacyConfig.slid, config.slid);
    QCOMPARE(legacyConfig.appPath, config.appPath);
    QCOMPARE(legacyConfig.examAccount, config.examAccount);
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

QTEST_GUILESS_MAIN(TerminalServiceTests)
#include "terminal_service_tests.moc"
