#include "resource_manager.h"
#include "../logging/logger.h"
#include <QFile>
#include <QMutexLocker>
#include <QDateTime>
#include <QTimer>
#include <QApplication>

// CEFBrowserGuard 实现
CEFBrowserGuard::CEFBrowserGuard(int browserId)
    : m_browserId(browserId), m_released(false)
{
    if (m_browserId > 0) {
        getResourceManager().registerCEFBrowser(m_browserId);
    }
}

CEFBrowserGuard::~CEFBrowserGuard()
{
    if (!m_released) {
        cleanup();
    }
}

CEFBrowserGuard::CEFBrowserGuard(CEFBrowserGuard&& other) noexcept
    : m_browserId(other.m_browserId), m_released(other.m_released)
{
    other.m_browserId = 0;
    other.m_released = true;
}

CEFBrowserGuard& CEFBrowserGuard::operator=(CEFBrowserGuard&& other) noexcept
{
    if (this != &other) {
        cleanup();
        m_browserId = other.m_browserId;
        m_released = other.m_released;
        other.m_browserId = 0;
        other.m_released = true;
    }
    return *this;
}

void CEFBrowserGuard::release()
{
    m_released = true;
}

void CEFBrowserGuard::reset(int browserId)
{
    cleanup();
    m_browserId = browserId;
    m_released = false;
    
    if (m_browserId > 0) {
        getResourceManager().registerCEFBrowser(m_browserId);
    }
}

void CEFBrowserGuard::cleanup()
{
    if (m_browserId > 0 && !m_released) {
        getResourceManager().unregisterCEFBrowser(m_browserId);
    }
    m_browserId = 0;
    m_released = true;
}

// FileGuard 实现
FileGuard::FileGuard(const QString& filePath, QIODevice::OpenMode mode)
    : m_file(std::make_unique<QFile>(filePath))
{
    if (m_file && !m_file->open(mode)) {
        // 如果打开失败，重置指针
        m_file.reset();
    }
}

FileGuard::~FileGuard()
{
    if (m_file && m_file->isOpen()) {
        m_file->close();
    }
}

FileGuard::FileGuard(FileGuard&& other) noexcept
    : m_file(std::move(other.m_file))
{
}

FileGuard& FileGuard::operator=(FileGuard&& other) noexcept
{
    if (this != &other) {
        if (m_file && m_file->isOpen()) {
            m_file->close();
        }
        m_file = std::move(other.m_file);
    }
    return *this;
}

bool FileGuard::isOpen() const
{
    return m_file && m_file->isOpen();
}

bool FileGuard::isValid() const
{
    return m_file != nullptr;
}

// ResourceManager 实现
ResourceManager::ResourceManager(QObject* parent)
    : QObject(parent)
    , m_logger(nullptr)
    , m_cleanupTimer(nullptr)
    , m_autoCleanupEnabled(true)
    , m_memoryThreshold(1024 * 1024 * 1024) // 1GB
{
    // 初始化统计信息
    m_stats.totalAllocated = 0;
    m_stats.totalDeallocated = 0;
    m_stats.activeResources = 0;
    m_stats.peakResources = 0;
    
    // 创建清理定时器
    m_cleanupTimer = new QTimer(this);
    m_cleanupTimer->setInterval(60000); // 1分钟
    connect(m_cleanupTimer, &QTimer::timeout, this, &ResourceManager::onCleanupTimer);
    
    if (m_autoCleanupEnabled) {
        m_cleanupTimer->start();
    }
}

ResourceManager::~ResourceManager()
{
    performCleanup();
}

void ResourceManager::setLogger(Logger* logger)
{
    m_logger = logger;
}

void ResourceManager::registerCEFBrowser(int browserId)
{
    QMutexLocker locker(&m_resourceMutex);
    
    m_cefBrowsers.insert(browserId);
    updateStats();
    
    if (m_logger) {
        m_logger->appEvent(QString("注册CEF浏览器: %1").arg(browserId));
    }
}

