#ifndef SECURE_BROWSER_H
#define SECURE_BROWSER_H

#include <QWidget>
#include <QTimer>
#include <QKeyEvent>
#include <QCloseEvent>
#include <QFocusEvent>
#include <QWindowStateChangeEvent>

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

signals:
    /**
     * @brief 内容加载完成信号
     * 当CEF浏览器完成初始页面加载时发出
     */
    void contentLoadFinished();

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
     * @brief 处理开发者工具热键（F12）
     */
    void handleDevToolsHotkey();

    /**
     * @brief 处理URL检测退出
     * @param url 触发退出的URL
     */
    void handleUrlExit(const QString& url);

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

private:
    // 初始化方法
    void initializeWindow();
    void initializeCEF();
    void initializeHotkeys();
    void initializeMaintenanceTimer();
    void initializeCEFMessageLoopTimer();
    void setupSecuritySettings();

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

    // 开发者工具管理
    void toggleDevTools();
    bool isDevToolsOpen() const;

    // 错误处理
    void handleBrowserError(const QString& error);
    void showSecurityViolationWarning(const QString& violation);

private:
    // 核心组件
    CEFManager* m_cefManager;
    Logger* m_logger;
    ConfigManager* m_configManager;

    // 热键管理
    QHotkey* m_exitHotkeyF10;
    QHotkey* m_exitHotkeyBackslash;
    QHotkey* m_devToolsHotkeyF12;

    // 定时器
    QTimer* m_maintenanceTimer;
    QTimer* m_cefMessageLoopTimer;

    // 状态管理
    bool m_needFocusCheck;
    bool m_needFullscreenCheck;
    bool m_cefBrowserCreated;
    int m_cefBrowserId;
    QUrl m_currentUrl;
    QString m_windowTitle;

    // 安全设置
    bool m_strictSecurityMode;
    bool m_keyboardFilterEnabled;
    bool m_contextMenuEnabled;

    // 开发者工具状态
    bool m_devToolsOpen;

    // 窗口句柄（用于CEF集成）
    void* m_windowHandle;
    
    // CEF消息循环日志计数器（避免日志过多）
    int m_cefMessageLoopLogCounter;
};

#endif // SECURE_BROWSER_H