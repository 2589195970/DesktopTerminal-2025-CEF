#include "windows_key_blocker.h"
#include "../logging/logger.h"

#ifdef Q_OS_WIN

// 静态成员初始化
WindowsKeyBlocker* WindowsKeyBlocker::s_instance = nullptr;

WindowsKeyBlocker::WindowsKeyBlocker(QObject *parent)
    : QObject(parent)
    , m_hookHandle(NULL)
    , m_logger(&Logger::instance())
    , m_blockedCount(0)
    , m_installRetryCount(0)
    , m_autoRecoveryEnabled(true)
    , m_fastRetryExhausted(false)
    , m_refreshRetryPending(false)
    , m_leftCtrlPressed(false)
    , m_rightCtrlPressed(false)
    , m_lastErrorCode(0)
    , m_retryTimer(new QTimer(this))
    , m_refreshTimer(new QTimer(this))
{
    m_retryTimer->setSingleShot(true);
    connect(m_retryTimer, &QTimer::timeout, this, &WindowsKeyBlocker::retryInstall);

    m_refreshTimer->setInterval(kHookRefreshIntervalMs);
    connect(m_refreshTimer, &QTimer::timeout, this, &WindowsKeyBlocker::refreshHook);
}

WindowsKeyBlocker::~WindowsKeyBlocker()
{
    m_autoRecoveryEnabled = false;
    stopAutoRecovery();
    uninstall();
}

bool WindowsKeyBlocker::install()
{
    if (m_hookHandle) {
        m_logger->appEvent("Windows键钩子已安装，跳过重复安装");
        return true;
    }

    m_autoRecoveryEnabled = true;
    m_fastRetryExhausted = false;
    m_refreshRetryPending = false;
    m_leftCtrlPressed = false;
    m_rightCtrlPressed = false;
    m_lastErrorCode = 0;
    startAutoRecovery();

    if (installHookInternal("初始安装")) {
        m_installRetryCount = 0;
        if (m_retryTimer->isActive()) {
            m_retryTimer->stop();
        }
        return true;
    }

    m_installRetryCount = 1;
    scheduleRetry("Windows键钩子初始安装失败");

    return false;
}

void WindowsKeyBlocker::uninstall()
{
    stopAutoRecovery();

    if (m_hookHandle) {
        if (UnhookWindowsHookEx(m_hookHandle)) {
            m_logger->appEvent(
                QString("Windows键钩子已卸载，共拦截 %1 次").arg(m_blockedCount)
            );
        } else {
            m_logger->errorEvent("Windows键钩子卸载失败");
        }
        m_hookHandle = NULL;
    }

    m_refreshRetryPending = false;
    m_fastRetryExhausted = false;
    m_installRetryCount = 0;
    m_leftCtrlPressed = false;
    m_rightCtrlPressed = false;
    s_instance = nullptr;
}

bool WindowsKeyBlocker::isInstalled() const
{
    return m_hookHandle != NULL;
}

bool WindowsKeyBlocker::isRecoveryPending() const
{
    return m_retryTimer && m_retryTimer->isActive();
}

bool WindowsKeyBlocker::hasFastRetryExhausted() const
{
    return m_fastRetryExhausted;
}

unsigned long WindowsKeyBlocker::lastErrorCode() const
{
    return static_cast<unsigned long>(m_lastErrorCode);
}

bool WindowsKeyBlocker::installHookInternal(const QString& context)
{
    s_instance = this;

    HHOOK hookHandle = SetWindowsHookEx(
        WH_KEYBOARD_LL,
        LowLevelKeyboardProc,
        GetModuleHandle(NULL),
        0
    );

    if (!hookHandle) {
        DWORD error = GetLastError();
        m_lastErrorCode = error;

        // 仅在当前没有活动钩子时清理静态实例，避免影响仍在生效的旧钩子。
        if (!m_hookHandle) {
            s_instance = nullptr;
        }

        m_logger->errorEvent(
            QString("Windows键钩子%1失败，错误码: %2").arg(context).arg(error)
        );
        return false;
    }

    m_hookHandle = hookHandle;
    m_fastRetryExhausted = false;
    m_lastErrorCode = 0;
    m_logger->appEvent(QString("Windows键钩子%1成功").arg(context));
    return true;
}

bool WindowsKeyBlocker::replaceHookInternal(const QString& context)
{
    HHOOK oldHookHandle = m_hookHandle;

    if (!installHookInternal(context)) {
        m_hookHandle = oldHookHandle;
        return false;
    }

    if (oldHookHandle && !UnhookWindowsHookEx(oldHookHandle)) {
        DWORD error = GetLastError();
        m_logger->errorEvent(QString("Windows键钩子旧钩子卸载失败，错误码: %1，新钩子已继续生效").arg(error));
    }

    return true;
}

void WindowsKeyBlocker::startAutoRecovery()
{
    if (!m_autoRecoveryEnabled) {
        return;
    }

    if (!m_refreshTimer->isActive()) {
        m_refreshTimer->start();
        m_logger->appEvent(QString("Windows键钩子后台恢复与定时重装已启用，周期: %1 ms")
            .arg(kHookRefreshIntervalMs));
    }
}

void WindowsKeyBlocker::stopAutoRecovery()
{
    if (m_retryTimer->isActive()) {
        m_retryTimer->stop();
    }

    if (m_refreshTimer->isActive()) {
        m_refreshTimer->stop();
    }
}

