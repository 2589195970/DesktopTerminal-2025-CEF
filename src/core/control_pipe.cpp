#include "control_pipe.h"

#ifdef _WIN32
#include <QMetaObject>
#include <QObject>

#include <memory>
#include <new>
#include <utility>
#include <windows.h>
#include <process.h>
#include <sddl.h>

namespace {

constexpr wchar_t pipeName[] = L"\\\\.\\pipe\\UMTerminal.CEF.Control";
constexpr char stopCommand[] = "STOP\n";

} // namespace

struct ControlPipe::WorkerContext
{
    ControlPipe* owner;
    HANDLE pipe;
    QObject* receiver;
    std::function<void()> onStop;
};

unsigned __stdcall ControlPipe::run(void* argument)
{
    std::unique_ptr<WorkerContext> context(static_cast<WorkerContext*>(argument));
    while (!context->owner->m_stopping) {
        if (!ConnectNamedPipe(context->pipe, nullptr) && GetLastError() != ERROR_PIPE_CONNECTED) {
            break;
        }

        context->owner->serveClient(*context);
        DisconnectNamedPipe(context->pipe);
    }
    CloseHandle(context->pipe);
    return 0;
}

void ControlPipe::serveClient(WorkerContext& context)
{
    // 逐字节读取首行，只记录与 STOP 命令匹配的字节数。
    size_t matched = 0;
    bool matchesStop = true;
    char byte = 0;
    do {
        DWORD received = 0;
        if (m_stopping || !ReadFile(context.pipe, &byte, 1, &received, nullptr) || received == 0) {
            return;
        }
        if (matchesStop) {
            matchesStop = matched < sizeof(stopCommand) - 1 && byte == stopCommand[matched];
            ++matched;
        }
    } while (byte != '\n');

    const bool stopRequested = matchesStop && matched == sizeof(stopCommand) - 1;
    const char* reply = stopRequested ? "OK\n" : "ERR\n";
    const DWORD replySize = stopRequested ? 3 : 4;
    DWORD written = 0;
    if (m_stopping || !WriteFile(context.pipe, reply, replySize, &written, nullptr) || written != replySize) {
        return;
    }

    // 等待客户端读取回执，避免断开管道时丢弃回执。
    FlushFileBuffers(context.pipe);
    if (stopRequested && !m_stopping) {
        QMetaObject::invokeMethod(context.receiver, context.onStop, Qt::QueuedConnection);
    }
}
#endif

ControlPipe::~ControlPipe()
{
    stop();
}

bool ControlPipe::start(QObject* receiver, std::function<void()> onStop)
{
#ifdef _WIN32
    if (m_worker) {
        return true;
    }

    PSECURITY_DESCRIPTOR descriptor = nullptr;
    if (!ConvertStringSecurityDescriptorToSecurityDescriptorW(
            L"D:P(A;;GA;;;SY)", SDDL_REVISION_1, &descriptor, nullptr)) {
        return false;
    }

    SECURITY_ATTRIBUTES attributes = {sizeof(SECURITY_ATTRIBUTES), descriptor, FALSE};
    HANDLE pipe = CreateNamedPipeW(pipeName, PIPE_ACCESS_DUPLEX | FILE_FLAG_FIRST_PIPE_INSTANCE,
                                   PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT | PIPE_REJECT_REMOTE_CLIENTS,
                                   1, 64, 64, 0, &attributes);
    LocalFree(descriptor);
    if (pipe == INVALID_HANDLE_VALUE) {
        return false;
    }

    auto* context = new (std::nothrow) WorkerContext{this, pipe, receiver, std::move(onStop)};
    if (!context) {
        CloseHandle(pipe);
        return false;
    }

    m_stopping = false;
    m_worker = reinterpret_cast<HANDLE>(_beginthreadex(nullptr, 0, run, context, 0, nullptr));
    if (!m_worker) {
        delete context;
        CloseHandle(pipe);
        return false;
    }
#else
    (void)receiver;
    (void)onStop;
#endif
    return true;
}

void ControlPipe::stop()
{
#ifdef _WIN32
    if (!m_worker) {
        return;
    }

    m_stopping = true;
    // 反复取消同步 I/O，覆盖线程在检查停止标志后才开始等待的情况。
    do {
        CancelSynchronousIo(m_worker);
    } while (WaitForSingleObject(m_worker, 10) == WAIT_TIMEOUT);
    CloseHandle(m_worker);
    m_worker = nullptr;
#endif
}
