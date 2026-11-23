#ifndef CONTROL_PIPE_H
#define CONTROL_PIPE_H

#include <functional>

#ifdef _WIN32
#include "../terminal_service/win_handle.h"

#include <atomic>
#endif

class QObject;

class ControlPipe
{
public:
    ControlPipe() = default;
    ~ControlPipe();
    ControlPipe(const ControlPipe&) = delete;
    ControlPipe& operator=(const ControlPipe&) = delete;

    bool start(QObject* receiver, std::function<void()> onStop);
    void stop();

private:
#ifdef _WIN32
    struct WorkerContext;
    static unsigned __stdcall run(void* argument);
    void serveClient(WorkerContext& context);

    std::atomic<bool> m_stopping{false};
    terminal::WinHandle m_worker;
#endif
};

#endif // CONTROL_PIPE_H
