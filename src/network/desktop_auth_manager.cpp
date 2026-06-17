#include "desktop_auth_manager.h"

#include "../config/config_manager.h"
#include "../logging/logger.h"
#include "../security/desktop_crypto.h"

#include <QCoreApplication>
#include <QEventLoop>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>
#include <QUrl>

DesktopAuthManager::DesktopAuthManager(QObject *parent)
    : QObject(parent)
    , m_configManager(nullptr)
    , m_logger(nullptr)
{
}

DesktopAuthManager& DesktopAuthManager::instance()
{
    static DesktopAuthManager manager;
    return manager;
}

bool DesktopAuthManager::initialize(ConfigManager *configManager, Logger *logger)
{
    m_configManager = configManager;
    m_logger = logger;
    m_clientId = configManager->getDesktopClientId();
    m_clientSecret = configManager->getDesktopClientSecret();
    m_authEndpoint = resolveAuthEndpoint();
    m_allowedHost = QUrl(configManager->getUrl()).host().toLower();

    if (m_clientId.isEmpty() || m_clientSecret.isEmpty() || m_authEndpoint.isEmpty()) {
        if (m_logger) {
            m_logger->appEvent("桌面端认证未配置，跳过启动认证");
        }
        return true;
    }

    return authenticate();
}

bool DesktopAuthManager::refreshIfNeeded()
{
    QMutexLocker locker(&m_mutex);
    if (m_aesKey.isEmpty()) {
        locker.unlock();
        return authenticate();
    }
    if (QDateTime::currentDateTimeUtc().secsTo(m_expireAt) > 300) {
        return true;
    }
    locker.unlock();
    return authenticate();
}

QString DesktopAuthManager::buildRequestToken() const
{
    QMutexLocker locker(&m_mutex);
    if (m_clientId.isEmpty() || m_aesKey.isEmpty()) {
        return QString();
    }

    const qint64 timestamp = QDateTime::currentMSecsSinceEpoch();
    const QString plainText = m_clientId + "|" + QString::number(timestamp);
    const QString cipherText = DesktopCrypto::encryptAesCbcBase64(plainText, m_aesKey);
    if (cipherText.isEmpty()) {
        return QString();
    }
    return m_clientId + "." + cipherText;
}

bool DesktopAuthManager::isReady() const
{
    QMutexLocker locker(&m_mutex);
    return !m_aesKey.isEmpty() && QDateTime::currentDateTimeUtc() < m_expireAt;
}

QString DesktopAuthManager::allowedHost() const
{
    return m_allowedHost;
}

bool DesktopAuthManager::authenticate()
{
    const qint64 timestamp = QDateTime::currentMSecsSinceEpoch();
    const QString appVersion = QCoreApplication::applicationVersion();
    const QString payload = m_clientId + "|" + appVersion + "|" + QString::number(timestamp);
    const QString sign = DesktopCrypto::hmacSha256Hex(payload, m_clientSecret);

    QJsonObject requestBody;
    requestBody.insert("clientId", m_clientId);
    requestBody.insert("appVersion", appVersion);
    requestBody.insert("timestamp", timestamp);
    requestBody.insert("sign", sign);

    QNetworkAccessManager networkManager;
    QUrl authUrl(m_authEndpoint);
    QNetworkRequest request(authUrl);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QNetworkReply *reply = networkManager.post(request, QJsonDocument(requestBody).toJson(QJsonDocument::Compact));
    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    timer.start(15000);
    loop.exec();

    if (!reply->isFinished() || reply->error() != QNetworkReply::NoError) {
        if (m_logger) {
            m_logger->errorEvent(QString("桌面端启动认证失败: %1").arg(reply->errorString()));
        }
        reply->deleteLater();
        return false;
    }

    const QJsonDocument responseDoc = QJsonDocument::fromJson(reply->readAll());
    reply->deleteLater();
    if (!responseDoc.isObject()) {
        if (m_logger) {
            m_logger->errorEvent("桌面端启动认证失败: 响应格式无效");
        }
        return false;
    }

    const QJsonObject root = responseDoc.object();
    const QJsonObject result = root.value("result").toObject();
    const QString aesKey = result.value("aesKey").toString();
    const qint64 expiresIn = result.value("expiresIn").toVariant().toLongLong();
    if (aesKey.isEmpty() || expiresIn <= 0) {
        if (m_logger) {
            m_logger->errorEvent("桌面端启动认证失败: 未返回有效密钥");
        }
        return false;
    }

    QMutexLocker locker(&m_mutex);
    m_aesKey = aesKey;
    m_expireAt = QDateTime::currentDateTimeUtc().addSecs(expiresIn);
    if (m_logger) {
        m_logger->appEvent("桌面端启动认证成功");
    }
    return true;
}

QString DesktopAuthManager::resolveAuthEndpoint() const
{
    if (!m_configManager) {
        return QString();
    }

    QString endpoint = m_configManager->getDesktopAuthEndpoint();
    if (!endpoint.isEmpty()) {
        return endpoint;
    }

    QUrl baseUrl(m_configManager->getUrl());
    if (baseUrl.scheme().isEmpty() || baseUrl.host().isEmpty()) {
        return QString();
    }
    return baseUrl.scheme() + "://" + baseUrl.host()
            + (baseUrl.port() > 0 ? ":" + QString::number(baseUrl.port()) : QString())
            + "/api/desktop/auth";
}
