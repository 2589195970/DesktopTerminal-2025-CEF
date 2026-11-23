#include "single_instance.h"

#ifdef _WIN32
#include <windows.h>
#include <sddl.h>

namespace {

constexpr wchar_t mutexName[] = L"Global\\UMTerminal.CEF.SingleInstance";
constexpr wchar_t windowProperty[] = L"UMTerminal.CEF.MainWindow";

BOOL CALLBACK activateMainWindow(HWND window, LPARAM)
{
    // 考试端标题由学校配置，使用主窗口的固定属性识别窗口。
    if (!GetPropW(window, windowProperty)) {
        return TRUE;
    }

    DWORD processId = 0;
    GetWindowThreadProcessId(window, &processId);
    AllowSetForegroundWindow(processId);
    if (IsIconic(window)) {
        ShowWindowAsync(window, SW_RESTORE);
    }
    SetForegroundWindow(window);
    return FALSE;
}

} // namespace
#endif

SingleInstance::~SingleInstance()
{
#ifdef _WIN32
    if (m_mutex) {
        CloseHandle(m_mutex);
    }
#endif
}

SingleInstance::Result SingleInstance::acquire()
{
#ifdef _WIN32
    PSECURITY_DESCRIPTOR descriptor = nullptr;
    // SYSTEM 服务和提权后的考试端可以跨会话访问该互斥体。
    if (!ConvertStringSecurityDescriptorToSecurityDescriptorW(
            L"D:P(A;;GA;;;SY)(A;;GA;;;BA)", SDDL_REVISION_1, &descriptor, nullptr)) {
        return Result::Error;
    }

    SECURITY_ATTRIBUTES attributes = {sizeof(SECURITY_ATTRIBUTES), descriptor, FALSE};
    m_mutex = CreateMutexW(&attributes, FALSE, mutexName);
    const DWORD error = GetLastError();
    LocalFree(descriptor);
    if (!m_mutex) {
        SetLastError(error);
        return Result::Error;
    }
    if (error == ERROR_ALREADY_EXISTS) {
        EnumWindows(activateMainWindow, 0);
        return Result::AlreadyRunning;
    }
#endif
    return Result::Acquired;
}

void SingleInstance::markMainWindow(void* window)
{
#ifdef _WIN32
    SetPropW(static_cast<HWND>(window), windowProperty, window);
#else
    (void)window;
#endif
}

void SingleInstance::unmarkMainWindow(void* window)
{
#ifdef _WIN32
    RemovePropW(static_cast<HWND>(window), windowProperty);
#else
    (void)window;
#endif
}
