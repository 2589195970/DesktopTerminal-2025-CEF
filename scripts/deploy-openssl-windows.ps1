# Windows 构建/打包时复制 OpenSSL 运行时，供 Qt QNetworkAccessManager HTTPS 使用
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
}
$searchRoots += @(
    "C:\Qt\Tools\OpenSSL\$opensslSubdir\bin",
    "${env:ProgramFiles}\OpenSSL-Win64\bin",
    "${env:ProgramFiles(x86)}\OpenSSL-Win32\bin"
)

$sourceDir = $null
foreach ($root in $searchRoots) {
    if (-not $root) { continue }
    $probe = Join-Path $root $dllNames[0]
    if (Test-Path -LiteralPath $probe) {
        $sourceDir = $root
        break
    }
}

if (-not $sourceDir) {
    Write-Host "[INFO] 未找到预装 OpenSSL，尝试 aqt 安装 tools_openssl..."
    $aqtOutput = if ($env:Qt5_Dir) {
        (Resolve-Path (Join-Path $env:Qt5_Dir "..\..\")).Path
    } else {
        "C:\Qt"
    }
    $toolName = if ($Arch -eq "x86") { "tools_openssl_x86" } else { "tools_openssl_x64" }
    python -m aqt install-tool windows desktop $toolName -O $aqtOutput
    $sourceDir = Join-Path $aqtOutput "Tools\OpenSSL\$opensslSubdir\bin"
    if (-not (Test-Path -LiteralPath (Join-Path $sourceDir $dllNames[0]))) {
        Write-Error "OpenSSL 安装后仍未找到 DLL: $sourceDir"
    }
}

if (-not (Test-Path -LiteralPath $TargetDir)) {
    New-Item -ItemType Directory -Force -Path $TargetDir | Out-Null
}

foreach ($name in $dllNames) {
    $src = Join-Path $sourceDir $name
    if (-not (Test-Path -LiteralPath $src)) {
        Write-Error "缺少 OpenSSL DLL: $src"
    }
    Copy-Item -LiteralPath $src -Destination (Join-Path $TargetDir $name) -Force
    $size = (Get-Item -LiteralPath $src).Length
    Write-Host "[OK] OpenSSL: $name -> $TargetDir ($([math]::Round($size/1KB, 1)) KB)"
}

Write-Host "[SUCCESS] OpenSSL 运行时已部署到 $TargetDir"
