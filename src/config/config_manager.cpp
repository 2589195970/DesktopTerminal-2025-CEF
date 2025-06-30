#include "config_manager.h"
#include "../logging/logger.h"

#include <QJsonDocument>
#include <QJsonArray>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QStandardPaths>
#include <QCoreApplication>

ConfigManager::ConfigManager()
{
    // 构造函数，单例模式
}

ConfigManager& ConfigManager::instance()
{
    static ConfigManager instance;
    return instance;
}

bool ConfigManager::loadConfig(const QString &configPath)
{
    // 尝试多个路径查找配置文件
    QStringList searchPaths;
    
    // 1. 指定路径
    if (!configPath.isEmpty()) {
        searchPaths << configPath;
    }
    
    // 2. 应用程序目录下的资源目录
    QString appDir = QCoreApplication::applicationDirPath();
    searchPaths << QDir(appDir).filePath("resources/config.json");
    searchPaths << QDir(appDir).filePath("config.json");
    
    // 3. 父目录（用于开发环境）
    QString parentDir = QDir(appDir).dirName() == "build" ? 
        QDir(appDir + "/../resources/config.json").absolutePath() :
        QDir(appDir + "/../resources/config.json").absolutePath();
    searchPaths << parentDir;
    
    // 4. 用户配置目录
    QString userConfigDir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    searchPaths << QDir(userConfigDir).filePath("config.json");
    
    // 查找配置文件
    QString foundPath;
    for (const QString& path : searchPaths) {
        if (QFileInfo::exists(path)) {
            foundPath = path;
            break;
        }
    }
    
    if (foundPath.isEmpty()) {
        // 创建默认配置
        QString defaultPath = QDir(appDir).filePath("resources/config.json");
        if (createDefaultConfig(defaultPath)) {
            foundPath = defaultPath;
        } else {
            return false;
        }
    }
    
    // 读取配置文件
    QFile file(foundPath);
    if (!file.open(QIODevice::ReadOnly)) {
        Logger::instance().errorEvent(QString("无法打开配置文件: %1").arg(foundPath));
        return false;
    }
    
    QByteArray data = file.readAll();
    file.close();
    
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(data, &error);
    
    if (error.error != QJsonParseError::NoError) {
        Logger::instance().errorEvent(QString("配置文件JSON解析错误: %1").arg(error.errorString()));
        return false;
    }
    
    config = doc.object();
    actualConfigPath = foundPath;
    
    // 验证配置
    if (!validateConfig()) {
        Logger::instance().errorEvent("配置文件验证失败");
        return false;
    }
    
    Logger::instance().configEvent(QString("配置文件加载成功: %1").arg(foundPath));
    return true;
}

bool ConfigManager::validateConfig() const
{
    // 验证必需字段
    if (!config.contains("url") || config["url"].toString().isEmpty()) {
        Logger::instance().errorEvent("配置缺少必需字段: url");
        return false;
    }
    
    if (!config.contains("exitPassword") || config["exitPassword"].toString().isEmpty()) {
        Logger::instance().errorEvent("配置缺少必需字段: exitPassword");
        return false;
    }
    
    if (!config.contains("appName") || config["appName"].toString().isEmpty()) {
        Logger::instance().errorEvent("配置缺少必需字段: appName");
        return false;
    }
    
    return true;
}

