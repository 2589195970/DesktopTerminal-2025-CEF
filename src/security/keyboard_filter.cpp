#include "keyboard_filter.h"
#include "../logging/logger.h"
#include "../config/config_manager.h"

#include <QKeySequence>
#include <QApplication>

KeyboardFilter::KeyboardFilter(QObject *parent)
    : QObject(parent)
    , m_logger(&Logger::instance())
    , m_configManager(&ConfigManager::instance())
    , m_filterEnabled(true)
    , m_strictMode(true)
    , m_developerModeEnabled(false)
    , m_totalKeyEvents(0)
    , m_filteredKeyEvents(0)
    , m_statisticsTimer(nullptr)
{
    m_logger->appEvent("KeyboardFilter创建");
}

KeyboardFilter::~KeyboardFilter()
{
    if (m_statisticsTimer) {
        m_statisticsTimer->stop();
        delete m_statisticsTimer;
    }
    m_logger->appEvent("KeyboardFilter销毁");
}

bool KeyboardFilter::initialize()
{
    m_logger->appEvent("KeyboardFilter初始化开始");
    
    // 从配置加载设置
    m_filterEnabled = m_configManager->isKeyboardFilterEnabled();
    m_strictMode = m_configManager->isStrictSecurityMode();
    
    // 初始化过滤规则
    initializeFilterRules();
    
    // 初始化统计定时器
    initializeStatisticsTimer();
    
    m_logger->appEvent("KeyboardFilter初始化完成");
    return true;
}

bool KeyboardFilter::shouldFilterKeyEvent(QKeyEvent* event)
{
    if (!m_filterEnabled || !event) {
        return false;
    }
    
    m_totalKeyEvents++;
    
    int key = event->key();
    Qt::KeyboardModifiers modifiers = event->modifiers();
    
    // 检查是否为安全退出热键
    if (isSecurityExitHotkey(event)) {
        emit securityExitRequested();
        return false; // 不过滤安全退出热键
    }
    
    // 检查是否为允许的功能键
    if (isAllowedFunctionKey(event)) {
        return false;
    }
    
    // 检查危险按键组合
    if (isDangerousKeyCombo(key, modifiers)) {
        m_filteredKeyEvents++;
        QString keyDesc = getKeyDescription(event);
        logFilterEvent(QString("过滤危险按键: %1").arg(keyDesc), true);
        emit dangerousKeyDetected(keyDesc);
        return true;
    }
    
    // 检查系统按键组合
    if (isSystemKeyCombo(key, modifiers)) {
        m_filteredKeyEvents++;
        QString keyDesc = getKeyDescription(event);
        logFilterEvent(QString("过滤系统按键: %1").arg(keyDesc), true);
        return true;
    }
    
    // 检查调试按键组合（在开发者模式下允许部分调试按键）
    if (isDebugKeyCombo(key, modifiers)) {
        // 开发者模式下允许F12和部分调试按键
        if (m_developerModeEnabled && (key == Qt::Key_F12 || 
                                       (key == Qt::Key_I && modifiers == (Qt::ControlModifier | Qt::ShiftModifier)) ||
                                       (key == Qt::Key_J && modifiers == (Qt::ControlModifier | Qt::ShiftModifier)) ||
                                       (key == Qt::Key_C && modifiers == (Qt::ControlModifier | Qt::ShiftModifier)))) {
            logFilterEvent(QString("开发者模式允许调试按键: %1").arg(getKeyDescription(event)), false);
            return false;
        }
        
        m_filteredKeyEvents++;
        QString keyDesc = getKeyDescription(event);
        logFilterEvent(QString("过滤调试按键: %1").arg(keyDesc), true);
        return true;
    }
    
    return false;
}

bool KeyboardFilter::isSecurityExitHotkey(QKeyEvent* event)
{
    int key = event->key();
    Qt::KeyboardModifiers modifiers = event->modifiers();
    
    // F10 单独按下
    if (key == Qt::Key_F10 && modifiers == Qt::NoModifier) {
        return true;
    }
    
    return false;
}

bool KeyboardFilter::isAllowedFunctionKey(QKeyEvent* event)
{
    int key = event->key();
    Qt::KeyboardModifiers modifiers = event->modifiers();
    
    // 允许 Ctrl+R 刷新
    if (key == Qt::Key_R && modifiers == Qt::ControlModifier) {
        return true;
    }
    
    // 允许单纯的 Shift 修饰键（与原项目逻辑一致）
    if (key >= Qt::Key_Shift && key <= Qt::Key_ScrollLock && modifiers == Qt::ShiftModifier) {
        return true;
    }
    
    // 允许基本的导航键
    if (modifiers == Qt::NoModifier) {
        switch (key) {
            case Qt::Key_Tab:
            case Qt::Key_Return:
            case Qt::Key_Enter:
            case Qt::Key_Space:
            case Qt::Key_Backspace:
            case Qt::Key_Delete:
            case Qt::Key_Left:
            case Qt::Key_Right:
            case Qt::Key_Up:
            case Qt::Key_Down:
            case Qt::Key_Home:
            case Qt::Key_End:
            case Qt::Key_PageUp:
            case Qt::Key_PageDown:
                return true;
        }
    }
    
    return false;
}

void KeyboardFilter::setFilterEnabled(bool enabled)
{
    m_filterEnabled = enabled;
    m_logger->appEvent(QString("键盘过滤: %1").arg(enabled ? "启用" : "禁用"));
}

bool KeyboardFilter::isFilterEnabled() const
{
    return m_filterEnabled;
}

int KeyboardFilter::getFilteredKeyCount() const
{
    return m_filteredKeyEvents;
}

