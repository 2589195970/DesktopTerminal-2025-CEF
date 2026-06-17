#ifndef DESKTOP_AUTH_MANAGER_H
#define DESKTOP_AUTH_MANAGER_H

#include <QDateTime>
#include <QMutex>
#include <QObject>
#include <QString>

class ConfigManager;
class Logger;

class DesktopAuthManager : public QObject
{
    Q_OBJECT

public:
    static DesktopAuthManager& instance();

    bool initialize(ConfigManager *configManager, Logger *logger);
    bool refreshIfNeeded();
    QString buildRequestToken() const;
    bool isReady() const;
    QString allowedHost() const;

private:
    explicit DesktopAuthManager(QObject *parent = nullptr);

    bool authenticate();
    QString resolveAuthEndpoint() const;

    ConfigManager *m_configManager;
    Logger *m_logger;
    QString m_clientId;
    QString m_clientSecret;
    QString m_authEndpoint;
    QString m_allowedHost;
    QString m_aesKey;
    QDateTime m_expireAt;
    mutable QMutex m_mutex;
};

#endif
