#ifndef COMPATIBILITY_MANAGER_H
#define COMPATIBILITY_MANAGER_H

#include "system_detector.h"
#include <QApplication>

class Logger;

/**
 * @brief 兼容性管理器类
 * 
 * 专门负责根据系统信息应用兼容性设置
 * 从Application类中分离出来以遵循单一职责原则
 */
class CompatibilityManager
{
public:
    explicit CompatibilityManager(QApplication* app);
    ~CompatibilityManager() = default;

    /**
     * @brief 应用兼容性设置
     * @param systemInfo 系统信息
     * @param logger 日志器
     */
    void applyCompatibilitySettings(const SystemDetector::SystemInfo& systemInfo, Logger* logger);

    /**
     * @brief 检查并应用Windows特定优化
     * @param systemInfo 系统信息
     * @param logger 日志器
     */
    void applyWindowsOptimizations(const SystemDetector::SystemInfo& systemInfo, Logger* logger);

    /**
     * @brief 应用32位系统优化
     * @param logger 日志器
     */
    void apply32BitOptimizations(Logger* logger);

    /**
     * @brief 应用Windows 7特定优化
     * @param logger 日志器
     */
    void applyWindows7Optimizations(Logger* logger);

    /**
     * @brief 获取建议的CEF启动参数
     * @param systemInfo 系统信息
     * @return CEF启动参数列表
     */
    QStringList getCEFStartupArgs(const SystemDetector::SystemInfo& systemInfo);

private:
    QApplication* m_application;
    
    void setOpenGLSettings(bool useSoftware, Logger* logger);
    void setRenderingSettings(const SystemDetector::SystemInfo& systemInfo, Logger* logger);
    void setMemoryOptimizations(const SystemDetector::SystemInfo& systemInfo, Logger* logger);
};

#endif // COMPATIBILITY_MANAGER_H