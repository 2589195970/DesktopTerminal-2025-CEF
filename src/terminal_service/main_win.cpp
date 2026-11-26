#include <QtGlobal>

#ifdef Q_OS_WIN

#include "service_runtime.h"
#include "monitor_windows.h"
#include "seat_display_store.h"
#include "win_handle.h"
#include "../app_version.h"

#include <aclapi.h>
#include <sddl.h>
#include <winsvc.h>

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QLockFile>

#include <atomic>
#include <mutex>
#include <string_view>

namespace {

using terminal::WinHandle;
using terminal::LocalMemory;

using terminal::TerminalServiceName;
using terminal::TerminalRefreshControl;
constexpr wchar_t DirectoryDacl[] = L"D:P(A;OICI;FA;;;SY)(A;OICI;FA;;;BA)";
constexpr wchar_t StatusDirectoryDacl[] = L"D:P(A;OICI;FA;;;SY)(A;OICI;FA;;;BA)(A;OICI;GRGX;;;BU)";
constexpr DWORD StatusWaitHintMs = 10000;

struct ServiceController {
    WinHandle stopEvent{CreateEventW(nullptr, TRUE, FALSE, nullptr)};
    SERVICE_STATUS_HANDLE handle = nullptr;
    SERVICE_STATUS status{};
    std::mutex statusMutex;
    std::atomic_bool refreshRequested{false};
    int argc = 0;
    char **argv = nullptr;

    void report(DWORD state, DWORD applicationError = 0)
    {
        const std::lock_guard<std::mutex> lock(statusMutex);
        if ((status.dwCurrentState == SERVICE_STOP_PENDING && state == SERVICE_RUNNING)
                || status.dwCurrentState == SERVICE_STOPPED) {
            return;
        }
        const bool pending = state == SERVICE_START_PENDING || state == SERVICE_STOP_PENDING;
        status.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
        status.dwCurrentState = state;
        status.dwControlsAccepted = state == SERVICE_RUNNING ? SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN : 0;
        status.dwWin32ExitCode = applicationError == 0 ? NO_ERROR : ERROR_SERVICE_SPECIFIC_ERROR;
        status.dwServiceSpecificExitCode = applicationError;
        status.dwCheckPoint = pending ? status.dwCheckPoint + 1 : 0;
        status.dwWaitHint = pending ? StatusWaitHintMs : 0;
        SetServiceStatus(handle, &status);
    }

    bool waitForStop(int timeout) const
    {
        return WaitForSingleObject(stopEvent.get(), static_cast<DWORD>(timeout)) == WAIT_OBJECT_0;
    }
};

ServiceController &controller()
{
    static ServiceController service;
    return service;
}

DWORD ensureDirectory(const QString &path, const wchar_t *directoryDacl, bool requireTrustedOwner = false)
{
    PSECURITY_DESCRIPTOR descriptor = nullptr;
    if (!ConvertStringSecurityDescriptorToSecurityDescriptorW(directoryDacl, SDDL_REVISION_1, &descriptor, nullptr)) {
        return GetLastError();
    }
    const LocalMemory security(descriptor, LocalFree);
    const std::wstring nativePath = QDir::toNativeSeparators(path).toStdWString();
    SECURITY_ATTRIBUTES attributes{sizeof(attributes), descriptor, FALSE};
    if (!CreateDirectoryW(nativePath.c_str(), &attributes)) {
        const DWORD error = GetLastError();
        if (error != ERROR_ALREADY_EXISTS) {
            return error;
        }
    }

    const auto openDirectory = [&nativePath](DWORD access) {
        return WinHandle(CreateFileW(nativePath.c_str(), access, FILE_SHARE_READ | FILE_SHARE_WRITE,
                                     nullptr, OPEN_EXISTING,
                                     FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, nullptr));
    };
    const WinHandle directory = openDirectory(READ_CONTROL | WRITE_DAC);
    if (!directory) {
        return GetLastError();
    }
    BY_HANDLE_FILE_INFORMATION info{};
    if (!GetFileInformationByHandle(directory.get(), &info)) {
        return GetLastError();
    }
    if (!(info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) || (info.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)) {
        return ERROR_DIRECTORY;
    }

    BOOL daclPresent = FALSE;
    BOOL daclDefaulted = FALSE;
    PACL dacl = nullptr;
    if (!GetSecurityDescriptorDacl(descriptor, &daclPresent, &dacl, &daclDefaulted)) {
        return GetLastError();
    }
    PSID owner = nullptr;
    PSECURITY_DESCRIPTOR existingDescriptor = nullptr;
    const DWORD query = GetSecurityInfo(directory.get(), SE_FILE_OBJECT, OWNER_SECURITY_INFORMATION,
                                        &owner, nullptr, nullptr, nullptr, &existingDescriptor);
    if (query != ERROR_SUCCESS) {
        return query;
    }
    const LocalMemory existingSecurity(existingDescriptor, LocalFree);
    // 目录所有者可以改写 DACL，确保所有者同样属于允许访问的安全主体。
    if (!owner || (!IsWellKnownSid(owner, WinLocalSystemSid) && !IsWellKnownSid(owner, WinBuiltinAdministratorsSid))) {
        if (requireTrustedOwner) {
            return ERROR_ACCESS_DENIED;
        }
        alignas(DWORD) BYTE adminSid[SECURITY_MAX_SID_SIZE];
        DWORD sidSize = sizeof(adminSid);
        if (!CreateWellKnownSid(WinBuiltinAdministratorsSid, nullptr, adminSid, &sidSize)) {
            return GetLastError();
        }
        const WinHandle writableOwner = openDirectory(WRITE_OWNER);
        if (!writableOwner) {
            return GetLastError();
        }
        const DWORD ownership = SetSecurityInfo(writableOwner.get(), SE_FILE_OBJECT, OWNER_SECURITY_INFORMATION,
                                                adminSid, nullptr, nullptr, nullptr);
        if (ownership != ERROR_SUCCESS) {
            return ownership;
        }
    }
    return SetSecurityInfo(directory.get(), SE_FILE_OBJECT, DACL_SECURITY_INFORMATION | PROTECTED_DACL_SECURITY_INFORMATION,
                           nullptr, nullptr, dacl, nullptr);
}

bool isFileOrMissing(const QString &path)
{
    const std::wstring nativePath = QDir::toNativeSeparators(path).toStdWString();
    const DWORD attributes = GetFileAttributesW(nativePath.c_str());
    if (attributes != INVALID_FILE_ATTRIBUTES) {
        return !(attributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT));
    }
    return GetLastError() == ERROR_FILE_NOT_FOUND;
}

