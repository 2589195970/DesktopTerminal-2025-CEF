#ifndef CEF_CLIENT_H
#define CEF_CLIENT_H

#include "cef_client.h"
#include "cef_display_handler.h"
#include "cef_life_span_handler.h"
#include "cef_load_handler.h"
#include "cef_request_handler.h"
#include "cef_keyboard_handler.h"
#include "cef_context_menu_handler.h"
#include "cef_jsdialog_handler.h"
#include "cef_download_handler.h"

#include <QString>
#include <QStringList>

class Logger;
class ConfigManager;

/**
 * @brief CEF客户端实现类
 * 
 * 实现CEF的各种处理器接口，提供安全控制和事件处理
 * 特别针对Windows 7 SP1和32位系统进行了优化
 */
class CEFClient : public CefClient,
                  public CefDisplayHandler,
                  public CefLifeSpanHandler,
                  public CefLoadHandler,
                  public CefRequestHandler,
                  public CefKeyboardHandler,
                  public CefContextMenuHandler,
                  public CefJSDialogHandler,
                  public CefDownloadHandler
{
public:
    CEFClient();
    virtual ~CEFClient();

    // CefClient接口
    virtual CefRefPtr<CefDisplayHandler> GetDisplayHandler() override { return this; }
    virtual CefRefPtr<CefLifeSpanHandler> GetLifeSpanHandler() override { return this; }
    virtual CefRefPtr<CefLoadHandler> GetLoadHandler() override { return this; }
    virtual CefRefPtr<CefRequestHandler> GetRequestHandler() override { return this; }
    virtual CefRefPtr<CefKeyboardHandler> GetKeyboardHandler() override { return this; }
    virtual CefRefPtr<CefContextMenuHandler> GetContextMenuHandler() override { return this; }
    virtual CefRefPtr<CefJSDialogHandler> GetJSDialogHandler() override { return this; }
    virtual CefRefPtr<CefDownloadHandler> GetDownloadHandler() override { return this; }

    // CefDisplayHandler接口 - 显示处理
    virtual void OnTitleChange(CefRefPtr<CefBrowser> browser, const CefString& title) override;
    virtual void OnAddressChange(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, const CefString& url) override;
    virtual bool OnConsoleMessage(CefRefPtr<CefBrowser> browser, cef_log_severity_t level, const CefString& message, const CefString& source, int line) override;

    // CefLifeSpanHandler接口 - 生命周期处理
    virtual bool OnBeforePopup(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, const CefString& target_url, const CefString& target_frame_name, CefLifeSpanHandler::WindowOpenDisposition target_disposition, bool user_gesture, const CefPopupFeatures& popupFeatures, CefWindowInfo& windowInfo, CefRefPtr<CefClient>& client, CefBrowserSettings& settings, CefRefPtr<CefDictionaryValue>& extra_info, bool* no_javascript_access) override;
    virtual void OnAfterCreated(CefRefPtr<CefBrowser> browser) override;
    virtual bool DoClose(CefRefPtr<CefBrowser> browser) override;
    virtual void OnBeforeClose(CefRefPtr<CefBrowser> browser) override;

    // CefLoadHandler接口 - 加载处理
    virtual void OnLoadStart(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, TransitionType transition_type) override;
    virtual void OnLoadEnd(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, int httpStatusCode) override;
    virtual void OnLoadError(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, ErrorCode errorCode, const CefString& errorText, const CefString& failedUrl) override;

    // CefRequestHandler接口 - 请求处理（安全控制核心）
    virtual bool OnBeforeBrowse(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, CefRefPtr<CefRequest> request, bool user_gesture, bool is_redirect) override;
    virtual bool OnOpenURLFromTab(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, const CefString& target_url, CefRequestHandler::WindowOpenDisposition target_disposition, bool user_gesture) override;
    virtual CefRefPtr<CefResourceRequestHandler> GetResourceRequestHandler(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, CefRefPtr<CefRequest> request, bool is_navigation, bool is_download, const CefString& request_initiator, bool& disable_default_handling) override;
    virtual bool OnBeforeResourceLoad(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, CefRefPtr<CefRequest> request, CefRefPtr<CefCallback> callback) override;

    // CefKeyboardHandler接口 - 键盘处理（安全控制）
    virtual bool OnPreKeyEvent(CefRefPtr<CefBrowser> browser, const CefKeyEvent& event, CefEventHandle os_event, bool* is_keyboard_shortcut) override;
    virtual bool OnKeyEvent(CefRefPtr<CefBrowser> browser, const CefKeyEvent& event, CefEventHandle os_event) override;

    // CefContextMenuHandler接口 - 右键菜单处理
    virtual void OnBeforeContextMenu(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, CefRefPtr<CefContextMenuParams> params, CefRefPtr<CefMenuModel> model) override;
    virtual bool OnContextMenuCommand(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, CefRefPtr<CefContextMenuParams> params, int command_id, EventFlags event_flags) override;

    // CefJSDialogHandler接口 - JavaScript对话框处理
    virtual bool OnJSDialog(CefRefPtr<CefBrowser> browser, const CefString& origin_url, JSDialogType dialog_type, const CefString& message_text, const CefString& default_prompt_text, CefRefPtr<CefJSDialogCallback> callback, bool& suppress_message) override;
    virtual bool OnBeforeUnloadDialog(CefRefPtr<CefBrowser> browser, const CefString& message_text, bool is_reload, CefRefPtr<CefJSDialogCallback> callback) override;

    // CefDownloadHandler接口 - 下载处理
    virtual void OnBeforeDownload(CefRefPtr<CefBrowser> browser, CefRefPtr<CefDownloadItem> download_item, const CefString& suggested_name, CefRefPtr<CefBeforeDownloadCallback> callback) override;
    virtual void OnDownloadUpdated(CefRefPtr<CefBrowser> browser, CefRefPtr<CefDownloadItem> download_item, CefRefPtr<CefDownloadItemCallback> callback) override;

    // 安全策略配置
    void setAllowedDomain(const QString& domain);
    void setSecurityMode(bool strict);
    void setKeyboardFilterEnabled(bool enabled);
    void setContextMenuEnabled(bool enabled);
    void setDownloadEnabled(bool enabled);

    // Windows 7特定优化
    void enableWindows7Compatibility(bool enable);
    void setLowMemoryMode(bool enable);

private:
    // URL验证
    bool isUrlAllowed(const QString& url);
    bool isDomainAllowed(const QString& domain);
    QString extractDomain(const QString& url);

    // 键盘事件过滤
    bool isKeyEventAllowed(const CefKeyEvent& event);
    bool isSystemShortcut(const CefKeyEvent& event);
    bool isAllowedShortcut(const CefKeyEvent& event);

    // 安全日志记录
    void logSecurityEvent(const QString& event, const QString& details);
    void logNavigationAttempt(const QString& url, bool allowed);
    void logKeyboardEvent(const CefKeyEvent& event, bool allowed);

    // Windows 7兼容性处理
    void applyWindows7Optimizations();
    bool handleWindows7KeyEvent(const CefKeyEvent& event);

private:
    Logger* m_logger;
    ConfigManager* m_configManager;

    // 安全配置
    QString m_allowedDomain;
    QStringList m_allowedDomains;
    bool m_strictSecurityMode;
    bool m_keyboardFilterEnabled;
    bool m_contextMenuEnabled;
    bool m_downloadEnabled;

    // Windows 7特定设置
    bool m_windows7CompatibilityMode;
    bool m_lowMemoryMode;

    // 浏览器引用
    CefRefPtr<CefBrowser> m_browser;
    int m_browserCount;

    // 性能优化标志
    bool m_reduceLogging;
    bool m_disableAnimations;

    IMPLEMENT_REFCOUNTING(CEFClient);
};

#endif // CEF_CLIENT_H