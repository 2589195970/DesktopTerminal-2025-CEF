#include "cef_app_impl.h"
#include "../logging/logger.h"
#include "../core/application.h"

#include "include/cef_browser.h"
#include "include/cef_command_line.h"
#include "include/cef_v8.h"
#include "include/wrapper/cef_helpers.h"

CEFApp::CEFApp()
    : m_logger(&Logger::instance())
    , m_lowMemoryMode(false)
    , m_strictSecurityMode(true)
    , m_windows7CompatibilityMode(false)
    , m_reduceLogging(false)
    , m_browserCount(0)
    , m_renderProcessCount(0)
{
    // 根据系统特性自动配置
    if (Application::is32BitSystem()) {
        setLowMemoryMode(true);
    }
    
    if (Application::isWindows7SP1()) {
        enableWindows7Compatibility(true);
    }
    
    m_logger->appEvent("CEFApp创建完成");
}

CEFApp::~CEFApp()
{
    m_logger->appEvent("CEFApp销毁");
}

// ==================== CefApp接口实现 ====================

void CEFApp::OnBeforeCommandLineProcessing(const CefString& process_type, CefRefPtr<CefCommandLine> command_line)
{
    std::string processTypeStr = process_type.ToString();
    
    if (!m_reduceLogging) {
        m_logger->appEvent(QString("处理命令行参数，进程类型: %1").arg(QString::fromStdString(processTypeStr)));
    }
    
    // 应用安全标志
    applySecurityFlags(command_line);
    
    // 应用性能标志
    applyPerformanceFlags(command_line);
    
    // 应用兼容性标志
    applyCompatibilityFlags(command_line);
    
    // 32位系统特殊优化
    if (Application::is32BitSystem()) {
        apply32BitOptimizations(command_line);
    }
    
    // Windows 7特殊标志
    if (m_windows7CompatibilityMode) {
        applyWindows7Flags(command_line);
    }
}

void CEFApp::OnRegisterCustomSchemes(CefRefPtr<CefSchemeRegistrar> registrar)
{
    // 注册自定义协议（如果需要）
    // 例如：exam:// 协议用于安全的考试资源
    
    // registrar->AddCustomScheme("exam", 
    //     CEF_SCHEME_OPTION_STANDARD | 
    //     CEF_SCHEME_OPTION_LOCAL | 
    //     CEF_SCHEME_OPTION_SECURE);
    
    m_logger->appEvent("自定义协议注册完成");
}

// ==================== CefBrowserProcessHandler接口实现 ====================

void CEFApp::OnContextInitialized()
{
    CEF_REQUIRE_UI_THREAD();
    
    m_logger->appEvent("CEF上下文初始化完成");
    
    // 设置消息处理器
    setupMessageHandlers();
}

void CEFApp::OnBeforeChildProcessLaunch(CefRefPtr<CefCommandLine> command_line)
{
    // 子进程启动前的配置
    if (m_lowMemoryMode) {
        command_line->AppendSwitch("--max-old-space-size=128");
        command_line->AppendSwitch("--memory-pressure-off");
    }
    
    if (m_strictSecurityMode) {
        command_line->AppendSwitch("--disable-web-security");
        command_line->AppendSwitch("--disable-features=VizDisplayCompositor");
    }
    
    m_logger->appEvent("子进程启动配置完成");
}

void CEFApp::OnRenderProcessThreadCreated(CefRefPtr<CefListValue> extra_info)
{
    m_renderProcessCount++;
    
    if (!m_reduceLogging) {
        m_logger->appEvent(QString("渲染进程线程创建，总数: %1").arg(m_renderProcessCount));
    }
}

// ==================== CefRenderProcessHandler接口实现 ====================

void CEFApp::OnRenderThreadCreated(CefRefPtr<CefListValue> extra_info)
{
    CEF_REQUIRE_RENDERER_THREAD();
    
    if (!m_reduceLogging) {
        m_logger->appEvent("渲染线程创建");
    }
}

