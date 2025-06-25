#include "window_manager.h"
#include "../logging/logger.h"
#include "../config/config_manager.h"

#include <QApplication>
#include <QDesktopWidget>
#include <QScreen>
#include <QWindow>

#ifdef Q_OS_WIN
#include <windows.h>
#elif defined(Q_OS_MAC)
#include <AppKit/AppKit.h>
#elif defined(Q_OS_LINUX)
#include <X11/Xlib.h>
#endif

WindowManager::WindowManager(QWidget* targetWindow, QObject *parent)
    : QObject(parent)
    , m_targetWindow(targetWindow)
    , m_logger(&Logger::instance())
    , m_configManager(&ConfigManager::instance())
    , m_monitoringEnabled(true)
    , m_fullscreenCheckEnabled(true)
    , m_focusCheckEnabled(true)
    , m_alwaysOnTopEnabled(true)
    , m_windowCheckTimer(nullptr)
    , m_checkInterval(1500)  // 每1.5秒检查一次（与原项目一致）
    , m_currentScreen(nullptr)
    , m_totalChecks(0)
    , m_fixCount(0)
    , m_fullscreenFixes(0)
    , m_focusFixes(0)
    , m_geometryFixes(0)
    , m_lastFullscreenState(false)
    , m_lastFocusState(false)
{
    m_logger->appEvent("WindowManager创建");
}

WindowManager::~WindowManager()
{
    if (m_windowCheckTimer) {
        m_windowCheckTimer->stop();
        delete m_windowCheckTimer;
    }
    m_logger->appEvent("WindowManager销毁");
}

bool WindowManager::initialize()
{
    if (!m_targetWindow) {
        m_logger->errorEvent("WindowManager初始化失败：目标窗口为空");
        return false;
    }
    
    m_logger->appEvent("WindowManager初始化开始");
    
    // 设置窗口标志
    setupWindowFlags();
    
    // 设置窗口几何形状
    setupWindowGeometry();
    
    // 连接屏幕信号
    connectScreenSignals();
    
    // 初始化监控
    initializeMonitoring();
    
    m_logger->appEvent("WindowManager初始化完成");
    return true;
}

void WindowManager::enforceFullscreen()
{
    if (!m_targetWindow) return;
    
    if (!isWindowFullscreen()) {
        m_logger->appEvent("强制全屏模式");
        
        // 设置窗口状态为全屏
        m_targetWindow->setWindowState(Qt::WindowFullScreen);
        m_targetWindow->showFullScreen();
        
        // 确保窗口几何形状正确
        setupWindowGeometry();
        
        m_fullscreenFixes++;
        emit windowStateFixed("恢复全屏模式");
    }
}

void WindowManager::enforceFocus()
{
    if (!m_targetWindow) return;
    
    if (!isWindowFocused()) {
        m_logger->appEvent("强制窗口焦点");
        
        // 多种方法确保窗口获得焦点
        m_targetWindow->raise();
        m_targetWindow->activateWindow();
        m_targetWindow->setFocus();
        
#ifdef Q_OS_WIN
        // Windows特定的焦点强制方法
        HWND hwnd = reinterpret_cast<HWND>(m_targetWindow->winId());
        if (hwnd) {
            SetForegroundWindow(hwnd);
            SetActiveWindow(hwnd);
            SetFocus(hwnd);
        }
#endif
        
        m_focusFixes++;
        emit windowStateFixed("恢复窗口焦点");
    }
}

void WindowManager::enforceAlwaysOnTop()
{
    if (!m_targetWindow || !m_alwaysOnTopEnabled) return;
    
    if (!isWindowOnTop()) {
        m_logger->appEvent("强制窗口置顶");
        
        // 重新设置窗口标志以确保置顶
        Qt::WindowFlags flags = m_targetWindow->windowFlags();
        flags |= Qt::WindowStaysOnTopHint;
        m_targetWindow->setWindowFlags(flags);
        m_targetWindow->show();
        
        emit windowStateFixed("恢复窗口置顶");
    }
}

void WindowManager::checkAndFixWindowState()
{
    if (!m_targetWindow || !m_monitoringEnabled) return;
    
    m_totalChecks++;
    bool needFix = false;
    QString fixDescription;
    
    // 检查全屏状态
    if (m_fullscreenCheckEnabled && !isWindowFullscreen()) {
        enforceFullscreen();
        needFix = true;
        fixDescription += "全屏 ";
    }
    
    // 检查焦点状态
    if (m_focusCheckEnabled && !isWindowFocused()) {
        enforceFocus();
        needFix = true;
        fixDescription += "焦点 ";
    }
    
    // 检查置顶状态
    if (m_alwaysOnTopEnabled && !isWindowOnTop()) {
        enforceAlwaysOnTop();
        needFix = true;
        fixDescription += "置顶 ";
    }
    
    if (needFix) {
        m_fixCount++;
        logWindowEvent("窗口状态修复", fixDescription.trimmed());
        emit windowStateViolation(QString("窗口状态异常已修复: %1").arg(fixDescription.trimmed()));
    }
}

void WindowManager::setMonitoringEnabled(bool enabled)
{
    m_monitoringEnabled = enabled;
    m_logger->appEvent(QString("窗口监控: %1").arg(enabled ? "启用" : "禁用"));
    
    if (m_windowCheckTimer) {
        if (enabled) {
            m_windowCheckTimer->start();
        } else {
            m_windowCheckTimer->stop();
        }
    }
}

bool WindowManager::isMonitoringEnabled() const
{
    return m_monitoringEnabled;
}