void WindowsKeyBlocker::scheduleRetry(const QString& reason)
{
    if (m_retryTimer->isActive()) {
        return;
    }

    m_logger->appEvent(QString("%1，将在 %2 ms 后进行第 %3 次重试")
        .arg(reason)
        .arg(kInstallRetryIntervalMs)
        .arg(m_installRetryCount));
    m_retryTimer->start(kInstallRetryIntervalMs);
}

void WindowsKeyBlocker::retryInstall()
{
    if (!m_autoRecoveryEnabled) {
        return;
    }

    const bool retryMissingHook = (m_hookHandle == NULL);
    const bool retryRefreshHook = (m_hookHandle != NULL) && m_refreshRetryPending;

    if (!retryMissingHook && !retryRefreshHook) {
        return;
    }

    const QString retryContext = retryRefreshHook
        ? QString("定时重装重试(%1/%2)").arg(m_installRetryCount).arg(kMaxInstallRetryCount)
        : QString("自动重试(%1/%2)").arg(m_installRetryCount).arg(kMaxInstallRetryCount);

    const bool success = retryRefreshHook
        ? replaceHookInternal(retryContext)
        : installHookInternal(retryContext);

    if (success) {
        m_installRetryCount = 0;
        m_refreshRetryPending = false;
        if (m_retryTimer->isActive()) {
            m_retryTimer->stop();
        }
        startAutoRecovery();
        return;
    }

    if (m_installRetryCount >= kMaxInstallRetryCount) {
        m_fastRetryExhausted = true;
        if (retryRefreshHook) {
            m_logger->errorEvent("Windows键钩子定时重装快速重试已达到上限，将保持现有钩子并等待下一轮定时重装");
        } else {
            m_logger->errorEvent("Windows键钩子启动快速重试已达到上限，将继续等待后台恢复");
        }
        m_refreshRetryPending = false;
        return;
    }

    ++m_installRetryCount;
    scheduleRetry(retryRefreshHook ? "Windows键钩子定时重装重试失败" : "Windows键钩子自动重试失败");
}

void WindowsKeyBlocker::recoverMissingHook()
{
    m_logger->appEvent("Windows键钩子执行后台恢复重试");

    if (installHookInternal("后台恢复")) {
        m_installRetryCount = 0;
        m_refreshRetryPending = false;
        m_logger->appEvent("Windows键钩子后台恢复成功");
    }
}

void WindowsKeyBlocker::refreshInstalledHook()
{
    m_logger->appEvent("Windows键钩子执行定时重装检查");

    if (replaceHookInternal("定时重装")) {
        m_installRetryCount = 0;
        m_refreshRetryPending = false;
        if (m_retryTimer->isActive()) {
            m_retryTimer->stop();
        }
        return;
    }

    m_refreshRetryPending = true;
    m_installRetryCount = 1;
    scheduleRetry("Windows键钩子定时重装失败");
}

void WindowsKeyBlocker::refreshHook()
{
    if (!m_autoRecoveryEnabled) {
        return;
    }

    if (!m_hookHandle) {
        recoverMissingHook();
        return;
    }

    refreshInstalledHook();
}

LRESULT CALLBACK WindowsKeyBlocker::LowLevelKeyboardProc(
    int nCode, WPARAM wParam, LPARAM lParam)
{
    if (nCode == HC_ACTION) {
        const KBDLLHOOKSTRUCT* pKeyboard = reinterpret_cast<const KBDLLHOOKSTRUCT*>(lParam);
        const bool isKeyDown = (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN);
        const bool isKeyUp = (wParam == WM_KEYUP || wParam == WM_SYSKEYUP);

        if (s_instance && (isKeyDown || isKeyUp)) {
            const bool isGenericCtrl = (pKeyboard->vkCode == VK_CONTROL);
            const bool isRightCtrl = (pKeyboard->vkCode == VK_RCONTROL)
                || (isGenericCtrl && ((pKeyboard->flags & LLKHF_EXTENDED) != 0));
            const bool isLeftCtrl = (pKeyboard->vkCode == VK_LCONTROL)
                || (isGenericCtrl && !isRightCtrl);

            if (isLeftCtrl) {
                s_instance->m_leftCtrlPressed = isKeyDown;
            }

            if (isRightCtrl) {
                s_instance->m_rightCtrlPressed = isKeyDown;
            }
        }

        const bool ctrlPressed = s_instance
            && (s_instance->m_leftCtrlPressed || s_instance->m_rightCtrlPressed);
        const bool ctrlEsc = (pKeyboard->vkCode == VK_ESCAPE) && ctrlPressed;

        // 拦截左右Win键（VK_LWIN=0x5B, VK_RWIN=0x5C）以及 Ctrl+Esc
        if (pKeyboard->vkCode == VK_LWIN || pKeyboard->vkCode == VK_RWIN || ctrlEsc) {
            if (s_instance && s_instance->m_logger) {
                if (isKeyDown) {
                    s_instance->m_blockedCount++;
                    s_instance->m_logger->logEvent(
                        "Windows键拦截",
                        QString("拦截系统按键 (VK: 0x%1, Ctrl+Esc: %2, 总计: %3)")
                            .arg(pKeyboard->vkCode, 0, 16)
                            .arg(ctrlEsc ? "是" : "否")
                            .arg(s_instance->m_blockedCount),
                        "keyboard.log",
                        L_DEBUG
                    );
                }
            }
            return 1; // 拦截按键，不传递给系统
        }
    }

    // 传递给下一个钩子
    return CallNextHookEx(NULL, nCode, wParam, lParam);
}

#endif // Q_OS_WIN
