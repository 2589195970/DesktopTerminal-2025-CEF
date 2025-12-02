#include "cef_client_impl.h"
#include "../logging/logger.h"
#include "../config/config_manager.h"
#include "../core/application.h"
#include "../core/cef_manager.h"

#include <QUrl>

CEFClient::CEFClient(CEFManager* cefManager)
    : m_logger(&Logger::instance())
    , m_configManager(&ConfigManager::instance())
    , m_cefManager(cefManager)
    , m_strictSecurityMode(true)
    , m_keyboardFilterEnabled(true)
    , m_contextMenuEnabled(false)
    , m_downloadEnabled(false)
    , m_windows7CompatibilityMode(false)
    , m_lowMemoryMode(false)
    , m_browser(nullptr)
    , m_browserCount(0)
    , m_reduceLogging(false)
    , m_disableAnimations(false)
{
    // 检测Windows 7兼容性模式
    if (Application::isWindows7SP1()) {
        enableWindows7Compatibility(true);
    }

    // 检测32位系统低内存模式
    if (Application::is32BitSystem()) {
        setLowMemoryMode(true);
    }

    m_logger->appEvent("CEFClient创建完成");
}

CEFClient::~CEFClient()
{
    m_logger->appEvent("CEFClient销毁");
}

// ==================== CefDisplayHandler接口实现 ====================

void CEFClient::OnTitleChange(CefRefPtr<CefBrowser> browser, const CefString& title)
{
    if (!m_reduceLogging) {
        m_logger->appEvent(QString("页面标题变更: %1").arg(QString::fromStdString(title.ToString())));
    }
}

void CEFClient::OnAddressChange(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, const CefString& url)
{
    QString urlStr = QString::fromStdString(url.ToString());
    
    if (frame->IsMain()) {
        m_logger->appEvent(QString("主框架地址变更: %1").arg(urlStr));
        
        // URL退出检测已移到OnBeforeBrowse方法中，避免JavaScript导航误触发（修复误检测logout问题）
        // 这里只记录地址变更，不再检测退出模式
        
    }
}

bool CEFClient::OnConsoleMessage(CefRefPtr<CefBrowser> browser, cef_log_severity_t level, const CefString& message, const CefString& source, int line)
{
    if (!m_reduceLogging && level >= LOGSEVERITY_WARNING) {
        QString logMessage = QString("控制台[%1:%2]: %3")
            .arg(QString::fromStdString(source.ToString()))
            .arg(line)
            .arg(QString::fromStdString(message.ToString()));
        
        if (level == LOGSEVERITY_ERROR) {
            m_logger->errorEvent(logMessage);
        } else {
            m_logger->appEvent(logMessage);
        }
    }
    
    return false; // 允许默认处理
}

// ==================== CefLifeSpanHandler接口实现 ====================

bool CEFClient::OnBeforePopup(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, const CefString& target_url, const CefString& target_frame_name, CefLifeSpanHandler::WindowOpenDisposition target_disposition, bool user_gesture, const CefPopupFeatures& popupFeatures, CefWindowInfo& windowInfo, CefRefPtr<CefClient>& client, CefBrowserSettings& settings, CefRefPtr<CefDictionaryValue>& extra_info, bool* no_javascript_access)
{
    QString url = QString::fromStdString(target_url.ToString());
    
    // 严格模式下禁止所有弹窗
    if (m_strictSecurityMode) {
        logSecurityEvent("弹窗被阻止", url);
        return true; // 阻止弹窗
    }

    return false; // 不做URL访问限制
}

void CEFClient::OnAfterCreated(CefRefPtr<CefBrowser> browser)
{
    m_browser = browser;
    m_browserCount++;
    
    m_logger->appEvent(QString("浏览器创建完成，ID: %1").arg(browser->GetIdentifier()));
    
    // Windows 7特定优化
    if (m_windows7CompatibilityMode) {
        applyWindows7Optimizations();
    }
}