void WindowManager::setFullscreenCheckEnabled(bool enabled)
{
    m_fullscreenCheckEnabled = enabled;
    m_logger->appEvent(QString("全屏检查: %1").arg(enabled ? "启用" : "禁用"));
}

bool WindowManager::isFullscreenCheckEnabled() const
{
    return m_fullscreenCheckEnabled;
}

void WindowManager::setFocusCheckEnabled(bool enabled)
{
    m_focusCheckEnabled = enabled;
    m_logger->appEvent(QString("焦点检查: %1").arg(enabled ? "启用" : "禁用"));
}

bool WindowManager::isFocusCheckEnabled() const
{
    return m_focusCheckEnabled;
}

QString WindowManager::getWindowStateInfo() const
{
    if (!m_targetWindow) return "窗口未设置";
    
    QString info;
    info += QString("全屏: %1, ").arg(isWindowFullscreen() ? "是" : "否");
    info += QString("焦点: %1, ").arg(isWindowFocused() ? "是" : "否");
    info += QString("置顶: %1, ").arg(isWindowOnTop() ? "是" : "否");
    info += QString("几何: %1x%2+%3+%4")
        .arg(m_targetWindow->width())
        .arg(m_targetWindow->height())
        .arg(m_targetWindow->x())
        .arg(m_targetWindow->y());
    
    return info;
}

int WindowManager::getFixCount() const
{
    return m_fixCount;
}

void WindowManager::resetStatistics()
{
    m_totalChecks = 0;
    m_fixCount = 0;
    m_fullscreenFixes = 0;
    m_focusFixes = 0;
    m_geometryFixes = 0;
    m_logger->appEvent("窗口管理统计已重置");
}

void WindowManager::performWindowCheck()
{
    checkAndFixWindowState();
}

void WindowManager::handleScreenChanged()
{
    m_logger->appEvent("屏幕配置变化，重新调整窗口");
    setupWindowGeometry();
    enforceFullscreen();
}

void WindowManager::initializeMonitoring()
{
    m_windowCheckTimer = new QTimer(this);
    connect(m_windowCheckTimer, &QTimer::timeout, this, &WindowManager::performWindowCheck);
    
    if (m_monitoringEnabled) {
        m_windowCheckTimer->start(m_checkInterval);
    }
    
    m_logger->appEvent(QString("窗口监控定时器启动，间隔: %1ms").arg(m_checkInterval));
}

void WindowManager::setupWindowFlags()
{
    if (!m_targetWindow) return;
    
    // 设置窗口标志（与原项目一致）
    Qt::WindowFlags flags = Qt::Window | Qt::FramelessWindowHint;
    
    if (m_alwaysOnTopEnabled) {
        flags |= Qt::WindowStaysOnTopHint;
    }
    
    m_targetWindow->setWindowFlags(flags);
    
    m_logger->appEvent("窗口标志设置完成");
}

void WindowManager::setupWindowGeometry()
{
    if (!m_targetWindow) return;
    
    // 获取主屏幕
    QScreen* screen = QApplication::primaryScreen();
    if (screen) {
        m_currentScreen = screen;
        m_expectedGeometry = screen->geometry();
        
        // 设置窗口几何形状为全屏
        m_targetWindow->setGeometry(m_expectedGeometry);
        m_targetWindow->setMinimumSize(1280, 800); // 最小尺寸保护
        
        m_logger->appEvent(QString("窗口几何设置: %1x%2+%3+%4")
            .arg(m_expectedGeometry.width())
            .arg(m_expectedGeometry.height())
            .arg(m_expectedGeometry.x())
            .arg(m_expectedGeometry.y()));
    }
}

void WindowManager::connectScreenSignals()
{
    // 连接屏幕变化信号
    QApplication* app = qobject_cast<QApplication*>(QApplication::instance());
    if (app) {
        connect(app, &QApplication::screenAdded, this, &WindowManager::handleScreenChanged);
        connect(app, &QApplication::screenRemoved, this, &WindowManager::handleScreenChanged);
        connect(app, &QApplication::primaryScreenChanged, this, &WindowManager::handleScreenChanged);
    }
    
    if (m_currentScreen) {
        connect(m_currentScreen, &QScreen::geometryChanged, this, &WindowManager::handleScreenChanged);
        connect(m_currentScreen, &QScreen::availableGeometryChanged, this, &WindowManager::handleScreenChanged);
    }
}

bool WindowManager::isWindowInCorrectState() const
{
    return isWindowFullscreen() && isWindowFocused() && isWindowOnTop();
}

bool WindowManager::isWindowFullscreen() const
{
    if (!m_targetWindow) return false;
    
    return (m_targetWindow->windowState() & Qt::WindowFullScreen) && 
           m_targetWindow->isFullScreen();
}

bool WindowManager::isWindowFocused() const
{
    if (!m_targetWindow) return false;
    
    return m_targetWindow->isActiveWindow() && 
           m_targetWindow->hasFocus() &&
           QApplication::activeWindow() == m_targetWindow;
}

bool WindowManager::isWindowOnTop() const
{
    if (!m_targetWindow) return false;
    
    Qt::WindowFlags flags = m_targetWindow->windowFlags();
    return flags & Qt::WindowStaysOnTopHint;
}

void WindowManager::fixWindowFlags()
{
    setupWindowFlags();
    m_targetWindow->show();
}

void WindowManager::fixWindowGeometry()
{
    setupWindowGeometry();
    m_geometryFixes++;
}

void WindowManager::fixWindowFocus()
{
    enforceFocus();
}

void WindowManager::logWindowEvent(const QString& event, const QString& details)
{
    m_logger->logEvent(event, details, "window.log", L_DEBUG);
}