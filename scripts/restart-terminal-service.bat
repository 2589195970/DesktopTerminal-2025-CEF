@echo off
setlocal
"%SystemRoot%\System32\WindowsPowerShell\v1.0\powershell.exe" -NoProfile -NonInteractive -Command "$ErrorActionPreference = 'Stop'; $service = Get-Service -Name 'UMTerminalService'; Restart-Service -InputObject $service; $service.WaitForStatus('Running', [TimeSpan]::FromSeconds(30))"
set "serviceExit=%ERRORLEVEL%"
"%SystemRoot%\System32\sc.exe" query UMTerminalService
if not "%serviceExit%"=="0" echo Restart failed. Run this file as Administrator and check the service status above.
pause
exit /b %serviceExit%
