#include "cef_app_impl.h"
#include "include/cef_app.h"

#ifdef Q_OS_WIN
#include <windows.h>
#endif

int main(int argc, char* argv[])
{
#ifdef Q_OS_WIN
    CefMainArgs mainArgs(GetModuleHandle(nullptr));
#else
    CefMainArgs mainArgs(argc, argv);
#endif

    CefRefPtr<CEFApp> app = new CEFApp();
    return CefExecuteProcess(mainArgs, app, nullptr);
}