QString secureDataDirectory()
{
    const QString path = terminal::SeatDisplayStore::dataDirectory();
    if (path.isEmpty()) {
        qCritical() << "ProgramData 环境变量无效";
        return {};
    }
    const QDir directory(path);
    DWORD error = ensureDirectory(directory.absolutePath(), DirectoryDacl);
    if (error == ERROR_SUCCESS) {
        error = ensureDirectory(directory.filePath(QStringLiteral("logs")), DirectoryDacl);
    }
    if (error != ERROR_SUCCESS) {
        qCritical().noquote() << terminal::windowsFailure(QStringLiteral("无法保护终端服务目录"), error);
        return {};
    }
    for (const QString &file : {QStringLiteral("service.ini"), QStringLiteral("state.json"),
                                QStringLiteral("state.json.lock"), QStringLiteral("logs/terminal-service.log")}) {
        if (!isFileOrMissing(directory.filePath(file))) {
            qCritical().noquote() << QStringLiteral("终端服务文件类型或访问权限无效：%1").arg(file);
            return {};
        }
    }
    return directory.absolutePath();
}

QString secureStatusDirectory(const QString &directory)
{
    const QString path = terminal::registrationStatusDirectory();
    DWORD error = path.isEmpty() ? ERROR_PATH_NOT_FOUND : ensureDirectory(path, StatusDirectoryDacl, true);
    if (error == ERROR_SUCCESS) {
        const QDir status(path);
        const QStringList files{QStringLiteral("status.json"), QStringLiteral("registration.log"),
                                QStringLiteral("registration.log.1"), QStringLiteral("registration.log.2"),
                                QStringLiteral("registration.log.3"), QStringLiteral("registration.log.4")};
        for (const QString &name : files) {
            const std::wstring file = QDir::toNativeSeparators(status.filePath(name)).toStdWString();
            const WinHandle handle(CreateFileW(file.c_str(), FILE_READ_ATTRIBUTES,
                FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
                FILE_FLAG_OPEN_REPARSE_POINT, nullptr));
            if (!handle) {
                const DWORD openError = GetLastError();
                if (openError == ERROR_FILE_NOT_FOUND) {
                    continue;
                }
                error = openError;
                break;
            }
            BY_HANDLE_FILE_INFORMATION info{};
            if (!GetFileInformationByHandle(handle.get(), &info)
                    || info.nNumberOfLinks != 1
                    || (info.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT))) {
                error = ERROR_INVALID_DATA;
                break;
            }
        }
    }
    if (error != ERROR_SUCCESS) {
        terminal::writeServiceLog(directory, terminal::windowsFailure(QStringLiteral("无法保护终端注册状态目录"), error));
        return {};
    }
    return path;
}

