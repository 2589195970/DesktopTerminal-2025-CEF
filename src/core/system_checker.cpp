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
#include <QHostAddress>
#include <QNetworkAddressEntry>
#include <QLibrary>
#include <QProcess>
#include <QSysInfo>

#ifdef Q_OS_WIN
#include <windows.h>
#include <lmcons.h>
#endif

SystemChecker::SystemChecker(QObject *parent)
    : QObject(parent)
    , m_logger(&Logger::instance())
    , m_configManager(&ConfigManager::instance())
    , m_networkTimeout(nullptr)
    , m_networkManager(nullptr)
    , m_currentCheck(0)
    , m_totalChecks(6)
    , m_checkInProgress(false)
{
    // 注册自定义类型用于信号槽
    qRegisterMetaType<SystemChecker::CheckResult>("SystemChecker::CheckResult");
    
    // 初始化网络管理器
    m_networkManager = new QNetworkAccessManager(this);
    m_logger->appEvent("网络管理器已初始化");
    
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
        "运行库依赖检查",
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
            case 2: result = checkRuntimeDependencies(); break;
            case 3: result = checkCEFDependencies(); break;
            case 4: result = checkConfigPermissions(); break;
            case 5: result = preloadComponents(); break;
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
        maxLevel = LEVEL_ERROR;
        
        // 如果没有活动接口，尝试检查更多信息
        bool hasAnyInterface = false;
        for (const QNetworkInterface &interface : interfaces) {
            if (!(interface.flags() & QNetworkInterface::IsLoopBack)) {
                hasAnyInterface = true;
                m_logger->appEvent(QString("发现网络接口 %1，但未激活").arg(interface.name()));
            }
        }
        
        if (!hasAnyInterface) {
            issues << "系统中没有发现任何网络适配器";
            maxLevel = LEVEL_FATAL;
            result.solution = "请检查：\n1. 网络适配器是否已启用\n2. 网络驱动程序是否正常\n3. 硬件连接是否正确";
        }
    }

    // 基于Qt 5的网络检测
    if (!hasActiveInterface) {
        // 未检测到活动的网络接口
        issues << "未检测到活动的网络连接";
        maxLevel = LEVEL_FATAL;
        result.solution = "网络连接完全断开，请检查：\n1. 网络电缆连接\n2. WiFi开关状态\n3. 网络适配器状态\n4. 联系网络管理员";
    } else {
        // 有活动接口，进一步检查网络连通性
        // 检查是否只有本地连接
        bool hasInternetCapability = false;
        for (const QNetworkInterface &interface : interfaces) {
            if (interface.flags() & QNetworkInterface::IsUp && 
                interface.flags() & QNetworkInterface::IsRunning &&
                !(interface.flags() & QNetworkInterface::IsLoopBack)) {
                
                // 检查是否有非本地IP地址
                QList<QNetworkAddressEntry> entries = interface.addressEntries();
                for (const QNetworkAddressEntry &entry : entries) {
                    QHostAddress addr = entry.ip();
                    if (!addr.isLoopback() && !addr.isLinkLocal()) {
                        hasInternetCapability = true;
                        break;
                    }
                }
            }
        }
        
        if (!hasInternetCapability) {
            issues << "仅本地网络连接，无法访问互联网";
            maxLevel = qMax(maxLevel, LEVEL_ERROR);
            result.solution = "请检查：\n1. 路由器互联网连接\n2. DNS设置\n3. 代理服务器设置";
        }
    }

    result.level = maxLevel;
    result.details = issues;
    
    if (maxLevel == LEVEL_OK) {
        result.message = "网络连接正常";
    } else if (maxLevel == LEVEL_WARNING) {
        result.message = "网络连接存在一些问题，但可以继续使用";
        if (result.solution.isEmpty()) {
            result.solution = "检查网络设置或切换到更稳定的网络连接";
        }
    } else if (maxLevel == LEVEL_ERROR) {
        result.message = "网络连接存在严重问题";
        if (result.solution.isEmpty()) {
            result.solution = "请检查网络连接并重试";
        }
    } else if (maxLevel == LEVEL_FATAL) {
        result.message = "网络连接完全断开，无法继续";
        // solution已在上面设置
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
        "libEGL.dll",
        "libGLESv2.dll"
    };

    // 32位系统需要额外的DirectX编译器库
    if (Application::is32BitSystem()) {
        requiredFiles << "d3dcompiler_47.dll";
    }

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

    // 注意：cef_dll_wrapper.lib 是编译时依赖，已静态链接到可执行文件中，运行时不需要检查

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

