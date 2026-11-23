#pragma once

#include <QtGlobal>

#ifdef Q_OS_WIN
#include <windows.h>

namespace terminal {

class WinHandle {
public:
    explicit WinHandle(HANDLE handle = nullptr) : m_handle(handle) {}
    WinHandle(const WinHandle &) = delete;
    WinHandle &operator=(const WinHandle &) = delete;

    ~WinHandle()
    {
        if (*this) {
            CloseHandle(m_handle);
        }
    }

    explicit operator bool() const { return m_handle && m_handle != INVALID_HANDLE_VALUE; }
    HANDLE get() const { return m_handle; }

private:
    HANDLE m_handle;
};

} // namespace terminal
#endif
