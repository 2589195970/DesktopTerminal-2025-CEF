param(
    [Parameter(Mandatory = $true)]
    [ValidateSet('Start', 'Stop', 'Restart')]
    [string]$Action
)

$ErrorActionPreference = 'Stop'
$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$installDir = Split-Path -Parent $scriptDir
$trayPath = Join-Path $installDir 'DesktopTerminalStatus.exe'
$sessionId = [Diagnostics.Process]::GetCurrentProcess().SessionId

function Get-StatusTray {
    Get-Process -Name 'DesktopTerminalStatus' -ErrorAction SilentlyContinue | Where-Object {
        $_.SessionId -eq $sessionId -and $_.Path -eq $trayPath
    }
}

try {
    if ($Action -ne 'Stop' -and -not (Test-Path -LiteralPath $trayPath -PathType Leaf)) {
        throw "Status tray executable not found: $trayPath"
    }

    if ($Action -ne 'Start') {
        foreach ($tray in (Get-StatusTray)) {
            Stop-Process -InputObject $tray -Force
            if (-not $tray.WaitForExit(10000)) {
                throw 'Timed out waiting for the status tray to stop.'
            }
        }
    }

    if ($Action -eq 'Stop') {
        Write-Output 'Status tray: STOPPED'
        exit 0
    }

    if (-not (Get-StatusTray)) {
        $tray = Start-Process -FilePath $trayPath -ArgumentList '--tray' -WorkingDirectory $installDir -PassThru
        if ($tray.WaitForExit(1000) -and $tray.ExitCode -ne 0) {
            throw "Status tray exited with code $($tray.ExitCode)."
        }
        if (-not (Get-StatusTray)) {
            throw 'The status tray did not stay running.'
        }
    }

    Write-Output 'Status tray: RUNNING'
    exit 0
} catch {
    [Console]::Error.WriteLine($_.Exception.Message)
    exit 1
}
