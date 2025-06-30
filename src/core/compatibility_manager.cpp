#include "compatibility_manager.h"
#include "../logging/logger.h"
#include <QApplication>

CompatibilityManager::CompatibilityManager(QApplication* app)
    : m_application(app)
{
}

void CompatibilityManager::applyCompatibilitySettings(const SystemDetector::SystemInfo& systemInfo, Logger* logger)
{
    if (!logger) {
        return;
    }

    if (systemInfo.compatibility == SystemDetector::CompatibilityLevel::LegacySystem) {
        logger->appEvent("应用传统系统兼容性设置");

        // Windows特定优化
        if (systemInfo.platform == SystemDetector::PlatformType::Windows) {
            applyWindowsOptimizations(systemInfo, logger);
        }

        // 32位系统内存优化
        if (systemInfo.architecture == SystemDetector::ArchType::X86_32) {
            apply32BitOptimizations(logger);
        }
    }
    
    // 设置渲染相关配置
    setRenderingSettings(systemInfo, logger);
    
    // 设置内存优化
    setMemoryOptimizations(systemInfo, logger);
}

void CompatibilityManager::applyWindowsOptimizations(const SystemDetector::SystemInfo& systemInfo, Logger* logger)
{
    if (systemInfo.platform != SystemDetector::PlatformType::Windows) {
        return;
    }

    logger->appEvent("应用Windows系统优化设置");

    // 禁用一些可能导致问题的特性
    m_application->setAttribute(Qt::AA_DisableWindowContextHelpButton, true);
    m_application->setAttribute(Qt::AA_DontCreateNativeWidgetSiblings, true);

    // Windows 7特定优化
    if (systemInfo.compatibility == SystemDetector::CompatibilityLevel::LegacySystem) {
        applyWindows7Optimizations(logger);
    }
}

void CompatibilityManager::apply32BitOptimizations(Logger* logger)
{
    logger->appEvent("应用32位系统内存优化设置");
    
    // 强制使用软件渲染以避免内存问题
    setOpenGLSettings(true, logger);
    
    // 设置内存限制相关的属性
    m_application->setAttribute(Qt::AA_UseDesktopOpenGL, false);
    m_application->setAttribute(Qt::AA_UseSoftwareOpenGL, true);
}

void CompatibilityManager::applyWindows7Optimizations(Logger* logger)
{
    logger->appEvent("应用Windows 7优化设置");

    // 强制软件渲染以避免GPU兼容性问题
    setOpenGLSettings(true, logger);
    
    // 设置环境变量
    qputenv("QT_OPENGL", "software");
    qputenv("QT_ANGLE_PLATFORM", "d3d9");
    
    logger->appEvent("已设置Windows 7兼容的OpenGL环境变量");
}

QStringList CompatibilityManager::getCEFStartupArgs(const SystemDetector::SystemInfo& systemInfo)
{
    QStringList args;
    
    // 基础启动参数
    args << "--disable-web-security";
    args << "--disable-features=VizDisplayCompositor";
    
    // 根据系统类型添加特定参数
    if (systemInfo.compatibility == SystemDetector::CompatibilityLevel::LegacySystem) {
        // 传统系统优化
        args << "--disable-gpu";
        args << "--disable-gpu-compositing";
        args << "--disable-software-rasterizer";
        args << "--disable-background-timer-throttling";
        args << "--disable-renderer-backgrounding";
        args << "--disable-backgrounding-occluded-windows";
    }
    
    if (systemInfo.architecture == SystemDetector::ArchType::X86_32) {
        // 32位系统内存优化
        args << "--memory-pressure-off";
        args << "--max_old_space_size=256";
        args << "--aggressive-cache-discard";
    }
    
    if (systemInfo.platform == SystemDetector::PlatformType::Windows) {
        // Windows特定参数
        args << "--disable-d3d11";
        args << "--disable-accelerated-2d-canvas";
        
        if (systemInfo.compatibility == SystemDetector::CompatibilityLevel::LegacySystem) {
            // Windows 7特定参数
            args << "--disable-direct-composition";
            args << "--disable-gpu-sandbox";
        }
    }
    
    return args;
}

void CompatibilityManager::setOpenGLSettings(bool useSoftware, Logger* logger)
{
    if (useSoftware) {
        m_application->setAttribute(Qt::AA_UseDesktopOpenGL, false);
        m_application->setAttribute(Qt::AA_UseSoftwareOpenGL, true);
        logger->appEvent("已启用软件OpenGL渲染");
    } else {
        m_application->setAttribute(Qt::AA_UseDesktopOpenGL, true);
        m_application->setAttribute(Qt::AA_UseSoftwareOpenGL, false);
        logger->appEvent("已启用硬件OpenGL渲染");
    }
}

void CompatibilityManager::setRenderingSettings(const SystemDetector::SystemInfo& systemInfo, Logger* logger)
{
    // 根据系统能力设置渲染模式
    if (systemInfo.compatibility == SystemDetector::CompatibilityLevel::LegacySystem ||
        systemInfo.architecture == SystemDetector::ArchType::X86_32) {
        // 传统系统或32位系统使用软件渲染
        setOpenGLSettings(true, logger);
    } else {
        // 现代系统可以尝试硬件渲染
        setOpenGLSettings(false, logger);
    }
}

void CompatibilityManager::setMemoryOptimizations(const SystemDetector::SystemInfo& systemInfo, Logger* logger)
{
    if (systemInfo.architecture == SystemDetector::ArchType::X86_32) {
        // 32位系统内存优化
        m_application->setAttribute(Qt::AA_DisableShaderDiskCache, true);
        logger->appEvent("已禁用着色器磁盘缓存以节约内存");
    }
    
    if (systemInfo.compatibility == SystemDetector::CompatibilityLevel::LegacySystem) {
        // 传统系统内存优化
        m_application->setAttribute(Qt::AA_ShareOpenGLContexts, false);
        logger->appEvent("已禁用OpenGL上下文共享以提高稳定性");
    }
}