void CEFApp::OnWebKitInitialized()
{
    CEF_REQUIRE_RENDERER_THREAD();
    
    m_logger->appEvent("WebKit初始化完成");
    
    // 在这里可以注册自定义的JavaScript扩展
    // 但在安全模式下，我们通常不需要额外的扩展
}

// 注意：CEF 75中此方法可能不存在，暂时注释掉
/*
void CEFApp::OnBrowserCreated(CefRefPtr<CefBrowser> browser, CefRefPtr<CefDictionaryValue> extra_info)
{
    CEF_REQUIRE_RENDERER_THREAD();
    
    m_browserCount++;
    
    if (!m_reduceLogging) {
        m_logger->appEvent(QString("渲染进程中浏览器创建，ID: %1，总数: %2")
            .arg(browser->GetIdentifier()).arg(m_browserCount));
    }
}
*/

void CEFApp::OnBrowserDestroyed(CefRefPtr<CefBrowser> browser)
{
    CEF_REQUIRE_RENDERER_THREAD();
    
    m_browserCount--;
    
    if (!m_reduceLogging) {
        m_logger->appEvent(QString("渲染进程中浏览器销毁，ID: %1，剩余: %2")
            .arg(browser->GetIdentifier()).arg(m_browserCount));
    }
}

CefRefPtr<CefLoadHandler> CEFApp::GetLoadHandler()
{
    // 返回nullptr使用默认处理，或者返回自定义的LoadHandler
    return nullptr;
}

bool CEFApp::OnBeforeNavigation(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, CefRefPtr<CefRequest> request, NavigationType navigation_type, bool is_redirect)
{
    CEF_REQUIRE_RENDERER_THREAD();
    
    // 在渲染进程中进行额外的导航检查
    std::string url = request->GetURL().ToString();
    
    if (!m_reduceLogging) {
        m_logger->appEvent(QString("渲染进程导航检查: %1").arg(QString::fromStdString(url)));
    }
    
    // 这里可以添加渲染进程级别的安全检查
    // 返回true阻止导航，false允许导航
    return false;
}

void CEFApp::OnContextCreated(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, CefRefPtr<CefV8Context> context)
{
    CEF_REQUIRE_RENDERER_THREAD();
    
    if (frame->IsMain()) {
        if (!m_reduceLogging) {
            m_logger->appEvent(QString("V8上下文创建，浏览器ID: %1").arg(browser->GetIdentifier()));
        }
        
        // 注入安全脚本
        if (m_strictSecurityMode) {
            injectSecurityScript(browser, frame, context);
        }
    }
}

void CEFApp::OnContextReleased(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, CefRefPtr<CefV8Context> context)
{
    CEF_REQUIRE_RENDERER_THREAD();
    
    if (frame->IsMain() && !m_reduceLogging) {
        m_logger->appEvent(QString("V8上下文释放，浏览器ID: %1").arg(browser->GetIdentifier()));
    }
}

void CEFApp::OnUncaughtException(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, CefRefPtr<CefV8Context> context, CefRefPtr<CefV8Exception> exception, CefRefPtr<CefV8StackTrace> stackTrace)
{
    CEF_REQUIRE_RENDERER_THREAD();
    
    // 记录JavaScript异常
    logV8Exception(exception, stackTrace);
    
    // 在严格安全模式下，JavaScript异常可能表示安全问题
    if (m_strictSecurityMode) {
        std::string url = frame->GetURL().ToString();
        m_logger->logEvent("安全警告", 
            QString("检测到JavaScript异常，URL: %1").arg(QString::fromStdString(url)), 
            "security.log", L_WARNING);
    }
}

bool CEFApp::OnProcessMessageReceived(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, CefProcessId source_process, CefRefPtr<CefProcessMessage> message)
{
    CEF_REQUIRE_RENDERER_THREAD();
    
    std::string messageName = message->GetName().ToString();
    
    if (!m_reduceLogging) {
        m_logger->appEvent(QString("收到进程消息: %1").arg(QString::fromStdString(messageName)));
    }
    
    // 处理安全相关消息
    if (messageName.find("security") == 0) {
        return handleSecurityMessage(browser, message);
    }
    
    return false; // 未处理的消息
}

// ==================== 配置方法 ====================

