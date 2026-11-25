param(
    [Parameter(Mandatory = $true)]
    [string]$DataDirectory,

    [Parameter(Mandatory = $true)]
    [string]$AppPath
)

$ErrorActionPreference = "Stop"

$directory = Get-Item -LiteralPath $DataDirectory
if ($directory.Attributes -band [System.IO.FileAttributes]::ReparsePoint) {
    throw "Invalid directory for terminal service data"
}

# 服务数据目录由管理员组拥有，仅系统和管理员可访问。
$acl = $directory.GetAccessControl()
$acl.SetSecurityDescriptorSddlForm(
    "O:BAD:P(A;OICI;FA;;;SY)(A;OICI;FA;;;BA)",
    ([System.Security.AccessControl.AccessControlSections]::Owner -bor
     [System.Security.AccessControl.AccessControlSections]::Access))
$directory.SetAccessControl($acl)

$iniPath = Join-Path $DataDirectory "service.ini"
$tempPath = Join-Path $DataDirectory ([System.IO.Path]::GetRandomFileName())
$backupPath = "$tempPath.bak"
$lines = @(
    "[server]",
    "url=$env:DTCEF_TERMINAL_URL",
    "slid=$env:DTCEF_TERMINAL_SLID",
    "[app]",
    "path=$AppPath"
)
$encoding = New-Object System.Text.UTF8Encoding -ArgumentList $false

try {
    [System.IO.File]::WriteAllLines($tempPath, [string[]]$lines, $encoding)
    if ([System.IO.File]::Exists($iniPath)) {
        [System.IO.File]::Replace($tempPath, $iniPath, $backupPath)
        [System.IO.File]::Delete($backupPath)
    } else {
        [System.IO.File]::Move($tempPath, $iniPath)
    }
} finally {
    [System.IO.File]::Delete($tempPath)
}
