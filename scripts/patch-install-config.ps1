param(
    [Parameter(Mandatory = $true)]
    [string]$ConfigPath,

    [Parameter(Mandatory = $true)]
    [string]$OverridePath,

    [Parameter(Mandatory = $true)]
    [string]$LogPath
)

$ErrorActionPreference = "Stop"

function Get-Utf8NoBomEncoding {
    return New-Object System.Text.UTF8Encoding -ArgumentList $false
}

function Write-InstallLog {
    param([string]$Message)

    $encoding = Get-Utf8NoBomEncoding
    $logDir = [System.IO.Path]::GetDirectoryName($LogPath)
    if ($logDir -and -not [System.IO.Directory]::Exists($logDir)) {
        [System.IO.Directory]::CreateDirectory($logDir) | Out-Null
    }

    $line = ("{0} {1}{2}" -f (Get-Date -Format "yyyy-MM-dd HH:mm:ss"), $Message, [Environment]::NewLine)
    [System.IO.File]::AppendAllText($LogPath, $line, $encoding)
}

function Get-RequiredEnv {
    param(
        [string]$Name,
        [string]$Label
    )

    $value = [Environment]::GetEnvironmentVariable($Name)
    if ($null -eq $value -or $value.Trim().Length -eq 0) {
        throw "$Label is empty"
    }

    return $value
}

function Get-OptionalEnv {
    param([string]$Name)

    $value = [Environment]::GetEnvironmentVariable($Name)
    if ($null -eq $value) {
        return ""
    }

    return $value
}

function Convert-ToBooleanValue {
    param([string]$Value)

    if ($null -eq $Value) {
        return $true
    }

    $normalized = $Value.Trim().ToLowerInvariant()
    return ($normalized -eq "1" -or $normalized -eq "true" -or $normalized -eq "yes" -or $normalized -eq "on")
}

function Read-TextFileUtf8 {
    param([string]$Path)

    if (-not [System.IO.File]::Exists($Path)) {
        throw "Config file not found: $Path"
    }

    $bytes = [System.IO.File]::ReadAllBytes($Path)
    $offset = 0
    if ($bytes.Length -ge 3 -and $bytes[0] -eq 0xEF -and $bytes[1] -eq 0xBB -and $bytes[2] -eq 0xBF) {
        $offset = 3
    }

    $strictUtf8 = New-Object System.Text.UTF8Encoding -ArgumentList $false, $true
    try {
        return $strictUtf8.GetString($bytes, $offset, $bytes.Length - $offset)
    } catch {
        $gb18030 = [System.Text.Encoding]::GetEncoding(54936)
        return $gb18030.GetString($bytes, $offset, $bytes.Length - $offset)
    }
}

function Write-TextFileUtf8Safely {
    param(
        [string]$Path,
        [string]$Text
    )

    $dir = [System.IO.Path]::GetDirectoryName($Path)
    if ($dir -and -not [System.IO.Directory]::Exists($dir)) {
        [System.IO.Directory]::CreateDirectory($dir) | Out-Null
    }

    $tempPath = $Path + ".tmp"
    $backupPath = $Path + ".bak"

    if ([System.IO.File]::Exists($tempPath)) {
        [System.IO.File]::Delete($tempPath)
    }
    if ([System.IO.File]::Exists($backupPath)) {
        [System.IO.File]::Delete($backupPath)
    }

    try {
        [System.IO.File]::WriteAllText($tempPath, $Text, (Get-Utf8NoBomEncoding))
        [void](Read-TextFileUtf8 $tempPath)

        if ([System.IO.File]::Exists($Path)) {
            [System.IO.File]::Replace($tempPath, $Path, $backupPath, $true)
            if ([System.IO.File]::Exists($backupPath)) {
                [System.IO.File]::Delete($backupPath)
            }
        } else {
            [System.IO.File]::Move($tempPath, $Path)
        }
    } catch {
        if ([System.IO.File]::Exists($tempPath)) {
            [System.IO.File]::Delete($tempPath)
        }
        throw
    }
}

