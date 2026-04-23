#include <QApplication>
#include <QDir>
#include <QStandardPaths>
#include <QMessageBox>
#include <QTextCodec>
#include <QMetaObject>
#include <QInputDialog>

#include "core/application.h"
#include "core/secure_browser.h"
#include "logging/logger.h"
#include "config/config_manager.h"
#include "ui/loading_dialog.h"
#include "cef/cef_app_impl.h"

#include "include/cef_app.h"

#ifdef Q_OS_WIN
#include <windows.h>
#include <io.h>
#include <fcntl.h>
#include <shellapi.h>
#include "include/cef_sandbox_win.h"
#endif

#ifdef Q_OS_WIN
/**
 * @brief 检查当前进程是否具有管理员权限
 * @return true：具有管理员权限，false：没有管理员权限
 */
bool isRunningAsAdministrator()
{
    BOOL isAdmin = FALSE;
    PSID adminGroup = nullptr;
    SID_IDENTIFIER_AUTHORITY ntAuthority = SECURITY_NT_AUTHORITY;
    
    // 创建管理员组的SID
    if (AllocateAndInitializeSid(
        &ntAuthority,
        2,
        SECURITY_BUILTIN_DOMAIN_RID,
        DOMAIN_ALIAS_RID_ADMINS,
        0, 0, 0, 0, 0, 0,
        &adminGroup)) {
        
        // 检查当前用户是否属于管理员组
        if (!CheckTokenMembership(nullptr, adminGroup, &isAdmin)) {
            isAdmin = FALSE;
        }
        
        FreeSid(adminGroup);
    }
    
    return isAdmin == TRUE;
}

/**
 * @brief 请求管理员权限重新启动应用程序
 * @param argc 命令行参数个数
 * @param argv 命令行参数数组
 * @return true：成功请求重新启动，false：用户拒绝或失败
 */
bool requestAdminPrivileges(int argc, char* argv[])
{
    // 获取当前可执行文件路径
    wchar_t szPath[MAX_PATH];
    if (GetModuleFileNameW(nullptr, szPath, ARRAYSIZE(szPath)) == 0) {
        return false;
    }
    
    // 构建命令行参数
    QString parameters;
    for (int i = 1; i < argc; ++i) {
        if (i > 1) parameters += " ";
        parameters += QString("\"%1\"").arg(argv[i]);
    }
    
    // 使用ShellExecute请求管理员权限
    HINSTANCE result = ShellExecuteW(
        nullptr,
        L"runas",           // 请求管理员权限
        szPath,             // 可执行文件路径
        parameters.toStdWString().c_str(),  // 命令行参数
        nullptr,            // 工作目录
        SW_NORMAL           // 显示方式
    );
    
    // 检查结果
    return reinterpret_cast<intptr_t>(result) > 32;
}

/**
 * @brief 要求管理员权限，否则退出当前进程
 * @param argc 命令行参数个数
 * @param argv 命令行参数数组
 * @param logger 日志器引用
 * @return true：继续执行，false：需要退出程序
 */
bool requireAdminPrivilegesOrExit(int argc, char* argv[], Logger& logger)
{
    // 检查是否已具有管理员权限
    if (isRunningAsAdministrator()) {
        logger.appEvent("应用程序正在以管理员权限运行");
        return true;  // 继续执行
    }

    // 未以管理员权限运行，尝试提权
    logger.appEvent("应用程序未以管理员权限运行，尝试提权");

    // 请求以管理员权限重新启动
    if (requestAdminPrivileges(argc, argv)) {
        logger.appEvent("管理员权限提升请求已成功发起，当前进程将静默退出");
        return false;  // 退出当前进程，等待新进程启动
    }

    // 提权失败（用户拒绝或系统错误）
    logger.errorEvent("管理员权限提升请求失败或被用户拒绝，当前进程将静默退出");
    return false;  // 退出当前进程
}
#endif

