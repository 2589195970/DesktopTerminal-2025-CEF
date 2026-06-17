# Patch config.json with user-supplied install values. Always outputs UTF-8 no BOM.
# Called by installer with: -WorkDir "$PLUGINSDIR"
# Reads: WorkDir\dtcef-config-path.txt  (single line: full path to config.json)
#        WorkDir\dtcef-install-params.txt (4 lines: url, password, apiBaseUrl, requirePassword)
param(
    [Parameter(Mandatory = $true)]
    [string]$WorkDir
)

$ErrorActionPreference = "Stop"

$configPathFile = Join-Path $WorkDir "dtcef-config-path.txt"
$paramsFile = Join-Path $WorkDir "dtcef-install-params.txt"

$ConfigPath = (Get-Content -LiteralPath $configPathFile -Raw -ErrorAction Stop).Trim()
$lines = @(Get-Content -LiteralPath $paramsFile -ErrorAction Stop)

if ($lines.Count -lt 4) {
    Write-Error "Invalid params file (expected 4 lines, got $($lines.Count))"
    exit 1
}

$Url = $lines[0]
$ExitPassword = $lines[1]
$ApiBaseUrl = $lines[2]
$RequirePassword = [System.Convert]::ToBoolean($lines[3])

if (-not (Test-Path -LiteralPath $ConfigPath)) {
    Write-Error "Config not found: $ConfigPath"
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

$logPath = Join-Path ([System.IO.Path]::GetDirectoryName($ConfigPath)) "install-config-patch.log"
$logLine = "{0} | url={1} apiBaseUrl={2} requirePassword={3}" -f (Get-Date -Format "yyyy-MM-dd HH:mm:ss"), $Url, $ApiBaseUrl, $RequirePassword
Add-Content -LiteralPath $logPath -Value $logLine -Encoding UTF8

$verify = [System.IO.File]::ReadAllText($ConfigPath, $utf8NoBom)
if ($verify.Contains($Url)) {
    Write-Host "VERIFY_OK"
    exit 0
} else {
    Write-Host "VERIFY_FAIL"
    exit 2
}
