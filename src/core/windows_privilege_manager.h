#ifndef WINDOWS_PRIVILEGE_MANAGER_H
#define WINDOWS_PRIVILEGE_MANAGER_H

#include <QString>
#include <QObject>

class Logger;
class ConfigManager;

/**
 * @brief Windows权限管理器
 * 
 * 负责Windows系统下的权限检查、VC++运行时管理等功能
 * 将这些功能从Application类中抽象出来，形成独立的组件
 */
class WindowsPrivilegeManager : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief 管理器结果枚举
     */
    enum class Result {
        Success,        // 操作成功
        Failed,         // 操作失败
        NotRequired,    // 不需要执行操作
        NotSupported    // 不支持的操作
    };

    /**
     * @brief VC++运行时安装结果
     */
    struct VCRuntimeResult {
        Result result;
        QString message;
        int exitCode = 0;
        
        VCRuntimeResult(Result r, const QString& msg = QString(), int code = 0)
            : result(r), message(msg), exitCode(code) {}
    };

    explicit WindowsPrivilegeManager(QObject* parent = nullptr);
    ~WindowsPrivilegeManager() = default;

    /**
     * @brief 设置日志记录器
     */
    void setLogger(Logger* logger);

    /**
     * @brief 设置配置管理器
     */
    void setConfigManager(ConfigManager* configManager);

    /**
     * @brief 检查是否以管理员权限运行
     * @return true如果具有管理员权限
     */
    static bool isRunningAsAdministrator();

    /**
     * @brief 检查VC++运行时是否已安装
     * @return true如果已安装必要的VC++运行时
     */
    bool isVCRuntimeInstalled() const;

    /**
     * @brief 获取VC++运行时安装包路径
     * @return 安装包的完整路径，如果未找到返回空字符串
     */
    QString getVCRuntimeInstallerPath() const;

    /**
     * @brief 安装VC++运行时
     * @param showPrompt 是否显示用户确认对话框
     * @return 安装结果
     */
    VCRuntimeResult installVCRuntime(bool showPrompt = true);

    /**
     * @brief 检查并处理VC++运行时依赖
     * @return 处理结果
     */
    Result checkAndHandleVCRuntime();

    /**
     * @brief 请求管理员权限（重启应用程序）
     * @return true如果成功请求到权限或已经具有权限
     */
    static bool requestAdministratorPrivileges();

    /**
     * @brief 检查Windows API可用性
     * @return true如果关键API可用
     */
    static bool checkWindowsAPIAvailability();

    /**
     * @brief 获取Windows版本信息
     * @return 版本信息字符串
     */
    static QString getWindowsVersionInfo();

    /**
     * @brief 检查是否为Windows 7系统
     * @return true如果是Windows 7 SP1或更高版本
     */
    static bool isWindows7OrLater();

signals:
    /**
     * @brief 权限状态变化信号
     */
    void privilegeStatusChanged(bool hasAdminRights);

    /**
     * @brief VC++运行时状态变化信号
     */
    void vcRuntimeStatusChanged(bool isInstalled);

    /**
     * @brief 操作进度信号
     */
    void operationProgress(const QString& operation, int percentage);

private:
    /**
     * @brief 检查注册表中的VC++运行时安装情况
     * @param registryPath 注册表路径
     * @return true如果在该路径下找到已安装的运行时
     */
    bool checkVCRuntimeInRegistry(const QString& registryPath) const;

    /**
     * @brief 检查系统目录中的VC++运行时DLL文件
     * @return true如果找到必要的DLL文件
     */
    bool checkVCRuntimeDLLs() const;

    /**
     * @brief 执行VC++运行时安装
     * @param installerPath 安装包路径
     * @return 安装结果
     */
    VCRuntimeResult executeVCRuntimeInstallation(const QString& installerPath);

    /**
     * @brief 显示VC++运行时提示对话框
     * @return 用户选择结果
     */
    bool showVCRuntimePrompt();

private:
    Logger* m_logger;
    ConfigManager* m_configManager;
    bool m_vcRuntimeChecked;
    bool m_vcRuntimeInstalled;
};

#endif // WINDOWS_PRIVILEGE_MANAGER_H