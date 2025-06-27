#!/usr/bin/env pwsh

# Support both parameter and environment variable modes
param(
    [string]$OutputPath,
    [string]$Architecture,
    [string]$Platform,
    [string]$CefVersion,
    [string]$QtVersion,
    [string]$BuildType,
    [string]$CommitSha
)

# Use environment variables if parameters are not provided (GitHub Actions compatibility)
if (-not $OutputPath) { $OutputPath = $env:BUILD_OUTPUT_PATH }
if (-not $Architecture) { $Architecture = $env:BUILD_ARCH }
if (-not $Platform) { $Platform = $env:BUILD_PLATFORM }
if (-not $CefVersion) { $CefVersion = $env:BUILD_CEF_VERSION }
if (-not $QtVersion) { $QtVersion = $env:BUILD_QT_VERSION }
if (-not $BuildType) { $BuildType = $env:BUILD_TYPE_INFO }
if (-not $CommitSha) { $CommitSha = $env:BUILD_COMMIT_SHA }

# Validate required values
if (-not $OutputPath) {
    Write-Error "OutputPath is required (parameter or BUILD_OUTPUT_PATH environment variable)"
    exit 1
}

# Generate build information using array method (most reliable)
$buildInfo = @(
    "DesktopTerminal-CEF Build Information"
    "===================================="
    "Architecture: $Architecture"
    "Platform: $Platform"
    "CEF Version: $CefVersion"
    "Qt Version: $QtVersion"
    "Build Type: $BuildType"
    "Build Date: $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss UTC')"
    "Commit: $CommitSha"
)

# Write to output file
$buildInfo | Set-Content -Encoding UTF8 -Path $OutputPath

Write-Host "✅ 构建信息文件已生成: $OutputPath" 