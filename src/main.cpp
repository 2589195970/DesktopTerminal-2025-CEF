#include <QApplication>
#include <QDir>
#include <QStandardPaths>
#include <QMessageBox>
#include <QTextCodec>

#include "core/application.h"
#include "core/secure_browser.h"
#include "logging/logger.h"
#include "config/config_manager.h"

#ifdef Q_OS_WIN
#include <windows.h>
#include <io.h>
#include <fcntl.h>
#include <shellapi.h>
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
 * @brief 检查并处理管理员权限
 * @param argc 命令行参数个数
 * @param argv 命令行参数数组
 * @param logger 日志器引用
 * @return true：继续执行，false：需要退出程序
 */
bool checkAndHandleAdminPrivileges(int argc, char* argv[], Logger& logger)
{
    if (isRunningAsAdministrator()) {
        logger.appEvent("应用程序正在以管理员权限运行");
        return true;
    }
    
    logger.appEvent("应用程序未以管理员权限运行");
    
    // 检查是否为静默模式（避免在自动化环境中弹出UAC对话框）
    bool silentMode = false;
    for (int i = 1; i < argc; ++i) {
        if (QString(argv[i]).toLower().contains("silent") ||
            QString(argv[i]).toLower().contains("batch")) {
            silentMode = true;
            break;
        }
    }
    
    if (silentMode) {
        logger.appEvent("静默模式下跳过管理员权限检查");
        return true;
    }
    
    // 询问用户是否需要管理员权限
    QMessageBox::StandardButton reply = QMessageBox::question(
        nullptr,
        "管理员权限",
        "检测到应用程序未以管理员权限运行。\n\n"
        "为确保所有安全功能正常工作，建议以管理员权限运行。\n\n"
        "是否现在重新以管理员权限启动？\n\n"
        "注意：选择\"否\"可能导致部分安全功能无法正常工作。",
        QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel,
        QMessageBox::Yes
    );
    
    switch (reply) {
        case QMessageBox::Yes:
            logger.appEvent("用户选择重新以管理员权限启动");
            if (requestAdminPrivileges(argc, argv)) {
                logger.appEvent("已请求管理员权限重新启动，当前进程将退出");
                return false;  // 退出当前进程
            } else {
                logger.errorEvent("请求管理员权限失败");
                QMessageBox::warning(
                    nullptr,
                    "权限请求失败",
                    "无法请求管理员权限。\n\n"
                    "您可以手动右键点击程序图标选择\"以管理员身份运行\"。\n\n"
                    "程序将以当前权限继续运行，但部分功能可能受限。"
                );
                return true;
            }
            
        case QMessageBox::No:
            logger.appEvent("用户选择以当前权限继续运行");
            QMessageBox::information(
                nullptr,
                "权限提示",
                "程序将以当前权限运行。\n\n"
                "注意：部分安全功能可能无法正常工作。"
            );
            return true;
            
        case QMessageBox::Cancel:
        default:
            logger.appEvent("用户取消启动");
            return false;
    }
}
#endif

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    
    // 设置应用程序基本信息
    app.setApplicationName("DesktopTerminal-CEF");
    app.setApplicationVersion("1.0.0");
    app.setOrganizationName("ZDF");
    app.setOrganizationDomain("sdzdf.com");
    
#ifdef Q_OS_WIN
    // Windows下设置UTF-8编码
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
    
    // 禁用Windows错误对话框
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
#endif
    
    // 初始化日志系统
    Logger& logger = Logger::instance();
    logger.appEvent("=== DesktopTerminal-CEF 启动 ===");
    logger.appEvent(QString("Qt版本: %1").arg(QT_VERSION_STR));
    logger.appEvent(QString("应用程序路径: %1").arg(app.applicationDirPath()));
    
    // 记录系统信息
    logger.logSystemInfo();
    
#ifdef Q_OS_WIN
    // Windows平台：检查管理员权限（作为清单文件的备用方案）
    if (!checkAndHandleAdminPrivileges(argc, argv, logger)) {
        // 用户选择退出或权限请求成功（将重新启动），当前进程退出
        logger.appEvent("由于权限检查结果，应用程序即将退出");
        return 0;
    }
#endif
    
    // 初始化配置管理器
    ConfigManager& configManager = ConfigManager::instance();
    if (!configManager.loadConfig()) {
        logger.errorEvent("配置文件加载失败");
        
        // 尝试创建默认配置
        QString configPath = app.applicationDirPath() + "/config.json";
        if (configManager.createDefaultConfig(configPath)) {
            logger.appEvent(QString("已创建默认配置文件: %1").arg(configPath));
            if (!configManager.loadConfig(configPath)) {
                QMessageBox::critical(nullptr, "配置错误", 
                    "无法加载配置文件，程序将退出。\\n\\n请检查配置文件格式或联系管理员。");
                return -1;
            }
        } else {
            QMessageBox::critical(nullptr, "配置错误", 
                "无法创建配置文件，程序将退出。\\n\\n请检查文件权限或联系管理员。");
            return -1;
        }
    }
    
    logger.appEvent(QString("配置文件加载成功: %1").arg(configManager.getActualConfigPath()));
    logger.appEvent(QString("应用程序名称: %1").arg(configManager.getAppName()));
    logger.appEvent(QString("目标URL: %1").arg(configManager.getUrl()));
    
    // 创建应用程序实例
    Application application(argc, argv);
    
    // 检查系统兼容性
    if (!Application::checkSystemRequirements()) {
        logger.errorEvent("系统兼容性检查失败");
        QMessageBox::critical(nullptr, "系统兼容性错误", 
            "当前系统不满足运行要求。\\n\\n详细信息请查看日志文件。");
        return -2;
    }
    
    // 使用同步初始化（直接启动）
    if (!application.initialize()) {
        logger.errorEvent("应用程序初始化失败");
        QMessageBox::critical(nullptr, "初始化错误", 
            "应用程序初始化失败。\\n\\n详细信息请查看日志文件。");
        return -3;
    }

    // 启动主窗口
    if (!application.startMainWindow()) {
        logger.errorEvent("主窗口启动失败");
        QMessageBox::critical(nullptr, "启动错误", 
            "主窗口启动失败。\\n\\n详细信息请查看日志文件。");
        return -4;
    }

    // 显示主窗口
    if (auto* mainWindow = application.getMainWindow()) {
        mainWindow->show();
        mainWindow->raise();
        mainWindow->activateWindow();
        logger.appEvent("主窗口已显示");
    }
    
    logger.appEvent("应用程序启动完成，进入事件循环");
    
    // 启动性能监控
    logger.startPerformanceMonitoring();
    
    // 运行应用程序
    int result = app.exec();
    
    logger.appEvent(QString("应用程序退出，返回码: %1").arg(result));
    logger.appEvent("=== DesktopTerminal-CEF 关闭 ===");
    
    return result;
}