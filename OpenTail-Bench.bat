@echo off
setlocal EnableExtensions
cd /d "%~dp0"
where powershell.exe >nul 2>&1
if errorlevel 1 (
  echo ERROR: Windows PowerShell is required.
  pause
  exit /b 1
)
echo.
echo OpenTail one-click USB + BLE bench session
echo Android: USB ADB, ESP32: USB serial logs, controller commands: phone BLE.
echo Firmware flash: OFF unless explicitly selected with "flash".
echo.
set "EXTRA="
if /I "%~1"=="flash" set "EXTRA=-FlashFirmware"
if /I "%~1"=="selftest" set "EXTRA=-SelfTest"
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0scripts\windows\OpenTail-Bench.ps1" %EXTRA%
set "RESULT=%ERRORLEVEL%"
echo.
echo Runner exit code: %RESULT%.
echo Log and report directories are under bench-reports.
pause
exit /b %RESULT%
