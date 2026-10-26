@echo off
setlocal EnableExtensions
set "ROOT=%~dp0"
cd /d "%ROOT%"
echo.
echo ==========================================
echo   OpenTail Android v0.1.15 - APK Builder
echo ==========================================
echo.
echo NOTE: Extract ALL files from the ZIP before running this launcher.
echo.
if not exist "%ROOT%BUILD-APK.ps1" goto :not_extracted
if not exist "%ROOT%settings.gradle" goto :not_extracted
if not exist "%ROOT%app\src\main\AndroidManifest.xml" goto :not_extracted
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%ROOT%BUILD-APK.ps1"
if errorlevel 1 goto :failed
if not exist "%ROOT%app\build\outputs\apk\debug\app-debug.apk" goto :failed
echo.
echo SUCCESS: APK generated at:
echo %ROOT%app\build\outputs\apk\debug\app-debug.apk
echo.
pause
exit /b 0
:not_extracted
echo ERROR: The complete extracted project was not found.
echo Close this window. Right-click the ZIP, select Extract All,
echo open the extracted OpenTail-Android-v0.1.15 folder, then run BUILD-APK.bat.
echo Do not launch BUILD-APK.bat from inside the ZIP preview.
echo.
pause
exit /b 1
:failed
echo.
echo ERROR: APK was NOT built. See the actual build error above.
echo.
pause
exit /b 1
