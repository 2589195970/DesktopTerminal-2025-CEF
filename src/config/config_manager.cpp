#include "config_manager.h"
#include <QCoreApplication>
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonParseError>

ConfigManager& ConfigManager::instance()
{
    static ConfigManager configManager;
    return configManager;
}

ConfigManager::ConfigManager()
    : QObject(nullptr)
{
    loadConfig();
}

bool ConfigManager::loadConfig(const QString &configPath)
{
    QString exe = QCoreApplication::applicationDirPath();
    QStringList paths;

    // 配置文件搜索路径（优先使用Qt资源系统）
    paths << ":/resources/config.json";  // Qt嵌入资源
    paths << exe + "/config.json";
    paths << QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation) + "/config.json";
    
#ifdef Q_OS_UNIX
    paths << "/etc/zdf-exam-desktop/config.json";
#endif
    
    paths << exe + "/" + configPath;
    paths << exe + "/../" + configPath;
    paths << configPath;
    
    if (QDir::isAbsolutePath(configPath)) {
        paths << configPath;
    }

    for (const QString &path : paths) {
        QFile file(path);
        if (!file.exists()) {
            continue;
        }
        
        if (!file.open(QIODevice::ReadOnly)) {
            continue;
        }
        
        QJsonParseError error;
        QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &error);
        file.close();
        
        if (doc.isNull() || !doc.isObject()) {
            continue;
        }
        
        config = doc.object();
        if (!validateConfig()) {
            continue;
        }
        
        actualConfigPath = path;
        return true;
    }
    
    return false;
}

bool ConfigManager::validateConfig() const
{
    // 验证必需的字段
    QStringList requiredFields = {"url", "exitPassword", "appName"};
    
    for (const QString &field : requiredFields) {
        if (!config.contains(field) || config[field].toString().isEmpty()) {
            return false;
        }
    }
    
    return true;
}

bool ConfigManager::isLoaded() const
{
    // 检查配置是否已加载（config对象不为空且通过验证）
    return !config.isEmpty() && validateConfig();
}

bool ConfigManager::createDefaultConfig(const QString &path)
{
    QJsonObject defaultConfig;
    
    // 基础配置（与原项目相同）
    defaultConfig["url"] = "http://stu.sdzdf.com/";
    defaultConfig["exitPassword"] = "sdzdf@2025";
    defaultConfig["appName"] = "智多分机考桌面端";
    defaultConfig["iconPath"] = "logo.svg";
    defaultConfig["appVersion"] = "1.0.0";
    
    // 性能和兼容性配置
    defaultConfig["disableHardwareAcceleration"] = false;
    defaultConfig["maxMemoryMB"] = 512;
    defaultConfig["lowMemoryMode"] = false;
    defaultConfig["processModel"] = "process-per-site";
    
    // CEF特定配置
    defaultConfig["cefLogLevel"] = "WARNING";
    defaultConfig["cefSingleProcessMode"] = false;
    defaultConfig["cefCacheSizeMB"] = 128;
    defaultConfig["cefWebSecurityEnabled"] = true;
    defaultConfig["cefUserAgent"] = "";
    
    // 安全策略配置
    defaultConfig["strictSecurityMode"] = true;
    defaultConfig["keyboardFilterEnabled"] = true;
    defaultConfig["contextMenuEnabled"] = false;
    defaultConfig["downloadEnabled"] = false;
    defaultConfig["javascriptDialogEnabled"] = false;
    defaultConfig["developerModeEnabled"] = true; // 默认开启开发者模式
    defaultConfig["developerModeEnabled"] = true; // 默认开启开发者模式
    
    // 架构和兼容性配置
    defaultConfig["autoArchDetection"] = true;
    defaultConfig["forceWindows7CompatMode"] = false;
    defaultConfig["forceLowMemoryMode"] = false;
    defaultConfig["forcedCEFVersion"] = "";
    
    // 日志配置
    defaultConfig["logLevel"] = "INFO";
    defaultConfig["logBufferingEnabled"] = true;
    defaultConfig["logFlushIntervalSeconds"] = 5;

    // 网络检查配置
    defaultConfig["checkUrl"] = "http://www.baidu.com";
    defaultConfig["backupCheckUrls"] = QJsonArray::fromStringList({"http://www.bing.com"});
    defaultConfig["networkCheckTimeout"] = 5000; // 5秒

    // 确保目录存在
    QFileInfo fileInfo(path);
    QDir dir = fileInfo.dir();
    if (!dir.exists() && !dir.mkpath(".")) {
        return false;
    }

    // 写入文件
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }
    
    file.write(QJsonDocument(defaultConfig).toJson());
    file.close();
    
    return true;
}

