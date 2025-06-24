#include "secure_browser.h"
#include "cef_manager.h"
#include "../logging/logger.h"
#include "../config/config_manager.h"

#include <QApplication>
#include <QDesktopWidget>
#include <QUrl>
#include <QInputDialog>
#include <QMessageBox>
#include <QContextMenuEvent>
#include <QKeyEvent>
#include <QResizeEvent>
#include <QShowEvent>
#include <QHotkey>

#ifdef Q_OS_WIN
#include <windows.h>
#elif defined(Q_OS_MAC)
#include <AppKit/AppKit.h>
#elif defined(Q_OS_LINUX)
#include <X11/Xlib.h>
#endif

SecureBrowser::SecureBrowser(QWidget *parent)
    : QWidget(parent)
    , m_cefManager(nullptr)
    , m_logger(&Logger::instance())
    , m_configManager(&ConfigManager::instance())
    , m_exitHotkeyF10(nullptr)
    , m_exitHotkeyBackslash(nullptr)
    , m_maintenanceTimer(nullptr)
    , m_needFocusCheck(true)
    , m_needFullscreenCheck(true)
    , m_cefBrowserCreated(false)
    , m_cefBrowserId(0)
    , m_strictSecurityMode(true)
    , m_keyboardFilterEnabled(true)
    , m_contextMenuEnabled(false)
    , m_windowHandle(nullptr)
{
    m_logger->appEvent("SecureBrowser创建开始");

    // 从配置读取安全设置
    m_strictSecurityMode = m_configManager->isStrictSecurityMode();
    m_keyboardFilterEnabled = m_configManager->isKeyboardFilterEnabled();
    m_contextMenuEnabled = m_configManager->isContextMenuEnabled();

    // 设置窗口标题
    m_windowTitle = m_configManager->getAppName();

    // 初始化各个组件
    initializeWindow();
    initializeCEF();
    initializeHotkeys();
    initializeMaintenanceTimer();
    setupSecuritySettings();

    m_logger->appEvent("SecureBrowser创建完成");
}

SecureBrowser::~SecureBrowser()
{
    m_logger->appEvent("SecureBrowser开始销毁");

    // 停止定时器
    if (m_maintenanceTimer) {
        m_maintenanceTimer->stop();
        delete m_maintenanceTimer;
        m_maintenanceTimer = nullptr;
    }

    // 销毁CEF浏览器
    destroyCEFBrowser();

    // 清理热键
    if (m_exitHotkeyF10) {
        delete m_exitHotkeyF10;
        m_exitHotkeyF10 = nullptr;
    }
    
    if (m_exitHotkeyBackslash) {
        delete m_exitHotkeyBackslash;
        m_exitHotkeyBackslash = nullptr;
    }

    m_logger->appEvent("SecureBrowser销毁完成");
}

void SecureBrowser::load(const QUrl& url)
{
    m_currentUrl = url;
    m_logger->appEvent(QString("加载URL: %1").arg(url.toString()));

    if (m_cefBrowserCreated) {
        // CEF浏览器已创建，可以直接导航
        // 这里需要通过CEF API导航到新URL
        // 由于CEF的异步特性，实际实现可能需要通过消息传递
        m_logger->appEvent("通过CEF导航到新URL");
    } else {
        // CEF浏览器还未创建，URL将在浏览器创建时加载
        m_logger->appEvent("CEF浏览器未创建，URL将在创建时加载");
    }
}

void SecureBrowser::reload()
{
    m_logger->appEvent("重新加载页面");
    
    if (m_cefBrowserCreated) {
        // 通过CEF API重新加载页面
        m_logger->appEvent("通过CEF重新加载页面");
    } else {
        m_logger->appEvent("CEF浏览器未创建，无法重新加载");
    }
}

QUrl SecureBrowser::url() const
{
    return m_currentUrl;
}

void SecureBrowser::setWindowTitle(const QString& title)
{
    m_windowTitle = title;
    QWidget::setWindowTitle(title);
    m_logger->appEvent(QString("窗口标题设置为: %1").arg(title));
}

