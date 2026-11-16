@echo off
setlocal EnableExtensions
cd /d "%~dp0"
where powershell.exe >nul 2>&1
if errorlevel 1 (
  echo ERROR: Windows PowerShell 5.1 is required.
  pause
  exit /b 1
)
set "MODE=bluetooth"
set "SECONDS=30"
set "EXTRA="
if /I "%~1"=="bluetooth" set "MODE=bluetooth"
if /I "%~1"=="ble" set "MODE=bluetooth"
if /I "%~1"=="layout" set "MODE=layout"
if /I "%~1"=="save" set "MODE=save"
if /I "%~1"=="playback" set "MODE=playback"
if /I "%~1"=="extended" set "SECONDS=120"
if /I "%~2"=="extended" set "SECONDS=120"
if /I "%~1"=="flash" set "EXTRA=-FlashFirmware"
if /I "%~1"=="flash-auto" set "EXTRA=-FlashFirmware -AutoFlash"
if /I "%~1"=="led" set "EXTRA=-DirectLedProbe"
if /I "%~1"=="selftest" set "EXTRA=-SelfTest"
echo.
if /I "%~1"=="flash-auto" (
  echo OpenTail VERIFIED auto-flash on detected ESP32 COM port. Firmware CI and hashes are mandatory.
  echo Application partition only. NVS is NOT erased. Close serial monitors first.
) else (
  echo OpenTail FAST bench v2026.11.15.2: %MODE% ^| %SECONDS% seconds
)
echo Read-only playback NEVER requires Firmware #127 CI or a firmware flash.
echo Android USB logcat + ESP COM receive-only; no automatic BLE, LED or firmware commands.
echo APK data erase requires a separate explicit approval prompt.
echo.
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0scripts\windows\OpenTail-Bench.ps1" -Mode %MODE% -CaptureSeconds %SECONDS% %EXTRA%
set "RESULT=%ERRORLEVEL%"
echo.
echo Runner exit code: %RESULT%.
echo REPORT.md and RESULTS.json are under bench-reports.
pause
exit /b %RESULT%