// 基础配置获取方法（与原项目完全相同）
QString ConfigManager::getUrl() const
{
    return config.value("url").toString("http://stu.sdzdf.com/");
}

QString ConfigManager::getExitPassword() const
{
    return config.value("exitPassword").toString("123456");
}

QString ConfigManager::getAppName() const
{
    return config.value("appName").toString("zdf-exam-desktop");
}

QString ConfigManager::getActualConfigPath() const
{
    return actualConfigPath;
}

// 性能和兼容性配置
bool ConfigManager::isHardwareAccelerationDisabled() const
{
    return config.value("disableHardwareAcceleration").toBool(false);
}

int ConfigManager::getMaxMemoryMB() const
{
    return config.value("maxMemoryMB").toInt(512);
}

bool ConfigManager::isLowMemoryMode() const
{
    return config.value("lowMemoryMode").toBool(false);
}

QString ConfigManager::getProcessModel() const
{
    return config.value("processModel").toString("process-per-site");
}

// CEF特定配置
QString ConfigManager::getCEFLogLevel() const
{
    return config.value("cefLogLevel").toString("WARNING");
}

bool ConfigManager::isCEFSingleProcessMode() const
{
    return config.value("cefSingleProcessMode").toBool(false);
}

int ConfigManager::getCEFCacheSizeMB() const
{
    return config.value("cefCacheSizeMB").toInt(128);
}

bool ConfigManager::isCEFWebSecurityEnabled() const
{
    return config.value("cefWebSecurityEnabled").toBool(true);
}

QString ConfigManager::getCEFUserAgent() const
{
    return config.value("cefUserAgent").toString("");
}

// 安全策略配置
bool ConfigManager::isStrictSecurityMode() const
{
    return config.value("strictSecurityMode").toBool(true);
}

bool ConfigManager::isKeyboardFilterEnabled() const
{
    return config.value("keyboardFilterEnabled").toBool(true);
}

bool ConfigManager::isContextMenuEnabled() const
{
    return config.value("contextMenuEnabled").toBool(false);
}

bool ConfigManager::isDownloadEnabled() const
{
    return config.value("downloadEnabled").toBool(false);
}

bool ConfigManager::isJavaScriptDialogEnabled() const
{
    return config.value("javascriptDialogEnabled").toBool(false);
}

// 开发者模式配置
bool ConfigManager::isDeveloperModeEnabled() const
{
    return config.value("developerModeEnabled").toBool(false);
}

bool ConfigManager::isUrlExitEnabled() const
{
    return config.value("urlExitEnabled").toBool(true);
}

QString ConfigManager::getUrlExitPattern() const
{
    return config.value("urlExitPattern").toString("/logout");
}

// 架构和兼容性配置
bool ConfigManager::isAutoArchDetectionEnabled() const
{
    return config.value("autoArchDetection").toBool(true);
}

bool ConfigManager::isWindows7CompatModeForced() const
{
    return config.value("forceWindows7CompatMode").toBool(false);
}

bool ConfigManager::isLowMemoryModeForced() const
{
    return config.value("forceLowMemoryMode").toBool(false);
}

QString ConfigManager::getForcedCEFVersion() const
{
    return config.value("forcedCEFVersion").toString("");
}

// 日志配置
QString ConfigManager::getLogLevel() const
{
    return config.value("logLevel").toString("INFO");
}

bool ConfigManager::isLogBufferingEnabled() const
{
    return config.value("logBufferingEnabled").toBool(true);
}

int ConfigManager::getLogFlushIntervalSeconds() const
{
    return config.value("logFlushIntervalSeconds").toInt(5);
}

// 网络检查配置
QString ConfigManager::getCheckUrl() const
{
    return config.value("checkUrl").toString("http://www.baidu.com");
}

QStringList ConfigManager::getBackupCheckUrls() const
{
    QStringList urls;
    QJsonArray array = config.value("backupCheckUrls").toArray();
    for (const QJsonValue &value : array) {
        urls.append(value.toString());
    }
    return urls;
}

int ConfigManager::getNetworkCheckTimeout() const
{
    return config.value("networkCheckTimeout").toInt(5000);
}