#include "system_checker.h"
#include "../logging/logger.h"
#include "../config/config_manager.h"

#include <QApplication>
#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>
#include <QVersionNumber>
#include <QOpenGLContext>
#include <QOffscreenSurface>
#include <QOpenGLFunctions>
#include <QStorageInfo>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>
#include <QTimer>
#include <QThread>

#ifdef Q_OS_WIN
#include <windows.h>
#include <lmcons.h>
#endif

SystemChecker::SystemChecker(QObject *parent)
    : QObject(parent)
    , m_logger(&Logger::instance())
    , m_configManager(&ConfigManager::instance())
    , m_networkTimeout(nullptr)
    , m_networkInfo(nullptr)
    , m_currentCheck(0)
    , m_totalChecks(5)
    , m_checkInProgress(false)
{
    // 注册自定义类型用于信号槽
    qRegisterMetaType<SystemChecker::CheckResult>("SystemChecker::CheckResult");
    
    // 初始化网络信息
    m_networkInfo = QNetworkInformation::instance();
    
    m_logger->appEvent("SystemChecker初始化完成");
}

SystemChecker::~SystemChecker()
{
    if (m_networkTimeout) {
        m_networkTimeout->stop();
        m_networkTimeout->deleteLater();
    }
}

void SystemChecker::startSystemCheck()
{
    if (m_checkInProgress) {
        m_logger->appEvent("系统检测已在进行中，跳过重复请求");
        return;
    }

    m_checkInProgress = true;
    m_currentCheck = 0;
    m_results.clear();

    m_logger->appEvent("=== 开始全面系统检测 ===");

    // 按顺序执行各项检测
    QStringList checkNames = {
        "系统兼容性检测",
        "网络连接检测", 
        "CEF依赖检查",
        "配置权限验证",
        "组件预加载"
    };

    for (int i = 0; i < checkNames.size(); ++i) {
        m_currentCheck = i + 1;
        emit checkProgress(m_currentCheck, m_totalChecks, checkNames[i]);
        
        CheckResult result;
        switch (i) {
            case 0: result = checkSystemCompatibility(); break;
            case 1: result = checkNetworkConnection(); break;
            case 2: result = checkCEFDependencies(); break;
            case 3: result = checkConfigPermissions(); break;
            case 4: result = preloadComponents(); break;
        }
        
        m_results.append(result);
        emit checkItemCompleted(result);
        
        // 如果遇到致命错误，停止后续检测
        if (result.level == LEVEL_FATAL) {
            m_logger->errorEvent(QString("检测到致命错误: %1").arg(result.message));
            break;
        }
        
        // 给UI一些时间更新
        QThread::msleep(100);
    }

    bool success = !hasFatalErrors();
    m_checkInProgress = false;
    
    m_logger->appEvent(QString("系统检测完成，结果: %1").arg(success ? "成功" : "失败"));
    emit checkCompleted(success, m_results);
}

SystemChecker::CheckResult SystemChecker::checkSystemCompatibility()
{
    CheckResult result;
    result.type = CHECK_SYSTEM_COMPATIBILITY;
    result.title = "系统兼容性检测";
    
    QStringList issues;
    CheckLevel maxLevel = LEVEL_OK;

    // 检查Qt版本兼容性
    QString qtCompileVersion = QT_VERSION_STR;
    QString qtRuntimeVersion = qVersion();
    
    if (qtCompileVersion != qtRuntimeVersion) {
        issues << QString("Qt版本不匹配：编译版本%1，运行时版本%2").arg(qtCompileVersion).arg(qtRuntimeVersion);
        maxLevel = qMax(maxLevel, LEVEL_WARNING);
    }

    // 检查操作系统版本
#ifdef Q_OS_WIN
    QVersionNumber osVersion = QVersionNumber::fromString(QSysInfo::kernelVersion());
    if (osVersion < QVersionNumber(6, 1)) { // Windows 7 = 6.1
        issues << "操作系统版本过低，建议Windows 7 SP1或更高版本";
        maxLevel = qMax(maxLevel, LEVEL_ERROR);
    }
#endif

    // 检查内存资源
    qint64 availableMemory = 0;
#ifdef Q_OS_WIN
    MEMORYSTATUSEX statex;
    statex.dwLength = sizeof(statex);
    if (GlobalMemoryStatusEx(&statex)) {
        availableMemory = statex.ullAvailPhys / (1024 * 1024); // MB
    }
#endif

    if (availableMemory > 0 && availableMemory < 512) {
        issues << QString("可用内存不足：%1MB，建议至少512MB").arg(availableMemory);
        maxLevel = qMax(maxLevel, LEVEL_WARNING);
    }

    // 检查OpenGL支持
    bool openglSupported = false;
    QString openglVersion;
    
    QOpenGLContext context;
    QOffscreenSurface surface;
    surface.create();
    
    if (context.create()) {
        context.makeCurrent(&surface);
        openglVersion = reinterpret_cast<const char*>(context.functions()->glGetString(GL_VERSION));
        openglSupported = !openglVersion.isEmpty();
        context.doneCurrent();
    }

    if (!openglSupported) {
        issues << "OpenGL支持检测失败，可能影响界面渲染";
        maxLevel = qMax(maxLevel, LEVEL_WARNING);
        result.solution = "请更新显卡驱动程序或确保系统支持OpenGL 2.1+";
    }

    result.level = maxLevel;
    result.details = issues;
    
    if (maxLevel == LEVEL_OK) {
        result.message = "系统兼容性良好";
    } else {
        result.message = QString("发现%1个兼容性问题").arg(issues.size());
    }

    return result;
}

