#include "security_controller.h"
#include "../logging/logger.h"
#include "../config/config_manager.h"

#include <QApplication>
#include <QMessageBox>
#include <QRegExp>
#include <QUrlQuery>

SecurityController::SecurityController(QObject *parent)
    : QObject(parent)
    , m_logger(&Logger::instance())
    , m_configManager(&ConfigManager::instance())
    , m_strictMode(true)
    , m_urlFilterEnabled(true)
    , m_violationCount(0)
    , m_securityTimer(nullptr)
    , m_totalUrlChecks(0)
    , m_blockedUrlCount(0)
{
    m_logger->appEvent("SecurityController创建");
}

SecurityController::~SecurityController()
{
    if (m_securityTimer) {
        m_securityTimer->stop();
        delete m_securityTimer;
    }
    m_logger->appEvent("SecurityController销毁");
}

bool SecurityController::initialize()
{
    m_logger->appEvent("SecurityController初始化开始");
    
    // 加载配置
    m_strictMode = m_configManager->isStrictSecurityMode();
    m_urlFilterEnabled = true; // 始终启用URL过滤
    
    // 初始化URL过滤器
    initializeUrlFilters();
    
    // 初始化安全定时器
    initializeSecurityTimer();
    
    // 加载安全规则
    loadSecurityRules();
    
    m_logger->appEvent("SecurityController初始化完成");
    return true;
}

bool SecurityController::isUrlAllowed(const QUrl& url)
{
    m_totalUrlChecks++;
    
    if (!m_urlFilterEnabled) {
        return true;
    }
    
    QString urlString = url.toString();
    QString host = url.host().toLower();
    
    m_logger->logEvent("URL检查", QString("检查URL: %1").arg(urlString), "security.log", L_DEBUG);
    
    // 检查是否为本地URL或数据URL
    if (url.scheme() == "data" || url.scheme() == "about" || 
        url.scheme() == "chrome" || url.scheme() == "devtools") {
        return true;
    }
    
    // 检查基础域名（从配置获取的主要URL）
    QUrl baseUrl(m_configManager->getUrl());
    QString baseDomain = baseUrl.host().toLower();
    
    if (host == baseDomain || host.endsWith("." + baseDomain)) {
        return true;
    }
    
    // 检查允许的域名列表
    for (const QString& allowedDomain : m_allowedDomains) {
        if (host == allowedDomain.toLower() || host.endsWith("." + allowedDomain.toLower())) {
            return true;
        }
    }
    
    // 检查被阻止的域名列表
    for (const QString& blockedDomain : m_blockedDomains) {
        if (host == blockedDomain.toLower() || host.endsWith("." + blockedDomain.toLower())) {
            handleSecurityViolation(UnauthorizedURL, QString("访问被禁止的域名: %1").arg(host));
            return false;
        }
    }
    
    // 检查URL模式匹配
    for (const QString& pattern : m_allowedUrlPatterns) {
        if (matchesPattern(urlString, pattern)) {
            return true;
        }
    }
    
    for (const QString& pattern : m_blockedUrlPatterns) {
        if (matchesPattern(urlString, pattern)) {
            handleSecurityViolation(UnauthorizedURL, QString("URL匹配被禁止的模式: %1").arg(pattern));
            return false;
        }
    }
    
    // 默认策略：严格模式下阻止未明确允许的URL
    if (m_strictMode) {
        handleSecurityViolation(UnauthorizedURL, QString("严格模式下未授权的URL: %1").arg(urlString));
        return false;
    }
    
    return true;
}

bool SecurityController::requiresSpecialHandling(const QUrl& url)
{
    QString urlString = url.toString();
    
    // 检查是否为下载链接
    if (urlString.contains(QRegExp("\\.(exe|msi|zip|rar|7z|tar|gz|pdf|doc|docx|xls|xlsx|ppt|pptx)$", Qt::CaseInsensitive))) {
        return true;
    }
    
    // 检查是否为外部链接
    if (url.scheme() == "ftp" || url.scheme() == "mailto" || url.scheme() == "tel") {
        return true;
    }
    
    // 检查是否为特殊协议
    if (!url.scheme().startsWith("http") && url.scheme() != "data" && url.scheme() != "about") {
        return true;
    }
    
    return false;
}

void SecurityController::handleSecurityViolation(SecurityViolationType violation, const QString& description)
{
    m_violationCount++;
    m_blockedUrlCount++;
    
    QString violationDesc = getViolationDescription(violation);
    QString fullDescription = QString("%1: %2").arg(violationDesc).arg(description);
    
    logSecurityEvent("安全违规", fullDescription);
    
    emit securityViolationDetected(violation, fullDescription);
    
    if (m_strictMode) {
        QMessageBox::warning(nullptr, "安全警告", 
            QString("检测到安全违规行为：\\n%1\\n\\n此行为已被记录。").arg(fullDescription));
    }
}

