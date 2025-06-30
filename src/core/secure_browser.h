#ifndef SECURE_BROWSER_H
#define SECURE_BROWSER_H

#include <QWidget>
#include <QTimer>
#include <QKeyEvent>
#include <QCloseEvent>
#include <QFocusEvent>
#include <QWindowStateChangeEvent>
#include <QLabel>
#include <QProgressBar>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFrame>
#include <QMovie>
#include <memory>

class QHotkey;
class CEFManager;
class Logger;
class ConfigManager;

/**
 * @brief 安全浏览器窗口类
 * 
 * 替代原项目的ShellBrowser类，提供相同的安全控制功能
 * 使用CEF作为底层浏览器引擎而非WebEngine
 */
class SecureBrowser : public QWidget
{
    Q_OBJECT

public:
    explicit SecureBrowser(CEFManager* cefManager, QWidget *parent = nullptr);
    ~SecureBrowser();

    /**
     * @brief 加载URL（与原项目接口兼容）
     */
    void load(const QUrl& url);

    /**
     * @brief 重新加载页面
     */
    void reload();

    /**
     * @brief 获取当前URL
     */
    QUrl url() const;

    /**
     * @brief 设置窗口标题
     */
    void setWindowTitle(const QString& title);

protected:
    // 事件处理（保持与原项目相同的安全控制逻辑）
    bool event(QEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void closeEvent(QCloseEvent *event) override;
    void focusOutEvent(QFocusEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void showEvent(QShowEvent *event) override;

    // 窗口状态事件
    void changeEvent(QEvent *event) override;

private slots:
    /**
     * @brief 处理安全退出热键
     */
    void handleExitHotkey();

    /**
     * @brief 定时维护任务
     */
    void onMaintenanceTimer();

    /**
     * @brief CEF消息循环处理
     */
    void onCEFMessageLoop();

    /**
     * @brief CEF浏览器创建完成回调
     */
    void onBrowserCreated();

    /**
     * @brief 显示加载动画
     */
    void showLoadingAnimation();

    /**
     * @brief 隐藏加载动画
     */
    void hideLoadingAnimation();

    /**
     * @brief 更新加载进度
     */
    void updateLoadingProgress(int progress);

    /**
     * @brief 页面开始加载回调
     */
    void onPageLoadStart();

    /**
     * @brief 页面加载完成回调
     */
    void onPageLoadEnd();

private:
    // 初始化方法
    void initializeWindow();
    void initializeCEF();
    void initializeHotkeys();
    void initializeMaintenanceTimer();
    void initializeCEFMessageLoopTimer();
    void setupSecuritySettings();
    void initializeLoadingAnimation();

    // 安全控制方法
    void enforceFullscreen();
    void enforceFocus();
    void enforceWindowState();
    bool isSecurityKeyEvent(QKeyEvent *event);
    void logKeyboardEvent(QKeyEvent *event, bool allowed);

    // 窗口管理
    void setFullscreenMode();
    void setAlwaysOnTop();
    void disableWindowControls();

    // CEF集成
    void createCEFBrowser();
    void destroyCEFBrowser();
    void resizeCEFBrowser();
    
    // CEF性能状态管理
    void setCEFPerformanceState(CEFPerformanceState state);
    void updateCEFMessageLoopInterval();

    // 错误处理
    void handleBrowserError(const QString& error);
    void showSecurityViolationWarning(const QString& violation);

private:
    // 核心组件
    CEFManager* m_cefManager;
    Logger* m_logger;
    ConfigManager* m_configManager;

    // 热键管理（使用智能指针）
    std::unique_ptr<QHotkey> m_exitHotkeyF10;
    std::unique_ptr<QHotkey> m_exitHotkeyBackslash;

    // 定时器（使用智能指针）
    std::unique_ptr<QTimer> m_maintenanceTimer;
    std::unique_ptr<QTimer> m_cefMessageLoopTimer;

    // CEF性能状态枚举
    enum class CEFPerformanceState {
        Loading,    // 页面加载中 - 高频率消息循环
        Loaded,     // 页面已加载 - 中频率消息循环
        Idle        // 空闲状态 - 低频率消息循环
    };

    // 状态管理
    bool m_needFocusCheck;
    bool m_needFullscreenCheck;
    bool m_cefBrowserCreated;
    int m_cefBrowserId;
    QUrl m_currentUrl;
    QString m_windowTitle;
    CEFPerformanceState m_cefPerformanceState;

    // 安全设置
    bool m_strictSecurityMode;
    bool m_keyboardFilterEnabled;
    bool m_contextMenuEnabled;

    // 窗口句柄（用于CEF集成）
    void* m_windowHandle;
    
    // CEF消息循环日志计数器（避免日志过多）
    int m_cefMessageLoopLogCounter;

    // 加载动画组件
    QFrame* m_loadingOverlay;
    QVBoxLayout* m_loadingLayout;
    QLabel* m_loadingIcon;
    QLabel* m_loadingText;
    QProgressBar* m_loadingProgressBar;
    QMovie* m_loadingMovie;
    bool m_isLoadingVisible;
};

#endif // SECURE_BROWSER_H