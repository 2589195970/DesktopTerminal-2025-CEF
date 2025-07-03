#include "network_checker.h"
#include "../config/config_manager.h"
#include "../logging/logger.h"

#include <QNetworkRequest>
#include <QNetworkProxy>
#include <QUrl>
#include <QHostInfo>
#include <QSslConfiguration>
#include <QNetworkInterface>

NetworkChecker::NetworkChecker(QObject *parent)
    : QObject(parent)
    , m_networkManager(nullptr)
    , m_currentReply(nullptr)
    , m_timeoutTimer(nullptr)
    , m_networkStatus(Unknown)
    , m_currentUrlIndex(0)
    , m_timeoutMs(10000)
    , m_checking(false)
    , m_hasInternet(false)
{
    m_networkManager = new QNetworkAccessManager(this);
    
    m_timeoutTimer = new QTimer(this);
    m_timeoutTimer->setSingleShot(true);
    connect(m_timeoutTimer, &QTimer::timeout, this, &NetworkChecker::onCheckTimeout);
    
    // 设置默认检测URL列表
    m_checkUrls << "https://www.baidu.com"
                << "https://www.qq.com"
                << "http://www.163.com"
                << "https://httpbin.org/get";
    
    detectNetworkConfiguration();
    
    Logger::instance().appEvent("NetworkChecker创建完成");
}

NetworkChecker::~NetworkChecker()
{
    stopCheck();
}

void NetworkChecker::startCheck(const QString& targetUrl, int timeoutMs)
{
    if (m_checking) {
        stopCheck();
    }
    
    m_checking = true;
    m_timeoutMs = timeoutMs;
    m_networkStatus = Unknown;
    m_errorDetails.clear();
    m_currentUrlIndex = 0;
    
    // 如果指定了目标URL，将其放在检测列表的首位
    if (!targetUrl.isEmpty()) {
        m_targetUrl = targetUrl;
        QStringList urls = m_checkUrls;
        urls.prepend(targetUrl);
        m_checkUrls = urls;
    } else {
        // 尝试从配置中获取目标URL
        ConfigManager& config = ConfigManager::instance();
        if (config.isLoaded()) {
            m_targetUrl = config.getUrl();
            if (!m_targetUrl.isEmpty()) {
                QStringList urls = m_checkUrls;
                urls.prepend(m_targetUrl);
                m_checkUrls = urls;
            }
        }
    }
    
    Logger::instance().appEvent(QString("开始网络检测，目标URL: %1").arg(m_targetUrl));
    emit checkProgress("正在检查网络连接...");
    
    // 首先检测本地网络配置
    detectNetworkConfiguration();
    
    // 开始URL检测
    checkNextUrl();
}

void NetworkChecker::stopCheck()
{
    if (!m_checking) {
        return;
    }
    
    m_checking = false;
    m_timeoutTimer->stop();
    
    if (m_currentReply) {
        m_currentReply->abort();
        m_currentReply->deleteLater();
        m_currentReply = nullptr;
    }
    
    Logger::instance().appEvent("网络检测已停止");
}

void NetworkChecker::setCheckUrls(const QStringList& urls)
{
    m_checkUrls = urls;
}

QString NetworkChecker::getStatusDescription() const
{
    switch (m_networkStatus) {
        case Connected:
            return "网络连接正常";
        case Disconnected:
            return "网络连接断开";
        case LimitedAccess:
            return "网络访问受限";
        case Timeout:
            return "连接超时";
        case DnsError:
            return "DNS解析失败";
        case ProxyError:
            return "代理配置错误";
        case SslError:
            return "SSL证书错误";
        case Unknown:
        default:
            return "网络状态未知";
    }
}

QString NetworkChecker::getNetworkConfigInfo() const
{
    QString info;
    info += QString("代理配置: %1\n").arg(m_proxyInfo);
    info += QString("DNS配置: %1\n").arg(m_dnsInfo);
    info += QString("网络接口状态: %1\n").arg(m_hasInternet ? "已连接" : "未连接");
    
    // 获取网络接口信息
    QList<QNetworkInterface> interfaces = QNetworkInterface::allInterfaces();
    info += "网络接口:\n";
    for (const QNetworkInterface& iface : interfaces) {
        if (iface.flags() & QNetworkInterface::IsUp && 
            iface.flags() & QNetworkInterface::IsRunning) {
            info += QString("  - %1 (%2)\n")
                .arg(iface.humanReadableName())
                .arg(iface.hardwareAddress());
        }
    }
    
    return info;
}