int main(int argc, char *argv[])
{
    int originalArgc = argc;
    char** originalArgv = argv;

#ifdef Q_OS_WIN
    // ==== 三重硬声明进程为PerMonitorV2 DPI感知 ====
    // 必须在任何窗口/Qt/CEF初始化之前调用，否则manifest/API声明会被Windows锁定为首次生效的状态。
    // 优先级：SetProcessDpiAwarenessContext(Win10 1703+) > SetProcessDpiAwareness(Win8.1+) > CefEnableHighDPISupport
    {
        // 1) Windows 10 1703+: SetProcessDpiAwarenessContext（最强：覆盖子窗口、动态切屏）
        typedef BOOL (WINAPI *SetProcessDpiAwarenessContextFn)(HANDLE);
        HMODULE hUser32 = GetModuleHandleW(L"user32.dll");
        BOOL ctxOk = FALSE;
        if (hUser32) {
            auto pSetCtx = reinterpret_cast<SetProcessDpiAwarenessContextFn>(
                GetProcAddress(hUser32, "SetProcessDpiAwarenessContext"));
            if (pSetCtx) {
                // DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2 == ((HANDLE)-4)
                ctxOk = pSetCtx(reinterpret_cast<HANDLE>(-4));
            }
        }
        // 2) 次优：Windows 8.1+ SetProcessDpiAwareness（仅当上面失败时调用）
        if (!ctxOk) {
            typedef HRESULT (WINAPI *SetProcessDpiAwarenessFn)(int);
            HMODULE hShcore = LoadLibraryW(L"shcore.dll");
            if (hShcore) {
                auto pSetDpi = reinterpret_cast<SetProcessDpiAwarenessFn>(
                    GetProcAddress(hShcore, "SetProcessDpiAwareness"));
                if (pSetDpi) {
                    pSetDpi(2); // PROCESS_PER_MONITOR_DPI_AWARE
                }
                // shcore保持加载，不FreeLibrary
            }
        }
        // 3) 兜底：CEF内部也会调SetProcessDpiAwareness，若前两步成功此处为no-op
        CefEnableHighDPISupport();
    }

    CefMainArgs cefMainArgs(GetModuleHandle(nullptr));
#else
    CefMainArgs cefMainArgs(originalArgc, originalArgv);
#endif
    CefRefPtr<CEFApp> sharedCefApp(new CEFApp());
    int exit_code = CefExecuteProcess(cefMainArgs, sharedCefApp, nullptr);
    if (exit_code >= 0) {
        return exit_code;
    }

    // 注意：不在这里创建QApplication，而是使用Application类（继承自QApplication）

#ifdef Q_OS_WIN
    // Windows下设置UTF-8编码
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

    // 禁用Windows错误对话框
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
#endif

    // Qt高DPI感知开启（与上面的SetProcessDpiAwarenessContext=PerMonitorV2配合）：
    // - AA_EnableHighDpiScaling：Qt按系统DPI自动缩放QWidget布局、字体、对话框
    // - AA_UseHighDpiPixmaps：位图资源按DPR加载，避免图标/启动页模糊
    // resizeCEFBrowser()内部已通过GetClientRect取物理像素传给CEF，换算自洽。
    QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);

    // 创建应用程序实例（必须在使用任何Qt功能之前创建）
    Application application(argc, argv, originalArgc, originalArgv);
    application.setSharedCEFApp(sharedCefApp);

    // 初始化日志系统
    Logger& logger = Logger::instance();
    logger.appEvent("=== DesktopTerminal-CEF 启动 ===");
    logger.appEvent(QString("Qt版本: %1").arg(QT_VERSION_STR));
    logger.appEvent(QString("应用程序路径: %1").arg(application.applicationDirPath()));
    
    // 记录系统信息
    logger.logSystemInfo();
    
#ifdef Q_OS_WIN
    // Windows平台：检查管理员权限（作为清单文件的备用方案）
    if (!requireAdminPrivilegesOrExit(argc, argv, logger)) {
        return 0;
    }
