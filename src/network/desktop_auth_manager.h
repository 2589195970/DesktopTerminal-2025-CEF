#ifndef DESKTOP_AUTH_MANAGER_H
#define DESKTOP_AUTH_MANAGER_H

#include <QDateTime>
#include <QMutex>
#include <QObject>
#include <QString>
#include <QTimer>

class ConfigManager;
class Logger;

class DesktopAuthManager : public QObject
{
    Q_OBJECT

public:
    static DesktopAuthManager& instance();

    bool initialize(ConfigManager *configManager, Logger *logger);
    void startAsyncAuth();
    bool refreshIfNeeded();
    QString buildRequestToken() const;
    bool isReady() const;
    QString allowedHost() const;
    int allowedPort() const;
    QString lastError() const;

signals:
    void authCompleted(bool success);

private slots:
    void onRetryTimeout();

private:
    explicit DesktopAuthManager(QObject *parent = nullptr);

    bool authenticate();
    QString resolveAuthEndpoint();
    QString fetchApiBaseUrlFromFrontendConfig();
    QString buildFrontendConfigUrl() const;
    void scheduleRetry();

    static const int MAX_RETRY_COUNT = 5;
    static const int BASE_RETRY_INTERVAL_SEC = 10;

    ConfigManager *m_configManager;
    Logger *m_logger;
    QString m_clientId;
    QString m_clientSecret;
    QString m_authEndpoint;
    QString m_allowedHost;
    int m_allowedPort;
    QString m_sessionToken;
    QDateTime m_expireAt;
    QString m_lastError;
    int m_retryCount;
    QTimer m_retryTimer;
    mutable QMutex m_mutex;
};

#endif