void NetworkChecker::checkNextUrl()
{
    if (m_currentUrlIndex >= m_checkUrls.size()) {
        // 所有URL都检测失败
        completeCheck(Disconnected, "无法连接到任何检测服务器");
        return;
    }
    
    QString url = m_checkUrls[m_currentUrlIndex];
    emit checkProgress(QString("正在检测连接: %1").arg(url));
    
    QNetworkRequest request(url);
    request.setRawHeader("User-Agent", "DesktopTerminal-CEF/1.0");
    request.setAttribute(QNetworkRequest::FollowRedirectsAttribute, true);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    
    // 设置超时
    request.setTransferTimeout(m_timeoutMs);
    
    m_currentReply = m_networkManager->get(request);
    
    connect(m_currentReply, &QNetworkReply::finished, 
            this, &NetworkChecker::onNetworkReplyFinished);
    connect(m_currentReply, QOverload<QNetworkReply::NetworkError>::of(&QNetworkReply::error),
            this, &NetworkChecker::onNetworkError);
    connect(m_currentReply, &QNetworkReply::sslErrors,
            this, &NetworkChecker::onSslErrors);
    
    // 启动超时计时器
    m_timeoutTimer->start(m_timeoutMs);
    
    Logger::instance().appEvent(QString("检测URL: %1").arg(url));
}

void NetworkChecker::onNetworkReplyFinished()
{
    if (!m_currentReply || !m_checking) {
        return;
    }
    
    m_timeoutTimer->stop();
    
    QNetworkReply::NetworkError error = m_currentReply->error();
    int httpStatus = m_currentReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    
    if (error == QNetworkReply::NoError && (httpStatus == 200 || httpStatus == 0)) {
        // 检测成功
        QString url = m_currentReply->url().toString();
        Logger::instance().appEvent(QString("网络检测成功: %1 (HTTP %2)").arg(url).arg(httpStatus));
        completeCheck(Connected, QString("网络连接正常，已成功连接到 %1").arg(url));
    } else {
        // 当前URL失败，尝试下一个
        QString errorMsg = getNetworkErrorDescription(error);
        Logger::instance().appEvent(QString("URL检测失败: %1 - %2 (HTTP %3)")
            .arg(m_currentReply->url().toString()).arg(errorMsg).arg(httpStatus));
        
        m_currentUrlIndex++;
        
        if (m_currentReply) {
            m_currentReply->deleteLater();
            m_currentReply = nullptr;
        }
        
        checkNextUrl();
    }
}

void NetworkChecker::onNetworkError(QNetworkReply::NetworkError error)
{
    if (!m_checking) {
        return;
    }
    
    QString errorDesc = getNetworkErrorDescription(error);
    Logger::instance().appEvent(QString("网络错误: %1").arg(errorDesc));
    
    // 根据错误类型设置状态
    NetworkStatus status;
    switch (error) {
        case QNetworkReply::HostNotFoundError:
        case QNetworkReply::TemporaryNetworkFailureError:
            status = DnsError;
            break;
        case QNetworkReply::TimeoutError:
            status = Timeout;
            break;
        case QNetworkReply::ProxyConnectionRefusedError:
        case QNetworkReply::ProxyConnectionClosedError:
        case QNetworkReply::ProxyNotFoundError:
        case QNetworkReply::ProxyTimeoutError:
        case QNetworkReply::ProxyAuthenticationRequiredError:
            status = ProxyError;
            break;
        default:
            status = Disconnected;
            break;
    }
    
    // 如果是严重错误，直接完成检测
    if (status == ProxyError || status == DnsError) {
        completeCheck(status, errorDesc);
        return;
    }
}

void NetworkChecker::onSslErrors(const QList<QSslError>& errors)
{
    if (!m_checking) {
        return;
    }
    
    QString errorDesc = getSslErrorDescription(errors);
    Logger::instance().appEvent(QString("SSL错误: %1").arg(errorDesc));
    completeCheck(SslError, errorDesc);
}

void NetworkChecker::onCheckTimeout()
{
    if (!m_checking) {
        return;
    }
    
    Logger::instance().appEvent("网络检测超时");
    
    if (m_currentReply) {
        m_currentReply->abort();
    }
    
    m_currentUrlIndex++;
    checkNextUrl();
}

void NetworkChecker::completeCheck(NetworkStatus status, const QString& details)
{
    if (!m_checking) {
        return;
    }
    
    m_checking = false;
    m_networkStatus = status;
    m_errorDetails = details;
    
    m_timeoutTimer->stop();
    
    if (m_currentReply) {
        m_currentReply->deleteLater();
        m_currentReply = nullptr;
    }
    
    Logger::instance().appEvent(QString("网络检测完成: %1 - %2")
        .arg(getStatusDescription()).arg(details));
    
    emit checkCompleted(status, details);
}