SystemChecker::CheckResult SystemChecker::checkRuntimeDependencies()
{
    CheckResult result;
    result.type = CHECK_RUNTIME_DEPENDENCIES;
    result.title = "运行库依赖检查";
    result.autoFixable = true;
    
    QStringList issues;
    CheckLevel maxLevel = LEVEL_OK;

#ifdef Q_OS_WIN
    QStringList requiredDlls = { "vcruntime140", "vcruntime140_1", "msvcp140" };
    for (const QString &dll : requiredDlls) {
        QLibrary lib(dll);
        if (!lib.load()) {
            issues << QString("%1.dll 未正确安装").arg(dll);
            maxLevel = qMax(maxLevel, LEVEL_ERROR);
            result.solution = "请运行自动修复以安装VC++运行库，或重新执行安装程序。";
        } else {
            lib.unload();
        }
    }
#else
    result.level = LEVEL_OK;
    result.message = "当前平台无需运行库检查";
    result.canRetry = false;
    return result;
#endif

    result.level = maxLevel;
    result.details = issues;
    
    if (maxLevel == LEVEL_OK) {
        result.message = "运行库依赖完整";
    } else {
        result.message = QString("发现%1个运行库问题").arg(issues.size());
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
    QString configPath = m_configManager->getActualConfigPath();
    if (configPath.isEmpty()) {
        configPath = "resources/config.json";
    }
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
        bool configLoaded = m_configManager->isLoaded();
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
    QList<CheckType> pendingFixes;
    for (const CheckResult &result : m_results) {
        if (result.autoFixable && result.level != LEVEL_OK) {
            pendingFixes.append(result.type);
        }
    }
    
    int fixedCount = 0;
    
    for (CheckType type : pendingFixes) {
        bool fixed = false;
        if (type == CHECK_CONFIG_PERMISSIONS) {
            QString defaultPath = QCoreApplication::applicationDirPath() + "/config.json";
            if (m_configManager->createDefaultConfig(defaultPath)) {
                m_logger->appEvent("自动修复：已重新生成默认配置文件");
                fixed = true;
            } else {
                m_logger->errorEvent("自动修复失败：无法创建默认配置文件");
            }
        } else if (type == CHECK_RUNTIME_DEPENDENCIES) {
            fixed = installVCRuntimePackage();
        }
        
        if (fixed) {
            retryCheck(type);
            fixedCount++;
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

bool SystemChecker::installVCRuntimePackage()
{
#ifdef Q_OS_WIN
    QString depDir = QCoreApplication::applicationDirPath() + "/resources/dependencies";
    bool is64Bit = QSysInfo::currentCpuArchitecture().contains("64", Qt::CaseInsensitive);
    QStringList candidates;
    if (is64Bit) {
        candidates << QDir(depDir).absoluteFilePath("VC_redist.x64.exe");
    }
    candidates << QDir(depDir).absoluteFilePath("VC_redist.x86.exe");
    
    QString installerPath;
    for (const QString &candidate : candidates) {
        if (QFileInfo::exists(candidate)) {
            installerPath = candidate;
            break;
        }
    }
    
    if (installerPath.isEmpty()) {
        m_logger->errorEvent("未找到离线VC++运行库安装包，无法自动修复运行库问题");
        return false;
    }
    
    QStringList args = { "/install", "/quiet", "/norestart" };
    QProcess process;
    process.start(installerPath, args);
    if (!process.waitForFinished(5 * 60 * 1000)) {
        process.kill();
        process.waitForFinished(1000);
        m_logger->errorEvent("VC++运行库自动安装超时");
        return false;
    }
    
    int exitCode = process.exitCode();
    if (exitCode == 0 || exitCode == 1638 || exitCode == 3010) {
        m_logger->appEvent(QString("VC++运行库安装完成，退出码%1").arg(exitCode));
        return true;
    }
    
    m_logger->errorEvent(QString("VC++运行库安装失败，退出码%1").arg(exitCode));
    return false;
#else
    m_logger->appEvent("非Windows平台无需自动安装VC++运行库");
    return false;
#endif
}

void SystemChecker::retryCheck(CheckType type)
{
    m_logger->appEvent(QString("重试检测项目: %1").arg(static_cast<int>(type)));
    
    CheckResult result;
    switch (type) {
        case CHECK_SYSTEM_COMPATIBILITY: result = checkSystemCompatibility(); break;
        case CHECK_NETWORK_CONNECTION: result = checkNetworkConnection(); break;
        case CHECK_CEF_DEPENDENCIES: result = checkCEFDependencies(); break;
        case CHECK_RUNTIME_DEPENDENCIES: result = checkRuntimeDependencies(); break;
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
