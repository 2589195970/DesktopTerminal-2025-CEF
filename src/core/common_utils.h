#ifndef COMMON_UTILS_H
#define COMMON_UTILS_H

#include <QString>
#include <functional>

class Logger;
class ConfigManager;
class QWidget;

/**
 * @brief 通用工具函数集合
 * 
 * 提供整个应用程序中常用的功能，减少代码重复
 */
namespace CommonUtils {

    /**
     * @brief 获取Logger和ConfigManager实例的便捷结构
     */
    struct Managers {
        Logger* logger;
        ConfigManager* configManager;
        
        Managers();
    };

    /**
     * @brief 获取管理器实例
     * @return 包含Logger和ConfigManager指针的结构
     */
    Managers getManagers();

    /**
     * @brief 记录错误并返回false的便捷函数
     * @param logger 日志记录器
     * @param message 错误消息
     * @return 始终返回false
     */
    bool logErrorAndReturnFalse(Logger* logger, const QString& message);

    /**
     * @brief 显示错误对话框并记录日志
     * @param parent 父窗口
     * @param title 对话框标题
     * @param message 错误消息
     * @param logger 日志记录器
     */
    void showErrorDialog(QWidget* parent, const QString& title, const QString& message, Logger* logger = nullptr);

    /**
     * @brief 安全执行操作，捕获异常并记录
     * @param operation 要执行的操作
     * @param errorMessage 失败时的错误消息
     * @param logger 日志记录器
     * @return 操作是否成功
     */
    bool safeExecute(const std::function<bool()>& operation, const QString& errorMessage, Logger* logger = nullptr);

    /**
     * @brief 安全执行void操作，捕获异常并记录
     * @param operation 要执行的操作
     * @param errorMessage 失败时的错误消息
     * @param logger 日志记录器
     * @return 操作是否成功
     */
    bool safeExecuteVoid(const std::function<void()>& operation, const QString& errorMessage, Logger* logger = nullptr);

    /**
     * @brief 检查初始化条件并记录结果
     * @param condition 检查条件
     * @param successMessage 成功消息
     * @param errorMessage 失败消息
     * @param logger 日志记录器
     * @return 条件检查结果
     */
    bool checkInitCondition(bool condition, const QString& successMessage, const QString& errorMessage, Logger* logger = nullptr);

    /**
     * @brief 格式化错误消息的便捷函数
     * @param operation 操作名称
     * @param details 详细信息
     * @return 格式化的错误消息
     */
    QString formatError(const QString& operation, const QString& details = QString());

    /**
     * @brief 格式化成功消息的便捷函数
     * @param operation 操作名称
     * @param details 详细信息
     * @return 格式化的成功消息
     */
    QString formatSuccess(const QString& operation, const QString& details = QString());

    /**
     * @brief 创建带有标准管理器引用的类的便捷宏
     */
    #define DECLARE_STANDARD_MANAGERS() \
        Logger* m_logger; \
        ConfigManager* m_configManager;

    #define INITIALIZE_STANDARD_MANAGERS() \
        CommonUtils::Managers managers = CommonUtils::getManagers(); \
        m_logger = managers.logger; \
        m_configManager = managers.configManager;

} // namespace CommonUtils

#endif // COMMON_UTILS_H