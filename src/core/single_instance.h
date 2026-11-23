#ifndef SINGLE_INSTANCE_H
#define SINGLE_INSTANCE_H

#ifdef _WIN32
#include "../terminal_service/win_handle.h"
#endif

class SingleInstance
{
public:
    enum class Result { Acquired, AlreadyRunning, Error };

    SingleInstance() = default;
    SingleInstance(const SingleInstance&) = delete;
    SingleInstance& operator=(const SingleInstance&) = delete;

    Result acquire();
    static void markMainWindow(void* window);
    static void unmarkMainWindow(void* window);

private:
#ifdef _WIN32
    terminal::WinHandle m_mutex;
#endif
};

#endif // SINGLE_INSTANCE_H
