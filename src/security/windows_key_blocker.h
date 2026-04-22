#ifndef WINDOWS_KEY_BLOCKER_H
#define WINDOWS_KEY_BLOCKER_H

#include <QObject>
#include <QString>
#include <QTimer>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

class Logger;

/**
 * @brief Windows键拦截器
 *
 * 使用Windows低级键盘钩子（WH_KEYBOARD_LL）在系统级别拦截Win键
 * 防止学生通过Win键退出考试环境
 */
class WindowsKeyBlocker : public QObject
{
    Q_OBJECT

public:
    explicit WindowsKeyBlocker(QObject *parent = nullptr);
    ~WindowsKeyBlocker();

    /**
     * @brief 安装键盘钩子
     * @return 立即安装成功返回true，失败返回false
     *
     * @note 返回值语义说明：
     * - 返回true：钩子已成功安装并生效
     * - 返回false：初始安装失败，但已启动后台自动重试机制
     *
     * @details 后台重试机制包括三层恢复策略：
     * 1. 快速重试：失败后按1秒间隔重试，最多3次
     * 2. 定时重试：每30秒检查钩子状态，未安装则重试
     * 3. 定时重装：每30秒重装钩子，降低静默失效风险
     *
     * @warning 即使返回false，调用者也应该继续执行，不应中断程序启动
     *          这是"软失败"设计的核心理念，确保程序在安全控制暂时不可用时
     *          仍能正常启动，后台恢复机制会持续尝试恢复钩子功能
     *
     * @see retryInstall() 快速重试机制
     * @see refreshHook() 定时重试和定时重装机制
     */
    bool install();

    /**
     * @brief 卸载键盘钩子
     */
    void uninstall();

    /**
     * @brief 检查钩子是否已安装
     * @return 已安装返回true，否则返回false
     */
    bool isInstalled() const;

    /**
     * @brief 检查是否仍处于自动恢复过程中
     * @return 正在恢复返回true，否则返回false
     */
    bool isRecoveryPending() const;

    /**
     * @brief 检查快速重试是否已耗尽
     * @return 已耗尽返回true，否则返回false
     */
    bool hasFastRetryExhausted() const;

    /**
     * @brief 获取最近一次安装失败的错误码
     * @return Windows错误码，0表示当前没有失败记录
     */
    unsigned long lastErrorCode() const;

private:
#ifdef Q_OS_WIN
    // 钩子安装重试配置
    static const int kMaxInstallRetryCount = 3;        // 快速重试上限：3次足以覆盖大多数瞬时失败
    static const int kInstallRetryIntervalMs = 1000;   // 快速重试间隔：1秒，兼顾恢复速度与系统负担
    static const int kHookRefreshIntervalMs = 30000;   // 定时重装周期：30秒，平衡可靠性与性能开销

    /**
     * @brief 执行一次实际钩子安装
     * @param context 日志上下文
     * @return 成功返回true，失败返回false
     */
    bool installHookInternal(const QString& context);

    /**
     * @brief 在保留旧钩子的前提下尝试替换钩子
     * @param context 日志上下文
     * @return 成功返回true，失败返回false
     */
    bool replaceHookInternal(const QString& context);

    /**
     * @brief 启动自动恢复定时器
     */
    void startAutoRecovery();

    /**
     * @brief 停止自动恢复定时器
     */
    void stopAutoRecovery();

    /**
     * @brief 安排下一次快速重试
     * @param reason 本次重试的日志原因
     */
    void scheduleRetry(const QString& reason);

    /**
     * @brief 当钩子缺失时执行后台恢复
     */
    void recoverMissingHook();

    /**
     * @brief 当钩子已安装时执行定时重装
     */
    void refreshInstalledHook();

private slots:
    /**
     * @brief 定时重试安装钩子
     */
    void retryInstall();

    /**
     * @brief 定时执行钩子重装，降低钩子静默失效风险
     */
    void refreshHook();

private:
    /**
     * @brief 低级键盘钩子回调函数
     * @param nCode 钩子代码
     * @param wParam 消息类型（WM_KEYDOWN等）
     * @param lParam 指向KBDLLHOOKSTRUCT结构的指针
     * @return 返回1拦截按键，返回CallNextHookEx传递给下一个钩子
     */
    static LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam);

    static WindowsKeyBlocker* s_instance;  // 静态实例指针，用于回调函数访问成员

    HHOOK m_hookHandle;      // 钩子句柄
    Logger* m_logger;        // 日志记录器
    int m_blockedCount;      // 拦截计数
    int m_installRetryCount; // 安装重试次数
    bool m_autoRecoveryEnabled; // 是否允许自动恢复
    bool m_fastRetryExhausted; // 是否已耗尽快速重试并转入后台恢复
    bool m_refreshRetryPending; // 是否存在待处理的定时重装快速重试
    bool m_leftCtrlPressed;  // 左Ctrl当前是否处于按下状态
    bool m_rightCtrlPressed; // 右Ctrl当前是否处于按下状态
    DWORD m_lastErrorCode;   // 最近一次安装失败错误码
    QTimer* m_retryTimer;    // 安装失败后的快速重试定时器
    QTimer* m_refreshTimer;  // 定时恢复与定时重装共用定时器
#endif
};

#endif // WINDOWS_KEY_BLOCKER_H
