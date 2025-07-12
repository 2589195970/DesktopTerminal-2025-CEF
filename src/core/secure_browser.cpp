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
#elif defined(Q_OS_LINUX)
#include <X11/Xlib.h>
#endif

SecureBrowser::SecureBrowser(CEFManager* cefManager, QWidget *parent)
    : QWidget(parent)
    , m_cefManager(cefManager)
    , m_logger(&Logger::instance())
    , m_configManager(&ConfigManager::instance())
    , m_exitHotkeyF10(nullptr)
    , m_exitHotkeyBackslash(nullptr)
    , m_devToolsHotkeyF12(nullptr)
    , m_maintenanceTimer(nullptr)
    , m_cefMessageLoopTimer(nullptr)
    , m_needFocusCheck(true)
    , m_needFullscreenCheck(true)
    , m_cefBrowserCreated(false)
    , m_cefBrowserId(0)
    , m_strictSecurityMode(true)
    , m_keyboardFilterEnabled(true)
    , m_contextMenuEnabled(false)
    , m_devToolsOpen(false)
    , m_windowHandle(nullptr)
    , m_cefMessageLoopLogCounter(0)
{
    m_logger->appEvent("SecureBrowser创建开始");
    
    // 输出编译和运行时兼容性诊断信息
    m_logger->appEvent("=== SecureBrowser兼容性诊断 ===");
    m_logger->appEvent(QString("Qt编译版本: %1").arg(QT_VERSION_STR));
    m_logger->appEvent(QString("Qt运行时版本: %1").arg(qVersion()));
    
#if QT_VERSION < QT_VERSION_CHECK(5, 12, 0)
    m_logger->errorEvent("警告：Qt版本过低，可能存在兼容性问题");
#endif
    
    // 检查关键Qt组件可用性
    try {
        WId testId = winId();
        m_logger->appEvent(QString("窗口系统兼容性检查 - WId类型: %1字节").arg(sizeof(WId)));
    } catch (...) {
        m_logger->errorEvent("窗口系统兼容性检查失败");
    }

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
    
    // 连接CEFManager的URL退出信号
    if (m_cefManager) {
        connect(m_cefManager, &CEFManager::urlExitTriggered, this, &SecureBrowser::handleUrlExit);
        m_logger->appEvent("URL退出信号已连接");
    }
    
    initializeMaintenanceTimer();
    initializeCEFMessageLoopTimer();
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
    
    if (m_cefMessageLoopTimer) {
        m_cefMessageLoopTimer->stop();
        delete m_cefMessageLoopTimer;
        m_cefMessageLoopTimer = nullptr;
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
    
    if (m_devToolsHotkeyF12) {
        delete m_devToolsHotkeyF12;
        m_devToolsHotkeyF12 = nullptr;
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
        emit pageLoadStarted();
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

void SecureBrowser::initializeCEFBrowser()
{
    if (m_cefBrowserCreated) {
        m_logger->appEvent("CEF浏览器已初始化，跳过重复初始化");
        return;
    }
    
    m_logger->appEvent("开始初始化CEF浏览器");
    
    // 确保窗口已显示并获取有效窗口句柄
    if (!isVisible()) {
        m_logger->errorEvent("窗口未显示，无法初始化CEF浏览器");
        return;
    }
    
    // 在窗口显示后重新获取窗口句柄
    if (!m_windowHandle) {
        try {
#ifdef Q_OS_WIN
            m_windowHandle = reinterpret_cast<void*>(winId());
#elif defined(Q_OS_MAC)
            m_windowHandle = reinterpret_cast<void*>(winId());
#elif defined(Q_OS_LINUX)
            m_windowHandle = reinterpret_cast<void*>(winId());
#else
            m_logger->errorEvent("不支持的平台，无法获取窗口句柄");
            return;
#endif
            
            if (m_windowHandle) {
                m_logger->appEvent(QString("窗口句柄获取成功: 0x%1").arg(reinterpret_cast<qulonglong>(m_windowHandle), 0, 16));
            } else {
                m_logger->errorEvent("窗口句柄获取失败：winId()返回0");
                return;
            }
        } catch (const std::exception& e) {
            m_logger->errorEvent(QString("窗口句柄获取异常: %1").arg(e.what()));
            return;
        } catch (...) {
            m_logger->errorEvent("窗口句柄获取发生未知异常");
            return;
        }
    }
    
    // 创建CEF浏览器
    createCEFBrowser();
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
    
    // 只有在窗口句柄无效时才重新获取（避免重复调用冲突）
    if (!m_cefBrowserCreated && !m_windowHandle) {
        m_logger->appEvent("ShowEvent触发，重新获取窗口句柄");
        
        try {
            // 重新获取窗口句柄
#ifdef Q_OS_WIN
            m_windowHandle = reinterpret_cast<void*>(winId());
#elif defined(Q_OS_MAC)
            m_windowHandle = reinterpret_cast<void*>(winId());
#elif defined(Q_OS_LINUX)
            m_windowHandle = reinterpret_cast<void*>(winId());
#else
            m_logger->errorEvent("ShowEvent: 不支持的平台，无法获取窗口句柄");
            return;
#endif
            
            if (m_windowHandle) {
                m_logger->appEvent(QString("ShowEvent中窗口句柄获取成功: 0x%1").arg(reinterpret_cast<qulonglong>(m_windowHandle), 0, 16));
                createCEFBrowser();
            } else {
                m_logger->errorEvent("ShowEvent中窗口句柄获取失败：winId()返回0");
            }
        } catch (const std::exception& e) {
            m_logger->errorEvent(QString("ShowEvent窗口句柄获取异常: %1").arg(e.what()));
        } catch (...) {
            m_logger->errorEvent("ShowEvent窗口句柄获取发生未知异常");
        }
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

void SecureBrowser::handleDevToolsHotkey()
{
    m_needFocusCheck = false; // 暂时禁用焦点检查
    
    m_logger->hotkeyEvent("F12开发者工具热键被触发");
    
    // 调试模式：暂时禁用密码验证以测试F12功能
    // 用户可以通过配置文件控制是否需要密码
    bool requirePassword = m_configManager->isStrictSecurityMode();
    
    if (requirePassword) {
        QString password;
        bool ok = m_logger->getPassword(this, "开发者工具", "请输入密码以开启/关闭开发者工具：", password);
        QString exitPassword = m_configManager->getExitPassword();
        
        if (ok && password == exitPassword) {
            m_logger->hotkeyEvent("开发者工具密码正确");
            toggleDevTools();
        } else {
            m_logger->hotkeyEvent(ok ? "开发者工具密码错误" : "取消开发者工具");
            m_logger->showMessage(this, "错误", ok ? "密码错误" : "已取消");
            m_needFocusCheck = true; // 恢复焦点检查
        }
    } else {
        // 调试模式：直接打开开发者工具
        m_logger->hotkeyEvent("调试模式：直接开启开发者工具（无需密码）");
        toggleDevTools();
        m_needFocusCheck = true; // 恢复焦点检查
    }
}

void SecureBrowser::handleUrlExit(const QString& url)
{
    m_logger->appEvent(QString("收到URL退出信号: %1").arg(url));
    
    // 记录URL退出事件到exit.log
    m_logger->exitEvent(QString("URL退出触发: %1").arg(url));
    
    // 直接退出，不需要密码验证
    m_logger->appEvent("URL检测退出，无需密码验证");
    m_logger->shutdown();
    QApplication::quit();
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

void SecureBrowser::onCEFMessageLoop()
{
    // 处理CEF消息循环（单进程模式必需）
    m_cefMessageLoopLogCounter++;
    
    if (!m_cefManager) {
        if (m_cefMessageLoopLogCounter % 1000 == 1) { // 每10秒记录一次错误
            m_logger->errorEvent("CEF消息循环错误: CEF管理器未初始化");
        }
        return;
    }
    
    if (!m_cefBrowserCreated) {
        if (m_cefMessageLoopLogCounter % 500 == 1) { // 每5秒记录一次状态
            m_logger->appEvent("CEF消息循环等待: 浏览器尚未创建完成");
        }
        return;
    }
    
    // 调用CEF消息循环处理
    try {
        m_cefManager->doMessageLoopWork();
        
        // 定期记录成功状态（每30秒一次）
        if (m_cefMessageLoopLogCounter % 3000 == 1) {
            m_logger->appEvent("CEF消息循环正常运行 - 白屏问题应已解决");
        }
    } catch (...) {
        if (m_cefMessageLoopLogCounter % 100 == 1) { // 每1秒记录一次异常
            m_logger->errorEvent("CEF消息循环处理异常");
        }
    }
}

void SecureBrowser::onBrowserCreated()
{
    m_cefBrowserCreated = true;
    m_logger->appEvent("CEF浏览器创建完成");
    m_logger->appEvent("CEF消息循环现在应该开始处理页面内容 - 白屏问题修复关键点");
    
    // 重置计数器，开始新的日志周期
    m_cefMessageLoopLogCounter = 0;
    
    // 如果有待加载的URL，现在加载它
    if (!m_currentUrl.isEmpty()) {
        emit pageLoadStarted();
        load(m_currentUrl);
    }
    
    // 延时发出内容加载完成信号，给CEF一些时间完成初始页面加载
    QTimer::singleShot(1000, this, [this]() {
        m_logger->appEvent("发出内容加载完成信号");
        emit contentLoadFinished();
        emit pageLoadFinished();
    });
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
        m_devToolsHotkeyF12 = new QHotkey(QKeySequence("F12"), true, this);
        
        // 连接信号（使用队列连接提高稳定性）
        connect(m_exitHotkeyF10, &QHotkey::activated, this, &SecureBrowser::handleExitHotkey, Qt::QueuedConnection);
        connect(m_exitHotkeyBackslash, &QHotkey::activated, this, &SecureBrowser::handleExitHotkey, Qt::QueuedConnection);
        connect(m_devToolsHotkeyF12, &QHotkey::activated, this, &SecureBrowser::handleDevToolsHotkey, Qt::QueuedConnection);
        
        // 检查每个热键的注册状态 - 修复F12无效的关键检查
        QString registrationStatus;
        bool allRegistered = true;
        
        if (m_exitHotkeyF10->isRegistered()) {
            registrationStatus += "F10: ✓ ";
        } else {
            registrationStatus += "F10: ✗ ";
            allRegistered = false;
        }
        
        if (m_exitHotkeyBackslash->isRegistered()) {
            registrationStatus += "\\: ✓ ";
        } else {
            registrationStatus += "\\: ✗ ";
            allRegistered = false;
        }
        
        if (m_devToolsHotkeyF12->isRegistered()) {
            registrationStatus += "F12: ✓";
        } else {
            registrationStatus += "F12: ✗ (可能与系统DevTools冲突)";
            allRegistered = false;
            
            // F12注册失败时，尝试使用备用热键
            m_logger->appEvent("F12热键注册失败，尝试备用方案...");
            
            // 删除失败的F12热键
            delete m_devToolsHotkeyF12;
            m_devToolsHotkeyF12 = nullptr;
            
            // 尝试使用Ctrl+F12作为备用
            m_devToolsHotkeyF12 = new QHotkey(QKeySequence("Ctrl+F12"), true, this);
            connect(m_devToolsHotkeyF12, &QHotkey::activated, this, &SecureBrowser::handleDevToolsHotkey, Qt::QueuedConnection);
            
            if (m_devToolsHotkeyF12->isRegistered()) {
                registrationStatus += " -> Ctrl+F12: ✓";
                m_logger->appEvent("使用Ctrl+F12作为开发者工具热键");
            } else {
                registrationStatus += " -> Ctrl+F12: ✗";
                m_logger->errorEvent("所有F12相关热键都注册失败");
            }
        }
        
        if (allRegistered) {
            m_logger->appEvent(QString("全局热键注册成功: %1").arg(registrationStatus));
        } else {
            m_logger->appEvent(QString("全局热键部分注册失败: %1").arg(registrationStatus));
        }
        
    } catch (...) {
        m_logger->errorEvent("全局热键注册异常");
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

void SecureBrowser::initializeCEFMessageLoopTimer()
{
    // 创建CEF消息循环定时器（单进程模式必需）
    m_cefMessageLoopTimer = new QTimer(this);
    connect(m_cefMessageLoopTimer, &QTimer::timeout, this, &SecureBrowser::onCEFMessageLoop, Qt::QueuedConnection);
    m_cefMessageLoopTimer->start(10); // 每10ms调用一次CEF消息循环（约100FPS）
    
    m_logger->appEvent("CEF消息循环定时器启动 - 间隔10ms，这对解决白屏问题至关重要");
    m_logger->appEvent(QString("CEF管理器状态: %1, 浏览器创建状态: %2")
        .arg(m_cefManager ? "已初始化" : "未初始化")
        .arg(m_cefBrowserCreated ? "已创建" : "未创建"));
}

void SecureBrowser::setupSecuritySettings()
{
    // 设置安全相关的属性
    setAttribute(Qt::WA_DeleteOnClose, false); // 防止意外关闭
    
    // 窗口句柄将在窗口完全显示后获取，避免早期获取无效句柄
    m_windowHandle = nullptr;
    
    m_logger->appEvent(QString("安全设置配置完成 - 窗口状态: 可见=%1, 原生ID=0x%2")
        .arg(isVisible() ? "是" : "否")
        .arg(static_cast<qulonglong>(winId()), 0, 16));
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
    if (m_cefBrowserCreated) {
        m_logger->appEvent("CEF浏览器已创建，跳过重复创建");
        return;
    }
    
    if (!m_windowHandle) {
        m_logger->errorEvent("窗口句柄无效，无法创建CEF浏览器");
        return;
    }
    
    m_logger->appEvent(QString("开始创建CEF浏览器，窗口句柄: 0x%1").arg(reinterpret_cast<qulonglong>(m_windowHandle), 0, 16));
    
    QString initialUrl = m_currentUrl.isEmpty() ? m_configManager->getUrl() : m_currentUrl.toString();
    
    try {
        if (!m_cefManager) {
            handleBrowserError("CEF管理器未初始化");
            return;
        }
        
        if (!m_cefManager->isInitialized()) {
            handleBrowserError("CEF尚未完成初始化");
            return;
        }
        
        // 调用CEF管理器创建浏览器
        m_logger->appEvent(QString("调用CEF管理器创建浏览器，URL: %1").arg(initialUrl));
        
        // 增强诊断：记录创建浏览器前的状态
        m_logger->appEvent(QString("CEF管理器状态检查 - 已初始化: %1").arg(m_cefManager->isInitialized() ? "是" : "否"));
        
        m_cefBrowserId = m_cefManager->createBrowser(m_windowHandle, initialUrl);
        
        if (m_cefBrowserId > 0) {
            m_logger->appEvent(QString("CEF浏览器创建成功，ID: %1").arg(m_cefBrowserId));
            onBrowserCreated();
        } else {
            handleBrowserError("CEF浏览器创建失败 - createBrowser返回0");
        }
    } catch (const std::exception& e) {
        handleBrowserError(QString("CEF浏览器创建异常: %1").arg(e.what()));
    } catch (...) {
        handleBrowserError("CEF浏览器创建发生未知异常");
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

void SecureBrowser::toggleDevTools()
{
    if (!m_cefManager || !m_cefBrowserCreated) {
        m_logger->errorEvent("开发者工具操作失败：CEF浏览器未准备就绪");
        m_logger->showMessage(this, "错误", "浏览器未准备就绪，无法操作开发者工具");
        return;
    }
    
    if (m_devToolsOpen) {
        // 关闭开发者工具
        if (m_cefManager->closeDevTools(m_cefBrowserId)) {
            m_devToolsOpen = false;
            m_logger->appEvent("开发者工具已关闭");
            m_logger->showMessage(this, "开发者工具", "开发者工具已关闭");
            
            // 通知键盘过滤器禁用开发者模式
            // TODO: 集成KeyboardFilter实例并调用setDeveloperModeEnabled(false)
        }
    } else {
        // 开启开发者工具
        if (m_cefManager->showDevTools(m_cefBrowserId)) {
            m_devToolsOpen = true;
            m_logger->appEvent("开发者工具已开启");
            m_logger->showMessage(this, "开发者工具", "开发者工具已开启");
            
            // 通知键盘过滤器启用开发者模式
            // TODO: 集成KeyboardFilter实例并调用setDeveloperModeEnabled(true)
        }
    }
    
    // 操作完成后恢复焦点检查
    m_needFocusCheck = true;
}

bool SecureBrowser::isDevToolsOpen() const
{
    return m_devToolsOpen;
}

void SecureBrowser::showSecurityViolationWarning(const QString& violation)
{
    m_logger->logEvent("安全警告", violation, "security.log", L_WARNING);
    
    if (m_strictSecurityMode) {
        QMessageBox::warning(this, "安全警告", 
            QString("检测到安全违规行为：\n%1").arg(violation));
    }
}