bool SecureBrowser::event(QEvent *event)
{
    if (event->type() == QEvent::ShortcutOverride) {
        QKeyEvent *keyEvent = static_cast<QKeyEvent*>(event);

        // 关键修改：更细粒度拦截（与原项目逻辑相同）
        const bool onlyShift = (keyEvent->modifiers() == Qt::ShiftModifier);
        const bool ctrlR = (keyEvent->key() == Qt::Key_R && keyEvent->modifiers() == Qt::ControlModifier);

        if (onlyShift || ctrlR) {
            // 放行单纯 Shift* 和 Ctrl+R
            return QWidget::event(event);
        }

        const bool hasSysMod = keyEvent->modifiers() & (Qt::AltModifier | Qt::ControlModifier | Qt::MetaModifier);
        if (hasSysMod) {
            // 其余带系统修饰键全部拦截
            logKeyboardEvent(keyEvent, false);
            event->accept();
            return true;
        }
    }

    return QWidget::event(event);
}

void SecureBrowser::keyPressEvent(QKeyEvent *event)
{
    QString keySequence = QKeySequence(event->key() | event->modifiers()).toString();
    
    if (!m_logger->getLogLevel() == L_DEBUG) {
        m_logger->appEvent(QString("按键事件: %1").arg(keySequence));
    }

    // 允许Ctrl+R刷新
    if (event->key() == Qt::Key_R && event->modifiers() == Qt::ControlModifier) {
        reload();
        event->accept();
        logKeyboardEvent(event, true);
        return;
    }

    // 阻止Escape组合键
    if (event->key() == Qt::Key_Escape &&
        (event->modifiers() & (Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier))) {
        event->ignore();
        logKeyboardEvent(event, false);
        return;
    }

    // 检查是否是安全相关的按键
    if (isSecurityKeyEvent(event)) {
        logKeyboardEvent(event, false);
        event->ignore();
        return;
    }

    QWidget::keyPressEvent(event);
    logKeyboardEvent(event, true);
}

void SecureBrowser::closeEvent(QCloseEvent *event)
{
    m_logger->appEvent("收到窗口关闭事件，忽略");
    event->ignore(); // 阻止关闭
}

void SecureBrowser::focusOutEvent(QFocusEvent *event)
{
    m_logger->appEvent("窗口失去焦点");
    QWidget::focusOutEvent(event);
    
    // 立即重新获取焦点（如果启用焦点检查）
    if (m_needFocusCheck) {
        QTimer::singleShot(100, this, [this]() {
            enforceFocus();
        });
    }
}

void SecureBrowser::contextMenuEvent(QContextMenuEvent *event)
{
    if (!m_contextMenuEnabled) {
        m_logger->appEvent("右键菜单被禁用");
        event->ignore();
    } else {
        QWidget::contextMenuEvent(event);
    }
}

void SecureBrowser::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    
    // 调整CEF浏览器大小
    if (m_cefBrowserCreated) {
        resizeCEFBrowser();
    }
}

void SecureBrowser::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    
    // 确保全屏模式
    setFullscreenMode();
    
    // 创建CEF浏览器（如果还未创建）
    if (!m_cefBrowserCreated) {
        createCEFBrowser();
    }
}

void SecureBrowser::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::WindowStateChange) {
        QWindowStateChangeEvent *stateEvent = static_cast<QWindowStateChangeEvent*>(event);
        
        if (m_needFullscreenCheck && !(windowState() & Qt::WindowFullScreen)) {
            m_logger->appEvent("检测到窗口状态变化，强制恢复全屏");
            setFullscreenMode();
        }
    }
    
    QWidget::changeEvent(event);
}

void SecureBrowser::handleExitHotkey()
{
    m_needFocusCheck = false; // 暂时禁用焦点检查
    
    QString password;
    bool ok = m_logger->getPassword(this, "安全退出", "请输入退出密码：", password);
    QString exitPassword = m_configManager->getExitPassword();
    
    if (ok && password == exitPassword) {
        m_logger->hotkeyEvent("密码正确，退出");
        m_logger->shutdown();
        QApplication::quit();
    } else {
        m_logger->hotkeyEvent(ok ? "密码错误" : "取消输入");
        m_logger->showMessage(this, "错误", ok ? "密码错误" : "已取消");
        m_needFocusCheck = true; // 恢复焦点检查
    }
}

