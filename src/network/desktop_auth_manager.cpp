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
    , m_allowedPort(-1)
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

    if (m_clientId.isEmpty() || m_clientSecret.isEmpty() || m_authEndpoint.isEmpty()) {
        if (m_logger) {
            m_logger->appEvent("桌面端认证未配置，跳过启动认证");
        }
        m_allowedHost = QUrl(configManager->getUrl()).host().toLower();
        m_allowedPort = QUrl(configManager->getUrl()).port(-1);
        return true;
    }

    return authenticate();
}

bool DesktopAuthManager::refreshIfNeeded()
{
    QMutexLocker locker(&m_mutex);
    if (m_sessionToken.isEmpty()) {
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
    return m_sessionToken;
}

bool DesktopAuthManager::isReady() const
{
    QMutexLocker locker(&m_mutex);
    return !m_sessionToken.isEmpty() && QDateTime::currentDateTimeUtc() < m_expireAt;
}

QString DesktopAuthManager::allowedHost() const
{
    return m_allowedHost;
}

int DesktopAuthManager::allowedPort() const
{
    return m_allowedPort;
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
    const int responseCode = root.value("code").toInt(-1);
    if (responseCode != 10200) {
        const QString message = root.value("message").toString();
        if (m_logger) {
            m_logger->errorEvent(QString("桌面端启动认证失败: code=%1, %2")
                                     .arg(responseCode)
                                     .arg(message.isEmpty() ? QStringLiteral("未知错误") : message));
        }
        return false;
    }

    const QJsonObject result = root.value("result").toObject();
    const QString sessionToken = result.value("aesKey").toString();
    const qint64 expiresIn = result.value("expiresIn").toVariant().toLongLong();
    if (sessionToken.isEmpty() || expiresIn <= 0) {
        if (m_logger) {
            m_logger->errorEvent("桌面端启动认证失败: 未返回有效会话令牌");
        }
        return false;
    }

    QMutexLocker locker(&m_mutex);
    m_sessionToken = sessionToken;
    m_expireAt = QDateTime::currentDateTimeUtc().addSecs(expiresIn);
    if (m_logger) {
        m_logger->appEvent("桌面端启动认证成功");
    }
    return true;
}

QString DesktopAuthManager::resolveAuthEndpoint()
{
    if (!m_configManager) {
        return QString();
    }

    // 优先级1: config.json 中显式配置了完整的 authEndpoint
    QString endpoint = m_configManager->getDesktopAuthEndpoint();
    if (!endpoint.isEmpty()) {
        QUrl endpointUrl(endpoint);
        m_allowedHost = endpointUrl.host().toLower();
        m_allowedPort = endpointUrl.port(-1);
        if (m_logger) {
            m_logger->appEvent(QString("使用显式配置的认证地址: %1").arg(endpoint));
        }
        return endpoint;
    }

    // 优先级2: config.json 中配置了 apiBaseUrl
    QString apiBaseUrl = m_configManager->getDesktopApiBaseUrl();
    if (!apiBaseUrl.isEmpty()) {
        QUrl apiUrl(apiBaseUrl);
        m_allowedHost = apiUrl.host().toLower();
        m_allowedPort = apiUrl.port(-1);
        QString resolved = apiBaseUrl;
        if (resolved.endsWith('/')) {
            resolved.chop(1);
        }
        resolved += "/api/desktop/auth";
        if (m_logger) {
            m_logger->appEvent(QString("使用配置的API基地址: %1").arg(resolved));
        }
        return resolved;
    }

    // 优先级3: 从前端 Config.json 自动发现后端 API 地址
    QString discoveredApiBase = fetchApiBaseUrlFromFrontendConfig();
    if (!discoveredApiBase.isEmpty()) {
        QUrl apiUrl(discoveredApiBase);
        m_allowedHost = apiUrl.host().toLower();
        m_allowedPort = apiUrl.port(-1);
        if (discoveredApiBase.endsWith('/')) {
            discoveredApiBase.chop(1);
        }
        QString resolved = discoveredApiBase + "/api/desktop/auth";
        if (m_logger) {
            m_logger->appEvent(QString("从前端Config.json发现API地址: %1").arg(resolved));
        }
        return resolved;
    }

    // 优先级4: 从 url 字段推导（仅适用于前后端同域部署）
    QUrl baseUrl(m_configManager->getUrl());
    if (baseUrl.scheme().isEmpty() || baseUrl.host().isEmpty()) {
        return QString();
    }
    m_allowedHost = baseUrl.host().toLower();
    m_allowedPort = baseUrl.port(-1);
    QString fallback = baseUrl.scheme() + "://" + baseUrl.host()
            + (baseUrl.port() > 0 ? ":" + QString::number(baseUrl.port()) : QString())
            + "/api/desktop/auth";
    if (m_logger) {
        m_logger->appEvent(QString("使用前端URL推导认证地址(可能不正确): %1").arg(fallback));
    }
    return fallback;
}

QString DesktopAuthManager::buildFrontendConfigUrl() const
{
    if (!m_configManager) {
        return QString();
    }
    QUrl frontendUrl(m_configManager->getUrl());
    if (frontendUrl.scheme().isEmpty() || frontendUrl.host().isEmpty()) {
        return QString();
    }

    // 从 url 提取前端基路径，例如:
    // "http://test.sdzdf.com:8011/stu?Client='ExamTerminal'" -> "http://test.sdzdf.com:8011/stu/"
    QString path = frontendUrl.path();
    if (path.isEmpty() || path == "/") {
        path = "/";
    } else if (!path.endsWith('/')) {
        path += "/";
    }

    return frontendUrl.scheme() + "://" + frontendUrl.host()
            + (frontendUrl.port() > 0 ? ":" + QString::number(frontendUrl.port()) : QString())
            + path + "static/config/Config.json";
}

QString DesktopAuthManager::fetchApiBaseUrlFromFrontendConfig()
{
    QString configUrl = buildFrontendConfigUrl();
    if (configUrl.isEmpty()) {
        return QString();
    }

    if (m_logger) {
        m_logger->appEvent(QString("尝试从前端获取API配置: %1").arg(configUrl));
    }

    QNetworkAccessManager networkManager;
    QUrl fetchUrl(configUrl);
    QNetworkRequest request(fetchUrl);

    QNetworkReply *reply = networkManager.get(request);
    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    timer.start(10000);
    loop.exec();

    if (!reply->isFinished() || reply->error() != QNetworkReply::NoError) {
        if (m_logger) {
            m_logger->appEvent(QString("获取前端Config.json失败: %1").arg(reply->errorString()));
        }
        reply->deleteLater();
        return QString();
    }

    const QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
    reply->deleteLater();
    if (!doc.isObject()) {
        if (m_logger) {
            m_logger->appEvent("前端Config.json格式无效");
        }
        return QString();
    }

    QString apiUrl = doc.object().value("apiUrl").toString();
    if (apiUrl.isEmpty()) {
        if (m_logger) {
            m_logger->appEvent("前端Config.json中未找到apiUrl字段");
        }
        return QString();
    }

    if (m_logger) {
        m_logger->appEvent(QString("从前端Config.json读取到apiUrl: %1").arg(apiUrl));
    }
    return apiUrl;
}
