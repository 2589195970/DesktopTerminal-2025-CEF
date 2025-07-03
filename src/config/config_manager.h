#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include <QObject>
#include <QString>
#include <QJsonObject>

/**
 * @brief 配置管理器类
 * 
 * 负责配置文件的加载、验证和访问
 * 保持与原项目完全相同的接口，并增加了架构检测相关配置
 */
class ConfigManager : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief 获取单例实例
     */
    static ConfigManager& instance();

    /**
     * @brief 加载配置文件
     * @param configPath 配置文件路径（默认为resources/config.json）
     * @return 成功返回true
     */
    bool loadConfig(const QString &configPath = "resources/config.json");

    /**
     * @brief 验证配置文件内容
     * @return 配置有效返回true
     */
    bool validateConfig() const;

    /**
     * @brief 创建默认配置文件
     * @param path 配置文件路径
     * @return 成功返回true
     */
    bool createDefaultConfig(const QString &path);

    // 基础配置获取方法（与原项目完全相同）
    QString getUrl() const;
    QString getExitPassword() const;
    QString getAppName() const;
    QString getActualConfigPath() const;

    // 性能和兼容性配置
    bool isHardwareAccelerationDisabled() const;
    int getMaxMemoryMB() const;
    bool isLowMemoryMode() const;
    QString getProcessModel() const;

    // CEF特定配置（新增）
    QString getCEFLogLevel() const;
    bool isCEFSingleProcessMode() const;
    int getCEFCacheSizeMB() const;
    bool isCEFWebSecurityEnabled() const;
    QString getCEFUserAgent() const;

    // 安全策略配置（新增）
    bool isStrictSecurityMode() const;
    bool isKeyboardFilterEnabled() const;
    bool isContextMenuEnabled() const;
    bool isDownloadEnabled() const;
    bool isJavaScriptDialogEnabled() const;
    
    // URL退出配置（新增）
    bool isUrlExitEnabled() const;
    QString getUrlExitPattern() const;

    // 架构和兼容性配置（新增）
    bool isAutoArchDetectionEnabled() const;
    bool isWindows7CompatModeForced() const;
    bool isLowMemoryModeForced() const;
    QString getForcedCEFVersion() const;

    // 日志配置
    QString getLogLevel() const;
    bool isLogBufferingEnabled() const;
    int getLogFlushIntervalSeconds() const;

    // 直接访问配置对象（与原项目兼容）
    QJsonObject config;

private:
    ConfigManager();
    ~ConfigManager() = default;
    ConfigManager(const ConfigManager&) = delete;
    ConfigManager& operator=(const ConfigManager&) = delete;

    QString actualConfigPath;
};

#endif // CONFIG_MANAGER_H