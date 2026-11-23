#pragma once

#include <QtGlobal>

#ifdef Q_OS_WIN
#include <windows.h>

#include <memory>
#include <utility>

namespace terminal {

using LocalMemory = std::unique_ptr<void, decltype(&LocalFree)>;

class WinHandle {
public:
    explicit WinHandle(HANDLE handle = nullptr) : m_handle(handle) {}
    WinHandle(const WinHandle &) = delete;
    WinHandle &operator=(const WinHandle &) = delete;
    WinHandle(WinHandle &&other) noexcept : m_handle(std::exchange(other.m_handle, nullptr)) {}

    ~WinHandle()
    {
        reset();
    }

    void reset(HANDLE handle = nullptr)
    {
        if (*this) {
            CloseHandle(m_handle);
        }
        m_handle = handle;
    }

    explicit operator bool() const { return m_handle && m_handle != INVALID_HANDLE_VALUE; }
    HANDLE get() const { return m_handle; }

private:
    HANDLE m_handle;
};

} // namespace terminal
#endif
