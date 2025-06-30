#include "config_watcher.h"
#include "config_validator.h"
#include <QJsonDocument>
#include <QJsonParseError>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QDebug>

ConfigWatcher::ConfigWatcher(QObject* parent)
    : QObject(parent)
    , m_fileWatcher(nullptr)
    , m_validator(nullptr)
    , m_reloadTimer(nullptr)
    , m_isWatching(false)
    , m_autoReloadEnabled(true)
    , m_reloadDelay(1000)
    , m_reloadPending(false)
{
    m_fileWatcher = new QFileSystemWatcher(this);
    m_reloadTimer = new QTimer(this);
    m_reloadTimer->setSingleShot(true);
    
    connect(m_fileWatcher, &QFileSystemWatcher::fileChanged, 
            this, &ConfigWatcher::onFileChanged);
    connect(m_fileWatcher, &QFileSystemWatcher::directoryChanged,
            this, &ConfigWatcher::onDirectoryChanged);
    connect(m_reloadTimer, &QTimer::timeout,
            this, &ConfigWatcher::onReloadTimer);
}

ConfigWatcher::~ConfigWatcher()
{
    stopWatching();
}

bool ConfigWatcher::startWatching(const QString& configPath)
{
    if (m_isWatching) {
        stopWatching();
    }
    
    QFileInfo fileInfo(configPath);
    if (!fileInfo.exists()) {
        qWarning() << "配置文件不存在:" << configPath;
        return false;
    }
    
    m_configPath = fileInfo.absoluteFilePath();
    m_configDir = fileInfo.absoluteDir().absolutePath();
    
    // 监控文件和目录
    m_fileWatcher->addPath(m_configPath);
    m_fileWatcher->addPath(m_configDir);
    
    m_isWatching = true;
    updateLastModified();
    
    qDebug() << "开始监控配置文件:" << m_configPath;
    return true;
}

void ConfigWatcher::stopWatching()
{
    if (!m_isWatching) {
        return;
    }
    
    cleanup();
    m_isWatching = false;
    qDebug() << "停止监控配置文件";
}

void ConfigWatcher::setValidator(ConfigValidator* validator)
{
    m_validator = validator;
}

void ConfigWatcher::setReloadDelay(int delayMs)
{
    m_reloadDelay = qMax(100, delayMs); // 最小100ms
}

void ConfigWatcher::setAutoReloadEnabled(bool enabled)
{
    m_autoReloadEnabled = enabled;
}

QString ConfigWatcher::getWatchedPath() const
{
    return m_configPath;
}

bool ConfigWatcher::isWatching() const
{
    return m_isWatching;
}

void ConfigWatcher::manualReload()
{
    if (!m_isWatching) {
        return;
    }
    
    qDebug() << "手动重载配置文件";
    performReload();
}

QDateTime ConfigWatcher::getLastModified() const
{
    return m_lastModified;
}

void ConfigWatcher::onFileChanged(const QString& path)
{
    if (!isConfigFile(path)) {
        return;
    }
    
    qDebug() << "检测到配置文件变化:" << path;
    
    // 检查文件是否被删除
    if (!QFile::exists(path)) {
        emit configFileRemoved(path);
        return;
    }
    
    // 更新最后修改时间
    updateLastModified();
    
    emit configFileChanged(path);
    
    if (m_autoReloadEnabled) {
        scheduleReload();
    }
}

void ConfigWatcher::onDirectoryChanged(const QString& path)
{
    Q_UNUSED(path)
    
    // 检查配置文件是否被重新创建
    if (QFile::exists(m_configPath)) {
        // 重新添加文件监控（可能被其他程序删除重建）
        if (!m_fileWatcher->files().contains(m_configPath)) {
            m_fileWatcher->addPath(m_configPath);
            qDebug() << "重新添加文件监控:" << m_configPath;
        }
        
        updateLastModified();
        emit configFileChanged(m_configPath);
        
        if (m_autoReloadEnabled) {
            scheduleReload();
        }
    }
}

