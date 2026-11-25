#ifndef MONITOR_WINDOWS_H
#define MONITOR_WINDOWS_H

#include <QString>

namespace terminal {

inline constexpr wchar_t TerminalServiceName[] = L"UMTerminalService";
QString registrationStatusDirectory();
bool registrationStatusDirectoryIsTrusted(const QString &path);

} // namespace terminal

#endif
