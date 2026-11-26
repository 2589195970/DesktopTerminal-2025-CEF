#ifndef MONITOR_WINDOWS_H
#define MONITOR_WINDOWS_H

#include <QString>

#include "win_handle.h"

namespace terminal {

inline constexpr wchar_t TerminalServiceName[] = L"UMTerminalService";
inline constexpr wchar_t ExamAppExecutable[] = L"DesktopTerminal-CEF.exe";
// 服务自定义控制码，状态托盘以普通用户权限请求立即注册或发送心跳。
inline constexpr unsigned long TerminalRefreshControl = 128;
QString registrationStatusDirectory();
bool registrationStatusDirectoryIsTrusted(const QString &path);
WinHandle openProtectedServiceDataDirectory(QString &path);

} // namespace terminal

#endif
