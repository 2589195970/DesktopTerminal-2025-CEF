$ErrorActionPreference = "Stop"

$serviceConfigScript = Join-Path $PSScriptRoot "write-terminal-service-config.ps1"
$serviceConfigBytes = [IO.File]::ReadAllBytes($serviceConfigScript)
if ($serviceConfigBytes.Length -lt 3 -or [BitConverter]::ToString($serviceConfigBytes, 0, 3) -ne "EF-BB-BF") {
    throw "write-terminal-service-config.ps1 requires a UTF-8 BOM for Windows PowerShell."
}

$testDir = Join-Path ([IO.Path]::GetTempPath()) ("dtcef install config " + [Guid]::NewGuid())
[void][IO.Directory]::CreateDirectory($testDir)

try {
    $templatePath = Join-Path (Split-Path $PSScriptRoot -Parent) "resources\config.json"
    $templateText = [IO.File]::ReadAllText($templatePath)
    $template = ConvertFrom-Json -InputObject $templateText
    $configPath = Join-Path $testDir "config.json"
    $overridePath = Join-Path $testDir "config.override.ini"
    $logPath = Join-Path $testDir "install-config-patch.log"

    $env:DTCEF_CONFIG_URL = "https://exam.example.invalid/stu?Client='ExamTerminal'&a=1"
    $env:DTCEF_CONFIG_PASSWORD = "Test-Only"
    $env:DTCEF_CONFIG_REQUIRE_PASSWORD = "true"
    $env:DTCEF_CONFIG_API_BASE_URL = "https://backend.example.invalid/api"

    foreach ($mode in @("multi", "single", "auto")) {
        $env:DTCEF_CONFIG_PROCESS_MODE = $mode
        if ($mode -eq "multi") {
            Copy-Item -LiteralPath $templatePath -Destination $configPath
        } elseif ($mode -eq "auto") {
            $env:DTCEF_CONFIG_REQUIRE_PASSWORD = "false"
            $env:DTCEF_CONFIG_API_BASE_URL = ""
            $legacy = ConvertFrom-Json -InputObject $templateText
            $legacy | Add-Member -NotePropertyName cefSingleProcessMode -NotePropertyValue $true
            $legacy.cefSettings | Add-Member -NotePropertyName singleProcess -NotePropertyValue $true
            [IO.File]::WriteAllText($configPath, (ConvertTo-Json $legacy), (New-Object System.Text.UTF8Encoding -ArgumentList $true))
        }

        & "$PSScriptRoot\patch-install-config.ps1" -ConfigPath $configPath -OverridePath $overridePath -LogPath $logPath
        if ($LASTEXITCODE -ne 0) {
            throw "Config patch failed for mode ${mode}: $(Get-Content -LiteralPath $logPath -Encoding UTF8 -Tail 1)"
        }

        $encoding = New-Object System.Text.UTF8Encoding -ArgumentList $false, $true
        $json = ConvertFrom-Json -InputObject ($encoding.GetString([IO.File]::ReadAllBytes($configPath)))
        if ($json.appName -cne $template.appName -or
            $json.desktopAuth.clientId -cne $template.desktopAuth.clientId -or
            $json.cefSettings.enableWebSecurity -ne $template.cefSettings.enableWebSecurity -or
            $json.url -cne $env:DTCEF_CONFIG_URL -or
            $json.exitPassword -cne $env:DTCEF_CONFIG_PASSWORD -or
            $json.desktopAuth.apiBaseUrl -cne [string]$env:DTCEF_CONFIG_API_BASE_URL -or
            $json.sensitiveOperationRequirePassword -ne ($env:DTCEF_CONFIG_REQUIRE_PASSWORD -eq "true")) {
            throw "Config fields do not match for mode $mode"
        }
        if ($mode -eq "auto") {
            if ($json.PSObject.Properties["cefSingleProcessMode"] -or
                $json.cefSettings.PSObject.Properties["singleProcess"] -or
                $json.cefSettings.PSObject.Properties["singleProcessMode"]) {
                throw "Auto mode must omit CEF process overrides"
            }
        } elseif ($json.cefSettings.singleProcessMode -ne ($mode -eq "single")) {
            throw "Incorrect CEF process mode: $mode"
        }
        if (-not (Test-Path -LiteralPath $overridePath -PathType Leaf)) {
            throw "Missing override file for mode $mode"
        }
        Write-Host "Config patch passed for mode $mode"
    }

    $env:DTCEF_CONFIG_URL = ""
    Copy-Item -LiteralPath $templatePath -Destination $configPath -Force
    & "$PSScriptRoot\patch-install-config.ps1" -ConfigPath $configPath -OverridePath $overridePath -LogPath $logPath
    $json = ConvertFrom-Json -InputObject ([IO.File]::ReadAllText($configPath))
    if ($LASTEXITCODE -ne 0 -or $json.url -cne $template.url) {
        throw "Installation without an exam URL must use the template URL."
    }
    Write-Host "Config patch passed with the template URL"

    $env:DTCEF_CONFIG_PASSWORD = -join [char[]](0x8003, 0x8BD5, 0x5BC6, 0x7801)
    $configLock = [IO.File]::Open($configPath, [IO.FileMode]::Open, [IO.FileAccess]::Read, [IO.FileShare]::Read)
    try {
        & "$PSScriptRoot\patch-install-config.ps1" -ConfigPath $configPath -OverridePath $overridePath -LogPath $logPath
        if ($LASTEXITCODE -ne 2) {
            throw "A blocked JSON update must report that the override was saved"
        }
    } finally {
        $configLock.Dispose()
    }
    $overrideLines = ($encoding.GetString([IO.File]::ReadAllBytes($overridePath))) -split '\r?\n'
    if ($overrideLines -cnotcontains "exitPassword=$($env:DTCEF_CONFIG_PASSWORD)") {
        throw "The override must preserve the Unicode password as UTF-8"
    }
    Write-Host "UTF-8 override passed with a blocked JSON update"
} finally {
    Remove-Item -LiteralPath $testDir -Recurse -Force
}
