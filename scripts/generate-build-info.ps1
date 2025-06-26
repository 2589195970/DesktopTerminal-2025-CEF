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

# Generate build information using simple Write-Output commands
Write-Output "DesktopTerminal-CEF Build Information" | Out-File -FilePath $OutputPath -Encoding UTF8
Write-Output "====================================" | Out-File -FilePath $OutputPath -Append -Encoding UTF8
Write-Output "Architecture: $Architecture" | Out-File -FilePath $OutputPath -Append -Encoding UTF8
Write-Output "Platform: $Platform" | Out-File -FilePath $OutputPath -Append -Encoding UTF8
Write-Output "CEF Version: $CefVersion" | Out-File -FilePath $OutputPath -Append -Encoding UTF8
Write-Output "Qt Version: $QtVersion" | Out-File -FilePath $OutputPath -Append -Encoding UTF8
Write-Output "Build Type: $BuildType" | Out-File -FilePath $OutputPath -Append -Encoding UTF8
Write-Output "Build Date: $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss UTC')" | Out-File -FilePath $OutputPath -Append -Encoding UTF8
Write-Output "Commit: $CommitSha" | Out-File -FilePath $OutputPath -Append -Encoding UTF8

Write-Host "✅ 构建信息文件已生成: $OutputPath" 