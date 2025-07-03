#ifndef KEYBOARD_FILTER_H
#define KEYBOARD_FILTER_H

#include <QObject>
#include <QKeyEvent>
#include <QStringList>
#include <QSet>
#include <QTimer>

class Logger;
class ConfigManager;

/**
 * @brief 键盘过滤器类
 * 
 * 负责拦截和过滤危险的键盘组合，防止用户绕过安全控制
 * 与原项目的键盘拦截逻辑保持一致
 */
class KeyboardFilter : public QObject
{
    Q_OBJECT

public:
    explicit KeyboardFilter(QObject *parent = nullptr);
    ~KeyboardFilter();

    /**
     * @brief 初始化键盘过滤器
     */
    bool initialize();

    /**
     * @brief 检查按键事件是否应该被过滤
     * @param event 键盘事件
     * @return 需要过滤返回true
     */
    bool shouldFilterKeyEvent(QKeyEvent* event);

    /**
     * @brief 检查按键组合是否为安全退出热键
     * @param event 键盘事件
     * @return 是安全退出热键返回true
     */
    bool isSecurityExitHotkey(QKeyEvent* event);

    /**
     * @brief 检查是否为允许的功能键
     * @param event 键盘事件
     * @return 允许的功能键返回true
     */
    bool isAllowedFunctionKey(QKeyEvent* event);

    /**
     * @brief 设置开发者模式状态
     * @param enabled 是否启用开发者模式
     */
    void setDeveloperModeEnabled(bool enabled);
    
    /**
     * @brief 检查是否启用开发者模式
     * @return 开发者模式启用状态
     */
    bool isDeveloperModeEnabled() const;

    /**
     * @brief 启用/禁用键盘过滤
     */
    void setFilterEnabled(bool enabled);
    bool isFilterEnabled() const;

    /**
     * @brief 获取被过滤的按键统计
     */
    int getFilteredKeyCount() const;

    /**
     * @brief 重置统计计数
     */
    void resetStatistics();

    /**
     * @brief 记录按键事件
     */
    void logKeyEvent(QKeyEvent* event, bool filtered);

public slots:
    /**
     * @brief 处理定时统计
     */
    void updateStatistics();

signals:
    /**
     * @brief 检测到危险按键时发出信号
     */
    void dangerousKeyDetected(const QString& keyDescription);

    /**
     * @brief 安全退出热键被触发
     */
    void securityExitRequested();

private:
    void initializeFilterRules();
    void initializeStatisticsTimer();
    bool isDangerousKeyCombo(int key, Qt::KeyboardModifiers modifiers);
    bool isSystemKeyCombo(int key, Qt::KeyboardModifiers modifiers);
    bool isDebugKeyCombo(int key, Qt::KeyboardModifiers modifiers);
    QString getKeyDescription(QKeyEvent* event);
    QString getKeyDescription(int key, Qt::KeyboardModifiers modifiers);
    void logFilterEvent(const QString& description, bool filtered);

private:
    Logger* m_logger;
    ConfigManager* m_configManager;
    
    // 过滤状态
    bool m_filterEnabled;
    bool m_strictMode;
    bool m_developerModeEnabled;
    
    // 危险按键组合集合
    QSet<QString> m_dangerousKeyCombos;
    QSet<QString> m_systemKeyCombos;
    QSet<QString> m_debugKeyCombos;
    
    // 允许的按键组合
    QSet<QString> m_allowedKeyCombos;
    
    // 统计信息
    int m_totalKeyEvents;
    int m_filteredKeyEvents;
    QTimer* m_statisticsTimer;
    
    // 安全退出热键
    QStringList m_exitHotkeys;
};

#endif // KEYBOARD_FILTER_H