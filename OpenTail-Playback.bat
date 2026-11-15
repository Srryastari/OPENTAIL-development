@echo off
setlocal EnableExtensions
cd /d "%~dp0"
echo.
echo OpenTail Playback Diagnostics v2026.11.15.2
echo READ-ONLY: Firmware #127 CI NOT required. Firmware flash DISABLED.
echo Use installed controller firmware; no NVS, HTTP or GPIO changes.
echo.
where powershell.exe >nul 2>&1
if errorlevel 1 (
  echo ERROR: Windows PowerShell 5.1 is required.
  pause
  exit /b 1
)
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0scripts\windows\OpenTail-Bench.ps1" -Mode playback -CaptureSeconds 30
set "RESULT=%ERRORLEVEL%"
echo.
echo Playback runner exit code: %RESULT%
echo REPORT.md and RESULTS.json are under bench-reports.
pause
exit /b %RESULT%
