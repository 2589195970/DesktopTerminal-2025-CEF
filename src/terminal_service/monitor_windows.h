#ifndef MONITOR_WINDOWS_H
#define MONITOR_WINDOWS_H

#include <QString>

#include "win_handle.h"

namespace terminal {

inline constexpr wchar_t TerminalServiceName[] = L"UMTerminalService";
QString registrationStatusDirectory();
bool registrationStatusDirectoryIsTrusted(const QString &path);
WinHandle openProtectedServiceDataDirectory(QString &path);

} // namespace terminal

#endif
