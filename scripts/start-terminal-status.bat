@echo off
setlocal
"%SystemRoot%\System32\WindowsPowerShell\v1.0\powershell.exe" -NoProfile -NonInteractive -ExecutionPolicy Bypass -File "%~dp0control-terminal-status.ps1" Start
set "trayExit=%ERRORLEVEL%"
pause
exit /b %trayExit%
