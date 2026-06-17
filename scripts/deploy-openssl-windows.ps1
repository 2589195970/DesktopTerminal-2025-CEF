# Best-effort OpenSSL deployment for Qt HTTPS. Non-fatal: always exits 0.
param(
    [Parameter(Mandatory = $true)]
    [string]$TargetDir,

    [Parameter(Mandatory = $true)]
    [ValidateSet("x64", "x86")]
    [string]$Arch
)

if ($Arch -eq "x86") {
    $opensslSubdir = "Win_x86"
    $dllNames = @("libssl-1_1.dll", "libcrypto-1_1.dll")
    $fallbackNames = @("libssl-1_1.dll", "libcrypto-1_1.dll")
} else {
    $opensslSubdir = "Win_x64"
    $dllNames = @("libssl-1_1-x64.dll", "libcrypto-1_1-x64.dll")
    $fallbackNames = @("libssl-1_1-x64.dll", "libcrypto-1_1-x64.dll")
}

$searchPaths = @()
if ($env:Qt5_Dir) {
    $qtRoot = (Resolve-Path (Join-Path $env:Qt5_Dir "..\..\") -ErrorAction SilentlyContinue).Path
    if ($qtRoot) {
        $searchPaths += (Join-Path $qtRoot "Tools\OpenSSL\$opensslSubdir\bin")
        $searchPaths += (Join-Path $qtRoot "Tools\OpenSSL\Win_x64\bin")
    }
}
$searchPaths += @(
    "C:\Program Files\OpenSSL\bin",
    "C:\Program Files\OpenSSL-Win64\bin",
    "${env:ProgramFiles(x86)}\OpenSSL-Win32\bin",
    "C:\OpenSSL-Win64\bin",
    "C:\OpenSSL-Win32\bin",
    "C:\Qt\Tools\OpenSSL\$opensslSubdir\bin",
    "C:\Qt\Tools\OpenSSL\Win_x64\bin"
)

$sourceDir = $null
foreach ($p in $searchPaths) {
    if (-not $p -or -not (Test-Path -LiteralPath $p -ErrorAction SilentlyContinue)) { continue }
    foreach ($name in ($dllNames + $fallbackNames) | Select-Object -Unique) {
        if (Test-Path -LiteralPath (Join-Path $p $name) -ErrorAction SilentlyContinue) {
            $sourceDir = $p
            Write-Host "[OK] Found OpenSSL at: $p"
            break
        }
    }
    if ($sourceDir) { break }
}

if (-not $sourceDir) {
    Write-Host "[INFO] Searching entire disk for OpenSSL 1.1 DLLs..."
    $hit = Get-ChildItem -Path "C:\" -Recurse -Filter $dllNames[0] -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($hit) {
        $sourceDir = $hit.DirectoryName
        Write-Host "[OK] Found via disk scan: $sourceDir"
    }
}

if (-not $sourceDir) {
    Write-Host "[WARN] OpenSSL 1.1 runtime not found for $Arch."
    Write-Host "[WARN] Qt HTTPS (QNetworkAccessManager) will not work."
    Write-Host "[WARN] CEF browser HTTPS is unaffected. Desktop auth retries in background."
    exit 0
}

if (-not (Test-Path -LiteralPath $TargetDir -ErrorAction SilentlyContinue)) {
    New-Item -ItemType Directory -Force -Path $TargetDir -ErrorAction SilentlyContinue | Out-Null
}

$copied = 0
foreach ($name in ($dllNames + $fallbackNames) | Select-Object -Unique) {
    $src = Join-Path $sourceDir $name
    if (-not (Test-Path -LiteralPath $src -ErrorAction SilentlyContinue)) { continue }
    try {
        Copy-Item -LiteralPath $src -Destination (Join-Path $TargetDir $name) -Force
        $size = (Get-Item -LiteralPath $src).Length
        Write-Host "[OK] $name ($([math]::Round($size/1KB, 1)) KB)"
        $copied++
    } catch {
        Write-Host "[WARN] Failed to copy $name : $_"
    }
}

if ($copied -gt 0) {
    Write-Host "[SUCCESS] OpenSSL: $copied DLLs deployed to $TargetDir"
} else {
    Write-Host "[WARN] No OpenSSL DLLs copied"
}
exit 0
