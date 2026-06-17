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
#include <QSslSocket>
#include <QtConcurrent/QtConcurrentRun>

namespace {

QString buildReplyDebugContext(QNetworkReply *reply)
{
    if (!reply) {
        return QString();
    }

    QStringList parts;
    const int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (httpStatus > 0) {
        parts << QString("HTTP %1").arg(httpStatus);
    }

    const QByteArray requestIdHeader = reply->rawHeader("requestId");
    if (!requestIdHeader.isEmpty()) {
        parts << QString("requestId=%1").arg(QString::fromUtf8(requestIdHeader));
    }

    const QByteArray traceIdHeader = reply->rawHeader("traceId");
    if (!traceIdHeader.isEmpty()) {
        parts << QString("traceId=%1").arg(QString::fromUtf8(traceIdHeader));
    }

    return parts.join(" | ");
}

QString extractServerErrorMessage(const QByteArray &body)
{
    if (body.isEmpty()) {
        return QString();
    }

    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(body, &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        QString plainText = QString::fromUtf8(body).trimmed();
        if (plainText.length() > 500) {
            plainText = plainText.left(500) + "...";
        }
        return plainText;
    }

    const QJsonObject root = doc.object();
    QStringList parts;
    const int code = root.value("code").toInt(-1);
    if (code >= 0) {
        parts << QString("code=%1").arg(code);
    }

    const QString message = root.value("message").toString().trimmed();
    if (!message.isEmpty()) {
        parts << message;
    }

    const QString error = root.value("error").toString().trimmed();
    if (!error.isEmpty() && error != message) {
        parts << error;
    }

    return parts.join(" | ");
}

QString simplifyNetworkError(QNetworkReply *reply)
{
    if (!reply) {
        return QStringLiteral("未知网络错误");
    }

    const int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QString errorText = reply->errorString().trimmed();
    const QString serverMessage = extractServerErrorMessage(reply->readAll());
    const QString debugContext = buildReplyDebugContext(reply);

    QStringList lines;
    lines << QString("认证接口: %1").arg(reply->url().toString());

    if (httpStatus > 0) {
        lines << QString("HTTP状态: %1").arg(httpStatus);
    }

    if (!serverMessage.isEmpty()) {
        lines << QString("服务端返回: %1").arg(serverMessage);
    } else if (!errorText.isEmpty()) {
        lines << QString("网络层返回: %1").arg(errorText);
    } else {
        lines << QStringLiteral("网络层返回: 未知错误");
    }

    if (!debugContext.isEmpty()) {
        lines << QString("调试信息: %1").arg(debugContext);
    }

    return lines.join("\n");
}

QString simplifyLogicalError(const QString &endpoint, int responseCode, const QString &message, const QJsonObject &root)
{
    QStringList lines;
    lines << QString("认证接口: %1").arg(endpoint);
    lines << QString("返回码: %1").arg(responseCode);
    lines << QString("服务端返回: %1").arg(message.isEmpty() ? QStringLiteral("未知错误") : message);

    const QString traceId = root.value("traceId").toString().trimmed();
    if (!traceId.isEmpty()) {
        lines << QString("traceId: %1").arg(traceId);
    }

    const QString requestId = root.value("requestId").toString().trimmed();
    if (!requestId.isEmpty()) {
        lines << QString("requestId: %1").arg(requestId);
    }

    return lines.join("\n");
}

}

DesktopAuthManager::DesktopAuthManager(QObject *parent)
    : QObject(parent)
    , m_configManager(nullptr)
    , m_logger(nullptr)
    , m_allowedPort(-1)
    , m_retryCount(0)
{
    m_retryTimer.setSingleShot(true);
    connect(&m_retryTimer, &QTimer::timeout, this, &DesktopAuthManager::onRetryTimeout);
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

    if (!QSslSocket::supportsSsl()) {
        if (m_logger) {
            m_logger->errorEvent(QString("TLS 不可用: %1").arg(QSslSocket::sslLibraryBuildVersionString()));
            m_logger->errorEvent("Qt 网络 HTTPS 需要 OpenSSL 运行时(libssl/libcrypto)，请确认与程序同目录已部署");
        }
    } else if (m_logger) {
        m_logger->appEvent(QString("TLS 已就绪: %1").arg(QSslSocket::sslLibraryVersionString()));
    }

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

    if (m_logger) {
        m_logger->appEvent(QString("桌面端认证已配置，将在后台异步获取会话令牌 (endpoint=%1)").arg(m_authEndpoint));
    }
    return true;
}

void DesktopAuthManager::startAsyncAuth()
{
    if (m_clientId.isEmpty() || m_clientSecret.isEmpty() || m_authEndpoint.isEmpty()) {
        return;
    }

    m_retryCount = 0;
    QtConcurrent::run([this]() {
        bool ok = authenticate();
        QMetaObject::invokeMethod(this, [this, ok]() {
            if (ok) {
                if (m_logger) {
                    m_logger->appEvent("后台认证成功，会话令牌已就绪");
                }
                emit authCompleted(true);
            } else {
                scheduleRetry();
            }
        }, Qt::QueuedConnection);
    });
}

