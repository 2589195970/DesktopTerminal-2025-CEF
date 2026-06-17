# 安装时按用户输入更新 config.json，始终输出 UTF-8（无 BOM）
param(
    [Parameter(Mandatory = $true)]
    [string]$ConfigPath,

    [Parameter(Mandatory = $true)]
    [string]$Url,

    [Parameter(Mandatory = $true)]
    [string]$ExitPassword,

    [string]$ApiBaseUrl = "",

    [bool]$RequirePassword = $true
)

$ErrorActionPreference = "Stop"

if (-not (Test-Path -LiteralPath $ConfigPath)) {
    Write-Error "配置文件不存在: $ConfigPath"
    exit 1
}

$utf8NoBom = New-Object System.Text.UTF8Encoding $false
$raw = [System.IO.File]::ReadAllText($ConfigPath, $utf8NoBom)

if ($raw.Length -ge 3 -and $raw[0] -eq [char]0xFEFF) {
    $raw = $raw.Substring(1)
}

$json = $raw | ConvertFrom-Json

$json.url = $Url
$json.exitPassword = $ExitPassword

if ($null -eq $json.desktopAuth) {
    $json | Add-Member -NotePropertyName desktopAuth -Value ([pscustomobject]@{})
}

$json.desktopAuth.apiBaseUrl = $ApiBaseUrl
$json | Add-Member -NotePropertyName sensitiveOperationRequirePassword -Value $RequirePassword -Force

$out = $json | ConvertTo-Json -Depth 10
[System.IO.File]::WriteAllText($ConfigPath, $out, $utf8NoBom)
exit 0