function Format-JsonText {
    param([string]$Json)

    $builder = New-Object System.Text.StringBuilder
    $indent = 0
    $inString = $false
    $escaped = $false
    $indentText = "  "

    for ($i = 0; $i -lt $Json.Length; $i++) {
        $ch = $Json[$i]

        if ($inString) {
            [void]$builder.Append($ch)
            if ($escaped) {
                $escaped = $false
            } elseif ($ch -eq "\") {
                $escaped = $true
            } elseif ($ch -eq '"') {
                $inString = $false
            }
            continue
        }

        switch ($ch) {
            '"' {
                $inString = $true
                [void]$builder.Append($ch)
            }
            "{" {
                [void]$builder.Append($ch)
                [void]$builder.Append([Environment]::NewLine)
                $indent++
                [void]$builder.Append($indentText * $indent)
            }
            "[" {
                [void]$builder.Append($ch)
                [void]$builder.Append([Environment]::NewLine)
                $indent++
                [void]$builder.Append($indentText * $indent)
            }
            "}" {
                [void]$builder.Append([Environment]::NewLine)
                $indent--
                [void]$builder.Append($indentText * $indent)
                [void]$builder.Append($ch)
            }
            "]" {
                [void]$builder.Append([Environment]::NewLine)
                $indent--
                [void]$builder.Append($indentText * $indent)
                [void]$builder.Append($ch)
            }
            "," {
                [void]$builder.Append($ch)
                [void]$builder.Append([Environment]::NewLine)
                [void]$builder.Append($indentText * $indent)
            }
            ":" {
                [void]$builder.Append(": ")
            }
            default {
                if (-not [char]::IsWhiteSpace($ch)) {
                    [void]$builder.Append($ch)
                }
            }
        }
    }

    return $builder.ToString()
}

function Set-DictionaryValue {
    param(
        [System.Collections.IDictionary]$Dictionary,
        [string]$Key,
        $Value
    )

    if ($Dictionary.Contains($Key)) {
        $Dictionary[$Key] = $Value
    } else {
        $Dictionary.Add($Key, $Value)
    }
}

function Remove-DictionaryValue {
    param(
        [System.Collections.IDictionary]$Dictionary,
        [string]$Key
    )

    if ($Dictionary.Contains($Key)) {
        $Dictionary.Remove($Key)
    }
}

function Get-DesktopAuthDictionary {
    param([System.Collections.IDictionary]$Config)

    if ($Config.Contains("desktopAuth") -and $Config["desktopAuth"] -is [System.Collections.IDictionary]) {
        return $Config["desktopAuth"]
    }

    $desktopAuth = New-Object System.Collections.Hashtable
    Set-DictionaryValue $Config "desktopAuth" $desktopAuth
    return $desktopAuth
}

function Get-CefSettingsDictionary {
    param([System.Collections.IDictionary]$Config)

    if ($Config.Contains("cefSettings") -and $Config["cefSettings"] -is [System.Collections.IDictionary]) {
        return $Config["cefSettings"]
    }

    $cefSettings = New-Object System.Collections.Hashtable
    Set-DictionaryValue $Config "cefSettings" $cefSettings
    return $cefSettings
}

function Normalize-CefProcessMode {
    param([string]$Value)

    if ($null -eq $Value -or $Value.Trim().Length -eq 0) {
        return "auto"
    }

    $normalized = $Value.Trim().ToLowerInvariant()
    if ($normalized -eq "single" -or $normalized -eq "single-process" -or $normalized -eq "true" -or $normalized -eq "1") {
        return "single"
    }
    if ($normalized -eq "multi" -or $normalized -eq "multi-process" -or $normalized -eq "false" -or $normalized -eq "0") {
        return "multi"
    }
    if ($normalized -eq "auto" -or $normalized -eq "default") {
        return "auto"
    }

    throw "Invalid CEF process mode: $Value"
}

function Apply-CefProcessMode {
    param(
        [System.Collections.IDictionary]$Config,
        [string]$ProcessMode
    )

    Remove-DictionaryValue $Config "cefSingleProcessMode"

    $cefSettings = Get-CefSettingsDictionary $Config
    if ($ProcessMode -eq "auto") {
        Remove-DictionaryValue $cefSettings "singleProcessMode"
        Remove-DictionaryValue $cefSettings "singleProcess"
        return
    }

    Set-DictionaryValue $cefSettings "singleProcessMode" ($ProcessMode -eq "single")
    Remove-DictionaryValue $cefSettings "singleProcess"
}

