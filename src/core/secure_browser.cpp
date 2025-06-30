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

SecureBrowser::SecureBrowser(CEFManager* cefManager, QWidget *parent)
    : QWidget(parent)
    , m_cefManager(cefManager)
    , m_logger(&Logger::instance())
    , m_configManager(&ConfigManager::instance())
    , m_needFocusCheck(true)
    , m_needFullscreenCheck(true)
    , m_cefBrowserCreated(false)
    , m_cefBrowserId(0)
    , m_cefPerformanceState(CEFPerformanceState::Loading)
    , m_strictSecurityMode(true)
    , m_keyboardFilterEnabled(true)
    , m_contextMenuEnabled(false)
    , m_windowHandle(nullptr)
    , m_cefMessageLoopLogCounter(0)
    , m_loadingOverlay(nullptr)
    , m_loadingLayout(nullptr)
    , m_loadingIcon(nullptr)
    , m_loadingText(nullptr)
    , m_loadingProgressBar(nullptr)
    , m_loadingMovie(nullptr)
    , m_isLoadingVisible(false)
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
    initializeCEFMessageLoopTimer();
    setupSecuritySettings();
    initializeLoadingAnimation();

    m_logger->appEvent("SecureBrowser创建完成");
}

SecureBrowser::~SecureBrowser()
{
    m_logger->appEvent("SecureBrowser开始销毁");

    // 停止定时器（智能指针自动管理内存）
    if (m_maintenanceTimer) {
        m_maintenanceTimer->stop();
    }
    
    if (m_cefMessageLoopTimer) {
        m_cefMessageLoopTimer->stop();
    }

    // 销毁CEF浏览器
    destroyCEFBrowser();

    // 清理热键（智能指针自动管理内存）
    // 智能指针会在析构时自动清理，无需手动delete

    // 清理加载动画组件
    if (m_loadingMovie) {
        m_loadingMovie->stop();
        delete m_loadingMovie;
        m_loadingMovie = nullptr;
    }
    
    if (m_loadingOverlay) {
        delete m_loadingOverlay;
        m_loadingOverlay = nullptr;
    }

    m_logger->appEvent("SecureBrowser销毁完成");
}

void SecureBrowser::load(const QUrl& url)
{
    m_currentUrl = url;
    m_logger->appEvent(QString("加载URL: %1").arg(url.toString()));

    // 显示加载动画
    onPageLoadStart();

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
    
    // CEF性能状态管理：如果页面已加载且空闲一段时间，切换到Idle状态以节约CPU
    static int idleCounter = 0;
    if (m_cefPerformanceState == CEFPerformanceState::Loaded) {
        idleCounter++;
        // 空闲15秒后（10次维护周期 * 1.5秒）切换到Idle状态
        if (idleCounter >= 10) {
            setCEFPerformanceState(CEFPerformanceState::Idle);
            idleCounter = 0;
        }
    } else {
        idleCounter = 0; // 重置计数器
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
        // 创建全局热键（使用智能指针）
        m_exitHotkeyF10 = std::make_unique<QHotkey>(QKeySequence("F10"), true, this);
        m_exitHotkeyBackslash = std::make_unique<QHotkey>(QKeySequence("\\"), true, this);
        
        // 连接信号（使用队列连接提高稳定性）
        connect(m_exitHotkeyF10.get(), &QHotkey::activated, this, &SecureBrowser::handleExitHotkey, Qt::QueuedConnection);
        connect(m_exitHotkeyBackslash.get(), &QHotkey::activated, this, &SecureBrowser::handleExitHotkey, Qt::QueuedConnection);
        
        m_logger->appEvent("全局热键注册成功");
    } catch (...) {
        m_logger->errorEvent("全局热键注册失败");
    }
}

void SecureBrowser::initializeMaintenanceTimer()
{
    // 创建维护定时器（使用智能指针）
    m_maintenanceTimer = std::make_unique<QTimer>(this);
    connect(m_maintenanceTimer.get(), &QTimer::timeout, this, &SecureBrowser::onMaintenanceTimer, Qt::QueuedConnection);
    m_maintenanceTimer->start(1500); // 每1.5秒检查一次
    
    m_logger->appEvent("维护定时器启动");
}

