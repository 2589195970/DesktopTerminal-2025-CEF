#!/usr/bin/env pwsh
param(
    [Parameter(Mandatory=$true)]
    [string]$OutputPath,
    
    [Parameter(Mandatory=$true)]
    [string]$Architecture,
    
    [Parameter(Mandatory=$true)]
    [string]$Platform,
    
    [Parameter(Mandatory=$true)]
    [string]$CefVersion,
    
    [Parameter(Mandatory=$true)]
    [string]$QtVersion,
    
    [Parameter(Mandatory=$true)]
    [string]$BuildType,
    
    [Parameter(Mandatory=$true)]
    [string]$CommitSha
)

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