SystemChecker::CheckResult SystemChecker::checkNetworkConnection()
{
    CheckResult result;
    result.type = CHECK_NETWORK_CONNECTION;
    result.title = "网络连接检测";
    result.canRetry = true;
    
    QStringList issues;
    CheckLevel maxLevel = LEVEL_OK;

    // 检查网络接口
    QList<QNetworkInterface> interfaces = QNetworkInterface::allInterfaces();
    bool hasActiveInterface = false;
    
    for (const QNetworkInterface &interface : interfaces) {
        if (interface.flags() & QNetworkInterface::IsUp && 
            interface.flags() & QNetworkInterface::IsRunning &&
            !(interface.flags() & QNetworkInterface::IsLoopBack)) {
            hasActiveInterface = true;
            break;
        }
    }

    if (!hasActiveInterface) {
        issues << "未检测到活动的网络接口";
        maxLevel = LEVEL_WARNING;
    }

    // 检查网络连接状态
    if (m_networkInfo) {
        QNetworkInformation::Reachability reachability = m_networkInfo->reachability();
        switch (reachability) {
            case QNetworkInformation::Reachability::Unknown:
                issues << "网络连接状态未知";
                maxLevel = qMax(maxLevel, LEVEL_WARNING);
                break;
            case QNetworkInformation::Reachability::Disconnected:
                issues << "网络连接已断开";
                maxLevel = qMax(maxLevel, LEVEL_ERROR);
                break;
            case QNetworkInformation::Reachability::Local:
                issues << "仅本地网络连接，无法访问互联网";
                maxLevel = qMax(maxLevel, LEVEL_WARNING);
                break;
            case QNetworkInformation::Reachability::Site:
            case QNetworkInformation::Reachability::Online:
                // 网络连接正常
                break;
        }

        // 检查是否为计量连接
        if (m_networkInfo->isMetered()) {
            issues << "当前使用计量网络连接，可能影响数据传输";
            maxLevel = qMax(maxLevel, LEVEL_WARNING);
        }
    }

    result.level = maxLevel;
    result.details = issues;
    
    if (maxLevel == LEVEL_OK) {
        result.message = "网络连接正常";
    } else if (maxLevel == LEVEL_WARNING) {
        result.message = "网络连接存在一些问题，但可以继续使用";
        result.solution = "检查网络设置或切换到更稳定的网络连接";
    } else {
        result.message = "网络连接严重问题";
        result.solution = "请检查网络连接并重试";
    }

    return result;
}