void SecureBrowser::onMaintenanceTimer()
{
    // 定期维护任务（与原项目逻辑相同）
    if (m_needFocusCheck && !isActiveWindow()) {
        enforceFocus();
    }
    
    if (m_needFullscreenCheck && windowState() != Qt::WindowFullScreen) {
        setFullscreenMode();
    }
}

void SecureBrowser::onBrowserCreated()
{
    m_cefBrowserCreated = true;
    m_logger->appEvent("CEF浏览器创建完成");
    
    // 如果有待加载的URL，现在加载它
    if (!m_currentUrl.isEmpty()) {
        load(m_currentUrl);
    }
}

void SecureBrowser::initializeWindow()
{
    // 设置窗口属性（与原项目相同）
    setWindowFlags(Qt::Window | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
    setWindowState(Qt::WindowFullScreen);
    setMinimumSize(1280, 800);
    
    // 设置窗口标题
    QWidget::setWindowTitle(m_windowTitle);
    
    // 禁用上下文菜单策略
    setContextMenuPolicy(Qt::NoContextMenu);
    
    m_logger->appEvent("窗口初始化完成");
}

void SecureBrowser::initializeCEF()
{
    // CEF管理器将在Application类中创建和传递
    // 这里不直接创建CEF管理器
    m_logger->appEvent("CEF初始化准备完成");
}

void SecureBrowser::initializeHotkeys()
{
    try {
        // 创建全局热键（与原项目相同）
        m_exitHotkeyF10 = new QHotkey(QKeySequence("F10"), true, this);
        m_exitHotkeyBackslash = new QHotkey(QKeySequence("\\"), true, this);
        
        // 连接信号（使用队列连接提高稳定性）
        connect(m_exitHotkeyF10, &QHotkey::activated, this, &SecureBrowser::handleExitHotkey, Qt::QueuedConnection);
        connect(m_exitHotkeyBackslash, &QHotkey::activated, this, &SecureBrowser::handleExitHotkey, Qt::QueuedConnection);
        
        m_logger->appEvent("全局热键注册成功");
    } catch (...) {
        m_logger->errorEvent("全局热键注册失败");
    }
}

void SecureBrowser::initializeMaintenanceTimer()
{
    // 创建维护定时器（与原项目相同）
    m_maintenanceTimer = new QTimer(this);
    connect(m_maintenanceTimer, &QTimer::timeout, this, &SecureBrowser::onMaintenanceTimer, Qt::QueuedConnection);
    m_maintenanceTimer->start(1500); // 每1.5秒检查一次
    
    m_logger->appEvent("维护定时器启动");
}

void SecureBrowser::setupSecuritySettings()
{
    // 设置安全相关的属性
    setAttribute(Qt::WA_DeleteOnClose, false); // 防止意外关闭
    
    // 获取窗口句柄用于CEF集成
#ifdef Q_OS_WIN
    m_windowHandle = reinterpret_cast<void*>(winId());
#elif defined(Q_OS_MAC)
    m_windowHandle = reinterpret_cast<void*>(winId());
#elif defined(Q_OS_LINUX)
    m_windowHandle = reinterpret_cast<void*>(winId());
#endif
    
    m_logger->appEvent("安全设置配置完成");
}

void SecureBrowser::enforceFullscreen()
{
    if (windowState() != Qt::WindowFullScreen) {
        setWindowState(Qt::WindowFullScreen);
        showFullScreen();
        m_logger->appEvent("强制恢复全屏模式");
    }
}

void SecureBrowser::enforceFocus()
{
    if (!isActiveWindow()) {
        raise();
        activateWindow();
        setFocus();
        m_logger->appEvent("强制恢复窗口焦点");
    }
}

void SecureBrowser::enforceWindowState()
{
    enforceFullscreen();
    enforceFocus();
}

bool SecureBrowser::isSecurityKeyEvent(QKeyEvent *event)
{
    // 检查是否是可能威胁安全的按键组合
    int modifiers = event->modifiers();
    int key = event->key();
    
    // Alt+Tab, Alt+F4
    if (modifiers & Qt::AltModifier) {
        if (key == Qt::Key_Tab || key == Qt::Key_F4) {
            return true;
        }
    }
    
    // Ctrl+Alt+Del, Ctrl+Shift+Esc
    if ((modifiers & Qt::ControlModifier) && (modifiers & Qt::AltModifier)) {
        if (key == Qt::Key_Delete) {
            return true;
        }
    }
    
    if ((modifiers & Qt::ControlModifier) && (modifiers & Qt::ShiftModifier)) {
        if (key == Qt::Key_Escape) {
            return true;
        }
    }
    
    // Windows键
    if (modifiers & Qt::MetaModifier) {
        return true;
    }
    
    // 其他危险的组合键
    if (modifiers & Qt::ControlModifier) {
        switch (key) {
            case Qt::Key_W:  // Ctrl+W (关闭窗口)
            case Qt::Key_T:  // Ctrl+T (新标签页)
            case Qt::Key_N:  // Ctrl+N (新窗口)
            case Qt::Key_O:  // Ctrl+O (打开文件)
                return true;
        }
    }
    
    return false;
}

void SecureBrowser::logKeyboardEvent(QKeyEvent *event, bool allowed)
{
    if (m_logger->getLogLevel() <= L_DEBUG) {
        QString status = allowed ? "允许" : "阻止";
        QString keyInfo = QString("键码: %1, 修饰符: %2")
            .arg(event->key())
            .arg(static_cast<int>(event->modifiers()));
        m_logger->logEvent("键盘控制", QString("%1 - %2").arg(status).arg(keyInfo), "keyboard.log", L_DEBUG);
    }
}

void SecureBrowser::setFullscreenMode()
{
    setWindowState(Qt::WindowFullScreen);
    showFullScreen();
    setAlwaysOnTop();
}

void SecureBrowser::setAlwaysOnTop()
{
    // 确保窗口始终置顶
    setWindowFlags(windowFlags() | Qt::WindowStaysOnTopHint);
    show(); // 重新显示以应用标志
}

void SecureBrowser::disableWindowControls()
{
    // 禁用窗口控制按钮（最小化、最大化、关闭）
    setWindowFlags(Qt::Window | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
}

void SecureBrowser::createCEFBrowser()
{
    if (m_cefBrowserCreated || !m_windowHandle) {
        return;
    }
    
    m_logger->appEvent("开始创建CEF浏览器");
    
    // 获取CEF管理器实例（需要从Application获取）
    // 这里暂时使用占位符逻辑
    QString initialUrl = m_currentUrl.isEmpty() ? m_configManager->getUrl() : m_currentUrl.toString();
    
    try {
        // 这里需要调用CEF管理器创建浏览器
        // m_cefBrowserId = m_cefManager->createBrowser(m_windowHandle, initialUrl);
        
        if (m_cefBrowserId > 0) {
            onBrowserCreated();
        } else {
            handleBrowserError("CEF浏览器创建失败");
        }
    } catch (...) {
        handleBrowserError("CEF浏览器创建异常");
    }
}

void SecureBrowser::destroyCEFBrowser()
{
    if (m_cefBrowserCreated) {
        m_logger->appEvent("销毁CEF浏览器");
        // 这里需要调用CEF API关闭浏览器
        m_cefBrowserCreated = false;
        m_cefBrowserId = 0;
    }
}

void SecureBrowser::resizeCEFBrowser()
{
    if (m_cefBrowserCreated) {
        // 调整CEF浏览器大小以匹配窗口
        QSize windowSize = size();
        m_logger->appEvent(QString("调整CEF浏览器大小: %1x%2").arg(windowSize.width()).arg(windowSize.height()));
        
        // 这里需要调用CEF API调整浏览器大小
    }
}

void SecureBrowser::handleBrowserError(const QString& error)
{
    m_logger->errorEvent(QString("浏览器错误: %1").arg(error));
    
    QMessageBox::critical(this, "浏览器错误", 
        QString("浏览器初始化失败：\n%1\n\n请检查CEF安装是否完整。").arg(error));
}

void SecureBrowser::showSecurityViolationWarning(const QString& violation)
{
    m_logger->logEvent("安全警告", violation, "security.log", L_WARNING);
    
    if (m_strictSecurityMode) {
        QMessageBox::warning(this, "安全警告", 
            QString("检测到安全违规行为：\n%1").arg(violation));
    }
}