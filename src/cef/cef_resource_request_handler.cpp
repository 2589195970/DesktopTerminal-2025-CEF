#include "cef_resource_request_handler.h"
#include "../logging/logger.h"
#include "../config/config_manager.h"

#include <QUrl>
#include <QRegularExpression>

CEFResourceRequestHandler::CEFResourceRequestHandler()
    : m_logger(&Logger::instance())
    , m_configManager(&ConfigManager::instance())
    , m_strictSecurityMode(true)
    , m_urlDetectionEnabled(false)
    , m_blockedResourceCount(0)
    , m_allowedResourceCount(0)
    , m_cookieBlockCount(0)
{
    m_logger->appEvent("CEFResourceRequestHandler创建 - CEF 109架构");
    
    // 从配置中加载设置
    // TODO: 实现配置加载逻辑
}

CEFResourceRequestHandler::~CEFResourceRequestHandler()
{
    m_logger->appEvent(QString("CEFResourceRequestHandler销毁 - 统计: 允许%1, 阻止%2, Cookie阻止%3")
        .arg(m_allowedResourceCount)
        .arg(m_blockedResourceCount)
        .arg(m_cookieBlockCount));
}

// ==================== CefResourceRequestHandler接口实现 ====================

CefResourceRequestHandler::ReturnValue CEFResourceRequestHandler::OnBeforeResourceLoad(
    CefRefPtr<CefBrowser> browser,
    CefRefPtr<CefFrame> frame,
    CefRefPtr<CefRequest> request,
    CefRefPtr<CefCallback> callback)
{
    QString url = QString::fromStdString(request->GetURL().ToString());
    
#ifdef CEF_VERSION_109
    // CEF 109特定的资源加载前处理
    m_logger->appEvent(QString("CEF 109资源加载前检查: %1").arg(url));
    
    // URL安全检查
    bool allowed = isUrlAllowed(url);
    logResourceAttempt(url, allowed);
    
    if (!allowed) {
        logSecurityEvent("资源加载被阻止", url);
        m_blockedResourceCount++;
        return RV_CANCEL;
    }
    
    // URL检测功能
    if (m_urlDetectionEnabled && checkExitUrlPattern(url)) {
        logSecurityEvent("检测到退出URL模式", url);
        // 触发退出信号（通过事件机制）
        // TODO: 实现退出信号触发
    }
    
    m_allowedResourceCount++;
    return RV_CONTINUE;
    
#else
    // CEF 75兼容模式
    m_logger->appEvent(QString("CEF 75资源加载检查: %1").arg(url));
    
    bool allowed = isUrlAllowed(url);
    logResourceAttempt(url, allowed);
    
    if (!allowed) {
        logSecurityEvent("资源加载被阻止", url);
        m_blockedResourceCount++;
        return RV_CANCEL;
    }
    
    m_allowedResourceCount++;
    return RV_CONTINUE;
#endif
}

CefRefPtr<CefResourceHandler> CEFResourceRequestHandler::GetResourceHandler(
    CefRefPtr<CefBrowser> browser,
    CefRefPtr<CefFrame> frame,
    CefRefPtr<CefRequest> request)
{
    // 目前不需要自定义资源处理器
    // 如果需要拦截特定资源或提供自定义内容，在这里实现
    return nullptr;
}

void CEFResourceRequestHandler::OnResourceRedirect(
    CefRefPtr<CefBrowser> browser,
    CefRefPtr<CefFrame> frame,
    CefRefPtr<CefRequest> request,
    CefRefPtr<CefResponse> response,
    CefString& new_url)
{
    QString oldUrl = QString::fromStdString(request->GetURL().ToString());
    QString redirectUrl = QString::fromStdString(new_url.ToString());
    
    m_logger->appEvent(QString("资源重定向: %1 -> %2").arg(oldUrl).arg(redirectUrl));
    
    // 检查重定向URL是否被允许
    if (!isUrlAllowed(redirectUrl)) {
        logSecurityEvent("重定向被阻止", QString("%1 -> %2").arg(oldUrl).arg(redirectUrl));
        new_url = ""; // 阻止重定向
        return;
    }
    
    // URL检测功能 - 检查重定向目标
    if (m_urlDetectionEnabled && checkExitUrlPattern(redirectUrl)) {
        logSecurityEvent("重定向检测到退出URL模式", redirectUrl);
        // TODO: 触发退出信号
    }
}

