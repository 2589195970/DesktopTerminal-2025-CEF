#ifndef RESOURCE_MANAGER_H
#define RESOURCE_MANAGER_H

#include <memory>
#include <QObject>
#include <QScopedPointer>
#include <QMap>
#include <QString>
#include <QMutex>
#include <QTimer>
#include <functional>

class Logger;

/**
 * @brief RAII资源守护模板类
 * 
 * 提供自动资源管理，确保资源在作用域结束时正确释放
 */
template<typename T>
class ResourceGuard
{
public:
    using DeleterFunc = std::function<void(T*)>;
    
    explicit ResourceGuard(T* resource = nullptr, DeleterFunc deleter = nullptr)
        : m_resource(resource), m_deleter(deleter)
    {
        if (!m_deleter && m_resource) {
            m_deleter = [](T* ptr) { delete ptr; };
        }
    }
    
    ~ResourceGuard()
    {
        reset();
    }
    
    // 移动构造和赋值
    ResourceGuard(ResourceGuard&& other) noexcept
        : m_resource(other.m_resource), m_deleter(std::move(other.m_deleter))
    {
        other.m_resource = nullptr;
    }
    
    ResourceGuard& operator=(ResourceGuard&& other) noexcept
    {
        if (this != &other) {
            reset();
            m_resource = other.m_resource;
            m_deleter = std::move(other.m_deleter);
            other.m_resource = nullptr;
        }
        return *this;
    }
    
    // 禁用拷贝
    ResourceGuard(const ResourceGuard&) = delete;
    ResourceGuard& operator=(const ResourceGuard&) = delete;
    
    T* get() const { return m_resource; }
    T* operator->() const { return m_resource; }
    T& operator*() const { return *m_resource; }
    
    bool isValid() const { return m_resource != nullptr; }
    explicit operator bool() const { return isValid(); }
    
    T* release()
    {
        T* temp = m_resource;
        m_resource = nullptr;
        return temp;
    }
    
    void reset(T* resource = nullptr)
    {
        if (m_resource && m_deleter) {
            m_deleter(m_resource);
        }
        m_resource = resource;
    }
    
private:
    T* m_resource;
    DeleterFunc m_deleter;
};

/**
 * @brief CEF浏览器资源守护类
 */
class CEFBrowserGuard
{
public:
    explicit CEFBrowserGuard(int browserId = 0);
    ~CEFBrowserGuard();
    
    // 移动语义
    CEFBrowserGuard(CEFBrowserGuard&& other) noexcept;
    CEFBrowserGuard& operator=(CEFBrowserGuard&& other) noexcept;
    
    // 禁用拷贝
    CEFBrowserGuard(const CEFBrowserGuard&) = delete;
    CEFBrowserGuard& operator=(const CEFBrowserGuard&) = delete;
    
    int getBrowserId() const { return m_browserId; }
    bool isValid() const { return m_browserId > 0; }
    
    void release();
    void reset(int browserId = 0);
    
private:
    void cleanup();
    
private:
    int m_browserId;
    bool m_released;
};

/**
 * @brief 文件资源守护类
 */
class FileGuard
{
public:
    explicit FileGuard(const QString& filePath, QIODevice::OpenMode mode = QIODevice::ReadOnly);
    ~FileGuard();
    
    // 移动语义
    FileGuard(FileGuard&& other) noexcept;
    FileGuard& operator=(FileGuard&& other) noexcept;
    
    // 禁用拷贝
    FileGuard(const FileGuard&) = delete;
    FileGuard& operator=(const FileGuard&) = delete;
    
    QFile* get() const { return m_file.get(); }
    QFile* operator->() const { return m_file.get(); }
    QFile& operator*() const { return *m_file; }
    
    bool isOpen() const;
    bool isValid() const;
    
private:
    std::unique_ptr<QFile> m_file;
};

/**
 * @brief 智能指针工厂类
 * 
 * 提供创建各种智能指针的便捷方法
 */
class SmartPtrFactory
{
public:
    /**
     * @brief 创建unique_ptr
     */
    template<typename T, typename... Args>
    static std::unique_ptr<T> makeUnique(Args&&... args)
    {
        return std::make_unique<T>(std::forward<Args>(args)...);
    }
    
    /**
     * @brief 创建shared_ptr
     */
    template<typename T, typename... Args>
    static std::shared_ptr<T> makeShared(Args&&... args)
    {
        return std::make_shared<T>(std::forward<Args>(args)...);
    }
    
    /**
     * @brief 创建带自定义删除器的unique_ptr
     */
    template<typename T, typename Deleter>
    static std::unique_ptr<T, Deleter> makeUniqueWithDeleter(T* ptr, Deleter deleter)
    {
        return std::unique_ptr<T, Deleter>(ptr, deleter);
    }
    
    /**
     * @brief 创建QScopedPointer
     */
    template<typename T, typename... Args>
    static QScopedPointer<T> makeQScoped(Args&&... args)
    {
        return QScopedPointer<T>(new T(std::forward<Args>(args)...));
    }
};