bool CEFClient::DoClose(CefRefPtr<CefBrowser> browser)
{
    m_logger->appEvent(QString("浏览器关闭请求，ID: %1").arg(browser->GetIdentifier()));
    return false; // 允许关闭
}

void CEFClient::OnBeforeClose(CefRefPtr<CefBrowser> browser)
{
    m_browserCount--;
    m_logger->appEvent(QString("浏览器关闭，ID: %1，剩余: %2").arg(browser->GetIdentifier()).arg(m_browserCount));
    
    if (browser->IsSame(m_browser)) {
        m_browser = nullptr;
    }
}

// ==================== CefLoadHandler接口实现 ====================

void CEFClient::OnLoadStart(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, TransitionType transition_type)
{
    if (frame->IsMain()) {
        QString url = QString::fromStdString(frame->GetURL().ToString());
        m_logger->appEvent(QString("开始加载页面: %1").arg(url));
    }
}

void CEFClient::OnLoadEnd(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, int httpStatusCode)
{
    if (frame->IsMain()) {
        QString url = QString::fromStdString(frame->GetURL().ToString());
        m_logger->appEvent(QString("页面加载完成: %1 (状态码: %2)").arg(url).arg(httpStatusCode));
        
        // 在低内存模式下，加载完成后清理不必要的资源
        if (m_lowMemoryMode) {
            // 可以在这里添加内存清理逻辑
        }
    }
}

void CEFClient::OnLoadError(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, ErrorCode errorCode, const CefString& errorText, const CefString& failedUrl)
{
    if (frame->IsMain()) {
        QString url = QString::fromStdString(failedUrl.ToString());
        QString error = QString::fromStdString(errorText.ToString());
        
        m_logger->errorEvent(QString("页面加载失败: %1 - 错误: %2 (代码: %3)")
            .arg(url).arg(error).arg(static_cast<int>(errorCode)));
    }
}

// ==================== CefRequestHandler接口实现（安全控制核心）====================

bool CEFClient::OnBeforeBrowse(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, CefRefPtr<CefRequest> request, bool user_gesture, bool is_redirect)
{
    QString url = QString::fromStdString(request->GetURL().ToString());
    
    // 只在主框架中检测退出模式（修复误检测logout问题）
    if (frame->IsMain() && user_gesture && !is_redirect) {
        // 只检测用户手动导航，不检测JavaScript导航和重定向
        if (m_configManager->isUrlExitEnabled() && m_cefManager) {
            QString exitPattern = m_configManager->getUrlExitPattern();
            if (!exitPattern.isEmpty() && url.contains(exitPattern, Qt::CaseInsensitive)) {
                m_logger->appEvent(QString("检测到用户手动导航到退出模式 URL '%1': %2").arg(exitPattern, url));
                m_cefManager->notifyUrlExitTriggered(url);
                return true; // 阻止导航并退出
            }
        }
    }
    
    return false; // 不做URL访问限制
}

bool CEFClient::OnOpenURLFromTab(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, const CefString& target_url, CefRequestHandler::WindowOpenDisposition target_disposition, bool user_gesture)
{
    QString url = QString::fromStdString(target_url.ToString());
    
    // 严格模式下禁止新标签页
    if (m_strictSecurityMode) {
        logSecurityEvent("新标签页被阻止", url);
        return true; // 阻止打开新标签页
    }
    
    return false;
}

CefRefPtr<CefResourceRequestHandler> CEFClient::GetResourceRequestHandler(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, CefRefPtr<CefRequest> request, bool is_navigation, bool is_download, const CefString& request_initiator, bool& disable_default_handling)
{
    // 在这里可以返回自定义的资源请求处理器
    // 目前返回nullptr使用默认处理
    return nullptr;
}

// ==================== CefKeyboardHandler接口实现（安全控制）====================

