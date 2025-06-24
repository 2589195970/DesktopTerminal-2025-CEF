#ifndef WINDOW_MANAGER_H
#define WINDOW_MANAGER_H

#include <QObject>
#include <QWidget>
#include <QTimer>
#include <QScreen>
#include <QRect>

class Logger;
class ConfigManager;

/**
 * @brief 窗口管理器类
 * 
 * 负责窗口的全屏控制、焦点管理和安全窗口状态维护
 * 确保应用程序始终保持在安全的全屏状态
 */
class WindowManager : public QObject
{
    Q_OBJECT

public:
    explicit WindowManager(QWidget* targetWindow, QObject *parent = nullptr);
    ~WindowManager();

    /**
     * @brief 初始化窗口管理器
     */
    bool initialize();

    /**
     * @brief 强制进入全屏模式
     */
    void enforceFullscreen();

    /**
     * @brief 强制窗口获得焦点
     */
    void enforceFocus();

    /**
     * @brief 强制窗口置顶
     */
    void enforceAlwaysOnTop();

    /**
     * @brief 检查并修复窗口状态
     */
    void checkAndFixWindowState();

    /**
     * @brief 启用/禁用窗口状态监控
     */
    void setMonitoringEnabled(bool enabled);
    bool isMonitoringEnabled() const;

    /**
     * @brief 启用/禁用全屏检查
     */
    void setFullscreenCheckEnabled(bool enabled);
    bool isFullscreenCheckEnabled() const;

    /**
     * @brief 启用/禁用焦点检查
     */
    void setFocusCheckEnabled(bool enabled);
    bool isFocusCheckEnabled() const;

    /**
     * @brief 获取窗口状态信息
     */
    QString getWindowStateInfo() const;

    /**
     * @brief 获取修复次数统计
     */
    int getFixCount() const;

    /**
     * @brief 重置统计计数
     */
    void resetStatistics();

public slots:
    /**
     * @brief 定时检查窗口状态
     */
    void performWindowCheck();

    /**
     * @brief 处理屏幕变化
     */
    void handleScreenChanged();

signals:
    /**
     * @brief 窗口状态被修复时发出信号
     */
    void windowStateFixed(const QString& description);

    /**
     * @brief 检测到窗口状态异常时发出信号
     */
    void windowStateViolation(const QString& description);

private:
    void initializeMonitoring();
    void setupWindowFlags();
    void setupWindowGeometry();
    void connectScreenSignals();
    bool isWindowInCorrectState();
    bool isWindowFullscreen();
    bool isWindowFocused();
    bool isWindowOnTop();
    void fixWindowFlags();
    void fixWindowGeometry();
    void fixWindowFocus();
    void logWindowEvent(const QString& event, const QString& details);

private:
    QWidget* m_targetWindow;
    Logger* m_logger;
    ConfigManager* m_configManager;
    
    // 监控设置
    bool m_monitoringEnabled;
    bool m_fullscreenCheckEnabled;
    bool m_focusCheckEnabled;
    bool m_alwaysOnTopEnabled;
    
    // 定时器
    QTimer* m_windowCheckTimer;
    int m_checkInterval;
    
    // 屏幕管理
    QScreen* m_currentScreen;
    QRect m_expectedGeometry;
    
    // 统计信息
    int m_totalChecks;
    int m_fixCount;
    int m_fullscreenFixes;
    int m_focusFixes;
    int m_geometryFixes;
    
    // 窗口状态缓存
    bool m_lastFullscreenState;
    bool m_lastFocusState;
    QRect m_lastGeometry;
};

#endif // WINDOW_MANAGER_H