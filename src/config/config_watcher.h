#ifndef CONFIG_WATCHER_H
#define CONFIG_WATCHER_H

#include <QObject>
#include <QFileSystemWatcher>
#include <QTimer>
#include <QDateTime>
#include <QJsonObject>

class ConfigValidator;

/**
 * @brief 配置文件监控器类
 * 
 * 监控配置文件变化并提供热重载功能
 */
class ConfigWatcher : public QObject
{
    Q_OBJECT

public:
    explicit ConfigWatcher(QObject* parent = nullptr);
    ~ConfigWatcher();

    /**
     * @brief 开始监控配置文件
     * @param configPath 配置文件路径
     * @return 是否成功开始监控
     */
    bool startWatching(const QString& configPath);

    /**
     * @brief 停止监控
     */
    void stopWatching();

    /**
     * @brief 设置配置验证器
     * @param validator 验证器实例
     */
    void setValidator(ConfigValidator* validator);

    /**
     * @brief 设置重载延迟时间
     * @param delayMs 延迟时间（毫秒）
     */
    void setReloadDelay(int delayMs);

    /**
     * @brief 设置是否启用自动重载
     * @param enabled 是否启用
     */
    void setAutoReloadEnabled(bool enabled);

    /**
     * @brief 获取当前监控的文件路径
     */
    QString getWatchedPath() const;

    /**
     * @brief 检查是否正在监控
     */
    bool isWatching() const;

    /**
     * @brief 手动触发重载
     */
    void manualReload();

    /**
     * @brief 获取最后修改时间
     */
    QDateTime getLastModified() const;

signals:
    /**
     * @brief 配置文件变化信号
     * @param filePath 文件路径
     */
    void configFileChanged(const QString& filePath);

    /**
     * @brief 配置重载完成信号
     * @param success 是否成功
     * @param config 新配置（成功时）
     * @param error 错误信息（失败时）
     */
    void configReloaded(bool success, const QJsonObject& config, const QString& error);

    /**
     * @brief 配置验证失败信号
     * @param errors 验证错误列表
     * @param warnings 警告列表
     */
    void configValidationFailed(const QStringList& errors, const QStringList& warnings);

    /**
     * @brief 配置文件移除信号
     */
    void configFileRemoved(const QString& filePath);

private slots:
    void onFileChanged(const QString& path);
    void onDirectoryChanged(const QString& path);
    void onReloadTimer();

private:
    // 重载方法
    void scheduleReload();
    void performReload();
    bool validateAndLoadConfig(const QString& configPath, QJsonObject& config, QString& error);
    
    // 文件监控辅助方法
    void setupFileWatcher();
    void cleanup();
    bool isConfigFile(const QString& filePath) const;
    void updateLastModified();

private:
    QFileSystemWatcher* m_fileWatcher;
    ConfigValidator* m_validator;
    QTimer* m_reloadTimer;
    
    QString m_configPath;
    QString m_configDir;
    QDateTime m_lastModified;
    
    bool m_isWatching;
    bool m_autoReloadEnabled;
    int m_reloadDelay;
    
    // 防止重复重载
    bool m_reloadPending;
    QDateTime m_lastReloadTime;
    static const int MIN_RELOAD_INTERVAL = 1000; // 最小重载间隔1秒
};

/**
 * @brief 配置热重载管理器
 * 
 * 集成配置管理器、验证器和监控器，提供完整的配置热重载解决方案
 */
class ConfigHotReloadManager : public QObject
{
    Q_OBJECT

public:
    explicit ConfigHotReloadManager(QObject* parent = nullptr);
    ~ConfigHotReloadManager();

    /**
     * @brief 初始化热重载系统
     * @param configPath 配置文件路径
     * @return 是否成功初始化
     */
    bool initialize(const QString& configPath);

    /**
     * @brief 关闭热重载系统
     */
    void shutdown();

    /**
     * @brief 获取当前配置
     */
    QJsonObject getCurrentConfig() const;

    /**
     * @brief 启用/禁用自动重载
     * @param enabled 是否启用
     */
    void setAutoReloadEnabled(bool enabled);

    /**
     * @brief 手动重载配置
     */
    void reloadConfig();

    /**
     * @brief 获取配置监控器
     */
    ConfigWatcher* getWatcher() const;

    /**
     * @brief 获取配置验证器
     */
    ConfigValidator* getValidator() const;

signals:
    /**
     * @brief 配置更新信号
     * @param newConfig 新配置
     * @param changedFields 变更的字段列表
     */
    void configUpdated(const QJsonObject& newConfig, const QStringList& changedFields);

    /**
     * @brief 配置重载失败信号
     * @param error 错误信息
     */
    void configReloadFailed(const QString& error);

private slots:
    void onConfigReloaded(bool success, const QJsonObject& config, const QString& error);
    void onValidationFailed(const QStringList& errors, const QStringList& warnings);

private:
    QStringList detectChangedFields(const QJsonObject& oldConfig, const QJsonObject& newConfig);
    
private:
    ConfigWatcher* m_watcher;
    ConfigValidator* m_validator;
    QJsonObject m_currentConfig;
    bool m_initialized;
};

#endif // CONFIG_WATCHER_H