bool CEFClient::OnPreKeyEvent(CefRefPtr<CefBrowser> browser, const CefKeyEvent& event, CefEventHandle os_event, bool* is_keyboard_shortcut)
{
    if (!m_keyboardFilterEnabled) {
        return false;
    }
    
    // Windows 7特殊处理
    if (m_windows7CompatibilityMode) {
        return handleWindows7KeyEvent(event);
    }
    
    // 检查是否是被允许的按键事件
    bool allowed = isKeyEventAllowed(event);
    
    if (!allowed) {
        logKeyboardEvent(event, false);
        return true; // 阻止事件
    }
    
    return false; // 允许事件
}

bool CEFClient::OnKeyEvent(CefRefPtr<CefBrowser> browser, const CefKeyEvent& event, CefEventHandle os_event)
{
    if (!m_keyboardFilterEnabled) {
        return false;
    }
    
    // 额外的键盘事件处理
    bool allowed = isKeyEventAllowed(event);
    
    if (!allowed) {
        logKeyboardEvent(event, false);
        return true; // 阻止事件
    }
    
    return false; // 允许事件
}

// ==================== CefContextMenuHandler接口实现 ====================

void CEFClient::OnBeforeContextMenu(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, CefRefPtr<CefContextMenuParams> params, CefRefPtr<CefMenuModel> model)
{
    if (!m_contextMenuEnabled) {
        // 禁用右键菜单 - 清除所有菜单项
        model->Clear();
        logSecurityEvent("右键菜单被禁用", "");
    }
}

bool CEFClient::OnContextMenuCommand(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, CefRefPtr<CefContextMenuParams> params, int command_id, EventFlags event_flags)
{
    if (!m_contextMenuEnabled) {
        logSecurityEvent("右键菜单命令被阻止", QString::number(command_id));
        return true; // 阻止命令执行
    }
    
    return false;
}

// ==================== CefJSDialogHandler接口实现 ====================

bool CEFClient::OnJSDialog(CefRefPtr<CefBrowser> browser, const CefString& origin_url, JSDialogType dialog_type, const CefString& message_text, const CefString& default_prompt_text, CefRefPtr<CefJSDialogCallback> callback, bool& suppress_message)
{
    QString message = QString::fromStdString(message_text.ToString());
    QString origin = QString::fromStdString(origin_url.ToString());
    
    // 在严格模式下禁用JavaScript对话框
    if (m_strictSecurityMode) {
        logSecurityEvent("JavaScript对话框被阻止", QString("来源: %1, 消息: %2").arg(origin).arg(message));
        suppress_message = true;
        return true; // 阻止对话框
    }
    
    return false; // 允许对话框
}

bool CEFClient::OnBeforeUnloadDialog(CefRefPtr<CefBrowser> browser, const CefString& message_text, bool is_reload, CefRefPtr<CefJSDialogCallback> callback)
{
    // 在严格模式下禁用卸载确认对话框
    if (m_strictSecurityMode) {
        logSecurityEvent("页面卸载对话框被阻止", QString::fromStdString(message_text.ToString()));
        return true; // 阻止对话框
    }
    
    return false;
}

// ==================== CefDownloadHandler接口实现 ====================

void CEFClient::OnBeforeDownload(CefRefPtr<CefBrowser> browser, CefRefPtr<CefDownloadItem> download_item, const CefString& suggested_name, CefRefPtr<CefBeforeDownloadCallback> callback)
{
    QString fileName = QString::fromStdString(suggested_name.ToString());
    QString url = QString::fromStdString(download_item->GetURL().ToString());
    
    if (!m_downloadEnabled) {
        logSecurityEvent("下载被阻止", QString("文件: %1, URL: %2").arg(fileName).arg(url));
        // 不调用callback，这会取消下载
        return;
    }
    
    // 如果允许下载，可以在这里设置下载路径
    m_logger->appEvent(QString("开始下载: %1").arg(fileName));
}

