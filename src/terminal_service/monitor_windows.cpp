#include "monitor_windows.h"
#include "win_handle.h"

#include <aclapi.h>
#include <shlobj.h>

#include <QDir>

namespace terminal {
namespace {

QString commonDataDirectory(const QString &name)
{
    wchar_t path[MAX_PATH]{};
    if (FAILED(SHGetFolderPathW(nullptr, CSIDL_COMMON_APPDATA, nullptr, SHGFP_TYPE_CURRENT, path))) {
        return {};
    }
    return QDir(QString::fromWCharArray(path)).filePath(name);
}

bool isSystemOrAdministrator(PSID sid)
{
    return sid && (IsWellKnownSid(sid, WinLocalSystemSid) || IsWellKnownSid(sid, WinBuiltinAdministratorsSid));
}

} // namespace

QString registrationStatusDirectory()
{
    return commonDataDirectory(QStringLiteral("UMTerminalStatus"));
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
    return isSystemOrAdministrator(owner)
            && GetSecurityDescriptorControl(descriptor, &control, &revision) && (control & SE_DACL_PROTECTED);
}

WinHandle openProtectedServiceDataDirectory(QString &path)
{
    path = commonDataDirectory(QStringLiteral("UMTerminal"));
    if (path.isEmpty()) {
        return WinHandle();
    }
    const std::wstring nativePath = QDir::toNativeSeparators(path).toStdWString();
    // 保存期间保持目录打开，防止目录被移动或替换。
    WinHandle directory(CreateFileW(nativePath.c_str(), READ_CONTROL | FILE_READ_ATTRIBUTES,
        FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, nullptr));
    BY_HANDLE_FILE_INFORMATION info{};
    if (!directory || !GetFileInformationByHandle(directory.get(), &info)
            || !(info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) || (info.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)) {
        return WinHandle();
    }
    PSID owner = nullptr;
    PACL dacl = nullptr;
    PSECURITY_DESCRIPTOR descriptor = nullptr;
    if (GetSecurityInfo(directory.get(), SE_FILE_OBJECT, OWNER_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION,
                        &owner, nullptr, &dacl, nullptr, &descriptor) != ERROR_SUCCESS) {
        return WinHandle();
    }
    const LocalMemory security(descriptor, LocalFree);
    SECURITY_DESCRIPTOR_CONTROL control{};
    DWORD revision = 0;
    if (!isSystemOrAdministrator(owner) || !dacl
            || !GetSecurityDescriptorControl(descriptor, &control, &revision) || !(control & SE_DACL_PROTECTED)) {
        return WinHandle();
    }
    for (DWORD index = 0; index < dacl->AceCount; ++index) {
        void *entry = nullptr;
        if (!GetAce(dacl, index, &entry) || static_cast<ACE_HEADER *>(entry)->AceType != ACCESS_ALLOWED_ACE_TYPE
                || !isSystemOrAdministrator(&static_cast<ACCESS_ALLOWED_ACE *>(entry)->SidStart)) {
            return WinHandle();
        }
    }
    return directory;
}

} // namespace terminal