bool ConfigManager::createDefaultConfig(const QString &path)
{
    QJsonObject defaultConfig;
    
    // 基础配置
    defaultConfig["url"] = "http://stu.sdzdf.com?Client='ExamTerminal'";
    defaultConfig["exitPassword"] = "sdzdf@2025";
    defaultConfig["appName"] = "智多分机考桌面端-CEF";
    defaultConfig["iconPath"] = "logo.ico";
    defaultConfig["appVersion"] = "1.0.0";
    
    // CEF设置
    QJsonObject cefSettings;
    cefSettings["enableGPU"] = true;
    cefSettings["enableWebSecurity"] = true;
    cefSettings["enableJavaScript"] = true;
    cefSettings["debugPort"] = 0;
    cefSettings["multiThreadedMessageLoop"] = true;
    defaultConfig["cefSettings"] = cefSettings;
    
    // URL检测与自动退出配置
    QJsonObject urlExitDetection;
    urlExitDetection["enabled"] = false;  // 默认禁用
    urlExitDetection["pattern"] = "^https?://[^/]+/#/login_s$";  // 标准模式
    QJsonArray patterns;
    patterns.append("^https?://[^/]+/#/login_s$");
    urlExitDetection["patterns"] = patterns;
    urlExitDetection["delayMs"] = 1000;  // 1秒延迟
    urlExitDetection["confirmationEnabled"] = false;  // 禁用确认对话框
    defaultConfig["urlExitDetection"] = urlExitDetection;
    
    // 安全策略
    QJsonObject security;
    security["strictMode"] = true;
    security["keyboardFilterEnabled"] = true;
    security["contextMenuEnabled"] = false;
    security["downloadEnabled"] = false;
    security["javascriptDialogEnabled"] = false;
    defaultConfig["security"] = security;
    
    // 性能配置
    QJsonObject performance;
    performance["hardwareAccelerationDisabled"] = false;
    performance["maxMemoryMB"] = 1024;
    performance["lowMemoryMode"] = false;
    performance["processModel"] = "auto";
    defaultConfig["performance"] = performance;
    
    // 日志配置
    QJsonObject logging;
    logging["level"] = "INFO";
    logging["bufferingEnabled"] = true;
    logging["flushIntervalSeconds"] = 5;
    defaultConfig["logging"] = logging;
    
    // 确保目录存在
    QFileInfo fileInfo(path);
    QDir dir = fileInfo.dir();
    if (!dir.exists()) {
        if (!dir.mkpath(".")) {
            Logger::instance().errorEvent(QString("无法创建配置目录: %1").arg(dir.absolutePath()));
            return false;
        }
    }
    
    // 写入文件
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        Logger::instance().errorEvent(QString("无法创建配置文件: %1").arg(path));
        return false;
    }
    
    QJsonDocument doc(defaultConfig);
    file.write(doc.toJson(QJsonDocument::Indented));
    file.close();
    
    Logger::instance().configEvent(QString("创建默认配置文件: %1").arg(path));
    return true;
}

// ==================== 基础配置获取方法 ====================

QString ConfigManager::getUrl() const
{
    return config.value("url").toString();
}

QString ConfigManager::getExitPassword() const
{
    return config.value("exitPassword").toString();
}

QString ConfigManager::getAppName() const
{
    return config.value("appName").toString();
}

QString ConfigManager::getActualConfigPath() const
{
    return actualConfigPath;
}

// ==================== 性能和兼容性配置 ====================

bool ConfigManager::isHardwareAccelerationDisabled() const
{
    return config.value("performance").toObject().value("hardwareAccelerationDisabled").toBool(false);
}

int ConfigManager::getMaxMemoryMB() const
{
    return config.value("performance").toObject().value("maxMemoryMB").toInt(1024);
}

bool ConfigManager::isLowMemoryMode() const
{
    return config.value("performance").toObject().value("lowMemoryMode").toBool(false);
}

QString ConfigManager::getProcessModel() const
{
    return config.value("performance").toObject().value("processModel").toString("auto");
}

// ==================== CEF特定配置 ====================

QString ConfigManager::getCEFLogLevel() const
{
    return config.value("cefSettings").toObject().value("logLevel").toString("WARNING");
}

bool ConfigManager::isCEFSingleProcessMode() const
{
    return config.value("cefSettings").toObject().value("singleProcess").toBool(false);
}

int ConfigManager::getCEFCacheSizeMB() const
{
    return config.value("cefSettings").toObject().value("cacheSizeMB").toInt(64);
}

