#include "config_manager.h"
#include <QCoreApplication>
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonParseError>
#include <QInputDialog>

ConfigManager& ConfigManager::instance()
{
    static ConfigManager configManager;
    return configManager;
}

ConfigManager::ConfigManager()
    : QObject(nullptr)
{
    // 不在构造函数中加载配置
    // loadConfig()需要在QApplication创建后调用
}

bool ConfigManager::loadConfig(const QString &configPath)
{
    m_lastError.clear();
    QString exe = QCoreApplication::applicationDirPath();
    QString targetPath = exe + "/resources/config.json";

    // 确保resources目录存在
    QDir resourcesDir(exe + "/resources");
    if (!resourcesDir.exists()) {
        resourcesDir.mkpath(".");
    }

    // 迁移旧配置文件到目标路径
    migrateConfig(targetPath);

    // 唯一的配置文件路径
    QFile file(targetPath);
    if (!file.exists()) {
        m_lastError = QString("配置文件不存在\n路径: %1\n应用目录: %2").arg(targetPath, exe);
        return false;
    }

    if (!file.open(QIODevice::ReadOnly)) {
        m_lastError = QString("无法打开配置文件\n路径: %1\n错误: %2").arg(targetPath, file.errorString());
        return false;
    }

    QByteArray data = file.readAll();
    file.close();

    // 跳过UTF-8 BOM（如果存在）
    if (data.size() >= 3 &&
        static_cast<unsigned char>(data[0]) == 0xEF &&
        static_cast<unsigned char>(data[1]) == 0xBB &&
        static_cast<unsigned char>(data[2]) == 0xBF) {
        data = data.mid(3);
    }

    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(data, &error);

    if (doc.isNull() || !doc.isObject()) {
        m_lastError = QString("JSON解析失败\n路径: %1\n错误: %2\n位置: %3").arg(targetPath, error.errorString()).arg(error.offset);
        return false;
    }

    config = doc.object();
    if (!validateConfig()) {
        QStringList missing;
        QStringList requiredFields = {"url", "exitPassword", "appName"};
        for (const QString &field : requiredFields) {
            if (!config.contains(field) || config[field].toString().isEmpty()) {
                missing << field;
            }
        }
        m_lastError = QString("配置验证失败\n路径: %1\n缺少字段: %2").arg(targetPath, missing.join(", "));
        return false;
    }

    actualConfigPath = targetPath;
    return true;
}

QString ConfigManager::getLastError() const
{
    return m_lastError;
}

void ConfigManager::migrateConfig(const QString &targetPath)
{
    QString exe = QCoreApplication::applicationDirPath();
    QStringList oldPaths;

    // 旧配置文件可能存在的位置
    oldPaths << exe + "/config.json";
    oldPaths << QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation) + "/config.json";
#ifdef Q_OS_UNIX
    oldPaths << "/etc/zdf-exam-desktop/config.json";
#endif

    // 如果目标路径已存在有效配置，不需要迁移
    QFile targetFile(targetPath);
    if (targetFile.exists()) {
        return;
    }

    // 尝试从旧路径迁移
    for (const QString &oldPath : oldPaths) {
        QFile oldFile(oldPath);
        if (!oldFile.exists()) {
            continue;
        }

        // 验证旧配置文件是否有效
        if (!oldFile.open(QIODevice::ReadOnly)) {
            continue;
        }

        QJsonParseError error;
        QJsonDocument doc = QJsonDocument::fromJson(oldFile.readAll(), &error);
        oldFile.close();

        if (doc.isNull() || !doc.isObject()) {
            continue;
        }

        // 复制到目标路径
        if (QFile::copy(oldPath, targetPath)) {
            qInfo() << "配置文件已迁移:" << oldPath << "->" << targetPath;
            // 删除旧文件
            QFile::remove(oldPath);
            return;
        }
    }
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

// 敏感操作密码配置
bool ConfigManager::isSensitiveOperationPasswordRequired() const
{
    return config.value("sensitiveOperationRequirePassword").toBool(true);
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