void ConfigWatcher::onReloadTimer()
{
    if (m_reloadPending) {
        performReload();
        m_reloadPending = false;
    }
}

void ConfigWatcher::scheduleReload()
{
    // 防止频繁重载
    QDateTime now = QDateTime::currentDateTime();
    if (m_lastReloadTime.isValid() && 
        m_lastReloadTime.msecsTo(now) < MIN_RELOAD_INTERVAL) {
        return;
    }
    
    if (!m_reloadPending) {
        m_reloadPending = true;
        m_reloadTimer->start(m_reloadDelay);
        qDebug() << "计划重载配置文件，延迟" << m_reloadDelay << "ms";
    }
}

void ConfigWatcher::performReload()
{
    QJsonObject config;
    QString error;
    
    bool success = validateAndLoadConfig(m_configPath, config, error);
    
    m_lastReloadTime = QDateTime::currentDateTime();
    
    if (success) {
        qDebug() << "配置重载成功";
        emit configReloaded(true, config, QString());
    } else {
        qWarning() << "配置重载失败:" << error;
        emit configReloaded(false, QJsonObject(), error);
    }
}

bool ConfigWatcher::validateAndLoadConfig(const QString& configPath, QJsonObject& config, QString& error)
{
    QFile file(configPath);
    if (!file.open(QIODevice::ReadOnly)) {
        error = QString("无法打开配置文件: %1").arg(file.errorString());
        return false;
    }
    
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseError);
    file.close();
    
    if (doc.isNull()) {
        error = QString("JSON解析失败: %1").arg(parseError.errorString());
        return false;
    }
    
    if (!doc.isObject()) {
        error = "配置文件根节点必须是JSON对象";
        return false;
    }
    
    config = doc.object();
    
    // 使用验证器验证配置
    if (m_validator) {
        ValidationResult result = m_validator->validateConfig(config);
        if (!result.isValid) {
            error = QString("配置验证失败: %1").arg(result.errors.join("; "));
            emit configValidationFailed(result.errors, result.warnings);
            return false;
        }
        
        if (!result.warnings.isEmpty()) {
            emit configValidationFailed(QStringList(), result.warnings);
        }
    }
    
    return true;
}

void ConfigWatcher::setupFileWatcher()
{
    // 确保监控列表是最新的
    QStringList watchedPaths = m_fileWatcher->files() + m_fileWatcher->directories();
    
    if (!watchedPaths.contains(m_configPath)) {
        m_fileWatcher->addPath(m_configPath);
    }
    
    if (!watchedPaths.contains(m_configDir)) {
        m_fileWatcher->addPath(m_configDir);
    }
}

void ConfigWatcher::cleanup()
{
    if (m_fileWatcher) {
        QStringList files = m_fileWatcher->files();
        QStringList dirs = m_fileWatcher->directories();
        
        if (!files.isEmpty()) {
            m_fileWatcher->removePaths(files);
        }
        if (!dirs.isEmpty()) {
            m_fileWatcher->removePaths(dirs);
        }
    }
    
    if (m_reloadTimer) {
        m_reloadTimer->stop();
    }
    
    m_reloadPending = false;
}

bool ConfigWatcher::isConfigFile(const QString& filePath) const
{
    return QFileInfo(filePath).absoluteFilePath() == m_configPath;
}

void ConfigWatcher::updateLastModified()
{
    QFileInfo fileInfo(m_configPath);
    if (fileInfo.exists()) {
        m_lastModified = fileInfo.lastModified();
    }
}

