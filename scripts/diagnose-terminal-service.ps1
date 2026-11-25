$ErrorActionPreference = 'Stop'

$installDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$dataDir = Join-Path $env:ProgramData 'UMTerminal'
$serviceExe = 'DesktopTerminalService.exe'
$trayExe = 'DesktopTerminalStatus.exe'
$serviceName = 'UMTerminalService'
$sc = Join-Path $env:SystemRoot 'System32\sc.exe'

function Show-FileInfo {
    param([string]$Path)

    try {
        $file = Get-Item -LiteralPath $Path
        $version = $file.VersionInfo.FileVersion
        if ($version) {
            Write-Output ('{0}: {1}' -f $file.FullName, $version)
        } else {
            Write-Output ('{0}: present' -f $file.FullName)
        }
    } catch {
        Write-Output ('{0}: {1}' -f $Path, $_.Exception.Message)
    }
}

function Show-LogTail {
    param([string]$Path, [string]$Pattern)

    Write-Output ('--- {0} ---' -f $Path)
    try {
        if ($Pattern) {
            Get-Content -LiteralPath $Path | Where-Object { $_ -match $Pattern } | Select-Object -Last 25
        } else {
            Get-Content -LiteralPath $Path -Encoding UTF8 | Select-Object -Last 25
        }
    } catch {
        Write-Output $_.Exception.Message
    }
}

function Write-Diagnostic {
    Write-Output ('Terminal service diagnostic - {0} - {1}' -f $env:COMPUTERNAME, (Get-Date -Format 'yyyy-MM-dd HH:mm:ss'))
    Write-Output ('Windows version: {0}' -f [Environment]::OSVersion.Version)
    Write-Output ''
    Write-Output '--- Windows service ---'
    & $sc qc $serviceName
    & $sc query $serviceName
    Show-FileInfo (Join-Path $installDir $serviceExe)
    Show-FileInfo (Join-Path $installDir $trayExe)
    Show-FileInfo (Join-Path $dataDir 'service.ini')
    Show-FileInfo (Join-Path $dataDir 'state.json')

    Write-Output ''
    Write-Output '--- System VC++ runtime files ---'
    $nativeSystem = Join-Path $env:SystemRoot 'System32'
    if ($env:PROCESSOR_ARCHITEW6432) {
        $nativeSystem = Join-Path $env:SystemRoot 'Sysnative'
    }
    foreach ($directory in @($nativeSystem, (Join-Path $env:SystemRoot 'SysWOW64'))) {
        if (Test-Path -LiteralPath $directory -PathType Container) {
            foreach ($dll in @('msvcp140.dll', 'vcruntime140.dll', 'vcruntime140_1.dll')) {
                Show-FileInfo (Join-Path $directory $dll)
            }
        }
    }

    Write-Output ''
    Show-LogTail (Join-Path $installDir 'install.log') '^(VC\+\+|write service\.ini|sc (query|create|config|start)|wait for UMTerminalService|Terminal service (start|will))'
    $serviceLog = Join-Path $dataDir 'logs\terminal-service.log'
    for ($archive = 4; $archive -ge 1; $archive--) {
        $path = '{0}.{1}' -f $serviceLog, $archive
        if (Test-Path -LiteralPath $path -PathType Leaf) {
            Show-LogTail $path
        }
    }
    Show-LogTail $serviceLog

    Write-Output ''
    Write-Output '--- Service Control Manager events (last 7 days) ---'
    try {
        $service = Get-Service -Name $serviceName -ErrorAction SilentlyContinue
        $events = Get-WinEvent -FilterHashtable @{
            LogName = 'System'
            ProviderName = 'Service Control Manager'
            Id = @(7000, 7001, 7009, 7011, 7022, 7023, 7024, 7031, 7034)
            StartTime = (Get-Date).AddDays(-7)
        } | Where-Object {
            $fields = @($_.Properties | ForEach-Object { $_.Value })
            $fields -contains $serviceName -or ($service -and $fields -contains $service.DisplayName)
        } | Select-Object -First 10
        if ($events) {
            $events | Format-List TimeCreated, Id, Message | Out-String | Write-Output
        } else {
            Write-Output "No matching service events found for $serviceName."
        }
    } catch {
        if ($_.FullyQualifiedErrorId -like 'NoMatchingEventsFound,*') {
            Write-Output "No matching service events found for $serviceName."
        } else {
            Write-Output $_.Exception.Message
        }
    }

    Write-Output ''
    Write-Output '--- Application crashes (last 7 days) ---'
    $crashPattern = ([regex]::Escape($serviceExe), [regex]::Escape($trayExe)) -join "|"
    try {
        $events = Get-WinEvent -FilterHashtable @{
            LogName = 'Application'
            ProviderName = 'Application Error'
            Id = 1000
            StartTime = (Get-Date).AddDays(-7)
        } | Where-Object { $_.Message -match $crashPattern } | Select-Object -First 3
        if ($events) {
            $events | Format-List TimeCreated, Id, Message | Out-String | Write-Output
        } else {
            Write-Output "No crash events found for $serviceExe or $trayExe."
        }
    } catch {
        if ($_.FullyQualifiedErrorId -like 'NoMatchingEventsFound,*') {
            Write-Output "No crash events found for $serviceExe or $trayExe."
        } else {
            Write-Output $_.Exception.Message
        }
    }
}

$reportPath = Join-Path ([IO.Path]::GetTempPath()) ('UMTerminal-diagnostic-{0}.txt' -f [Guid]::NewGuid().ToString('N'))
Write-Diagnostic | Out-File -FilePath $reportPath -Encoding UTF8 -Width 4096
Write-Host "Diagnostic report: $reportPath"
