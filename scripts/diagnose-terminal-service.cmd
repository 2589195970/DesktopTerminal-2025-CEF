@echo off
setlocal
"%SystemRoot%\System32\WindowsPowerShell\v1.0\powershell.exe" -NoProfile -NonInteractive -ExecutionPolicy Bypass -File "%~dp0diagnose-terminal-service.ps1"
set "diagnosticExit=%ERRORLEVEL%"
pause
exit /b %diagnosticExit%