bool SecurityController::isSafeExternalUrl(const QUrl& url)
{
    QString host = url.host().toLower();
    
    // 安全的外部域名白名单
    QStringList safeDomains = {
        "www.google.com",
        "www.baidu.com",
        "www.bing.com",
        "github.com",
        "stackoverflow.com",
        "developer.mozilla.org"
    };
    
    for (const QString& safeDomain : safeDomains) {
        if (host == safeDomain || host.endsWith("." + safeDomain)) {
            return true;
        }
    }
    
    return false;
}

int SecurityController::getViolationCount() const
{
    return m_violationCount;
}

void SecurityController::resetViolationCount()
{
    m_violationCount = 0;
    m_blockedUrlCount = 0;
    m_totalUrlChecks = 0;
    m_logger->appEvent("安全违规计数已重置");
}

void SecurityController::setStrictMode(bool enabled)
{
    m_strictMode = enabled;
    m_logger->appEvent(QString("严格安全模式: %1").arg(enabled ? "启用" : "禁用"));
}

bool SecurityController::isStrictModeEnabled() const
{
    return m_strictMode;
}

void SecurityController::updateSecurityPolicy()
{
    loadSecurityRules();
    m_logger->appEvent("安全策略已更新");
}

void SecurityController::performSecurityCheck()
{
    // 定期安全检查
    if (m_violationCount > 10) {
        m_logger->logEvent("安全警告", 
            QString("违规次数过多: %1次").arg(m_violationCount), 
            "security.log", L_WARNING);
    }
    
    // 记录统计信息
    if (m_totalUrlChecks > 0) {
        double blockRate = (double)m_blockedUrlCount / m_totalUrlChecks * 100.0;
        m_logger->logEvent("安全统计", 
            QString("总检查: %1, 阻止: %2, 阻止率: %3%")
                .arg(m_totalUrlChecks)
                .arg(m_blockedUrlCount)
                .arg(blockRate, 0, 'f', 1),
            "security.log", L_DEBUG);
    }
}

void SecurityController::initializeUrlFilters()
{
    // 获取基础URL配置
    QUrl baseUrl(m_configManager->getUrl());
    m_baseDomain = baseUrl.host().toLower();
    
    // 默认允许的域名（可以从配置文件扩展）
    m_allowedDomains.clear();
    m_allowedDomains << m_baseDomain;
    
    // 常见的安全资源域名
    m_allowedDomains << "cdn.jsdelivr.net"
                     << "cdnjs.cloudflare.com"
                     << "fonts.googleapis.com"
                     << "fonts.gstatic.com";
    
    // 默认阻止的域名
    m_blockedDomains.clear();
    m_blockedDomains << "malware.com"
                     << "phishing.com"
                     << "dangerous.site";
    
    // URL模式（可以使用通配符）
    m_allowedUrlPatterns.clear();
    m_allowedUrlPatterns << QString("https://%1/*").arg(m_baseDomain)
                         << QString("http://%1/*").arg(m_baseDomain)
                         << "data:*"
                         << "about:*";
    
    m_blockedUrlPatterns.clear();
    m_blockedUrlPatterns << "*.exe"
                         << "*.msi"
                         << "javascript:*"
                         << "vbscript:*";
    
    m_logger->appEvent(QString("URL过滤器初始化完成，基础域名: %1").arg(m_baseDomain));
}

void SecurityController::initializeSecurityTimer()
{
    m_securityTimer = new QTimer(this);
    connect(m_securityTimer, &QTimer::timeout, this, &SecurityController::performSecurityCheck);
    m_securityTimer->start(30000); // 每30秒检查一次
    
    m_logger->appEvent("安全检查定时器启动");
}

void SecurityController::loadSecurityRules()
{
    // 从配置管理器加载额外的安全规则
    // 这里可以扩展从配置文件加载自定义规则
    
    m_logger->appEvent("安全规则加载完成");
}

bool SecurityController::matchesPattern(const QString& text, const QString& pattern)
{
    // 简单的通配符匹配
    QRegExp regex(pattern);
    regex.setPatternSyntax(QRegExp::Wildcard);
    return regex.exactMatch(text);
}

QString SecurityController::getViolationDescription(SecurityViolationType type)
{
    switch (type) {
        case UnauthorizedURL:
            return "未授权URL访问";
        case ForbiddenKeyboard:
            return "禁止的键盘操作";
        case WindowManipulation:
            return "窗口操作违规";
        case ProcessViolation:
            return "进程安全违规";
        default:
            return "未知安全违规";
    }
}

void SecurityController::logSecurityEvent(const QString& event, const QString& details)
{
    m_logger->logEvent(event, details, "security.log", L_WARNING);
}