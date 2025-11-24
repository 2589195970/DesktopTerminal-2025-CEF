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
    if (!loadConfig()) {
        qFatal("配置文件加载失败，程序无法启动");
    }
}

bool ConfigManager::loadConfig(const QString &configPath)
{
    QString exe = QCoreApplication::applicationDirPath();
    QString path = exe + "/resources/config.json";

    QFile file(path);
    if (!file.exists()) {
        qWarning() << "配置文件不存在:" << path << "，使用内置默认配置";

        // 使用内置默认配置
        config = QJsonObject{
            {"url", "http://stu.sdzdf.com/"},
            {"exitPassword", "sdzdf@2025"},
            {"appName", "智多分机考桌面端"},
            {"configVersion", "1.0.0-builtin"},
            {"cefLogLevel", "WARNING"},
            {"strictSecurityMode", true},
            {"keyboardFilterEnabled", true},
            {"contextMenuEnabled", false}
        };

        actualConfigPath = "builtin";
        return true;
    }

    if (!file.open(QIODevice::ReadOnly)) {
        qCritical() << "无法打开配置文件:" << path;
        return false;
    }

    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &error);
    file.close();

    if (doc.isNull() || !doc.isObject()) {
        qCritical() << "配置文件JSON格式错误:" << error.errorString();
        return false;
    }

    config = doc.object();
    if (!validateConfig()) {
        qCritical() << "配置文件验证失败";
        return false;
    }

    actualConfigPath = path;
    return true;
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
    // 不再支持创建默认配置
    qCritical() << "不支持创建默认配置，必须提供有效的config.json文件";
    return false;
}

// 基础配置获取方法
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

QString ConfigManager::getConfigVersion() const
{
    return config.value("configVersion").toString("unknown");
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