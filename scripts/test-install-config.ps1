$ErrorActionPreference = "Stop"

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
            throw "Config patch failed for mode ${mode}: $(Get-Content -LiteralPath $logPath -Tail 1)"
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
} finally {
    Remove-Item -LiteralPath $testDir -Recurse -Force
}
