#include "cef_client_impl.h"
#include "cef_resource_request_handler_impl.h"
#include "../logging/logger.h"
#include "../config/config_manager.h"
#include "../core/application.h"
#include "../core/secure_browser.h"
#include "../core/cef_manager.h"

#include <algorithm>
#include <QApplication>
#include <QDateTime>
#include <QMetaObject>
#include <QUrl>
#include "include/base/cef_bind.h"
#include "include/wrapper/cef_closure_task.h"

#ifdef Q_OS_WIN
#include <windows.h>
#endif

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
    , m_lastCrashReloadAt(0)
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
    // DevTools 窗口也使用本客户端，只记录最先创建的考试主浏览器。
    if (!m_browser) {
        m_browser = browser;
    }
    m_browserCount++;
    
    m_logger->appEvent(QString("浏览器创建完成，ID: %1").arg(browser->GetIdentifier()));

    if (m_cefManager) {
        m_cefManager->notifyBrowserCreated(browser->GetIdentifier());
    }
    
    // Windows 7特定优化
    if (m_windows7CompatibilityMode) {
        applyWindows7Optimizations();
    }
}

bool CEFClient::DoClose(CefRefPtr<CefBrowser> browser)
{
    m_logger->appEvent(QString("浏览器关闭请求，ID: %1").arg(browser->GetIdentifier()));
#ifdef Q_OS_WIN
    // 返回 false 后 CEF 会向浏览器窗口的根窗口发送 WM_CLOSE。考试主窗口拒绝关闭，
    // 浏览器因此无法销毁，随后 CefShutdown 会卡住进程。先让浏览器窗口脱离宿主，
    // WM_CLOSE 就会发给浏览器窗口自身并完成销毁。
    const HWND browserWindow = browser->GetHost()->GetWindowHandle();
    if (browserWindow && GetAncestor(browserWindow, GA_ROOT) != browserWindow) {
        ShowWindow(browserWindow, SW_HIDE);
        SetParent(browserWindow, nullptr);
    }
#endif
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
        
        // 通知CEFManager主框架加载完成，用于替代固定延时的白屏修复
        if (m_cefManager) {
            m_cefManager->notifyMainFrameLoadEnd(httpStatusCode);
        }
        
        if (m_lowMemoryMode) {
            // 低内存模式下可在此清理资源
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
    
    // 只在主框架中检测退出模式，避免子资源和子框架误触发
    if (frame->IsMain()) {
        if (m_configManager->isUrlExitEnabled() && m_cefManager) {
            QString exitPattern = m_configManager->getUrlExitPattern();
            if (!exitPattern.isEmpty() && url.contains(exitPattern, Qt::CaseInsensitive)) {
                m_logger->appEvent(QString("检测到主框架导航命中退出模式 URL '%1': %2").arg(exitPattern, url));
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
    Q_UNUSED(browser)
    Q_UNUSED(frame)
    Q_UNUSED(request)
    Q_UNUSED(is_navigation)
    Q_UNUSED(is_download)
    Q_UNUSED(request_initiator)
    Q_UNUSED(disable_default_handling)
    return new CEFResourceRequestHandlerImpl();
}

void CEFClient::OnRenderProcessTerminated(CefRefPtr<CefBrowser> browser, TerminationStatus status)
{
    // 渲染进程异常退出后页面停在空白页且无法操作，自动重新加载一次恢复考试页面；
    // 30秒内再次退出说明页面一加载就崩溃，不再自动重载，避免死循环，由人工按Ctrl+R或工具箱刷新。
    constexpr qint64 crashReloadIntervalMs = 30000;
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    const bool reload = now - m_lastCrashReloadAt > crashReloadIntervalMs;
    m_logger->errorEvent(QString("渲染进程异常退出，状态: %1，%2")
        .arg(static_cast<int>(status))
        .arg(reload ? "自动重新加载页面" : "短时间内重复退出，等待手动刷新"));
    if (reload && browser) {
        m_lastCrashReloadAt = now;
        browser->Reload();
    }
}

// ==================== CefKeyboardHandler接口实现（安全控制）====================

bool CEFClient::OnPreKeyEvent(CefRefPtr<CefBrowser> browser, const CefKeyEvent& event, CefEventHandle os_event, bool* is_keyboard_shortcut)
{
    constexpr int f11KeyCode = 122;
    constexpr uint32 shortcutModifiers = EVENTFLAG_SHIFT_DOWN | EVENTFLAG_CONTROL_DOWN |
        EVENTFLAG_ALT_DOWN | EVENTFLAG_COMMAND_DOWN;
    if (event.windows_key_code == f11KeyCode && !(event.modifiers & shortcutModifiers) && !browser->IsPopup()) {
        if (event.type == KEYEVENT_RAWKEYDOWN
#ifdef Q_OS_WIN
            && !(event.native_key_code & (1 << 30))
#endif
        ) {
            if (auto *application = qobject_cast<Application *>(QApplication::instance())) {
                QMetaObject::invokeMethod(application, [application] {
                    if (auto *window = application->getMainWindow()) {
                        window->openToolbox();
                    }
                }, Qt::QueuedConnection);
            }
        }
        return true;
    }

    if (handleExitHotkeyEvent(event)) {
        if (is_keyboard_shortcut) {
            *is_keyboard_shortcut = true;
        }
        return true;
    }

    if (!browser->IsPopup() && handleReloadHotkeyEvent(event)) {
        return true;
    }

    if (!m_keyboardFilterEnabled) {
        return false;
    }
    
    // Windows 7特殊处理
    if (m_windows7CompatibilityMode) {
        return handleWindows7KeyEvent(event);
    }
    
    // 临时调试：记录所有键盘事件
    if (event.modifiers & EVENTFLAG_IS_KEY_PAD) {
        QString debugInfo = QString("小键盘事件 - keycode:%1, modifiers:0x%2, type:%3")
            .arg(event.windows_key_code)
            .arg(event.modifiers, 0, 16)
            .arg(event.type);
        m_logger->appEvent(debugInfo);
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
    
    // 严格模式下不显示原生对话框。必须按 CEF 约定以 suppress_message=true 且返回 false 的方式抑制，
    // 由 CEF 立即应答页面（confirm 视为取消）；返回 true 而不执行 callback 会让页面永久等待应答而卡死。
    if (m_strictSecurityMode) {
        logSecurityEvent("JavaScript对话框被阻止", QString("来源: %1, 消息: %2").arg(origin).arg(message));
        suppress_message = true;
        return false;
    }
    
    return false; // 允许对话框
}

bool CEFClient::OnBeforeUnloadDialog(CefRefPtr<CefBrowser> browser, const CefString& message_text, bool is_reload, CefRefPtr<CefJSDialogCallback> callback)
{
    // 严格模式下不显示离开确认框，直接允许离开（刷新、重置登录等导航由本程序或监考发起）。
    // 返回 true 时必须执行 callback，否则页面会一直等待确认结果，所有按钮无法点击且无法刷新。
    if (m_strictSecurityMode) {
        logSecurityEvent("页面卸载对话框被跳过并允许离开",
            QString("%1%2").arg(is_reload ? "刷新: " : "").arg(QString::fromStdString(message_text.ToString())));
        callback->Continue(true, CefString());
        return true;
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

bool CEFClient::handleExitHotkeyEvent(const CefKeyEvent& event)
{
    const uint32 modifiersWithoutLocks = event.modifiers & ~(
        EVENTFLAG_NUM_LOCK_ON |
        EVENTFLAG_CAPS_LOCK_ON |
        EVENTFLAG_IS_KEY_PAD |
        EVENTFLAG_IS_LEFT |
        EVENTFLAG_IS_RIGHT
    );

    if (event.type != KEYEVENT_RAWKEYDOWN ||
        event.windows_key_code != 121 ||
        modifiersWithoutLocks != 0) {
        return false;
    }

    m_logger->hotkeyEvent("CEF F10安全退出热键被触发");
    QObject* applicationObject = QApplication::instance();
    if (!applicationObject) {
        return true;
    }

    QMetaObject::invokeMethod(applicationObject, []() {
        Application* application = qobject_cast<Application*>(QApplication::instance());
        if (application) {
            application->requestSafeExit(nullptr, "CEF F10");
        }
    }, Qt::QueuedConnection);

    return true;
}

bool CEFClient::handleReloadHotkeyEvent(const CefKeyEvent& event)
{
    // 只用 Ctrl+R 刷新；F5 紧挨 F4/F6，考试中容易误按导致未保存的作答丢失，不作为刷新键
    const uint32 modifiersWithoutLocks = event.modifiers & ~(
        EVENTFLAG_NUM_LOCK_ON |
        EVENTFLAG_CAPS_LOCK_ON |
        EVENTFLAG_IS_KEY_PAD |
        EVENTFLAG_IS_LEFT |
        EVENTFLAG_IS_RIGHT
    );
    if (event.windows_key_code != 'R' || modifiersWithoutLocks != EVENTFLAG_CONTROL_DOWN) {
        return false;
    }

    // 按下、抬起和字符事件都吞掉，只在首次按下时刷新，长按不重复刷新
    if (event.type == KEYEVENT_RAWKEYDOWN
#ifdef Q_OS_WIN
        && !(event.native_key_code & (1 << 30))
#endif
    ) {
        m_logger->hotkeyEvent("Ctrl+R刷新热键被触发");
        reloadPageOnUIThread();
    }
    return true;
}

bool CEFClient::isKeyEventAllowed(const CefKeyEvent& event)
{
    // 允许的快捷键：Ctrl+R（刷新）
    if (event.modifiers == EVENTFLAG_CONTROL_DOWN && event.windows_key_code == 'R') {
        return true;
    }

    // 优先检查小键盘按键（在移除修饰符之前）
    // 小键盘数字键 (VK_NUMPAD0=96 到 VK_NUMPAD9=105)
    if (event.windows_key_code >= 96 && event.windows_key_code <= 105) {
        // 只允许无修饰符或仅Shift的小键盘数字输入
        uint32 mods = event.modifiers & ~(EVENTFLAG_NUM_LOCK_ON | EVENTFLAG_CAPS_LOCK_ON | EVENTFLAG_IS_KEY_PAD | EVENTFLAG_IS_LEFT | EVENTFLAG_IS_RIGHT);
        if (mods == 0 || mods == EVENTFLAG_SHIFT_DOWN) {
            return true;
        }
    }

    // 小键盘运算符 (VK_MULTIPLY=106 到 VK_DIVIDE=111)
    if (event.windows_key_code >= 106 && event.windows_key_code <= 111) {
        uint32 mods = event.modifiers & ~(EVENTFLAG_NUM_LOCK_ON | EVENTFLAG_CAPS_LOCK_ON | EVENTFLAG_IS_KEY_PAD | EVENTFLAG_IS_LEFT | EVENTFLAG_IS_RIGHT);
        if (mods == 0 || mods == EVENTFLAG_SHIFT_DOWN) {
            return true;
        }
    }

    // 移除所有非安全相关的修饰符（NumLock、CapsLock、小键盘标志等）
    uint32 modifiersWithoutLocks = event.modifiers & ~(
        EVENTFLAG_NUM_LOCK_ON |
        EVENTFLAG_CAPS_LOCK_ON |
        EVENTFLAG_IS_KEY_PAD |
        EVENTFLAG_IS_LEFT |
        EVENTFLAG_IS_RIGHT
    );

    // 允许普通字符输入
    if (modifiersWithoutLocks == 0 || modifiersWithoutLocks == EVENTFLAG_SHIFT_DOWN) {
        return true;
    }

    // 允许方向键、回车、退格等基本导航键
    if (event.windows_key_code >= 37 && event.windows_key_code <= 40) {
        return true;
    }

    if (event.windows_key_code == 13 || event.windows_key_code == 8 ||
        event.windows_key_code == 9 || event.windows_key_code == 27) {
        return true;
    }

    // 允许Delete键 (VK_DELETE=46)
    if (event.windows_key_code == 46) {
        return true;
    }

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

void CEFClient::reloadPage()
{
    if (!CefCurrentlyOn(TID_UI)) {
        CefPostTask(TID_UI, base::Bind(&CEFClient::reloadPageOnUIThread, this));
        return;
    }

    reloadPageOnUIThread();
}

void CEFClient::reloadPageOnUIThread()
{
    if (!m_browser) {
        m_logger->errorEvent("刷新页面失败：浏览器实例未初始化");
        return;
    }

    m_logger->appEvent("重新加载页面（忽略缓存）");
    m_browser->ReloadIgnoreCache();
}

void CEFClient::showDevTools()
{
    if (!m_browser) {
        m_logger->errorEvent("开发者工具操作失败：浏览器实例未初始化");
        return;
    }

    // 必须在CEF UI线程上执行ShowDevTools
    if (!CefCurrentlyOn(TID_UI)) {
        CefPostTask(TID_UI, base::Bind(&CEFClient::showDevToolsOnUIThread, this));
        return;
    }

    showDevToolsOnUIThread();
}

void CEFClient::showDevToolsOnUIThread()
{
    if (!m_browser) {
        m_logger->errorEvent("DevTools: 浏览器实例为空");
        return;
    }

    CefRefPtr<CefBrowserHost> host = m_browser->GetHost();
    if (!host) {
        m_logger->errorEvent("DevTools: 浏览器主机为空");
        return;
    }

    CefWindowInfo windowInfo;
    CefBrowserSettings settings;

#ifdef Q_OS_WIN
    HWND parentHwnd = host->GetWindowHandle();
    if (!parentHwnd) {
        m_logger->errorEvent("DevTools: 无法获取父窗口句柄");
        return;
    }

    windowInfo.SetAsPopup(parentHwnd, "DesktopTerminal DevTools");
    windowInfo.x = 100;
    windowInfo.y = 100;
    windowInfo.width = 1200;
    windowInfo.height = 800;

    m_logger->appEvent(QString("DevTools: Windows平台，父窗口句柄=0x%1")
        .arg(reinterpret_cast<qulonglong>(parentHwnd), 0, 16));
#elif defined(Q_OS_MAC)
    CefString(&windowInfo.window_name).FromASCII("DesktopTerminal DevTools");
    windowInfo.x = 100;
    windowInfo.y = 100;
    windowInfo.width = 1200;
    windowInfo.height = 800;
    windowInfo.hidden = false;

    m_logger->appEvent("DevTools: macOS平台，创建独立窗口");
#else
    CefString(&windowInfo.window_name).FromASCII("DesktopTerminal DevTools");
    windowInfo.x = 100;
    windowInfo.y = 100;
    windowInfo.width = 1200;
    windowInfo.height = 800;

    m_logger->appEvent("DevTools: Linux平台，创建独立窗口");
#endif

    settings.javascript = STATE_ENABLED;
    settings.javascript_close_windows = STATE_ENABLED;
    settings.javascript_access_clipboard = STATE_ENABLED;
    settings.web_security = STATE_DISABLED;
    settings.file_access_from_file_urls = STATE_ENABLED;
    settings.universal_access_from_file_urls = STATE_ENABLED;
    settings.image_loading = STATE_ENABLED;
    settings.background_color = 0xFFFFFFFF;

    m_logger->appEvent("DevTools: 开始调用ShowDevTools");
    m_logger->appEvent(QString("  浏览器ID: %1").arg(m_browser->GetIdentifier()));
    m_logger->appEvent(QString("  是否单进程模式: %1").arg(
        Application::is32BitSystem() || Application::isWindows7SP1() ? "是" : "否"));

    try {
        host->ShowDevTools(windowInfo, this, settings, CefPoint());
        m_logger->appEvent("DevTools: ShowDevTools调用成功");
        m_logger->appEvent("==================================================");
        m_logger->appEvent("DevTools故障排查指南:");
        m_logger->appEvent("1. 如果DevTools窗口是白屏，这通常是因为：");
        m_logger->appEvent("   a) CEF消息循环未正常运行（请检查日志中的'CEF消息循环正常运行'）");
        m_logger->appEvent("   b) 单进程模式下DevTools渲染需要额外时间（等待5-10秒）");
        m_logger->appEvent("   c) GPU加速问题（32位/Win7系统已禁用GPU）");
        m_logger->appEvent("");
        m_logger->appEvent("2. 可以通过以下方式访问DevTools：");
        m_logger->appEvent("   a) F12弹出的窗口（如果不是白屏）");
        m_logger->appEvent("   b) 浏览器中访问: chrome://inspect");
        m_logger->appEvent("   c) 访问远程调试接口: http://localhost:9222");
        m_logger->appEvent("");
        m_logger->appEvent("3. 如果持续白屏，请尝试：");
        m_logger->appEvent("   a) 等待10秒看是否能加载出来");
        m_logger->appEvent("   b) 关闭DevTools窗口再重新按F12");
        m_logger->appEvent("   c) 使用浏览器访问 http://localhost:9222 查看调试目标");
        m_logger->appEvent("==================================================");
    } catch (...) {
        m_logger->errorEvent("DevTools: ShowDevTools调用发生异常");
    }
}

void CEFClient::closeDevTools()
{
    if (!m_browser) {
        return;
    }

    // 必须在CEF UI线程上执行CloseDevTools
    if (!CefCurrentlyOn(TID_UI)) {
        CefPostTask(TID_UI, base::Bind(&CEFClient::closeDevToolsOnUIThread, this));
        return;
    }

    closeDevToolsOnUIThread();
}

void CEFClient::resizeBrowser(int width, int height)
{
    if (!m_browser) {
        // CreateBrowser是异步的，OnAfterCreated前m_browser为空属于正常时序
        return;
    }

    width = std::max(width, 1);
    height = std::max(height, 1);

    if (!CefCurrentlyOn(TID_UI)) {
        CefPostTask(TID_UI, base::Bind(&CEFClient::resizeBrowserOnUIThread, this, width, height));
        return;
    }

    resizeBrowserOnUIThread(width, height);
}

void CEFClient::setBrowserZoomLevel(double zoomLevel)
{
    if (!m_browser) {
        return;
    }

    if (!CefCurrentlyOn(TID_UI)) {
        CefPostTask(TID_UI, base::Bind(&CEFClient::setBrowserZoomLevelOnUIThread, this, zoomLevel));
        return;
    }

    setBrowserZoomLevelOnUIThread(zoomLevel);
}

void CEFClient::closeDevToolsOnUIThread()
{
    if (!m_browser) {
        return;
    }

    CefRefPtr<CefBrowserHost> host = m_browser->GetHost();
    if (host) {
        host->CloseDevTools();
        m_logger->appEvent("CEF DevTools窗口已关闭");
    }
}

void CEFClient::resizeBrowserOnUIThread(int width, int height)
{
    if (!m_browser) {
        return;
    }

    CefRefPtr<CefBrowserHost> host = m_browser->GetHost();
    if (!host) {
        return;
    }

#ifdef Q_OS_WIN
    HWND browserWindow = host->GetWindowHandle();
    if (browserWindow) {
        // ==== 关键修复：CEF UI 线程的 DPI awareness 不一定是 PerMonitorV2 ====
        // CEF 75 是 2019 年版本，早于 Win10 1803 的 DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2，
        // 其内部线程创建时可能继承默认/非感知上下文。若当前线程非 V2，
        // MoveWindow 的坐标会被 Windows 按 96 DPI 虚拟化（实际尺寸=传入/DPR），
        // 导致 CEF 子 HWND 只占 Qt 父窗口的 1/DPR × 1/DPR（视觉上左上一块）。
        typedef HANDLE (WINAPI *SetThreadDpiAwarenessContextFn)(HANDLE);
        typedef HANDLE (WINAPI *GetWindowDpiAwarenessContextFn)(HWND);
        typedef int (WINAPI *GetAwarenessFromDpiAwarenessContextFn)(HANDLE);

        static SetThreadDpiAwarenessContextFn pSetThreadDpi = nullptr;
        static GetWindowDpiAwarenessContextFn pGetWindowDpi = nullptr;
        static GetAwarenessFromDpiAwarenessContextFn pGetAwareFromCtx = nullptr;
        static bool apiInited = false;
        if (!apiInited) {
            HMODULE hUser32 = GetModuleHandleW(L"user32.dll");
            if (hUser32) {
                pSetThreadDpi = reinterpret_cast<SetThreadDpiAwarenessContextFn>(
                    GetProcAddress(hUser32, "SetThreadDpiAwarenessContext"));
                pGetWindowDpi = reinterpret_cast<GetWindowDpiAwarenessContextFn>(
                    GetProcAddress(hUser32, "GetWindowDpiAwarenessContext"));
                pGetAwareFromCtx = reinterpret_cast<GetAwarenessFromDpiAwarenessContextFn>(
                    GetProcAddress(hUser32, "GetAwarenessFromDpiAwarenessContext"));
            }
            apiInited = true;
        }

        // 切换当前线程到 PerMonitorV2，保证 MoveWindow 按物理像素解释
        HANDLE prevCtx = nullptr;
        if (pSetThreadDpi) {
            prevCtx = pSetThreadDpi(reinterpret_cast<HANDLE>(-4)); // DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2
        }

        // 查询 CEF 子 HWND 原本的 DPI awareness（诊断用）
        int hwndAware = -1;
        if (pGetWindowDpi && pGetAwareFromCtx) {
            HANDLE ctx = pGetWindowDpi(browserWindow);
            if (ctx) hwndAware = pGetAwareFromCtx(ctx);
        }

        BOOL mvOk = MoveWindow(browserWindow, 0, 0, width, height, TRUE);
        DWORD mvErr = mvOk ? 0 : GetLastError();

        // 验证 MoveWindow 是否生效
        RECT actual{};
        GetWindowRect(browserWindow, &actual);
        int actualW = actual.right - actual.left;
        int actualH = actual.bottom - actual.top;

        m_logger->appEvent(QString("[CEF子HWND] 传入=%1x%2 MoveWindow=%3(err=%4) 实际=%5x%6 HWND_DpiAware=%7")
            .arg(width).arg(height)
            .arg(mvOk ? "OK" : "FAIL").arg(mvErr)
            .arg(actualW).arg(actualH)
            .arg(hwndAware));

        if (!mvOk) {
            m_logger->errorEvent(QString("调整CEF原生窗口大小失败，错误码: %1").arg(mvErr));
        }

        // 恢复线程 DPI context
        if (pSetThreadDpi && prevCtx) {
            pSetThreadDpi(prevCtx);
        }
    }
#endif

    host->WasResized();
}

void CEFClient::setBrowserZoomLevelOnUIThread(double zoomLevel)
{
    if (!m_browser) {
        return;
    }

    CefRefPtr<CefBrowserHost> host = m_browser->GetHost();
    if (!host) {
        return;
    }

    host->SetZoomLevel(zoomLevel);
}

bool CEFClient::handleWindows7KeyEvent(const CefKeyEvent& event)
{
    // Windows 7特殊键盘事件处理
    // 更严格的过滤以避免兼容性问题

    // 允许的快捷键：Ctrl+R（刷新）
    if (isAllowedShortcut(event)) {
        return false; // 允许
    }

    // 移除所有非安全相关的修饰符（NumLock、CapsLock、小键盘标志等）
    uint32 modifiersWithoutLocks = event.modifiers & ~(
        EVENTFLAG_NUM_LOCK_ON |
        EVENTFLAG_CAPS_LOCK_ON |
        EVENTFLAG_IS_KEY_PAD |
        EVENTFLAG_IS_LEFT |
        EVENTFLAG_IS_RIGHT
    );

    // 基本输入键（包括小键盘）
    if (modifiersWithoutLocks == 0 || modifiersWithoutLocks == EVENTFLAG_SHIFT_DOWN) {
        return false; // 允许
    }

    // 其他都阻止
    return true;
}