QString NetworkChecker::getNetworkErrorDescription(QNetworkReply::NetworkError error) const
{
    switch (error) {
        case QNetworkReply::ConnectionRefusedError:
            return "连接被拒绝";
        case QNetworkReply::RemoteHostClosedError:
            return "远程主机关闭连接";
        case QNetworkReply::HostNotFoundError:
            return "无法解析主机名";
        case QNetworkReply::TimeoutError:
            return "连接超时";
        case QNetworkReply::OperationCanceledError:
            return "操作被取消";
        case QNetworkReply::SslHandshakeFailedError:
            return "SSL握手失败";
        case QNetworkReply::TemporaryNetworkFailureError:
            return "临时网络故障";
        case QNetworkReply::NetworkSessionFailedError:
            return "网络会话失败";
        case QNetworkReply::BackgroundRequestNotAllowedError:
            return "后台请求不被允许";
        case QNetworkReply::ProxyConnectionRefusedError:
            return "代理连接被拒绝";
        case QNetworkReply::ProxyConnectionClosedError:
            return "代理连接被关闭";
        case QNetworkReply::ProxyNotFoundError:
            return "代理服务器未找到";
        case QNetworkReply::ProxyTimeoutError:
            return "代理超时";
        case QNetworkReply::ProxyAuthenticationRequiredError:
            return "代理需要身份验证";
        case QNetworkReply::ContentAccessDenied:
            return "内容访问被拒绝";
        case QNetworkReply::ContentOperationNotPermittedError:
            return "内容操作不被允许";
        case QNetworkReply::ContentNotFoundError:
            return "内容未找到 (404)";
        case QNetworkReply::AuthenticationRequiredError:
            return "需要身份验证";
        case QNetworkReply::ProtocolUnknownError:
            return "未知协议";
        case QNetworkReply::ProtocolInvalidOperationError:
            return "协议操作无效";
        case QNetworkReply::UnknownNetworkError:
            return "未知网络错误";
        case QNetworkReply::UnknownProxyError:
            return "未知代理错误";
        case QNetworkReply::UnknownContentError:
            return "未知内容错误";
        case QNetworkReply::ProtocolFailure:
            return "协议失败";
        default:
            return QString("网络错误 (代码: %1)").arg(static_cast<int>(error));
    }
}

QString NetworkChecker::getSslErrorDescription(const QList<QSslError>& errors) const
{
    QStringList errorStrings;
    for (const QSslError& error : errors) {
        switch (error.error()) {
            case QSslError::CertificateExpired:
                errorStrings << "证书已过期";
                break;
            case QSslError::CertificateNotYetValid:
                errorStrings << "证书尚未生效";
                break;
            case QSslError::SelfSignedCertificate:
                errorStrings << "自签名证书";
                break;
            case QSslError::HostNameMismatch:
                errorStrings << "主机名不匹配";
                break;
            case QSslError::UnableToGetLocalIssuerCertificate:
                errorStrings << "无法获取本地颁发者证书";
                break;
            default:
                errorStrings << error.errorString();
                break;
        }
    }
    return errorStrings.join("; ");
}

void NetworkChecker::detectNetworkConfiguration()
{
    // 检测代理配置
    QNetworkProxy proxy = QNetworkProxy::applicationProxy();
    if (proxy.type() == QNetworkProxy::NoProxy) {
        m_proxyInfo = "无代理";
    } else {
        m_proxyInfo = QString("%1:%2")
            .arg(proxy.hostName())
            .arg(proxy.port());
    }
    
    // 检测DNS配置（简化版）
    m_dnsInfo = "系统默认";
    
    // 检测网络接口状态
    m_hasInternet = false;
    QList<QNetworkInterface> interfaces = QNetworkInterface::allInterfaces();
    for (const QNetworkInterface& iface : interfaces) {
        if (iface.flags() & QNetworkInterface::IsUp && 
            iface.flags() & QNetworkInterface::IsRunning &&
            !(iface.flags() & QNetworkInterface::IsLoopBack)) {
            m_hasInternet = true;
            break;
        }
    }
    
    Logger::instance().appEvent(QString("网络配置检测: 代理=%1, DNS=%2, 接口=%3")
        .arg(m_proxyInfo).arg(m_dnsInfo).arg(m_hasInternet ? "可用" : "不可用"));
}