bool ConfigManager::isCEFWebSecurityEnabled() const
{
    return config.value("cefSettings").toObject().value("enableWebSecurity").toBool(true);
}

QString ConfigManager::getCEFUserAgent() const
{
    return config.value("cefSettings").toObject().value("userAgent").toString();
}

// ==================== 安全策略配置 ====================

bool ConfigManager::isStrictSecurityMode() const
{
    return config.value("security").toObject().value("strictMode").toBool(true);
}

bool ConfigManager::isKeyboardFilterEnabled() const
{
    return config.value("security").toObject().value("keyboardFilterEnabled").toBool(true);
}

bool ConfigManager::isContextMenuEnabled() const
{
    return config.value("security").toObject().value("contextMenuEnabled").toBool(false);
}

bool ConfigManager::isDownloadEnabled() const
{
    return config.value("security").toObject().value("downloadEnabled").toBool(false);
}

bool ConfigManager::isJavaScriptDialogEnabled() const
{
    return config.value("security").toObject().value("javascriptDialogEnabled").toBool(false);
}

// ==================== 架构和兼容性配置 ====================

bool ConfigManager::isAutoArchDetectionEnabled() const
{
    return config.value("compatibility").toObject().value("autoArchDetection").toBool(true);
}

bool ConfigManager::isWindows7CompatModeForced() const
{
    return config.value("compatibility").toObject().value("forceWindows7Compat").toBool(false);
}

bool ConfigManager::isLowMemoryModeForced() const
{
    return config.value("compatibility").toObject().value("forceLowMemoryMode").toBool(false);
}

QString ConfigManager::getForcedCEFVersion() const
{
    return config.value("compatibility").toObject().value("forcedCEFVersion").toString();
}

// ==================== 日志配置 ====================

QString ConfigManager::getLogLevel() const
{
    return config.value("logging").toObject().value("level").toString("INFO");
}

bool ConfigManager::isLogBufferingEnabled() const
{
    return config.value("logging").toObject().value("bufferingEnabled").toBool(true);
}

int ConfigManager::getLogFlushIntervalSeconds() const
{
    return config.value("logging").toObject().value("flushIntervalSeconds").toInt(5);
}

// ==================== VC++运行时配置 ====================

bool ConfigManager::isVCRuntimeAutoInstallEnabled() const
{
    return config.value("vcRuntime").toObject().value("autoInstallEnabled").toBool(false);
}

bool ConfigManager::isVCRuntimePromptEnabled() const
{
    return config.value("vcRuntime").toObject().value("promptEnabled").toBool(true);
}

QString ConfigManager::getVCRuntimeInstallerFileName() const
{
    return config.value("vcRuntime").toObject().value("installerFileName").toString("VC_redist.x86.exe");
}

// ==================== URL检测与自动退出配置 ====================

bool ConfigManager::isUrlExitDetectionEnabled() const
{
    return config.value("urlExitDetection").toObject().value("enabled").toBool(false);
}

QString ConfigManager::getUrlExitDetectionPattern() const
{
    return config.value("urlExitDetection").toObject().value("pattern").toString("^https?://[^/]+/#/login_s$");
}

QStringList ConfigManager::getUrlExitDetectionPatterns() const
{
    QStringList patterns;
    QJsonArray jsonArray = config.value("urlExitDetection").toObject().value("patterns").toArray();
    
    for (const QJsonValue& value : jsonArray) {
        patterns << value.toString();
    }
    
    // 如果数组为空，返回默认模式
    if (patterns.isEmpty()) {
        patterns << getUrlExitDetectionPattern();
    }
    
    return patterns;
}

int ConfigManager::getUrlExitDetectionDelayMs() const
{
    return config.value("urlExitDetection").toObject().value("delayMs").toInt(1000);
}

bool ConfigManager::isUrlExitConfirmationEnabled() const
{
    return config.value("urlExitDetection").toObject().value("confirmationEnabled").toBool(false);
}