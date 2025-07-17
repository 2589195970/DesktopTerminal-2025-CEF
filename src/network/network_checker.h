#ifndef NETWORK_CHECKER_H
#define NETWORK_CHECKER_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QTimer>
#include <QStringList>

/**
 * @brief 网络连接状态检测器
 * 
 * 检测网络连接状态，验证目标URL的可达性，
 * 支持多个备用检测URL和超时处理
 */
class NetworkChecker : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief 网络状态枚举
     */
    enum NetworkStatus {
        Unknown,        // 未知状态
        Connected,      // 网络连接正常
        Disconnected,   // 网络断开
        LimitedAccess,  // 网络受限（可以连接但访问受限）
        Timeout,        // 连接超时
        DnsError,       // DNS解析错误
        ProxyError,     // 代理配置错误
        SslError        // SSL证书错误
    };

    explicit NetworkChecker(QObject *parent = nullptr);
    ~NetworkChecker() override;

    /**
     * @brief 开始网络检测
     * @param targetUrl 目标URL（可选，使用配置中的URL）
     * @param timeoutMs 超时时间（毫秒）
     */
    void startCheck(const QString& targetUrl = QString(), int timeoutMs = 10000);

    /**
     * @brief 停止网络检测
     */
    void stopCheck();

    /**
     * @brief 获取当前网络状态
     */
    NetworkStatus getNetworkStatus() const { return m_networkStatus; }

    /**
     * @brief 获取详细错误信息
     */
    QString getErrorDetails() const { return m_errorDetails; }

    /**
     * @brief 获取网络状态描述
     */
    QString getStatusDescription() const;

    /**
     * @brief 检查是否正在检测
     */
    bool isChecking() const { return m_checking; }

    /**
     * @brief 设置检测URL列表
     * @param urls 检测URL列表
     */
    void setCheckUrls(const QStringList& urls);

    /**
     * @brief 获取网络配置信息
     */
    QString getNetworkConfigInfo() const;

signals:
    /**
     * @brief 网络检测完成
     * @param status 网络状态
     * @param details 详细信息
     */
    void checkCompleted(NetworkStatus status, const QString& details);

    /**
     * @brief 检测进度更新
     * @param message 当前检测的描述
     */
    void checkProgress(const QString& message);

private slots:
    void onNetworkReplyFinished();
    void onNetworkError(QNetworkReply::NetworkError error);
    void onSslErrors(const QList<QSslError>& errors);
    void onCheckTimeout();

private:
    void checkNextUrl();
    void completeCheck(NetworkStatus status, const QString& details);
    QString getNetworkErrorDescription(QNetworkReply::NetworkError error) const;
    QString getSslErrorDescription(const QList<QSslError>& errors) const;
    void detectNetworkConfiguration();

private:
    QNetworkAccessManager* m_networkManager;
    QNetworkReply* m_currentReply;
    QTimer* m_timeoutTimer;

    NetworkStatus m_networkStatus;
    QString m_errorDetails;
    QString m_targetUrl;
    QStringList m_checkUrls;
    int m_currentUrlIndex;
    int m_timeoutMs;
    bool m_checking;

    // 网络配置信息
    QString m_proxyInfo;
    QString m_dnsInfo;
    bool m_hasInternet;
};

#endif // NETWORK_CHECKER_H