void CEFClient::OnDownloadUpdated(CefRefPtr<CefBrowser> browser, CefRefPtr<CefDownloadItem> download_item, CefRefPtr<CefDownloadItemCallback> callback)
{
    if (!m_downloadEnabled) {
        callback->Cancel(); // 取消下载
        return;
    }
    
    // 下载进度更新
    if (download_item->IsComplete()) {
        m_logger->appEvent(QString("下载完成: %1").arg(QString::fromStdString(download_item->GetFullPath().ToString())));
    }
}

// ==================== 公共配置方法 ====================

void CEFClient::setSecurityMode(bool strict)
{
    m_strictSecurityMode = strict;
    m_logger->configEvent(QString("安全模式: %1").arg(strict ? "严格" : "宽松"));
}

void CEFClient::setKeyboardFilterEnabled(bool enabled)
{
    m_keyboardFilterEnabled = enabled;
    m_logger->configEvent(QString("键盘过滤: %1").arg(enabled ? "启用" : "禁用"));
}

void CEFClient::setContextMenuEnabled(bool enabled)
{
    m_contextMenuEnabled = enabled;
    m_logger->configEvent(QString("右键菜单: %1").arg(enabled ? "启用" : "禁用"));
}

void CEFClient::setDownloadEnabled(bool enabled)
{
    m_downloadEnabled = enabled;
    m_logger->configEvent(QString("下载功能: %1").arg(enabled ? "启用" : "禁用"));
}

void CEFClient::enableWindows7Compatibility(bool enable)
{
    m_windows7CompatibilityMode = enable;
    if (enable) {
        m_reduceLogging = true;
        m_disableAnimations = true;
        m_logger->appEvent("启用Windows 7兼容模式");
    }
}

void CEFClient::setLowMemoryMode(bool enable)
{
    m_lowMemoryMode = enable;
    if (enable) {
        m_reduceLogging = true;
        m_logger->appEvent("启用低内存模式");
    }
}

// ==================== 私有辅助方法 ====================

bool CEFClient::isKeyEventAllowed(const CefKeyEvent& event)
{
    // 允许的快捷键：Ctrl+R（刷新）
    if (event.modifiers == EVENTFLAG_CONTROL_DOWN && event.windows_key_code == 'R') {
        return true;
    }
    
    // 允许普通字符输入
    if (event.modifiers == 0 || event.modifiers == EVENTFLAG_SHIFT_DOWN) {
        return true;
    }
    
    // 允许方向键、回车、退格等基本导航键
    // CEF 75兼容性：使用数值而不是VK_常量以确保跨版本兼容
    if (event.windows_key_code >= 37 && event.windows_key_code <= 40) { // VK_LEFT到VK_DOWN
        return true;
    }
    
    if (event.windows_key_code == 13 || event.windows_key_code == 8 ||  // VK_RETURN, VK_BACK
        event.windows_key_code == 9 || event.windows_key_code == 27) {   // VK_TAB, VK_ESCAPE
        return true;
    }
    
    // 其他系统快捷键都被阻止
    return false;
}

bool CEFClient::isSystemShortcut(const CefKeyEvent& event)
{
    // 检测系统快捷键
    // CEF 75兼容性：使用数值而不是VK_常量
    if (event.modifiers & EVENTFLAG_ALT_DOWN) {
        if (event.windows_key_code == 9 || event.windows_key_code == 115) { // VK_TAB, VK_F4
            return true;
        }
    }
    
    if (event.modifiers & EVENTFLAG_CONTROL_DOWN) {
        if (event.windows_key_code == 'W' || event.windows_key_code == 'T' || 
            event.windows_key_code == 'N' || event.windows_key_code == 115) { // VK_F4
            return true;
        }
    }
    
    return false;
}

bool CEFClient::isAllowedShortcut(const CefKeyEvent& event)
{
    // 只允许Ctrl+R
    return (event.modifiers == EVENTFLAG_CONTROL_DOWN && event.windows_key_code == 'R');
}