#endif
    
    // 初始化配置管理器
    ConfigManager& configManager = ConfigManager::instance();
    if (!configManager.loadConfig()) {
        QString errorDetail = configManager.getLastError();
        logger.errorEvent(QString("配置文件加载失败: %1").arg(errorDetail));
        QMessageBox::critical(nullptr, "配置错误",
            QString("配置加载失败，程序将退出。\n\n详细信息:\n%1").arg(errorDetail));
        return -1;
    }
    
    logger.appEvent(QString("配置文件加载成功: %1").arg(configManager.getActualConfigPath()));
    logger.appEvent(QString("配置版本: %1").arg(configManager.getConfigVersion()));
    logger.appEvent(QString("应用程序名称: %1").arg(configManager.getAppName()));
    logger.appEvent(QString("目标URL: %1").arg(configManager.getUrl()));

    // 创建并显示加载对话框
    LoadingDialog* loadingDialog = new LoadingDialog();
    loadingDialog->show();
    loadingDialog->raise();
    loadingDialog->activateWindow();
    logger.appEvent("显示加载对话框");

    // 标记应用程序初始化状态
    bool applicationInitialized = false;
    bool shouldExit = false;

    // 连接系统检测完成信号（必须在startSystemCheck之前连接）
    QObject::connect(loadingDialog, &LoadingDialog::systemCheckCompleted, 
                     [&](bool checkSuccess) {
        if (checkSuccess) {
            logger.appEvent("系统检测通过，开始初始化应用程序");
            
            // 连接初始化进度信号
            QObject::connect(&application, &Application::initializationProgress,
                             loadingDialog, &LoadingDialog::setStatus);
            QObject::connect(&application, &Application::initializationError,
                             loadingDialog, &LoadingDialog::setError);
            
            // 开始应用程序初始化
            loadingDialog->startApplicationLoad();
            if (application.initialize()) {
                applicationInitialized = true;
                logger.appEvent("应用程序初始化成功，准备启动主窗口");

                // 初始化成功后，触发主窗口启动
                // 注意：不能直接调用 startMainWindow()，因为需要通过 readyToStartApplication 信号
                // 来确保所有相关的信号连接都已建立
                QMetaObject::invokeMethod(loadingDialog, "readyToStartApplication", Qt::QueuedConnection);
            } else {
                logger.errorEvent("应用程序初始化失败");
                loadingDialog->setError("应用程序初始化失败\n请查看日志文件");
            }
        } else {
            logger.errorEvent("系统检测失败，阻止应用程序启动");
            // 停留在LoadingDialog显示错误，等待用户操作
        }
    });
    
    // 处理重试和取消
    QObject::connect(loadingDialog, &LoadingDialog::retryClicked, [&]() {
        logger.appEvent("用户点击重试");
        if (!applicationInitialized) {
            // 如果应用程序还未初始化，重新进行系统检测
            loadingDialog->startSystemCheck();
        } else {
            // 如果应用程序已初始化，重新初始化应用程序
            loadingDialog->startAnimation();
            loadingDialog->setStatus("重新初始化...");
            application.initialize();
        }
    });
    
    QObject::connect(loadingDialog, &LoadingDialog::cancelClicked, [&]() {
        logger.appEvent("用户取消启动");
        shouldExit = true;
        QApplication::quit();
    });
    
    // 将启动流程移到readyToStartApplication信号的处理中
    QObject::connect(loadingDialog, &LoadingDialog::readyToStartApplication,
                     [&]() {
        // 启动主窗口
        loadingDialog->setStatus("正在创建主窗口...");
        if (!application.startMainWindow()) {
            logger.errorEvent("主窗口启动失败");
            loadingDialog->setError("主窗口启动失败\n请查看日志文件");
            return;
        }

        // 获取主窗口并连接页面加载信号
        if (auto* mainWindow = application.getMainWindow()) {
            // 连接页面加载信号
            QObject::connect(mainWindow, &SecureBrowser::pageLoadStarted, [loadingDialog]() {
                if (loadingDialog) {
                    loadingDialog->setStatus("正在加载网页...");
                }
            });
            
            QObject::connect(mainWindow, &SecureBrowser::pageLoadFinished, [loadingDialog, mainWindow]() {
                Logger::instance().appEvent("页面加载完成，关闭加载对话框");
                if (loadingDialog) {
                    loadingDialog->close();
                    loadingDialog->deleteLater();
                }
                
                // 显示主窗口
                mainWindow->show();
                mainWindow->raise();
                mainWindow->activateWindow();
                Logger::instance().appEvent("主窗口已显示");
            });
            
            // 先显示主窗口以确保窗口句柄有效
            mainWindow->show();
            
            // 确保窗口完全显示和初始化
            QApplication::processEvents();
            
            // 然后隐藏等待页面加载完成
            mainWindow->hide();
            
            // 初始化CEF浏览器（窗口句柄现在应该有效）
            loadingDialog->setStatus("正在初始化浏览器...");
            logger.appEvent("开始初始化CEF浏览器");
            mainWindow->initializeCEFBrowser();
        }
    });

    // 所有信号连接完成后，开始系统检测
    logger.appEvent("开始系统检测流程");
    loadingDialog->startSystemCheck();

    logger.appEvent("应用程序启动完成，进入事件循环");

    // 启动性能监控
    logger.startPerformanceMonitoring();

    // 运行应用程序
    int result = application.exec();
    
    logger.appEvent(QString("应用程序退出，返回码: %1").arg(result));
    logger.appEvent("=== DesktopTerminal-CEF 关闭 ===");
    
    return result;
}