int runTerminal(bool console)
{
    auto &service = controller();
    QCoreApplication app(service.argc, service.argv);
    QCoreApplication::setApplicationName(QStringLiteral("DesktopTerminalService"));
    QCoreApplication::setApplicationVersion(QStringLiteral(DESKTOP_TERMINAL_VERSION));
    const QString directory = secureDataDirectory();
    if (directory.isEmpty()) {
        return 1;
    }
    const QDir data(directory);
    QLockFile lock(data.filePath(QStringLiteral("state.json.lock")));
    lock.setStaleLockTime(0);
    if (!lock.tryLock(0)) {
        terminal::writeServiceLog(directory, QStringLiteral("无法锁定 state.json，检查是否已有服务或控制台实例"));
        return 1;
    }

    terminal::RegistrationMonitor monitor(console ? QString() : secureStatusDirectory(directory));
    terminal::RegistrationMonitor::pruneServiceLog(directory);
    monitor.start();
    terminal::Config config;
    terminal::State state;
    QString error;
    if (!terminal::Config::load(data.filePath(QStringLiteral("service.ini")), config, error)) {
        terminal::writeServiceLog(directory, QStringLiteral("读取 service.ini 失败：%1").arg(error));
        monitor.startupFailed(terminal::RegistrationProblem::Configuration);
        return 1;
    }
    monitor.setSchoolInstance(config.slid);
    if (!terminal::State::load(data.filePath(QStringLiteral("state.json")), state, error)) {
        terminal::writeServiceLog(directory, QStringLiteral("读取 state.json 失败：%1").arg(error));
        monitor.startupFailed(terminal::RegistrationProblem::State);
        return 1;
    }

    terminal::ServiceRuntime *activeRuntime = nullptr;
    QTimer stopCheck;
    QObject::connect(&stopCheck, &QTimer::timeout, &app, [&app, &service, &activeRuntime] {
        if (service.waitForStop(0)) {
            app.quit();
            return;
        }
        if (service.refreshRequested.exchange(false) && activeRuntime) {
            activeRuntime->refreshNow();
        }
    });
    stopCheck.start(100);
    int result;
    {
        terminal::ServiceRuntime runtime(directory, config, state, monitor, [&service](int timeout) {
            return service.waitForStop(timeout);
        });
        runtime.start();
        service.refreshRequested = false;
        activeRuntime = &runtime;
        if (!console) {
            service.report(SERVICE_RUNNING);
        }
        result = app.exec();
        activeRuntime = nullptr;
    }
    monitor.stop();
    terminal::writeServiceLog(directory, QStringLiteral("终端服务已停止"));
    return result;
}

DWORD WINAPI handleServiceControl(DWORD code, DWORD, LPVOID, LPVOID context)
{
    auto &service = *static_cast<ServiceController *>(context);
    switch (code) {
    case SERVICE_CONTROL_STOP:
    case SERVICE_CONTROL_SHUTDOWN:
        service.report(SERVICE_STOP_PENDING);
        SetEvent(service.stopEvent.get());
        return NO_ERROR;
    case SERVICE_CONTROL_INTERROGATE: {
        const std::lock_guard<std::mutex> lock(service.statusMutex);
        if (service.handle && service.status.dwCurrentState != SERVICE_STOPPED) {
            SetServiceStatus(service.handle, &service.status);
        }
        return NO_ERROR;
    }
    case TerminalRefreshControl:
        service.refreshRequested = true;
        return NO_ERROR;
    default:
        return ERROR_CALL_NOT_IMPLEMENTED;
    }
}

void WINAPI serviceMain(DWORD, LPWSTR *)
{
    auto &service = controller();
    const SERVICE_STATUS_HANDLE handle = RegisterServiceCtrlHandlerExW(TerminalServiceName, handleServiceControl, &service);
    if (!handle) {
        return;
    }
    {
        const std::lock_guard<std::mutex> lock(service.statusMutex);
        service.handle = handle;
    }
    service.report(SERVICE_START_PENDING);
    const int result = runTerminal(false);
    service.report(SERVICE_STOPPED, static_cast<DWORD>(result));
}

BOOL WINAPI handleConsoleControl(DWORD code)
{
    if (code == CTRL_C_EVENT || code == CTRL_BREAK_EVENT || code == CTRL_CLOSE_EVENT) {
        SetEvent(controller().stopEvent.get());
        return TRUE;
    }
    return FALSE;
}

} // namespace

int main(int argc, char **argv)
{
    auto &service = controller();
    if (!service.stopEvent) {
        return 1;
    }
    service.argc = argc;
    service.argv = argv;
    for (int index = 1; index < argc; ++index) {
        if (std::string_view(argv[index]) == "--console") {
            if (!SetConsoleCtrlHandler(handleConsoleControl, TRUE)) {
                return 1;
            }
            const int result = runTerminal(true);
            SetConsoleCtrlHandler(handleConsoleControl, FALSE);
            return result;
        }
    }
    const SERVICE_TABLE_ENTRYW services[] = {{const_cast<wchar_t *>(TerminalServiceName), serviceMain}, {nullptr, nullptr}};
    return StartServiceCtrlDispatcherW(services) ? 0 : 1;
}

#endif
