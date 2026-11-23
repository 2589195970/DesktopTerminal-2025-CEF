#include "control_pipe.h"

#ifdef _WIN32
#include "../terminal_service/app_control_protocol.h"

#include <QMetaObject>
#include <QObject>

#include <memory>
#include <new>
#include <utility>
#include <process.h>
#include <sddl.h>

struct ControlPipe::WorkerContext
{
    ControlPipe* owner;
    terminal::WinHandle pipe;
    QObject* receiver;
    std::function<void()> onStop;
};

unsigned __stdcall ControlPipe::run(void* argument)
{
    std::unique_ptr<WorkerContext> context(static_cast<WorkerContext*>(argument));
    while (!context->owner->m_stopping) {
        if (!ConnectNamedPipe(context->pipe.get(), nullptr) && GetLastError() != ERROR_PIPE_CONNECTED) {
            break;
        }

        context->owner->serveClient(*context);
        DisconnectNamedPipe(context->pipe.get());
    }
    return 0;
}

void ControlPipe::serveClient(WorkerContext& context)
{
    const HANDLE pipe = context.pipe.get();
    constexpr size_t commandSize = sizeof(terminal::StopCommand) - 1;
    // 读到行尾再回执，避免客户端尚在写入时断开管道。
    size_t matched = 0;
    bool matchesStop = true;
    char byte = 0;
    do {
        DWORD received = 0;
        if (m_stopping || !ReadFile(pipe, &byte, 1, &received, nullptr) || received == 0) {
            return;
        }
        if (matchesStop) {
            matchesStop = matched < commandSize && byte == terminal::StopCommand[matched];
            ++matched;
        }
    } while (byte != '\n');

    const bool stopRequested = matchesStop && matched == commandSize;
    const char* reply = stopRequested ? terminal::StopAccepted : terminal::StopRejected;
    const DWORD replySize = (stopRequested ? sizeof(terminal::StopAccepted) : sizeof(terminal::StopRejected)) - 1;
    DWORD written = 0;
    if (m_stopping || !WriteFile(pipe, reply, replySize, &written, nullptr) || written != replySize) {
        return;
    }

    // 等待客户端读取回执，避免断开管道时丢弃回执。
    FlushFileBuffers(pipe);
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

    terminal::LocalMemory security(descriptor, LocalFree);
    SECURITY_ATTRIBUTES attributes = {sizeof(SECURITY_ATTRIBUTES), security.get(), FALSE};
    terminal::WinHandle pipe(CreateNamedPipeW(terminal::ControlPipeName,
            PIPE_ACCESS_DUPLEX | FILE_FLAG_FIRST_PIPE_INSTANCE,
            PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT | PIPE_REJECT_REMOTE_CLIENTS,
            1, 64, 64, 0, &attributes));
    if (!pipe) {
        return false;
    }

    auto* context = new (std::nothrow) WorkerContext{this, std::move(pipe), receiver, std::move(onStop)};
    if (!context) {
        return false;
    }

    m_stopping = false;
    m_worker.reset(reinterpret_cast<HANDLE>(_beginthreadex(nullptr, 0, run, context, 0, nullptr)));
    if (!m_worker) {
        delete context;
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
        CancelSynchronousIo(m_worker.get());
    } while (WaitForSingleObject(m_worker.get(), 10) == WAIT_TIMEOUT);
    m_worker.reset();
#endif
}
