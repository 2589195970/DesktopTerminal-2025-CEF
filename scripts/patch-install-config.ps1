# Patch config.json with user-supplied values during install. Always outputs UTF-8 no BOM.
param(
    [Parameter(Mandatory = $true)]
    [string]$ConfigPath,

    [string]$ParamsPath = "",

    [string]$Url = "",

    [string]$ExitPassword = "",

    [string]$ApiBaseUrl = "",

    [bool]$RequirePassword = $true
)

$ErrorActionPreference = "Stop"

function Write-InstallLog {
    param([string]$Message)
    $logPath = Join-Path ([System.IO.Path]::GetDirectoryName($ConfigPath)) "install-config-patch.log"
    $line = "{0} | {1}" -f (Get-Date -Format "yyyy-MM-dd HH:mm:ss"), $Message
    Add-Content -LiteralPath $logPath -Value $line -Encoding UTF8
}

if ($ParamsPath -ne "" -and (Test-Path -LiteralPath $ParamsPath)) {
    $lines = Get-Content -LiteralPath $ParamsPath -Encoding UTF8
    if ($lines.Count -lt 4) {
        Write-Error "Invalid params file: $ParamsPath (expected 4 lines, got $($lines.Count))"
        exit 1
    }
    $Url = $lines[0]
    $ExitPassword = $lines[1]
    $ApiBaseUrl = $lines[2]
    $RequirePassword = [System.Convert]::ToBoolean($lines[3])
}

if (-not (Test-Path -LiteralPath $ConfigPath)) {
    Write-Error "Config file not found: $ConfigPath"
    exit 1
}

if ([string]::IsNullOrWhiteSpace($Url)) {
    Write-Error "URL is empty"
    exit 1
}

$utf8NoBom = New-Object System.Text.UTF8Encoding $false
$raw = [System.IO.File]::ReadAllText($ConfigPath, $utf8NoBom)

if ($raw.Length -ge 1 -and $raw[0] -eq [char]0xFEFF) {
    $raw = $raw.Substring(1)
}

$json = $raw | ConvertFrom-Json

$json.url = $Url
$json.exitPassword = $ExitPassword

if ($null -eq $json.desktopAuth) {
    $json | Add-Member -NotePropertyName desktopAuth -Value ([pscustomobject]@{
        clientId = "DesktopTerminal-CEF"
        clientSecret = "DesktopAuthKey"
        authEndpoint = ""
        apiBaseUrl = ""
    })
}

$json.desktopAuth.apiBaseUrl = $ApiBaseUrl
$json | Add-Member -NotePropertyName sensitiveOperationRequirePassword -Value $RequirePassword -Force

$out = $json | ConvertTo-Json -Depth 10
[System.IO.File]::WriteAllText($ConfigPath, $out, $utf8NoBom)

Write-InstallLog "config.json updated url=$Url apiBaseUrl=$ApiBaseUrl requirePassword=$RequirePassword"
exit 0
