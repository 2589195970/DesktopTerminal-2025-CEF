#ifndef CEF_APP_H
#define CEF_APP_H

#include "include/cef_app.h"
#include "include/cef_browser_process_handler.h"
#include "include/cef_render_process_handler.h"

#include <QString>

class Logger;

/**
 * @brief CEF应用程序实现类
 * 
 * 实现CEF应用程序接口，处理浏览器进程和渲染进程的初始化
 * 针对32位系统和Windows 7 SP1进行了特殊优化
 */
class CEFApp : public CefApp,
               public CefBrowserProcessHandler,
               public CefRenderProcessHandler
{
public:
    CEFApp();
    virtual ~CEFApp();

    // CefApp接口
    virtual CefRefPtr<CefBrowserProcessHandler> GetBrowserProcessHandler() override { return this; }
    virtual CefRefPtr<CefRenderProcessHandler> GetRenderProcessHandler() override { return this; }
    virtual void OnBeforeCommandLineProcessing(const CefString& process_type, CefRefPtr<CefCommandLine> command_line) override;
    virtual void OnRegisterCustomSchemes(CefRawPtr<CefSchemeRegistrar> registrar) override;

    // CefBrowserProcessHandler接口
    virtual void OnContextInitialized() override;
    virtual void OnBeforeChildProcessLaunch(CefRefPtr<CefCommandLine> command_line) override;
    virtual void OnRenderProcessThreadCreated(CefRefPtr<CefListValue> extra_info) override;

    // CefRenderProcessHandler接口
    virtual void OnRenderThreadCreated(CefRefPtr<CefListValue> extra_info) override;
    virtual void OnWebKitInitialized() override;
    virtual void OnBrowserCreated(CefRefPtr<CefBrowser> browser, CefRefPtr<CefDictionary> extra_info) override;
    virtual void OnBrowserDestroyed(CefRefPtr<CefBrowser> browser) override;
    virtual CefRefPtr<CefLoadHandler> GetLoadHandler() override;
    virtual bool OnBeforeNavigation(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, CefRefPtr<CefRequest> request, NavigationType navigation_type, bool is_redirect) override;
    virtual void OnContextCreated(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, CefRefPtr<CefV8Context> context) override;
    virtual void OnContextReleased(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, CefRefPtr<CefV8Context> context) override;
    virtual void OnUncaughtException(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, CefRefPtr<CefV8Context> context, CefRefPtr<CefV8Exception> exception, CefRefPtr<CefV8StackTrace> stackTrace) override;
    virtual bool OnProcessMessageReceived(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, CefProcessId source_process, CefRefPtr<CefProcessMessage> message) override;

    // 配置方法
    void setLowMemoryMode(bool enable);
    void setStrictSecurityMode(bool enable);
    void enableWindows7Compatibility(bool enable);

private:
    // 命令行参数处理
    void applySecurityFlags(CefRefPtr<CefCommandLine> command_line);
    void applyPerformanceFlags(CefRefPtr<CefCommandLine> command_line);
    void applyCompatibilityFlags(CefRefPtr<CefCommandLine> command_line);
    void apply32BitOptimizations(CefRefPtr<CefCommandLine> command_line);
    void applyWindows7Flags(CefRefPtr<CefCommandLine> command_line);

    // JavaScript安全控制
    void injectSecurityScript(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, CefRefPtr<CefV8Context> context);
    void disableDangerousAPIs(CefRefPtr<CefV8Context> context);
    void setupSecurityMonitoring(CefRefPtr<CefV8Context> context);

    // 进程间通信
    void setupMessageHandlers();
    bool handleSecurityMessage(CefRefPtr<CefBrowser> browser, CefRefPtr<CefProcessMessage> message);

    // 错误处理
    void handleRenderProcessError(const QString& error, CefRefPtr<CefBrowser> browser = nullptr);
    void logV8Exception(CefRefPtr<CefV8Exception> exception, CefRefPtr<CefV8StackTrace> stackTrace);

private:
    Logger* m_logger;
    
    // 配置标志
    bool m_lowMemoryMode;
    bool m_strictSecurityMode;
    bool m_windows7CompatibilityMode;
    bool m_reduceLogging;

    // 统计信息
    int m_browserCount;
    int m_renderProcessCount;

    IMPLEMENT_REFCOUNTING(CEFApp);
};

#endif // CEF_APP_H