void KeyboardFilter::resetStatistics()
{
    m_totalKeyEvents = 0;
    m_filteredKeyEvents = 0;
    m_logger->appEvent("键盘过滤统计已重置");
}

void KeyboardFilter::logKeyEvent(QKeyEvent* event, bool filtered)
{
    if (m_logger->getLogLevel() <= L_DEBUG) {
        QString keyDesc = getKeyDescription(event);
        QString status = filtered ? "过滤" : "允许";
        m_logger->logEvent("键盘过滤", QString("%1: %2").arg(status).arg(keyDesc), "keyboard.log", L_DEBUG);
    }
}

void KeyboardFilter::updateStatistics()
{
    if (m_totalKeyEvents > 0) {
        double filterRate = (double)m_filteredKeyEvents / m_totalKeyEvents * 100.0;
        m_logger->logEvent("键盘统计", 
            QString("总按键: %1, 过滤: %2, 过滤率: %3%")
                .arg(m_totalKeyEvents)
                .arg(m_filteredKeyEvents)
                .arg(filterRate, 0, 'f', 1),
            "keyboard.log", L_DEBUG);
    }
}

void KeyboardFilter::initializeFilterRules()
{
    // 危险按键组合（完全禁止）
    m_dangerousKeyCombos.clear();
    
    // Alt + 危险键
    m_dangerousKeyCombos << "Alt+Tab"      // 切换应用程序
                         << "Alt+F4"       // 关闭应用程序
                         << "Alt+Esc"      // 切换应用程序
                         << "Alt+Space";   // 系统菜单
    
    // Ctrl + Alt + 危险键
    m_dangerousKeyCombos << "Ctrl+Alt+Del"    // 任务管理器
                         << "Ctrl+Alt+Esc"    // 任务管理器
                         << "Ctrl+Alt+F4";    // 虚拟终端
    
    // Ctrl + Shift + 危险键
    m_dangerousKeyCombos << "Ctrl+Shift+Esc"  // 任务管理器
                         << "Ctrl+Shift+Del"  // 高级启动
                         << "Ctrl+Shift+N";   // 新建私密窗口
    
    // Windows 键组合
    m_dangerousKeyCombos << "Meta+L"      // 锁定屏幕
                         << "Meta+D"      // 显示桌面
                         << "Meta+M"      // 最小化所有窗口
                         << "Meta+R"      // 运行对话框
                         << "Meta+X"      // 高级用户菜单
                         << "Meta+Tab";   // 应用程序切换
    
    // 系统按键组合
    m_systemKeyCombos.clear();
    m_systemKeyCombos << "F1"           // 帮助
                      << "F5"           // 刷新（部分情况）
                      << "F11"          // 全屏切换
                      << "F12"          // 开发者工具
                      << "Ctrl+F11"     // 全屏切换
                      << "Ctrl+F12"     // 开发者工具
                      << "Shift+F10";   // 右键菜单
    
    // 调试按键组合
    m_debugKeyCombos.clear();
    m_debugKeyCombos << "Ctrl+Shift+I"    // 开发者工具
                     << "Ctrl+Shift+J"    // 控制台
                     << "Ctrl+Shift+C"    // 元素检查器
                     << "Ctrl+U"          // 查看源代码
                     << "Ctrl+Shift+U"    // 查看源代码
                     << "F12";            // 开发者工具
    
    // 允许的按键组合
    m_allowedKeyCombos.clear();
    m_allowedKeyCombos << "Ctrl+R"        // 刷新页面
                       << "Ctrl+F5"       // 强制刷新
                       << "Shift+F5";     // 强制刷新
    
    // 安全退出热键
    m_exitHotkeys.clear();
    m_exitHotkeys << "F10";
    
    m_logger->appEvent(QString("键盘过滤规则初始化完成，危险组合: %1个").arg(m_dangerousKeyCombos.size()));
}

void KeyboardFilter::initializeStatisticsTimer()
{
    m_statisticsTimer = new QTimer(this);
    connect(m_statisticsTimer, &QTimer::timeout, this, &KeyboardFilter::updateStatistics);
    m_statisticsTimer->start(60000); // 每分钟更新一次统计
    
    m_logger->appEvent("键盘统计定时器启动");
}

bool KeyboardFilter::isDangerousKeyCombo(int key, Qt::KeyboardModifiers modifiers)
{
    QString combo = getKeyDescription(key, modifiers);
    return m_dangerousKeyCombos.contains(combo);
}

bool KeyboardFilter::isSystemKeyCombo(int key, Qt::KeyboardModifiers modifiers)
{
    QString combo = getKeyDescription(key, modifiers);
    return m_systemKeyCombos.contains(combo);
}

bool KeyboardFilter::isDebugKeyCombo(int key, Qt::KeyboardModifiers modifiers)
{
    QString combo = getKeyDescription(key, modifiers);
    return m_debugKeyCombos.contains(combo);
}

QString KeyboardFilter::getKeyDescription(QKeyEvent* event)
{
    return getKeyDescription(event->key(), event->modifiers());
}

QString KeyboardFilter::getKeyDescription(int key, Qt::KeyboardModifiers modifiers)
{
    QKeySequence sequence(key | modifiers);
    return sequence.toString();
}

void KeyboardFilter::setDeveloperModeEnabled(bool enabled)
{
    m_developerModeEnabled = enabled;
    m_logger->appEvent(QString("开发者模式键盘过滤: %1").arg(enabled ? "启用" : "禁用"));
}

bool KeyboardFilter::isDeveloperModeEnabled() const
{
    return m_developerModeEnabled;
}

void KeyboardFilter::logFilterEvent(const QString& description, bool filtered)
{
    if (m_logger->getLogLevel() <= L_DEBUG) {
        m_logger->logEvent("键盘过滤", description, "keyboard.log", L_DEBUG);
    }
}