void CEFApp::setLowMemoryMode(bool enable)
{
    m_lowMemoryMode = enable;
    if (enable) {
        m_reduceLogging = true;
    }
    m_logger->appEvent(QString("CEFApp低内存模式: %1").arg(enable ? "启用" : "禁用"));
}

void CEFApp::setStrictSecurityMode(bool enable)
{
    m_strictSecurityMode = enable;
    m_logger->appEvent(QString("CEFApp严格安全模式: %1").arg(enable ? "启用" : "禁用"));
}

void CEFApp::enableWindows7Compatibility(bool enable)
{
    m_windows7CompatibilityMode = enable;
    if (enable) {
        m_reduceLogging = true;
    }
    m_logger->appEvent(QString("CEFApp Windows 7兼容模式: %1").arg(enable ? "启用" : "禁用"));
}

// ==================== 私有方法实现 ====================

void CEFApp::applySecurityFlags(CefRefPtr<CefCommandLine> command_line)
{
    if (m_strictSecurityMode) {
        command_line->AppendSwitch("--disable-web-security");
        command_line->AppendSwitch("--disable-extensions");
        command_line->AppendSwitch("--disable-plugins");
        command_line->AppendSwitch("--disable-default-apps");
        command_line->AppendSwitch("--disable-sync");
        command_line->AppendSwitch("--disable-translate");
        command_line->AppendSwitch("--disable-background-networking");
    }
}

void CEFApp::applyPerformanceFlags(CefRefPtr<CefCommandLine> command_line)
{
    // 基础性能优化
    command_line->AppendSwitch("--disable-background-timer-throttling");
    command_line->AppendSwitch("--disable-renderer-backgrounding");
    command_line->AppendSwitch("--disable-backgrounding-occluded-windows");
    
    if (m_lowMemoryMode) {
        command_line->AppendSwitch("--memory-pressure-off");
        command_line->AppendSwitch("--max-old-space-size=256");
        command_line->AppendSwitch("--disable-dev-shm-usage");
    }
}

void CEFApp::applyCompatibilityFlags(CefRefPtr<CefCommandLine> command_line)
{
    // 通用兼容性标志
    command_line->AppendSwitch("--no-sandbox");
    command_line->AppendSwitch("--disable-features=VizDisplayCompositor");
    command_line->AppendSwitch("--disable-ipc-flooding-protection");
}

void CEFApp::apply32BitOptimizations(CefRefPtr<CefCommandLine> command_line)
{
    // 32位系统特殊优化
    command_line->AppendSwitch("--single-process");
    command_line->AppendSwitch("--disable-gpu");
    command_line->AppendSwitch("--disable-gpu-compositing");
    command_line->AppendSwitch("--disable-gpu-rasterization");
    command_line->AppendSwitch("--disable-software-rasterizer");
    command_line->AppendSwitch("--disable-accelerated-2d-canvas");
    command_line->AppendSwitch("--disable-accelerated-jpeg-decoding");
    command_line->AppendSwitch("--disable-accelerated-mjpeg-decode");
    command_line->AppendSwitch("--disable-accelerated-video-decode");
    command_line->AppendSwitch("--max-old-space-size=128");
    
    m_logger->appEvent("应用32位系统CEF优化参数");
}

void CEFApp::applyWindows7Flags(CefRefPtr<CefCommandLine> command_line)
{
    // Windows 7特殊兼容性标志
    command_line->AppendSwitch("--disable-d3d11");
    command_line->AppendSwitch("--disable-gpu-sandbox");
    command_line->AppendSwitch("--disable-features=AudioServiceOutOfProcess");
    command_line->AppendSwitch("--disable-features=AudioServiceSandbox");
    command_line->AppendSwitch("--disable-win32k-lockdown");
    command_line->AppendSwitch("--no-zygote");
    command_line->AppendSwitch("--disable-renderer-accessibility");
    
    m_logger->appEvent("应用Windows 7 CEF兼容性参数");
}

void CEFApp::injectSecurityScript(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, CefRefPtr<CefV8Context> context)
{
    // 禁用危险的API
    disableDangerousAPIs(context);
    
    // 设置安全监控
    setupSecurityMonitoring(context);
}

