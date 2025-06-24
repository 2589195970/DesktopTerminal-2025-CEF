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
    explicit SecureBrowser(QWidget *parent = nullptr);
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
    void setWindowTitle(const QString& title) override;

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
     * @brief CEF浏览器创建完成回调
     */
    void onBrowserCreated();

private:
    // 初始化方法
    void initializeWindow();
    void initializeCEF();
    void initializeHotkeys();
    void initializeMaintenanceTimer();
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

    // 定时器
    QTimer* m_maintenanceTimer;

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

    // 窗口句柄（用于CEF集成）
    void* m_windowHandle;
};

#endif // SECURE_BROWSER_H