bool CEFResourceRequestHandler::OnResourceResponse(
    CefRefPtr<CefBrowser> browser,
    CefRefPtr<CefFrame> frame,
    CefRefPtr<CefRequest> request,
    CefRefPtr<CefResponse> response)
{
    QString url = QString::fromStdString(request->GetURL().ToString());
    int statusCode = response->GetStatus();
    
    if (statusCode >= 400) {
        m_logger->appEvent(QString("资源响应错误: %1 (状态码: %2)").arg(url).arg(statusCode));
    }
    
    // 继续默认处理
    return false;
}

void CEFResourceRequestHandler::OnResourceLoadComplete(
    CefRefPtr<CefBrowser> browser,
    CefRefPtr<CefFrame> frame,
    CefRefPtr<CefRequest> request,
    CefRefPtr<CefResponse> response,
    URLRequestStatus status,
    int64 received_content_length)
{
    QString url = QString::fromStdString(request->GetURL().ToString());
    
    if (status != UR_SUCCESS) {
        m_logger->errorEvent(QString("资源加载失败: %1 (状态: %2)")
            .arg(url).arg(static_cast<int>(status)));
    } else {
        // 只在调试模式记录成功的资源加载
        if (received_content_length > 0) {
            // m_logger->appEvent(QString("资源加载完成: %1 (%2 bytes)").arg(url).arg(received_content_length));
        }
    }
}

void CEFResourceRequestHandler::OnProtocolExecution(
    CefRefPtr<CefBrowser> browser,
    CefRefPtr<CefFrame> frame,
    CefRefPtr<CefRequest> request,
    bool& allow_os_execution)
{
    QString url = QString::fromStdString(request->GetURL().ToString());
    QUrl qurl(url);
    
    // 安全模式下禁止所有外部协议执行
    if (m_strictSecurityMode) {
        allow_os_execution = false;
        logSecurityEvent("外部协议执行被阻止", url);
        return;
    }
    
    // 只允许安全的协议
    QString scheme = qurl.scheme().toLower();
    QStringList allowedSchemes = {"http", "https", "data", "blob"};
    
    if (!allowedSchemes.contains(scheme)) {
        allow_os_execution = false;
        logSecurityEvent("不安全协议被阻止", url);
    } else {
        allow_os_execution = true;
        m_logger->appEvent(QString("允许协议执行: %1").arg(url));
    }
}

// ==================== Cookie管理接口（CEF 109迁移到UI线程）====================

bool CEFResourceRequestHandler::CanGetCookies(
    CefRefPtr<CefBrowser> browser,
    CefRefPtr<CefFrame> frame,
    CefRefPtr<CefRequest> request)
{
#ifdef CEF_VERSION_109
    // CEF 109: Cookie回调在UI线程执行
    QString url = QString::fromStdString(request->GetURL().ToString());
    
    // 严格模式下可能需要限制Cookie访问
    if (m_strictSecurityMode) {
        // 只允许同域Cookie
        if (!isDomainAllowed(extractDomain(url))) {
            logCookieOperation("Cookie读取被阻止", url);
            m_cookieBlockCount++;
            return false;
        }
    }
    
    logCookieOperation("Cookie读取允许", url);
    return true;
    
#else
    // CEF 75兼容模式
    return true;
#endif
}