void SecureBrowser::initializeCEFMessageLoopTimer()
{
    // 创建CEF消息循环定时器（使用智能指针）
    m_cefMessageLoopTimer = std::make_unique<QTimer>(this);
    connect(m_cefMessageLoopTimer.get(), &QTimer::timeout, this, &SecureBrowser::onCEFMessageLoop, Qt::QueuedConnection);
    
    // 使用动态间隔而非固定10ms
    updateCEFMessageLoopInterval();
    
    m_logger->appEvent("CEF消息循环定时器启动 - 使用动态间隔优化性能");
    m_logger->appEvent(QString("CEF管理器状态: %1, 浏览器创建状态: %2")
        .arg(m_cefManager ? "已初始化" : "未初始化")
        .arg(m_cefBrowserCreated ? "已创建" : "未创建"));
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
        if (!m_cefManager) {
            handleBrowserError("CEF管理器未初始化");
            return;
        }
        
        // 调用CEF管理器创建浏览器
        m_cefBrowserId = m_cefManager->createBrowser(m_windowHandle, initialUrl);
        
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

void SecureBrowser::initializeLoadingAnimation()
{
    // 创建加载动画覆盖层
    m_loadingOverlay = new QFrame(this);
    m_loadingOverlay->setObjectName("loadingOverlay");
    m_loadingOverlay->setFrameStyle(QFrame::NoFrame);
    m_loadingOverlay->setStyleSheet(
        "#loadingOverlay {"
        "    background-color: rgba(255, 255, 255, 220);"
        "    border: none;"
        "}"
    );
    
    // 创建垂直布局
    m_loadingLayout = new QVBoxLayout(m_loadingOverlay);
    m_loadingLayout->setAlignment(Qt::AlignCenter);
    m_loadingLayout->setSpacing(20);
    
    // 创建加载图标
    m_loadingIcon = new QLabel();
    m_loadingIcon->setAlignment(Qt::AlignCenter);
    m_loadingIcon->setFixedSize(64, 64);
    
    // 创建简单的加载动画（旋转圆点）
    QString loadingStyle = 
        "QLabel {"
        "    border: 6px solid #f3f3f3;"
        "    border-top: 6px solid #3498db;"
        "    border-radius: 32px;"
        "    background-color: transparent;"
        "}";
    m_loadingIcon->setStyleSheet(loadingStyle);
    
    // 创建加载文本
    m_loadingText = new QLabel("正在加载页面...");
    m_loadingText->setAlignment(Qt::AlignCenter);
    m_loadingText->setStyleSheet(
        "QLabel {"
        "    font-size: 16px;"
        "    color: #333333;"
        "    background-color: transparent;"
        "    font-weight: bold;"
        "}"
    );
    
    // 创建进度条
    m_loadingProgressBar = new QProgressBar();
    m_loadingProgressBar->setRange(0, 100);
    m_loadingProgressBar->setValue(0);
    m_loadingProgressBar->setFixedWidth(300);
    m_loadingProgressBar->setFixedHeight(20);
    m_loadingProgressBar->setStyleSheet(
        "QProgressBar {"
        "    border: 2px solid #cccccc;"
        "    border-radius: 10px;"
        "    text-align: center;"
        "    background-color: #f0f0f0;"
        "}"
        "QProgressBar::chunk {"
        "    background-color: #3498db;"
        "    border-radius: 8px;"
        "}"
    );
    
    // 添加组件到布局
    m_loadingLayout->addWidget(m_loadingIcon);
    m_loadingLayout->addWidget(m_loadingText);
    m_loadingLayout->addWidget(m_loadingProgressBar);
    
    // 初始状态隐藏
    m_loadingOverlay->hide();
    m_isLoadingVisible = false;
    
    m_logger->appEvent("加载动画初始化完成");
}

void SecureBrowser::showLoadingAnimation()
{
    if (!m_loadingOverlay || m_isLoadingVisible) {
        return;
    }
    
    // 调整覆盖层大小以匹配窗口
    m_loadingOverlay->resize(size());
    m_loadingOverlay->raise(); // 确保在最前面
    m_loadingOverlay->show();
    m_isLoadingVisible = true;
    
    // 重置进度条
    m_loadingProgressBar->setValue(0);
    
    m_logger->appEvent("显示加载动画");
}

void SecureBrowser::hideLoadingAnimation()
{
    if (!m_loadingOverlay || !m_isLoadingVisible) {
        return;
    }
    
    m_loadingOverlay->hide();
    m_isLoadingVisible = false;
    
    m_logger->appEvent("隐藏加载动画");
}

void SecureBrowser::updateLoadingProgress(int progress)
{
    if (!m_loadingProgressBar) {
        return;
    }
    
    // 确保进度值在有效范围内
    progress = qBound(0, progress, 100);
    m_loadingProgressBar->setValue(progress);
    
    // 更新加载文本
    if (m_loadingText) {
        if (progress < 100) {
            m_loadingText->setText(QString("正在加载页面... %1%").arg(progress));
        } else {
            m_loadingText->setText("页面加载完成");
        }
    }
    
    m_logger->appEvent(QString("更新加载进度: %1%").arg(progress));
}

void SecureBrowser::onPageLoadStart()
{
    // 页面开始加载，切换到Loading状态以提高消息循环频率
    setCEFPerformanceState(CEFPerformanceState::Loading);
    
    showLoadingAnimation();
    updateLoadingProgress(0);
    m_logger->appEvent("页面开始加载，显示加载动画");
}

void SecureBrowser::onPageLoadEnd()
{
    updateLoadingProgress(100);
    
    // 延迟隐藏加载动画，让用户看到完成状态
    QTimer::singleShot(500, this, [this]() {
        hideLoadingAnimation();
    });
    
    // 页面加载完成，切换到Loaded状态以降低消息循环频率
    setCEFPerformanceState(CEFPerformanceState::Loaded);
    
    m_logger->appEvent("页面加载完成，隐藏加载动画");
}

void SecureBrowser::setCEFPerformanceState(CEFPerformanceState state)
{
    if (m_cefPerformanceState != state) {
        CEFPerformanceState oldState = m_cefPerformanceState;
        m_cefPerformanceState = state;
        
        // 更新消息循环间隔
        updateCEFMessageLoopInterval();
        
        // 记录状态变化
        QString oldStateStr, newStateStr;
        switch (oldState) {
            case CEFPerformanceState::Loading: oldStateStr = "Loading"; break;
            case CEFPerformanceState::Loaded: oldStateStr = "Loaded"; break;
            case CEFPerformanceState::Idle: oldStateStr = "Idle"; break;
        }
        switch (state) {
            case CEFPerformanceState::Loading: newStateStr = "Loading"; break;
            case CEFPerformanceState::Loaded: newStateStr = "Loaded"; break;
            case CEFPerformanceState::Idle: newStateStr = "Idle"; break;
        }
        
        m_logger->appEvent(QString("CEF性能状态变化: %1 -> %2").arg(oldStateStr).arg(newStateStr));
    }
}

void SecureBrowser::updateCEFMessageLoopInterval()
{
    if (!m_cefMessageLoopTimer) {
        return;
    }
    
    int interval;
    QString description;
    
    switch (m_cefPerformanceState) {
        case CEFPerformanceState::Loading:
            interval = 10;  // 高频率 - 页面加载中需要快速响应
            description = "Loading状态 - 10ms间隔（高性能）";
            break;
        case CEFPerformanceState::Loaded:
            interval = 30;  // 中频率 - 页面已加载，保持正常响应
            description = "Loaded状态 - 30ms间隔（平衡性能）";
            break;
        case CEFPerformanceState::Idle:
            interval = 100; // 低频率 - 空闲状态，节约CPU资源
            description = "Idle状态 - 100ms间隔（节能模式）";
            break;
        default:
            interval = 30;
            description = "默认状态 - 30ms间隔";
            break;
    }
    
    m_cefMessageLoopTimer->setInterval(interval);
    
    // 只在调试模式下记录详细的间隔变化
    if (m_logger->getLogLevel() <= L_DEBUG) {
        m_logger->appEvent(QString("CEF消息循环间隔更新: %1").arg(description));
    }
}