void ResourceManager::unregisterCEFBrowser(int browserId)
{
    QMutexLocker locker(&m_resourceMutex);
    
    if (m_cefBrowsers.remove(browserId)) {
        updateStats();
        
        if (m_logger) {
            m_logger->appEvent(QString("注销CEF浏览器: %1").arg(browserId));
        }
    }
}

std::unique_ptr<FileGuard> ResourceManager::createFileGuard(const QString& filePath, QIODevice::OpenMode mode)
{
    auto guard = std::make_unique<FileGuard>(filePath, mode);
    
    if (!guard->isValid()) {
        if (m_logger) {
            m_logger->errorEvent(QString("创建文件守护失败: %1").arg(filePath));
        }
        return nullptr;
    }
    
    updateStats();
    return guard;
}

std::unique_ptr<CEFBrowserGuard> ResourceManager::createCEFBrowserGuard(int browserId)
{
    auto guard = std::make_unique<CEFBrowserGuard>(browserId);
    
    if (!guard->isValid()) {
        if (m_logger) {
            m_logger->errorEvent(QString("创建CEF浏览器守护失败: %1").arg(browserId));
        }
        return nullptr;
    }
    
    return guard;
}

ResourceManager::MemoryStats ResourceManager::getMemoryStats() const
{
    QMutexLocker locker(&m_statsMutex);
    return m_stats;
}

void ResourceManager::performCleanup()
{
    QMutexLocker locker(&m_resourceMutex);
    
    int beforeCount = m_cefBrowsers.size();
    
    // 这里可以添加具体的清理逻辑
    // 例如：检查无效的CEF浏览器ID并清理
    
    updateStats();
    
    if (m_logger) {
        m_logger->appEvent(QString("资源清理完成，CEF浏览器: %1 -> %2")
            .arg(beforeCount).arg(m_cefBrowsers.size()));
    }
    
    checkMemoryThreshold();
}

void ResourceManager::setAutoCleanupEnabled(bool enabled)
{
    m_autoCleanupEnabled = enabled;
    
    if (enabled && !m_cleanupTimer->isActive()) {
        m_cleanupTimer->start();
    } else if (!enabled && m_cleanupTimer->isActive()) {
        m_cleanupTimer->stop();
    }
}

void ResourceManager::setCleanupInterval(int intervalMs)
{
    m_cleanupTimer->setInterval(intervalMs);
}

void ResourceManager::onCleanupTimer()
{
    performCleanup();
}

void ResourceManager::updateStats()
{
    QMutexLocker locker(&m_statsMutex);
    
    m_stats.activeResources = m_cefBrowsers.size();
    
    if (m_stats.activeResources > m_stats.peakResources) {
        m_stats.peakResources = m_stats.activeResources;
    }
    
    m_stats.lastActivity = QDateTime::currentDateTime().toString();
}

void ResourceManager::checkMemoryThreshold()
{
    // 简单的内存检查逻辑
    qint64 currentMemory = m_stats.activeResources * 50 * 1024 * 1024; // 假设每个资源50MB
    
    if (currentMemory > m_memoryThreshold) {
        emit memoryWarning(currentMemory, m_memoryThreshold);
        
        if (m_logger) {
            m_logger->errorEvent(QString("内存使用警告: %1 MB > %2 MB")
                .arg(currentMemory / 1024 / 1024).arg(m_memoryThreshold / 1024 / 1024));
        }
    }
    
    // 检查资源泄漏
    if (m_stats.activeResources > 10) { // 假设正常情况下不超过10个活跃资源
        emit resourceLeakWarning("CEF浏览器", m_stats.activeResources);
        
        if (m_logger) {
            m_logger->errorEvent(QString("可能的资源泄漏: %1 个活跃CEF浏览器")
                .arg(m_stats.activeResources));
        }
    }
}

// 全局资源管理器实例
ResourceManager& getResourceManager()
{
    static ResourceManager instance;
    return instance;
}