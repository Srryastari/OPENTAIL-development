@echo off
cd /d "%~dp0"
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0Flash-GPIO16-Smoke.ps1" -Port COM9
if errorlevel 1 echo Runner cancelled or failed. See messages above.
pause
