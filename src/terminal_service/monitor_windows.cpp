#include "monitor_windows.h"
#include "win_handle.h"

#include <aclapi.h>
#include <shlobj.h>

#include <QDir>

namespace terminal {

QString registrationStatusDirectory()
{
    wchar_t path[MAX_PATH]{};
    if (FAILED(SHGetFolderPathW(nullptr, CSIDL_COMMON_APPDATA, nullptr, SHGFP_TYPE_CURRENT, path))) {
        return {};
    }
    return QDir(QString::fromWCharArray(path)).filePath(QStringLiteral("UMTerminalStatus"));
}

bool registrationStatusDirectoryIsTrusted(const QString &path)
{
    if (path.isEmpty()) {
        return false;
    }
    const std::wstring nativePath = QDir::toNativeSeparators(path).toStdWString();
    const WinHandle directory(CreateFileW(nativePath.c_str(), READ_CONTROL,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, nullptr));
    BY_HANDLE_FILE_INFORMATION info{};
    if (!directory || !GetFileInformationByHandle(directory.get(), &info)
            || !(info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) || (info.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)) {
        return false;
    }
    PSID owner = nullptr;
    PSECURITY_DESCRIPTOR descriptor = nullptr;
    if (GetSecurityInfo(directory.get(), SE_FILE_OBJECT, OWNER_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION,
                        &owner, nullptr, nullptr, nullptr, &descriptor) != ERROR_SUCCESS) {
        return false;
    }
    const LocalMemory security(descriptor, LocalFree);
    SECURITY_DESCRIPTOR_CONTROL control{};
    DWORD revision = 0;
    return owner && (IsWellKnownSid(owner, WinLocalSystemSid) || IsWellKnownSid(owner, WinBuiltinAdministratorsSid))
            && GetSecurityDescriptorControl(descriptor, &control, &revision) && (control & SE_DACL_PROTECTED);
}

} // namespace terminal