bool CEFResourceRequestHandler::CanSetCookie(
    CefRefPtr<CefBrowser> browser,
    CefRefPtr<CefFrame> frame,
    CefRefPtr<CefRequest> request,
    const CefCookie& cookie)
{
#ifdef CEF_VERSION_109
    // CEF 109: Cookie回调在UI线程执行
    QString url = QString::fromStdString(request->GetURL().ToString());
    QString cookieName = QString::fromStdString(cookie.name.ToString());
    
    // 严格模式下限制Cookie设置
    if (m_strictSecurityMode) {
        // 只允许同域Cookie设置
        if (!isDomainAllowed(extractDomain(url))) {
            logCookieOperation("Cookie设置被阻止", QString("%1=%2 (来源:%3)")
                .arg(cookieName).arg("***").arg(url));
            m_cookieBlockCount++;
            return false;
        }
    }
    
    logCookieOperation("Cookie设置允许", QString("%1 (来源:%2)").arg(cookieName).arg(url));
    return true;
    
#else
    // CEF 75兼容模式
    return true;
#endif
}

// ==================== 安全策略配置 ====================

void CEFResourceRequestHandler::setAllowedDomain(const QString& domain)
{
    m_allowedDomain = domain;
    m_allowedDomains.clear();
    m_allowedDomains.append(domain);
    m_logger->appEvent(QString("设置允许域名: %1").arg(domain));
}

void CEFResourceRequestHandler::setAllowedDomains(const QStringList& domains)
{
    m_allowedDomains = domains;
    if (!domains.isEmpty()) {
        m_allowedDomain = domains.first();
    }
    m_logger->appEvent(QString("设置允许域名列表: %1").arg(domains.join(", ")));
}

void CEFResourceRequestHandler::setStrictSecurityMode(bool strict)
{
    m_strictSecurityMode = strict;
    m_logger->appEvent(QString("严格安全模式: %1").arg(strict ? "启用" : "禁用"));
}

void CEFResourceRequestHandler::setUrlDetectionEnabled(bool enabled)
{
    m_urlDetectionEnabled = enabled;
    m_logger->appEvent(QString("URL检测功能: %1").arg(enabled ? "启用" : "禁用"));
}

void CEFResourceRequestHandler::setUrlDetectionPatterns(const QStringList& patterns)
{
    m_urlDetectionPatterns = patterns;
    m_logger->appEvent(QString("设置URL检测模式: %1").arg(patterns.join(", ")));
}

// ==================== 私有方法实现 ====================

bool CEFResourceRequestHandler::isUrlAllowed(const QString& url)
{
    if (url.isEmpty()) {
        return false;
    }
    
    // 特殊URL总是允许
    if (url.startsWith("data:") || url.startsWith("blob:") || url == "about:blank") {
        return true;
    }
    
    // 如果没有配置允许域名，默认允许
    if (m_allowedDomains.isEmpty()) {
        return true;
    }
    
    QString domain = extractDomain(url);
    return isDomainAllowed(domain);
}

bool CEFResourceRequestHandler::isDomainAllowed(const QString& domain)
{
    if (domain.isEmpty()) {
        return false;
    }
    
    for (const QString& allowedDomain : m_allowedDomains) {
        if (domain == allowedDomain || domain.endsWith("." + allowedDomain)) {
            return true;
        }
    }
    
    return false;
}

QString CEFResourceRequestHandler::extractDomain(const QString& url)
{
    QUrl qurl(url);
    return qurl.host();
}

bool CEFResourceRequestHandler::checkExitUrlPattern(const QString& url)
{
    if (!m_urlDetectionEnabled || m_urlDetectionPatterns.isEmpty()) {
        return false;
    }
    
    for (const QString& pattern : m_urlDetectionPatterns) {
        QRegularExpression regex(pattern, QRegularExpression::CaseInsensitiveOption);
        if (regex.match(url).hasMatch()) {
            return true;
        }
    }
    
    return false;
}

void CEFResourceRequestHandler::logSecurityEvent(const QString& event, const QString& details)
{
    m_logger->exitEvent(QString("[安全] %1: %2").arg(event).arg(details));
}

void CEFResourceRequestHandler::logResourceAttempt(const QString& url, bool allowed)
{
    if (!allowed) {
        m_logger->appEvent(QString("[资源阻止] %1").arg(url));
    }
    // 成功的资源加载不记录日志以避免过多输出
}

void CEFResourceRequestHandler::logCookieOperation(const QString& operation, const QString& details)
{
    m_logger->appEvent(QString("[Cookie] %1: %2").arg(operation).arg(details));
}