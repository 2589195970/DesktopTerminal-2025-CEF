#include <QtGlobal>

#ifdef Q_OS_WIN

#include "service_runtime.h"
#include "win_handle.h"

#include <aclapi.h>
#include <sddl.h>
#include <winsvc.h>

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QLockFile>

#include <mutex>
#include <string_view>

namespace {

using terminal::WinHandle;
using terminal::LocalMemory;

wchar_t ServiceName[] = L"UMTerminalService";
constexpr wchar_t DirectoryDacl[] = L"D:P(A;OICI;FA;;;SY)(A;OICI;FA;;;BA)";
constexpr DWORD StatusWaitHintMs = 10000;

struct ServiceController {
    WinHandle stopEvent{CreateEventW(nullptr, TRUE, FALSE, nullptr)};
    SERVICE_STATUS_HANDLE handle = nullptr;
    SERVICE_STATUS status{};
    std::mutex statusMutex;
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

DWORD ensureRestrictedDirectory(const QString &path)
{
    PSECURITY_DESCRIPTOR descriptor = nullptr;
    if (!ConvertStringSecurityDescriptorToSecurityDescriptorW(DirectoryDacl, SDDL_REVISION_1, &descriptor, nullptr)) {
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
    const QString programData = qEnvironmentVariable("ProgramData");
    if (!QDir::isAbsolutePath(programData)) {
        qCritical() << "ProgramData 环境变量无效";
        return {};
    }
    const QDir directory(QDir(programData).filePath(QStringLiteral("UMTerminal")));
    DWORD error = ensureRestrictedDirectory(directory.absolutePath());
    if (error == ERROR_SUCCESS) {
        error = ensureRestrictedDirectory(directory.filePath(QStringLiteral("logs")));
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

int runTerminal(bool console)
{
    auto &service = controller();
    QCoreApplication app(service.argc, service.argv);
    QCoreApplication::setApplicationName(QStringLiteral("DesktopTerminalService"));
    QCoreApplication::setApplicationVersion(QStringLiteral("1.0.0"));
    const QString directory = secureDataDirectory();
    if (directory.isEmpty()) {
        return 1;
    }
    const QDir data(directory);
    terminal::Config config;
    terminal::State state;
    QString error;
    if (!terminal::Config::load(data.filePath(QStringLiteral("service.ini")), config, error)) {
        terminal::writeServiceLog(directory, QStringLiteral("读取 service.ini 失败：%1").arg(error));
        return 1;
    }

    QLockFile lock(data.filePath(QStringLiteral("state.json.lock")));
    if (!config.sample) {
        lock.setStaleLockTime(0);
        if (!lock.tryLock(0)) {
            terminal::writeServiceLog(directory, QStringLiteral("无法锁定 state.json，检查是否已有服务或控制台实例"));
            return 1;
        }
        if (!terminal::State::load(data.filePath(QStringLiteral("state.json")), state, error)) {
            terminal::writeServiceLog(directory, QStringLiteral("读取 state.json 失败：%1").arg(error));
            return 1;
        }
    }

    QTimer stopCheck;
    QObject::connect(&stopCheck, &QTimer::timeout, &app, [&app, &service] {
        if (service.waitForStop(0)) {
            app.quit();
        }
    });
    stopCheck.start(100);
    int result;
    {
        terminal::ServiceRuntime runtime(directory, config, state, [&service](int timeout) {
            return service.waitForStop(timeout);
        });
        runtime.start();
        if (!console) {
            service.report(SERVICE_RUNNING);
        }
        result = app.exec();
    }
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
    default:
        return ERROR_CALL_NOT_IMPLEMENTED;
    }
}

void WINAPI serviceMain(DWORD, LPWSTR *)
{
    auto &service = controller();
    const SERVICE_STATUS_HANDLE handle = RegisterServiceCtrlHandlerExW(ServiceName, handleServiceControl, &service);
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
    const SERVICE_TABLE_ENTRYW services[] = {{ServiceName, serviceMain}, {nullptr, nullptr}};
    return StartServiceCtrlDispatcherW(services) ? 0 : 1;
}

#endif
