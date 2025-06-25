#include <QApplication>
#include <QDir>
#include <QStandardPaths>
#include <QMessageBox>
#include <QTextCodec>

#include "core/application.h"
#include "logging/logger.h"
#include "config/config_manager.h"

#ifdef Q_OS_WIN
#include <windows.h>
#include <io.h>
#include <fcntl.h>
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
    
    // 初始化应用程序
    if (!application.initialize()) {
        logger.errorEvent("应用程序初始化失败");
        QMessageBox::critical(nullptr, "初始化错误", 
            "应用程序初始化失败。\\n\\n详细信息请查看日志文件。");
        return -3;
    }
    
    // 显示主窗口
    if (!application.startMainWindow()) {
        logger.errorEvent("主窗口显示失败");
        QMessageBox::critical(nullptr, "窗口错误", 
            "无法显示主窗口。\\n\\n详细信息请查看日志文件。");
        return -4;
    }
    
    logger.appEvent("应用程序启动完成，进入事件循环");
    
    // 运行应用程序
    int result = app.exec();
    
    logger.appEvent(QString("应用程序退出，返回码: %1").arg(result));
    logger.appEvent("=== DesktopTerminal-CEF 关闭 ===");
    
    return result;
}