#include "platform_win.h"

#ifdef Q_OS_WIN

#include "win_handle.h"

#include <reason.h>
#include <userenv.h>
#include <winreg.h>
#include <wtsapi32.h>

#include <QDir>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QUdpSocket>

#include <algorithm>
#include <memory>

namespace terminal {
namespace {

constexpr wchar_t AppMutexName[] = L"Global\\UMTerminal.CEF.SingleInstance";
constexpr wchar_t ControlPipeName[] = L"\\\\.\\pipe\\UMTerminal.CEF.Control";
constexpr int PipeTimeoutMs = 5000;
constexpr int AppExitTimeoutMs = 30000;
constexpr int StopPollMs = 100;

QString windowsFailure(const QString &message, DWORD error)
{
    return QStringLiteral("%1（Windows 错误 %2）").arg(message).arg(error);
}

QString sessionText(DWORD sessionId, WTS_INFO_CLASS field)
{
    LPWSTR buffer = nullptr;
    DWORD size = 0;
    const BOOL available = WTSQuerySessionInformationW(WTS_CURRENT_SERVER_HANDLE, sessionId, field, &buffer, &size);
    const std::unique_ptr<wchar_t, decltype(&WTSFreeMemory)> text(buffer, WTSFreeMemory);
    if (!available || !text || size < sizeof(wchar_t)) {
        return {};
    }
    return QString::fromWCharArray(text.get());
}

CommandResult startApp(const Command &command, const Config &config)
{
    if (appIsRunning()) {
        return {command.id, true, QStringLiteral("考试端已在运行")};
    }
    constexpr DWORD NoSession = 0xffffffffu;
    const DWORD sessionId = WTSGetActiveConsoleSessionId();
    if (sessionId == NoSession
            || !examAccountMatches(config.examAccount, sessionText(sessionId, WTSUserName),
                                   sessionText(sessionId, WTSDomainName))) {
        return {command.id, false, QStringLiteral("Windows 未登录考试账户")};
    }

    const QFileInfo app(config.appPath);
    if (!app.isAbsolute() || !app.isFile()) {
        return {command.id, false, QStringLiteral("考试端程序路径无效")};
    }
    HANDLE sessionToken = nullptr;
    if (!WTSQueryUserToken(sessionId, &sessionToken)) {
        const DWORD error = GetLastError();
        if (error == ERROR_NO_TOKEN || error == ERROR_NO_SUCH_LOGON_SESSION) {
            return {command.id, false, QStringLiteral("Windows 未登录考试账户")};
        }
        return {command.id, false, windowsFailure(QStringLiteral("无法获取考试账户令牌"), error)};
    }
    const WinHandle originalToken(sessionToken);
    TOKEN_ELEVATION_TYPE elevationType{};
    DWORD tokenInfoSize = 0;
    if (!GetTokenInformation(originalToken.get(), TokenElevationType, &elevationType, sizeof(elevationType),
                             &tokenInfoSize)) {
        return {command.id, false, windowsFailure(QStringLiteral("无法查询考试账户令牌的提权类型"), GetLastError())};
    }

    TOKEN_LINKED_TOKEN linkedTokenInfo{};
    // 启用 UAC 的管理员会话使用受限令牌，考试端的 requireAdministrator 清单需要关联的完整令牌。
    if (elevationType == TokenElevationTypeLimited
            && !GetTokenInformation(originalToken.get(), TokenLinkedToken, &linkedTokenInfo, sizeof(linkedTokenInfo),
                                    &tokenInfoSize)) {
        return {command.id, false, windowsFailure(QStringLiteral("无法获取考试账户关联的完整令牌"), GetLastError())};
    }
    const WinHandle linkedToken(linkedTokenInfo.LinkedToken);
    const HANDLE launchToken = elevationType == TokenElevationTypeLimited ? linkedToken.get() : originalToken.get();
    HANDLE primaryToken = nullptr;
    if (!DuplicateTokenEx(launchToken, MAXIMUM_ALLOWED, nullptr, SecurityImpersonation, TokenPrimary, &primaryToken)) {
        return {command.id, false, windowsFailure(QStringLiteral("无法复制考试账户令牌"), GetLastError())};
    }
    const WinHandle token(primaryToken);
    LPVOID block = nullptr;
    if (!CreateEnvironmentBlock(&block, token.get(), FALSE)) {
        return {command.id, false, windowsFailure(QStringLiteral("无法创建考试账户环境"), GetLastError())};
    }
    const std::unique_ptr<void, decltype(&DestroyEnvironmentBlock)> environment(block, DestroyEnvironmentBlock);

    // 验证用户名后控制台会话可能切换，启动前再次核对当前会话。
    if (WTSGetActiveConsoleSessionId() != sessionId
            || !examAccountMatches(config.examAccount, sessionText(sessionId, WTSUserName),
                                   sessionText(sessionId, WTSDomainName))) {
        return {command.id, false, QStringLiteral("Windows 未登录考试账户")};
    }
    const std::wstring executable = QDir::toNativeSeparators(app.absoluteFilePath()).toStdWString();
    std::wstring commandLine = L"\"" + executable + L"\"";
    const std::wstring workingDirectory = QDir::toNativeSeparators(app.absolutePath()).toStdWString();
    wchar_t desktop[] = L"winsta0\\default";
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    startup.lpDesktop = desktop;
    PROCESS_INFORMATION process{};
    if (!CreateProcessAsUserW(token.get(), executable.c_str(), commandLine.data(), nullptr, nullptr, FALSE,
                              CREATE_UNICODE_ENVIRONMENT, environment.get(), workingDirectory.c_str(),
                              &startup, &process)) {
        const DWORD error = GetLastError();
        if (error == ERROR_ELEVATION_REQUIRED) {
            return {command.id, false, QStringLiteral("考试端需要管理员权限，当前账户无法提权")};
        }
        return {command.id, false, windowsFailure(QStringLiteral("启动考试端失败"), error)};
    }
    const WinHandle appProcess(process.hProcess);
    const WinHandle appThread(process.hThread);
    return {command.id, true, QStringLiteral("考试端已启动")};
}

DWORD awaitControlPipe(const WaitForStop &waitForStop)
{
    QElapsedTimer timer;
    timer.start();
    for (;;) {
        const qint64 remaining = PipeTimeoutMs - timer.elapsed();
        if (remaining <= 0) {
            return ERROR_SEM_TIMEOUT;
        }
        const DWORD wait = static_cast<DWORD>(std::min<qint64>(StopPollMs, remaining));
        if (waitForStop(0)) {
            return ERROR_OPERATION_ABORTED;
        }
        if (WaitNamedPipeW(ControlPipeName, wait)) {
            return ERROR_SUCCESS;
        }
        const DWORD error = GetLastError();
        if (error != ERROR_SEM_TIMEOUT && error != ERROR_FILE_NOT_FOUND && error != ERROR_PIPE_BUSY) {
            return error;
        }
        if (error != ERROR_SEM_TIMEOUT && waitForStop(static_cast<int>(wait))) {
            return ERROR_OPERATION_ABORTED;
        }
    }
}

DWORD transferPipe(HANDLE pipe, bool writing, char *bytes, DWORD size, DWORD &transferred,
                   const QElapsedTimer &timer, const WaitForStop &waitForStop)
{
    const WinHandle event(CreateEventW(nullptr, TRUE, FALSE, nullptr));
    if (!event) {
        return GetLastError();
    }
    OVERLAPPED operation{};
    operation.hEvent = event.get();
    const BOOL complete = writing ? WriteFile(pipe, bytes, size, &transferred, &operation)
                                  : ReadFile(pipe, bytes, size, &transferred, &operation);
    if (complete) {
        return ERROR_SUCCESS;
    }
    const DWORD error = GetLastError();
    if (error != ERROR_IO_PENDING) {
        return error;
    }
    for (;;) {
        const qint64 remaining = PipeTimeoutMs - timer.elapsed();
        if (remaining <= 0 || waitForStop(0)) {
            CancelIoEx(pipe, &operation);
            // 等待取消完成，避免系统继续访问栈上的 OVERLAPPED 和缓冲区。
            GetOverlappedResult(pipe, &operation, &transferred, TRUE);
            return remaining <= 0 ? ERROR_SEM_TIMEOUT : ERROR_OPERATION_ABORTED;
        }
        const DWORD wait = WaitForSingleObject(event.get(), static_cast<DWORD>(std::min<qint64>(remaining, StopPollMs)));
        if (wait == WAIT_OBJECT_0) {
            return GetOverlappedResult(pipe, &operation, &transferred, FALSE) ? ERROR_SUCCESS : GetLastError();
        }
    }
}

QString requestAppStop(const WaitForStop &waitForStop)
{
    const DWORD ready = awaitControlPipe(waitForStop);
    if (ready != ERROR_SUCCESS) {
        return windowsFailure(QStringLiteral("等待考试端关闭管道失败"), ready);
    }
    const WinHandle pipe(CreateFileW(ControlPipeName, GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING,
                                     FILE_FLAG_OVERLAPPED | SECURITY_SQOS_PRESENT | SECURITY_IDENTIFICATION, nullptr));
    if (!pipe) {
        return windowsFailure(QStringLiteral("连接考试端关闭管道失败"), GetLastError());
    }
    QElapsedTimer timer;
    timer.start();
    char request[] = "STOP\n";
    constexpr DWORD requestLength = sizeof(request) - 1;
    DWORD transferred = 0;
    DWORD error = transferPipe(pipe.get(), true, request, requestLength, transferred, timer, waitForStop);
    if (error != ERROR_SUCCESS || transferred != requestLength) {
        return windowsFailure(QStringLiteral("发送考试端关闭请求失败"), error == ERROR_SUCCESS ? ERROR_WRITE_FAULT : error);
    }
    QByteArray reply;
    while (reply.size() < 4 && !reply.contains('\n')) {
        char buffer[4];
        error = transferPipe(pipe.get(), false, buffer, sizeof(buffer), transferred, timer, waitForStop);
        if (error != ERROR_SUCCESS || transferred == 0) {
            return windowsFailure(QStringLiteral("读取考试端关闭响应失败"), error == ERROR_SUCCESS ? ERROR_BROKEN_PIPE : error);
        }
        reply.append(buffer, static_cast<int>(transferred));
    }
    return reply == "OK\n" ? QString() : QStringLiteral("考试端未确认关闭请求");
}

CommandResult stopApp(const Command &command, const WaitForStop &waitForStop)
{
    if (!appIsRunning()) {
        return {command.id, true, QStringLiteral("考试端未运行")};
    }
    const QString error = requestAppStop(waitForStop);
    if (!error.isEmpty()) {
        return {command.id, false, error};
    }
    QElapsedTimer timer;
    timer.start();
    for (;;) {
        if (!appIsRunning()) {
            return {command.id, true, QStringLiteral("考试端已退出")};
        }
        const qint64 remaining = AppExitTimeoutMs - timer.elapsed();
        if (remaining <= 0) {
            return {command.id, false, QStringLiteral("考试端未在 30 秒内退出")};
        }
        if (waitForStop(static_cast<int>(std::min<qint64>(remaining, StopPollMs)))) {
            return {command.id, false, QStringLiteral("终端服务正在停止")};
        }
    }
}

CommandResult relayWake(const Command &command, const NetworkIdentity &network, const WaitForStop &waitForStop)
{
    QUdpSocket socket;
    const QHostAddress directed = broadcastAddress(network.localIp, network.prefixLength);
    const QHostAddress limited(QHostAddress::Broadcast);
    QVector<QByteArray> packets;
    for (const QString &mac : command.macs) {
        const QByteArray packet = magicPacket(mac);
        if (!packet.isEmpty()) {
            packets.append(packet);
        }
    }
    if (!directed.isNull()) {
        socket.bind(network.localIp, 0);
    }
    QVector<bool> sent(packets.size(), false);
    for (int round = 0; round < 3; ++round) {
        if (waitForStop(round == 0 ? 0 : 100)) {
            return {command.id, false, QStringLiteral("终端服务正在停止")};
        }
        for (int index = 0; index < packets.size(); ++index) {
            const QByteArray &packet = packets.at(index);
            const bool sentDirected = !directed.isNull() && socket.writeDatagram(packet, directed, 9) == packet.size();
            const bool sentLimited = socket.writeDatagram(packet, limited, 9) == packet.size();
            sent[index] = sent.at(index) || sentDirected || sentLimited;
        }
    }
    return {command.id, true, QStringLiteral("已发送 %1 个").arg(std::count(sent.cbegin(), sent.cend(), true))};
}

} // namespace

bool appIsRunning()
{
    const WinHandle mutex(OpenMutexW(SYNCHRONIZE, FALSE, AppMutexName));
    return static_cast<bool>(mutex);
}

CommandResult executeAppOrWake(const Command &command, const Config &config, const NetworkIdentity &network,
                               const WaitForStop &waitForStop)
{
    if (command.type == QStringLiteral("START_APP")) {
        return startApp(command, config);
    }
    if (command.type == QStringLiteral("STOP_APP")) {
        return stopApp(command, waitForStop);
    }
    if (command.type == QStringLiteral("WAKE_RELAY")) {
        return relayWake(command, network, waitForStop);
    }
    return {command.id, false, QStringLiteral("不支持的指令")};
}

unsigned long initiatePowerChange(bool reboot)
{
    HANDLE processToken = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &processToken)) {
        return GetLastError();
    }
    const WinHandle token(processToken);
    TOKEN_PRIVILEGES privileges{};
    privileges.PrivilegeCount = 1;
    privileges.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
    if (!LookupPrivilegeValueW(nullptr, SE_SHUTDOWN_NAME, &privileges.Privileges[0].Luid)) {
        return GetLastError();
    }
    TOKEN_PRIVILEGES previous{};
    DWORD previousSize = 0;
    if (!AdjustTokenPrivileges(token.get(), FALSE, &privileges, sizeof(previous), &previous, &previousSize)) {
        return GetLastError();
    }
    const DWORD adjustmentError = GetLastError();
    if (adjustmentError != ERROR_SUCCESS) {
        return adjustmentError;
    }
    const DWORD flags = SHUTDOWN_FORCE_OTHERS | (reboot ? SHUTDOWN_RESTART : SHUTDOWN_POWEROFF);
    const DWORD status = InitiateShutdownW(nullptr, nullptr, 0, flags, SHTDN_REASON_FLAG_PLANNED);
    AdjustTokenPrivileges(token.get(), FALSE, &previous, 0, nullptr, nullptr);
    return status;
}

} // namespace terminal

#endif
