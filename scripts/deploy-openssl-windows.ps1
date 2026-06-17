param(
    [Parameter(Mandatory = $true)]
    [string]$TargetDir,

    [Parameter(Mandatory = $true)]
    [ValidateSet("x64", "x86")]
    [string]$Arch
)

$ErrorActionPreference = "Stop"

if ($Arch -eq "x86") {
    $opensslSubdir = "Win_x86"
    $dllNames = @("libssl-1_1.dll", "libcrypto-1_1.dll")
} else {
    $opensslSubdir = "Win_x64"
    $dllNames = @("libssl-1_1-x64.dll", "libcrypto-1_1-x64.dll")
}

$searchRoots = @()
if ($env:Qt5_Dir) {
    $qtRoot = (Resolve-Path (Join-Path $env:Qt5_Dir "..\..\")).Path
    $searchRoots += (Join-Path $qtRoot "Tools\OpenSSL\$opensslSubdir\bin")
    $searchRoots += (Join-Path $qtRoot "Tools\OpenSSL\Win_x64\bin")
}
$searchRoots += @(
    "C:\Qt\Tools\OpenSSL\$opensslSubdir\bin",
    "C:\Qt\Tools\OpenSSL\Win_x64\bin",
    "${env:ProgramFiles}\OpenSSL-Win64\bin",
    "${env:ProgramFiles(x86)}\OpenSSL-Win32\bin",
    "C:\OpenSSL-Win32\bin",
    "C:\OpenSSL-Win64\bin"
)

$sourceDir = $null
foreach ($root in $searchRoots) {
    if (-not $root) { continue }
    Write-Host "[INFO] Checking: $root"
    $probe = Join-Path $root $dllNames[0]
    if (Test-Path -LiteralPath $probe) {
        $sourceDir = $root
        Write-Host "[OK] Found OpenSSL at: $root"
        break
    }
}

if (-not $sourceDir) {
    Write-Host "[INFO] No pre-installed OpenSSL found, trying aqt install..."
    $aqtOutput = if ($env:Qt5_Dir) {
        (Resolve-Path (Join-Path $env:Qt5_Dir "..\..\")).Path
    } else {
        "C:\Qt"
    }

    $toolCandidates = @("tools_openssl_$Arch", "tools_openssl_x64", "tools_openssl")
    foreach ($toolName in $toolCandidates) {
        Write-Host "[INFO] Trying aqt tool: $toolName"
        $result = python -m aqt install-tool windows desktop $toolName -O $aqtOutput 2>&1
        Write-Host $result
        $probe = Join-Path $aqtOutput "Tools\OpenSSL\$opensslSubdir\bin\$($dllNames[0])"
        if (Test-Path -LiteralPath $probe) {
            $sourceDir = Join-Path $aqtOutput "Tools\OpenSSL\$opensslSubdir\bin"
            Write-Host "[OK] Installed via aqt ($toolName)"
            break
        }
        $probe64 = Join-Path $aqtOutput "Tools\OpenSSL\Win_x64\bin\$($dllNames[0])"
        if (Test-Path -LiteralPath $probe64) {
            $sourceDir = Join-Path $aqtOutput "Tools\OpenSSL\Win_x64\bin"
            Write-Host "[OK] Installed via aqt ($toolName) at Win_x64 path"
            break
        }
    }
}

if (-not $sourceDir) {
    Write-Host "[WARN] OpenSSL runtime not available for $Arch. Qt HTTPS (QNetworkAccessManager) will not work."
    Write-Host "[WARN] CEF browser HTTPS is unaffected (uses its own SSL)."
    Write-Host "[WARN] Desktop auth will retry in background; manual apiBaseUrl config may be needed."
    exit 0
}

if (-not (Test-Path -LiteralPath $TargetDir)) {
    New-Item -ItemType Directory -Force -Path $TargetDir | Out-Null
}

$copied = 0
foreach ($name in $dllNames) {
    $src = Join-Path $sourceDir $name
    if (-not (Test-Path -LiteralPath $src)) {
        Write-Host "[WARN] Missing: $src"
        continue
    }
    Copy-Item -LiteralPath $src -Destination (Join-Path $TargetDir $name) -Force
    $size = (Get-Item -LiteralPath $src).Length
    Write-Host "[OK] $name -> $TargetDir ($([math]::Round($size/1KB, 1)) KB)"
    $copied++
}

if ($copied -gt 0) {
    Write-Host "[SUCCESS] OpenSSL deployed ($copied DLLs) to $TargetDir"
} else {
    Write-Host "[WARN] No OpenSSL DLLs were copied"
}
exit 0