void CEFClient::logSecurityEvent(const QString& event, const QString& details)
{
    QString logMessage = QString("%1: %2").arg(event).arg(details);
    m_logger->logEvent("安全控制", logMessage, "security.log", L_WARNING);
}

void CEFClient::logKeyboardEvent(const CefKeyEvent& event, bool allowed)
{
    if (m_reduceLogging && allowed) {
        return; // 在低内存模式下减少日志
    }
    
    QString status = allowed ? "允许" : "阻止";
    QString keyInfo = QString("键码: %1, 修饰符: %2").arg(event.windows_key_code).arg(event.modifiers);
    m_logger->logEvent("键盘控制", QString("%1 - %2").arg(status).arg(keyInfo), "keyboard.log", L_INFO);
}

void CEFClient::applyWindows7Optimizations()
{
    m_logger->appEvent("应用Windows 7浏览器优化");
    
    // 在Windows 7下减少不必要的处理
    m_reduceLogging = true;
    m_disableAnimations = true;
    
    // 可以在这里添加更多Windows 7特定的优化
}

void CEFClient::showDevTools()
{
    if (!m_browser) {
        m_logger->errorEvent("开发者工具操作失败：浏览器实例未初始化");
        return;
    }

    CefRefPtr<CefBrowserHost> host = m_browser->GetHost();
    if (!host) {
        m_logger->errorEvent("开发者工具操作失败：无法获取浏览器主机");
        return;
    }

    try {
        // 创建开发者工具窗口信息
        CefWindowInfo windowInfo;
        CefBrowserSettings settings;

#ifdef Q_OS_WIN
        // Windows平台：使用弹出窗口
        windowInfo.SetAsPopup(nullptr, "DevTools");
#elif defined(Q_OS_MAC)
        // macOS平台：设置窗口大小，不设置parent_view让CEF创建独立窗口
        windowInfo.x = 100;
        windowInfo.y = 100;
        windowInfo.width = 1024;
        windowInfo.height = 768;
        windowInfo.hidden = false;
        windowInfo.parent_view = nullptr;
        CefString(&windowInfo.window_name).FromASCII("DevTools");
#else
        // Linux平台：设置窗口大小
        windowInfo.x = 100;
        windowInfo.y = 100;
        windowInfo.width = 1024;
        windowInfo.height = 768;
        windowInfo.parent_window = 0;
        CefString(&windowInfo.window_name).FromASCII("DevTools");
#endif

        // 显示开发者工具
        host->ShowDevTools(windowInfo, nullptr, settings, CefPoint());

        logSecurityEvent("开发者工具", "已开启");
        m_logger->appEvent("CEF DevTools窗口已请求创建");
    } catch (...) {
        m_logger->errorEvent("开发者工具开启异常");
    }
}

void CEFClient::closeDevTools()
{
    if (!m_browser) {
        m_logger->errorEvent("开发者工具操作失败：浏览器实例未初始化");
        return;
    }
    
    CefRefPtr<CefBrowserHost> host = m_browser->GetHost();
    if (!host) {
        m_logger->errorEvent("开发者工具操作失败：无法获取浏览器主机");
        return;
    }
    
    try {
        // 关闭开发者工具
        host->CloseDevTools();
        
        logSecurityEvent("开发者工具", "已关闭");
    } catch (...) {
        m_logger->errorEvent("开发者工具关闭异常");
    }
}

bool CEFClient::handleWindows7KeyEvent(const CefKeyEvent& event)
{
    // Windows 7特殊键盘事件处理
    // 更严格的过滤以避免兼容性问题
    
    if (isAllowedShortcut(event)) {
        return false; // 允许
    }
    
    // 基本输入键
    if (event.modifiers == 0 || event.modifiers == EVENTFLAG_SHIFT_DOWN) {
        return false; // 允许
    }
    
    // 其他都阻止
    return true;
}