void DesktopAuthManager::scheduleRetry()
{
    m_retryCount++;
    if (m_retryCount > MAX_RETRY_COUNT) {
        if (m_logger) {
            m_logger->errorEvent(QString("桌面端认证: 已达最大重试次数(%1)，放弃认证。程序可正常使用，但后端可能拒绝未认证请求")
                                     .arg(MAX_RETRY_COUNT));
        }
        emit authCompleted(false);
        return;
    }

    const int delaySec = BASE_RETRY_INTERVAL_SEC * m_retryCount;
    if (m_logger) {
        m_logger->appEvent(QString("桌面端认证: 第%1/%2次重试，%3秒后执行")
                               .arg(m_retryCount).arg(MAX_RETRY_COUNT).arg(delaySec));
    }
    m_retryTimer.start(delaySec * 1000);
}

void DesktopAuthManager::onRetryTimeout()
{
    QtConcurrent::run([this]() {
        bool ok = authenticate();
        QMetaObject::invokeMethod(this, [this, ok]() {
            if (ok) {
                if (m_logger) {
                    m_logger->appEvent(QString("桌面端认证: 第%1次重试成功").arg(m_retryCount));
                }
                emit authCompleted(true);
            } else {
                scheduleRetry();
            }
        }, Qt::QueuedConnection);
    });
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

QString DesktopAuthManager::lastError() const
{
    QMutexLocker locker(&m_mutex);
    return m_lastError;
}

bool DesktopAuthManager::authenticate()
{
    {
        QMutexLocker locker(&m_mutex);
        m_lastError.clear();
    }

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

    if (!reply->isFinished()) {
        const QString detail = QString("认证接口: %1\n网络层返回: 请求超时，15秒内未收到响应")
                                   .arg(authUrl.toString());
        if (m_logger) {
            m_logger->errorEvent(QString("桌面端启动认证失败\n%1").arg(detail));
        }
        {
            QMutexLocker locker(&m_mutex);
            m_lastError = detail;
        }
        reply->abort();
        reply->deleteLater();
        return false;
    }

    if (reply->error() != QNetworkReply::NoError) {
        const QString detail = simplifyNetworkError(reply);
        if (m_logger) {
            m_logger->errorEvent(QString("桌面端启动认证失败\n%1").arg(detail));
        }
        {
            QMutexLocker locker(&m_mutex);
            m_lastError = detail;
        }
        reply->deleteLater();
        return false;
    }

    const QByteArray responseBody = reply->readAll();
    const QJsonDocument responseDoc = QJsonDocument::fromJson(responseBody);
    reply->deleteLater();
    if (!responseDoc.isObject()) {
        const QString detail = QString("认证接口: %1\n服务端返回: 响应格式无效，无法解析为JSON")
                                   .arg(authUrl.toString());
        if (m_logger) {
            m_logger->errorEvent(QString("桌面端启动认证失败\n%1").arg(detail));
        }
        {
            QMutexLocker locker(&m_mutex);
            m_lastError = detail;
        }
        return false;
    }

    const QJsonObject root = responseDoc.object();
    const int responseCode = root.value("code").toInt(-1);
    if (responseCode != 10200) {
        const QString message = root.value("message").toString();
        const QString detail = simplifyLogicalError(authUrl.toString(), responseCode, message, root);
        if (m_logger) {
            m_logger->errorEvent(QString("桌面端启动认证失败\n%1").arg(detail));
        }
        {
            QMutexLocker locker(&m_mutex);
            m_lastError = detail;
        }
        return false;
    }

    const QJsonObject result = root.value("result").toObject();
    const QString sessionToken = result.value("aesKey").toString();
    const qint64 expiresIn = result.value("expiresIn").toVariant().toLongLong();
    if (sessionToken.isEmpty() || expiresIn <= 0) {
        const QString detail = QString("认证接口: %1\n服务端返回: 未返回有效会话令牌或过期时间")
                                   .arg(authUrl.toString());
        if (m_logger) {
            m_logger->errorEvent(QString("桌面端启动认证失败\n%1").arg(detail));
        }
        {
            QMutexLocker locker(&m_mutex);
            m_lastError = detail;
        }
        return false;
    }

    QMutexLocker locker(&m_mutex);
    m_sessionToken = sessionToken;
    m_expireAt = QDateTime::currentDateTimeUtc().addSecs(expiresIn);
    m_lastError.clear();
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
            if (!QSslSocket::supportsSsl()) {
                m_logger->appEvent("提示: 可在 config.json 的 desktopAuth.apiBaseUrl 中直接配置后端 API 基地址");
            }
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