SystemChecker::CheckResult SystemChecker::checkCEFDependencies()
{
    CheckResult result;
    result.type = CHECK_CEF_DEPENDENCIES;
    result.title = "CEF依赖检查";
    
    QStringList issues;
    CheckLevel maxLevel = LEVEL_OK;

    // 检查CEF根目录
    QString appDir = QCoreApplication::applicationDirPath();
    QDir cefDir(appDir);
    
    // 检查关键CEF文件
    QStringList requiredFiles = {
        "libcef.dll",
        "chrome_elf.dll", 
        "d3dcompiler_47.dll",
        "libEGL.dll",
        "libGLESv2.dll"
    };

    for (const QString &file : requiredFiles) {
        QString filePath = cefDir.absoluteFilePath(file);
        if (!QFileInfo::exists(filePath)) {
            issues << QString("缺少关键文件: %1").arg(file);
            maxLevel = LEVEL_FATAL;
        }
    }

    // 检查CEF资源目录
    QStringList resourceDirs = { "locales", "swiftshader" };
    for (const QString &dir : resourceDirs) {
        QString dirPath = cefDir.absoluteFilePath(dir);
        if (!QDir(dirPath).exists()) {
            issues << QString("缺少资源目录: %1").arg(dir);
            maxLevel = qMax(maxLevel, LEVEL_ERROR);
        }
    }

    // 检查CEF wrapper库
    QString wrapperLib = QString("%1/lib/Release/cef_dll_wrapper.lib").arg(QCoreApplication::applicationDirPath());
    if (!QFileInfo::exists(wrapperLib)) {
        // 在运行时这可能不是问题，因为可能已经链接到可执行文件中
        issues << "CEF wrapper库文件未找到（可能已静态链接）";
        maxLevel = qMax(maxLevel, LEVEL_WARNING);
    }

    result.level = maxLevel;
    result.details = issues;
    
    if (maxLevel == LEVEL_OK) {
        result.message = "CEF依赖文件完整";
    } else if (maxLevel == LEVEL_FATAL) {
        result.message = "CEF关键文件缺失，无法启动";
        result.solution = "请重新安装应用程序或下载完整的CEF运行时";
    } else {
        result.message = QString("CEF依赖检查发现%1个问题").arg(issues.size());
        result.solution = "部分功能可能受限，建议重新安装应用程序";
    }

    return result;
}

SystemChecker::CheckResult SystemChecker::checkConfigPermissions()
{
    CheckResult result;
    result.type = CHECK_CONFIG_PERMISSIONS;
    result.title = "配置权限验证";
    result.autoFixable = true;
    
    QStringList issues;
    CheckLevel maxLevel = LEVEL_OK;

    // 检查配置文件
    QString configPath = m_configManager->getConfigFilePath();
    QFileInfo configInfo(configPath);
    
    if (!configInfo.exists()) {
        issues << "配置文件不存在";
        maxLevel = LEVEL_WARNING; // 可以自动创建默认配置
    } else if (!configInfo.isReadable()) {
        issues << "配置文件无法读取";
        maxLevel = LEVEL_ERROR;
    }

    // 检查日志目录权限
    QString logDir = QCoreApplication::applicationDirPath() + "/log";
    QDir logDirInfo(logDir);
    
    if (!logDirInfo.exists()) {
        if (!logDirInfo.mkpath(".")) {
            issues << "无法创建日志目录";
            maxLevel = qMax(maxLevel, LEVEL_ERROR);
        }
    }

    // 测试写入权限
    QString testFile = logDir + "/test_write.tmp";
    QFile testFileObj(testFile);
    if (!testFileObj.open(QIODevice::WriteOnly)) {
        issues << "日志目录无写入权限";
        maxLevel = qMax(maxLevel, LEVEL_ERROR);
    } else {
        testFileObj.close();
        testFileObj.remove();
    }

    // 检查磁盘空间
    QStorageInfo storage(QCoreApplication::applicationDirPath());
    qint64 availableSpace = storage.bytesAvailable() / (1024 * 1024); // MB
    
    if (availableSpace < 100) {
        issues << QString("磁盘空间不足: %1MB可用").arg(availableSpace);
        maxLevel = qMax(maxLevel, LEVEL_WARNING);
    }

    // 检查管理员权限（Windows）
#ifdef Q_OS_WIN
    if (!isAdministrator()) {
        issues << "未以管理员权限运行，部分功能可能受限";
        maxLevel = qMax(maxLevel, LEVEL_WARNING);
        result.solution = "右键选择'以管理员身份运行'以获得完整功能";
    }
#endif

    result.level = maxLevel;
    result.details = issues;
    
    if (maxLevel == LEVEL_OK) {
        result.message = "配置和权限检查通过";
    } else {
        result.message = QString("发现%1个配置或权限问题").arg(issues.size());
    }

    return result;
}

