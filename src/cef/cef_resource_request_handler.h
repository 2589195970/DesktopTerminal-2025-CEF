#ifndef CEF_RESOURCE_REQUEST_HANDLER_H
#define CEF_RESOURCE_REQUEST_HANDLER_H

#include "include/cef_resource_request_handler.h"
#include "include/cef_browser.h"
#include "include/cef_frame.h"
#include "include/cef_request.h"
#include "include/cef_response.h"
#include "include/cef_callback.h"
#include "include/cef_resource_handler.h"
#include "include/cef_cookie.h"

#include <QString>
#include <QStringList>

class Logger;
class ConfigManager;

/**
 * @brief CEF资源请求处理器实现
 * 
 * CEF 109将资源相关回调从IRequestHandler迁移到了这个新接口
 * 包含OnBeforeResourceLoad、Cookie管理等功能
 */
class CEFResourceRequestHandler : public CefResourceRequestHandler
{
public:
    CEFResourceRequestHandler();
    virtual ~CEFResourceRequestHandler();

    // ==================== CefResourceRequestHandler接口实现 ====================

    /**
     * @brief 资源加载前回调（从IRequestHandler迁移而来）
     * CEF 109: 此方法从IRequestHandler迁移到IResourceRequestHandler
     */
    virtual CefResourceRequestHandler::ReturnValue OnBeforeResourceLoad(
        CefRefPtr<CefBrowser> browser,
        CefRefPtr<CefFrame> frame,
        CefRefPtr<CefRequest> request,
        CefRefPtr<CefCallback> callback) override;

    /**
     * @brief 获取资源处理器
     * 如果需要自定义资源处理，在这里返回自定义的IResourceHandler
     */
    virtual CefRefPtr<CefResourceHandler> GetResourceHandler(
        CefRefPtr<CefBrowser> browser,
        CefRefPtr<CefFrame> frame,
        CefRefPtr<CefRequest> request) override;

    /**
     * @brief 资源重定向回调
     */
    virtual void OnResourceRedirect(
        CefRefPtr<CefBrowser> browser,
        CefRefPtr<CefFrame> frame,
        CefRefPtr<CefRequest> request,
        CefRefPtr<CefResponse> response,
        CefString& new_url) override;

    /**
     * @brief 资源响应回调
     */
    virtual bool OnResourceResponse(
        CefRefPtr<CefBrowser> browser,
        CefRefPtr<CefFrame> frame,
        CefRefPtr<CefRequest> request,
        CefRefPtr<CefResponse> response) override;

    /**
     * @brief 资源加载完成回调
     */
    virtual void OnResourceLoadComplete(
        CefRefPtr<CefBrowser> browser,
        CefRefPtr<CefFrame> frame,
        CefRefPtr<CefRequest> request,
        CefRefPtr<CefResponse> response,
        URLRequestStatus status,
        int64 received_content_length) override;

    /**
     * @brief 协议执行回调（从IRequestHandler迁移而来）
     * CEF 109: 此方法从IRequestHandler迁移到IResourceRequestHandler
     */
    virtual void OnProtocolExecution(
        CefRefPtr<CefBrowser> browser,
        CefRefPtr<CefFrame> frame,
        CefRefPtr<CefRequest> request,
        bool& allow_os_execution) override;

    // ==================== Cookie管理接口（CEF版本兼容性处理）====================

#if defined(CEF_VERSION_75) || (defined(CEF_VERSION_109) && defined(CEF_SUPPORTS_COOKIE_CALLBACKS))
    /**
     * @brief Cookie访问权限回调
     * 注意：在某些CEF版本中此方法可能不可用
     */
    virtual bool CanGetCookies(
        CefRefPtr<CefBrowser> browser,
        CefRefPtr<CefFrame> frame,
        CefRefPtr<CefRequest> request) override;

    /**
     * @brief Cookie设置权限回调
     * 注意：在某些CEF版本中此方法可能不可用
     */
    virtual bool CanSetCookie(
        CefRefPtr<CefBrowser> browser,
        CefRefPtr<CefFrame> frame,
        CefRefPtr<CefRequest> request,
        const CefCookie& cookie) override;
#endif

    // ==================== 安全策略配置 ====================

    /**
     * @brief 设置允许的域名
     */
    void setAllowedDomain(const QString& domain);

    /**
     * @brief 设置允许的域名列表
     */
    void setAllowedDomains(const QStringList& domains);

    /**
     * @brief 设置严格安全模式
     */
    void setStrictSecurityMode(bool strict);

    /**
     * @brief 启用/禁用URL检测功能
     */
    void setUrlDetectionEnabled(bool enabled);

    /**
     * @brief 设置URL检测模式
     */
    void setUrlDetectionPatterns(const QStringList& patterns);

private:
    // ==================== URL和安全验证 ====================

    /**
     * @brief 检查URL是否被允许
     */
    bool isUrlAllowed(const QString& url);

    /**
     * @brief 检查域名是否被允许
     */
    bool isDomainAllowed(const QString& domain);

    /**
     * @brief 从URL提取域名
     */
    QString extractDomain(const QString& url);

    /**
     * @brief 检查是否匹配退出URL模式
     */
    bool checkExitUrlPattern(const QString& url);

    // ==================== 安全日志记录 ====================

    /**
     * @brief 记录安全事件
     */
    void logSecurityEvent(const QString& event, const QString& details);

    /**
     * @brief 记录资源加载尝试
     */
    void logResourceAttempt(const QString& url, bool allowed);

    /**
     * @brief 记录Cookie操作
     */
    void logCookieOperation(const QString& operation, const QString& details);

private:
    Logger* m_logger;
    ConfigManager* m_configManager;

    // 安全配置
    QString m_allowedDomain;
    QStringList m_allowedDomains;
    bool m_strictSecurityMode;
    bool m_urlDetectionEnabled;
    QStringList m_urlDetectionPatterns;

    // 统计信息
    int m_blockedResourceCount;
    int m_allowedResourceCount;
    int m_cookieBlockCount;

    IMPLEMENT_REFCOUNTING(CEFResourceRequestHandler);
};

#endif // CEF_RESOURCE_REQUEST_HANDLER_H