// ConfigHotReloadManager 实现
ConfigHotReloadManager::ConfigHotReloadManager(QObject* parent)
    : QObject(parent)
    , m_watcher(nullptr)
    , m_validator(nullptr)
    , m_initialized(false)
{
    m_watcher = new ConfigWatcher(this);
    m_validator = new DefaultConfigValidator(this);
    
    m_watcher->setValidator(m_validator);
    
    connect(m_watcher, &ConfigWatcher::configReloaded,
            this, &ConfigHotReloadManager::onConfigReloaded);
    connect(m_watcher, &ConfigWatcher::configValidationFailed,
            this, &ConfigHotReloadManager::onValidationFailed);
}

ConfigHotReloadManager::~ConfigHotReloadManager()
{
    shutdown();
}

bool ConfigHotReloadManager::initialize(const QString& configPath)
{
    if (m_initialized) {
        shutdown();
    }
    
    // 加载初始配置
    QJsonObject config;
    QString error;
    QFile file(configPath);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "无法打开配置文件:" << configPath;
        return false;
    }
    
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseError);
    file.close();
    
    if (doc.isNull() || !doc.isObject()) {
        qWarning() << "配置文件格式错误:" << parseError.errorString();
        return false;
    }
    
    config = doc.object();
    
    // 验证配置
    ValidationResult result = m_validator->validateConfig(config);
    if (!result.isValid) {
        qWarning() << "配置验证失败:" << result.errors.join("; ");
        return false;
    }
    
    m_currentConfig = config;
    
    // 开始监控
    if (!m_watcher->startWatching(configPath)) {
        qWarning() << "无法开始监控配置文件";
        return false;
    }
    
    m_initialized = true;
    qDebug() << "配置热重载管理器初始化成功";
    return true;
}

void ConfigHotReloadManager::shutdown()
{
    if (!m_initialized) {
        return;
    }
    
    m_watcher->stopWatching();
    m_currentConfig = QJsonObject();
    m_initialized = false;
    
    qDebug() << "配置热重载管理器已关闭";
}

QJsonObject ConfigHotReloadManager::getCurrentConfig() const
{
    return m_currentConfig;
}

void ConfigHotReloadManager::setAutoReloadEnabled(bool enabled)
{
    m_watcher->setAutoReloadEnabled(enabled);
}

void ConfigHotReloadManager::reloadConfig()
{
    m_watcher->manualReload();
}

ConfigWatcher* ConfigHotReloadManager::getWatcher() const
{
    return m_watcher;
}

ConfigValidator* ConfigHotReloadManager::getValidator() const
{
    return m_validator;
}

void ConfigHotReloadManager::onConfigReloaded(bool success, const QJsonObject& config, const QString& error)
{
    if (success) {
        QStringList changedFields = detectChangedFields(m_currentConfig, config);
        m_currentConfig = config;
        
        qDebug() << "配置已更新，变更字段:" << changedFields;
        emit configUpdated(config, changedFields);
    } else {
        qWarning() << "配置重载失败:" << error;
        emit configReloadFailed(error);
    }
}

void ConfigHotReloadManager::onValidationFailed(const QStringList& errors, const QStringList& warnings)
{
    if (!errors.isEmpty()) {
        QString errorMsg = QString("配置验证失败: %1").arg(errors.join("; "));
        qWarning() << errorMsg;
        emit configReloadFailed(errorMsg);
    }
    
    if (!warnings.isEmpty()) {
        qWarning() << "配置验证警告:" << warnings.join("; ");
    }
}

QStringList ConfigHotReloadManager::detectChangedFields(const QJsonObject& oldConfig, const QJsonObject& newConfig)
{
    QStringList changedFields;
    
    // 检查新增和修改的字段
    for (auto it = newConfig.begin(); it != newConfig.end(); ++it) {
        if (!oldConfig.contains(it.key()) || oldConfig[it.key()] != it.value()) {
            changedFields.append(it.key());
        }
    }
    
    // 检查删除的字段
    for (auto it = oldConfig.begin(); it != oldConfig.end(); ++it) {
        if (!newConfig.contains(it.key())) {
            changedFields.append(QString("-%1").arg(it.key())); // 用减号标记删除的字段
        }
    }
    
    return changedFields;
}