/**
 * @brief 资源池管理器
 * 
 * 管理可重用资源的生命周期
 */
template<typename T>
class ResourcePool
{
public:
    using CreatorFunc = std::function<std::unique_ptr<T>()>;
    using ResetFunc = std::function<void(T*)>;
    
    explicit ResourcePool(CreatorFunc creator, ResetFunc resetter = nullptr, int maxSize = 10)
        : m_creator(creator), m_resetter(resetter), m_maxSize(maxSize)
    {
    }
    
    ~ResourcePool()
    {
        clear();
    }
    
    /**
     * @brief 获取资源
     */
    std::unique_ptr<T> acquire()
    {
        QMutexLocker locker(&m_mutex);
        
        if (!m_pool.isEmpty()) {
            return std::move(m_pool.takeLast());
        }
        
        return m_creator ? m_creator() : nullptr;
    }
    
    /**
     * @brief 归还资源
     */
    void release(std::unique_ptr<T> resource)
    {
        if (!resource) {
            return;
        }
        
        QMutexLocker locker(&m_mutex);
        
        if (m_pool.size() >= m_maxSize) {
            // 池已满，直接销毁
            return;
        }
        
        // 重置资源状态
        if (m_resetter) {
            m_resetter(resource.get());
        }
        
        m_pool.append(std::move(resource));
    }
    
    /**
     * @brief 清空资源池
     */
    void clear()
    {
        QMutexLocker locker(&m_mutex);
        m_pool.clear();
    }
    
    /**
     * @brief 获取池大小
     */
    int size() const
    {
        QMutexLocker locker(&m_mutex);
        return m_pool.size();
    }
    
private:
    CreatorFunc m_creator;
    ResetFunc m_resetter;
    int m_maxSize;
    QList<std::unique_ptr<T>> m_pool;
    mutable QMutex m_mutex;
};

/**
 * @brief 资源管理器主类
 * 
 * 统一管理应用程序中的各种资源
 */
class ResourceManager : public QObject
{
    Q_OBJECT

public:
    explicit ResourceManager(QObject* parent = nullptr);
    ~ResourceManager();

    /**
     * @brief 设置日志器
     */
    void setLogger(Logger* logger);

    /**
     * @brief 注册CEF浏览器
     */
    void registerCEFBrowser(int browserId);

    /**
     * @brief 注销CEF浏览器
     */
    void unregisterCEFBrowser(int browserId);

    /**
     * @brief 创建文件守护
     */
    std::unique_ptr<FileGuard> createFileGuard(const QString& filePath, 
                                               QIODevice::OpenMode mode = QIODevice::ReadOnly);

    /**
     * @brief 创建CEF浏览器守护
     */
    std::unique_ptr<CEFBrowserGuard> createCEFBrowserGuard(int browserId);

    /**
     * @brief 获取内存使用统计
     */
    struct MemoryStats {
        qint64 totalAllocated;
        qint64 totalDeallocated;
        int activeResources;
        int peakResources;
        QString lastActivity;
    };
    MemoryStats getMemoryStats() const;

    /**
     * @brief 执行资源清理
     */
    void performCleanup();

    /**
     * @brief 启用/禁用自动清理
     */
    void setAutoCleanupEnabled(bool enabled);

    /**
     * @brief 设置清理间隔
     */
    void setCleanupInterval(int intervalMs);

signals:
    /**
     * @brief 内存警告信号
     */
    void memoryWarning(qint64 currentUsage, qint64 threshold);

    /**
     * @brief 资源泄漏警告信号
     */
    void resourceLeakWarning(const QString& resourceType, int count);

private slots:
    void onCleanupTimer();

private:
    void updateStats();
    void checkMemoryThreshold();

private:
    Logger* m_logger;
    
    // 资源跟踪
    QSet<int> m_cefBrowsers;
    mutable QMutex m_resourceMutex;
    
    // 统计信息
    MemoryStats m_stats;
    mutable QMutex m_statsMutex;
    
    // 自动清理
    QTimer* m_cleanupTimer;
    bool m_autoCleanupEnabled;
    qint64 m_memoryThreshold;
};

/**
 * @brief 便捷的智能指针类型别名
 */
template<typename T>
using UniquePtr = std::unique_ptr<T>;

template<typename T>
using SharedPtr = std::shared_ptr<T>;

template<typename T>
using WeakPtr = std::weak_ptr<T>;

template<typename T>
using ScopedPtr = QScopedPointer<T>;

/**
 * @brief 全局资源管理器实例
 */
ResourceManager& getResourceManager();

/**
 * @brief RAII辅助宏
 */
#define RAII_GUARD(type, var, resource) \
    ResourceGuard<type> var(resource, [](type* ptr) { delete ptr; })

#define CEF_BROWSER_GUARD(var, browserId) \
    auto var = getResourceManager().createCEFBrowserGuard(browserId)

#define FILE_GUARD(var, filePath, mode) \
    auto var = getResourceManager().createFileGuard(filePath, mode)

#endif // RESOURCE_MANAGER_H