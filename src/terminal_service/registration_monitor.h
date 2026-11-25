#ifndef REGISTRATION_MONITOR_H
#define REGISTRATION_MONITOR_H

#include <QList>
#include <QString>

#include <optional>

namespace terminal {

enum class RegistrationPhase {
    Starting, Registering, WaitingForBinding, WaitingForHeartbeat, Online,
    RegistrationFailed, HeartbeatFailed, Stopped, StartupFailed
};

enum class RegistrationProblem {
    None, Configuration, State, Dns, Timeout, Tls, Connection, Http, Response
};

enum class RegistrationEvent {
    Started, Stopped, StartupFailed, WaitingForBinding, Bound, FirstHeartbeat,
    CodeInvalid, RegistrationFailed, HeartbeatFailed, RegistrationRecovered, HeartbeatRecovered
};

struct RegistrationLogEntry
{
    QString id;
    RegistrationEvent event = RegistrationEvent::Started;
    RegistrationProblem problem = RegistrationProblem::None;
    int httpStatus = 0;
    qint64 firstAt = 0;
    qint64 lastAt = 0;
    int count = 1;

    bool isFailure() const;
};

struct RegistrationStatus
{
    qint64 pid = 0;
    qint64 updatedAt = 0;
    int schoolInstance = 0;
    QString computerName;
    QString localIp;
    QString version;
    QString seat;
    qint64 lastRegistration = 0;
    qint64 lastHeartbeat = 0;
    qint64 nextAttempt = 0;
    RegistrationPhase phase = RegistrationPhase::Starting;
    std::optional<RegistrationLogEntry> recentFailure;

    bool isCurrent(qint64 servicePid, qint64 now) const;
    bool heartbeatIsCurrent(qint64 now) const;
};

class RegistrationMonitor
{
public:
    explicit RegistrationMonitor(const QString &directory);

    void start();
    void setSchoolInstance(int instance);
    void setLocalIp(const QString &ip);
    void beginRequest(bool registering);
    void registered(bool bound);
    void heartbeatReceived(const std::optional<QString> &seat);
    void codeInvalid();
    bool failed(bool registering, RegistrationProblem problem, int httpStatus = 0);
    void startupFailed(RegistrationProblem problem);
    void stop();
    void setNextAttempt(int delayMs);
    void publish();

    static std::optional<RegistrationStatus> readStatus(const QString &directory);
    static QList<RegistrationLogEntry> readLogs(const QString &directory, qint64 since);
    static void pruneServiceLog(const QString &directory);
    static bool appendServiceLog(const QString &directory, const QByteArray &line);

private:
    void record(RegistrationEvent event, RegistrationProblem problem = RegistrationProblem::None);
    void endFailure(bool recovered);
    bool append(const RegistrationLogEntry &event) const;

    QString m_directory;
    RegistrationStatus m_status;
    bool m_failureActive = false;
    bool m_waitingLogged = false;
    bool m_heartbeatConfirmed = false;
    qint64 m_nextCleanup = 0;
};

QString registrationProblemText(RegistrationProblem problem, int httpStatus = 0);
QString registrationEventText(RegistrationEvent event);

} // namespace terminal

#endif