function Write-OverrideConfig {
    param(
        [string]$Url,
        [string]$ExitPassword,
        [string]$ApiBaseUrl,
        [bool]$RequirePassword,
        [string]$ProcessMode
    )

    $overrideDir = [System.IO.Path]::GetDirectoryName($OverridePath)
    if ($overrideDir -and -not [System.IO.Directory]::Exists($overrideDir)) {
        [System.IO.Directory]::CreateDirectory($overrideDir) | Out-Null
    }

    $requirePasswordText = "false"
    if ($RequirePassword) {
        $requirePasswordText = "true"
    }

    $lines = New-Object System.Collections.Generic.List[string]
    $lines.Add("[override]")
    $lines.Add("url=$Url")
    $lines.Add("exitPassword=$ExitPassword")
    $lines.Add("apiBaseUrl=$ApiBaseUrl")
    $lines.Add("sensitiveOperationRequirePassword=$requirePasswordText")

    if ($ProcessMode -ne "auto") {
        $lines.Add("cefProcessMode=$ProcessMode")
    }

    [System.IO.File]::WriteAllLines($OverridePath, [string[]]$lines.ToArray(), (Get-Utf8NoBomEncoding))
}

try {
    Write-InstallLog "config patch started"

    $url = Get-RequiredEnv "DTCEF_CONFIG_URL" "Config URL"
    $exitPassword = Get-RequiredEnv "DTCEF_CONFIG_PASSWORD" "Config password"
    $apiBaseUrl = Get-OptionalEnv "DTCEF_CONFIG_API_BASE_URL"
    $requirePassword = Convert-ToBooleanValue (Get-OptionalEnv "DTCEF_CONFIG_REQUIRE_PASSWORD")
    $processMode = Normalize-CefProcessMode (Get-OptionalEnv "DTCEF_CONFIG_PROCESS_MODE")

    Add-Type -AssemblyName System.Web.Extensions
    $serializer = New-Object System.Web.Script.Serialization.JavaScriptSerializer
    $serializer.MaxJsonLength = 1048576

    $jsonText = Read-TextFileUtf8 $ConfigPath
    $config = $serializer.DeserializeObject($jsonText)
    if ($null -eq $config -or -not ($config -is [System.Collections.IDictionary])) {
        throw "Config root must be a JSON object"
    }

    Set-DictionaryValue $config "url" $url
    Set-DictionaryValue $config "exitPassword" $exitPassword
    Set-DictionaryValue $config "sensitiveOperationRequirePassword" $requirePassword

    $desktopAuth = Get-DesktopAuthDictionary $config
    Set-DictionaryValue $desktopAuth "apiBaseUrl" $apiBaseUrl
    Apply-CefProcessMode $config $processMode

    $patchedJson = Format-JsonText ($serializer.Serialize($config))
    Write-TextFileUtf8Safely $ConfigPath ($patchedJson + [Environment]::NewLine)
    Write-OverrideConfig $url $exitPassword $apiBaseUrl $requirePassword $processMode

    $verifyText = Read-TextFileUtf8 $ConfigPath
    $verifyConfig = $serializer.DeserializeObject($verifyText)
    $verifyDesktopAuth = $verifyConfig["desktopAuth"]
    $verifyCefSettings = $verifyConfig["cefSettings"]
    if ($verifyConfig["url"] -ne $url -or
        $verifyConfig["exitPassword"] -ne $exitPassword -or
        $verifyConfig["sensitiveOperationRequirePassword"] -ne $requirePassword -or
        $verifyDesktopAuth["apiBaseUrl"] -ne $apiBaseUrl) {
        throw "Config verification failed"
    }
    if ($processMode -eq "auto") {
        if ($verifyConfig.Contains("cefSingleProcessMode") -or
            ($verifyCefSettings -is [System.Collections.IDictionary] -and $verifyCefSettings.Contains("singleProcessMode"))) {
            throw "CEF process mode verification failed"
        }
    } elseif ($verifyCefSettings["singleProcessMode"] -ne ($processMode -eq "single")) {
        throw "CEF process mode verification failed"
    }

    if (-not [System.IO.File]::Exists($OverridePath)) {
        throw "Override config was not written"
    }

    Write-InstallLog "config patch completed"
    exit 0
} catch {
    try {
        Write-InstallLog ("config patch failed: " + $_.Exception.Message)
    } catch {
    }
    exit 1
}