void CEFApp::disableDangerousAPIs(CefRefPtr<CefV8Context> context)
{
    CefRefPtr<CefV8Value> global = context->GetGlobal();
    
    // 禁用一些可能被滥用的API
    if (global->HasValue("eval")) {
        global->SetValue("eval", CefV8Value::CreateUndefined(), V8_PROPERTY_ATTRIBUTE_READONLY);
    }
    
    // 禁用Function构造器
    if (global->HasValue("Function")) {
        CefRefPtr<CefV8Value> function = global->GetValue("Function");
        if (function->IsFunction()) {
            // 可以选择完全禁用或者替换为安全版本
        }
    }
    
    // 限制window.open
    if (global->HasValue("open")) {
        global->SetValue("open", CefV8Value::CreateUndefined(), V8_PROPERTY_ATTRIBUTE_READONLY);
    }
}

void CEFApp::setupSecurityMonitoring(CefRefPtr<CefV8Context> context)
{
    // 设置JavaScript安全监控
    // 可以在这里注入监控脚本，检测可疑行为
    
    std::string monitoringScript = R"(
        (function() {
            // 监控可疑的DOM操作
            var originalCreateElement = document.createElement;
            document.createElement = function(tag) {
                if (tag === 'script' || tag === 'iframe') {
                    console.warn('Suspicious element creation attempt: ' + tag);
                }
                return originalCreateElement.call(this, tag);
            };
            
            // 监控XHR请求
            var originalXHR = window.XMLHttpRequest;
            window.XMLHttpRequest = function() {
                var xhr = new originalXHR();
                var originalOpen = xhr.open;
                xhr.open = function(method, url) {
                    console.log('XHR request: ' + method + ' ' + url);
                    return originalOpen.apply(this, arguments);
                };
                return xhr;
            };
        })();
    )";
    
    CefRefPtr<CefV8Value> result;
    CefRefPtr<CefV8Exception> exception;
    context->Eval(monitoringScript, "", 0, result, exception);
    
    if (exception) {
        m_logger->errorEvent("安全监控脚本注入失败");
    }
}

void CEFApp::setupMessageHandlers()
{
    // 设置进程间消息处理器
    // 用于浏览器进程和渲染进程之间的安全通信
}

bool CEFApp::handleSecurityMessage(CefRefPtr<CefBrowser> browser, CefRefPtr<CefProcessMessage> message)
{
    std::string messageName = message->GetName().ToString();
    
    if (messageName == "security.violation") {
        // 处理安全违规报告
        CefRefPtr<CefListValue> args = message->GetArgumentList();
        if (args->GetSize() > 0) {
            std::string violation = args->GetString(0).ToString();
            m_logger->logEvent("安全违规", QString::fromStdString(violation), "security.log", L_ERROR);
        }
        return true;
    }
    
    return false;
}

void CEFApp::handleRenderProcessError(const QString& error, CefRefPtr<CefBrowser> browser)
{
    m_logger->errorEvent(QString("渲染进程错误: %1").arg(error));
    
    if (browser) {
        // 可以在这里实施错误恢复策略
        // 例如重新加载页面或显示错误页面
    }
}

void CEFApp::logV8Exception(CefRefPtr<CefV8Exception> exception, CefRefPtr<CefV8StackTrace> stackTrace)
{
    QString errorMessage = QString("JavaScript异常: %1").arg(QString::fromStdString(exception->GetMessage().ToString()));
    
    if (stackTrace) {
        int frameCount = stackTrace->GetFrameCount();
        for (int i = 0; i < frameCount && i < 5; ++i) { // 只记录前5帧
            CefRefPtr<CefV8StackFrame> frame = stackTrace->GetFrame(i);
            errorMessage += QString("\n  在 %1:%2:%3 (%4)")
                .arg(QString::fromStdString(frame->GetScriptName().ToString()))
                .arg(frame->GetLineNumber())
                .arg(frame->GetColumn())
                .arg(QString::fromStdString(frame->GetFunctionName().ToString()));
        }
    }
    
    m_logger->errorEvent(errorMessage);
}