SystemChecker::CheckResult SystemChecker::preloadComponents()
{
    CheckResult result;
    result.type = CHECK_PRELOAD_COMPONENTS;
    result.title = "组件预加载";
    
    QStringList loadedComponents;
    CheckLevel maxLevel = LEVEL_OK;

    try {
        // 预加载配置管理器
        bool configLoaded = m_configManager->isValid();
        if (configLoaded) {
            loadedComponents << "配置管理器";
        } else {
            result.level = LEVEL_WARNING;
            result.details << "配置管理器加载异常";
        }

        // 预加载日志系统
        loadedComponents << "日志系统";

        // 这里可以添加更多组件的预加载逻辑
        
        result.message = QString("成功预加载%1个组件").arg(loadedComponents.size());
        result.details = loadedComponents;

    } catch (...) {
        result.level = LEVEL_ERROR;
        result.message = "组件预加载过程中发生异常";
    }

    return result;
}

bool SystemChecker::hasFatalErrors() const
{
    for (const CheckResult &result : m_results) {
        if (result.level == LEVEL_FATAL) {
            return true;
        }
    }
    return false;
}

QList<SystemChecker::CheckResult> SystemChecker::getFatalErrors() const
{
    QList<CheckResult> fatalErrors;
    for (const CheckResult &result : m_results) {
        if (result.level == LEVEL_FATAL) {
            fatalErrors.append(result);
        }
    }
    return fatalErrors;
}

void SystemChecker::attemptAutoFix()
{
    int fixedCount = 0;
    
    for (CheckResult &result : m_results) {
        if (result.autoFixable && result.level != LEVEL_OK) {
            // 尝试自动修复
            if (result.type == CHECK_CONFIG_PERMISSIONS) {
                // 尝试创建默认配置文件
                if (m_configManager->createDefaultConfig()) {
                    result.level = LEVEL_OK;
                    result.message = "已自动创建默认配置文件";
                    fixedCount++;
                }
            }
        }
    }
    
    emit autoFixCompleted(fixedCount);
}

bool SystemChecker::isAdministrator()
{
#ifdef Q_OS_WIN
    BOOL isAdmin = FALSE;
    PSID adminGroup = nullptr;
    SID_IDENTIFIER_AUTHORITY ntAuthority = SECURITY_NT_AUTHORITY;
    
    if (AllocateAndInitializeSid(&ntAuthority, 2, SECURITY_BUILTIN_DOMAIN_RID,
                                DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &adminGroup)) {
        CheckTokenMembership(nullptr, adminGroup, &isAdmin);
        FreeSid(adminGroup);
    }
    
    return isAdmin == TRUE;
#else
    return true; // 非Windows系统暂时返回true
#endif
}

QString SystemChecker::formatFileSize(qint64 bytes)
{
    if (bytes < 1024) return QString("%1 B").arg(bytes);
    if (bytes < 1024 * 1024) return QString("%1 KB").arg(bytes / 1024.0, 0, 'f', 1);
    if (bytes < 1024 * 1024 * 1024) return QString("%1 MB").arg(bytes / (1024.0 * 1024.0), 0, 'f', 1);
    return QString("%1 GB").arg(bytes / (1024.0 * 1024.0 * 1024.0), 0, 'f', 1);
}

void SystemChecker::retryCheck(CheckType type)
{
    m_logger->appEvent(QString("重试检测项目: %1").arg(static_cast<int>(type)));
    
    CheckResult result;
    switch (type) {
        case CHECK_SYSTEM_COMPATIBILITY: result = checkSystemCompatibility(); break;
        case CHECK_NETWORK_CONNECTION: result = checkNetworkConnection(); break;
        case CHECK_CEF_DEPENDENCIES: result = checkCEFDependencies(); break;
        case CHECK_CONFIG_PERMISSIONS: result = checkConfigPermissions(); break;
        case CHECK_PRELOAD_COMPONENTS: result = preloadComponents(); break;
    }
    
    // 更新结果列表中对应的项目
    for (int i = 0; i < m_results.size(); ++i) {
        if (m_results[i].type == type) {
            m_results[i] = result;
            break;
        }
    }
    
    emit checkItemCompleted(result);
}

void SystemChecker::onNetworkCheckTimeout()
{
    m_logger->appEvent("网络检测超时");
}