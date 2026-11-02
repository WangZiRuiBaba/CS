@echo off
rem ============================================================
rem  CS2_ESP one-click build (Release, dev build: no license timer)
rem  Wraps scripts\build.ps1 with ExecutionPolicy bypass.
rem  Output: build\Release\CS2_ESP.exe
rem ============================================================
setlocal
cd /d "%~dp0"

set "PS=%SystemRoot%\System32\WindowsPowerShell\v1.0\powershell.exe"
if not exist "%PS%" set "PS=powershell"

echo [build] repo   : %~dp0
echo [build] script : scripts\build.ps1
echo.

"%PS%" -NoProfile -ExecutionPolicy Bypass -File "%~dp0scripts\build.ps1"
set "RC=%ERRORLEVEL%"

if not "%RC%"=="0" goto fail
if not exist "%~dp0build\Release\CS2_ESP.exe" goto fail

echo.
echo [build] OK  -^> %~dp0build\Release\CS2_ESP.exe
for %%F in ("%~dp0build\Release\CS2_ESP.exe") do echo [build] size : %%~zF bytes   time: %%~tF
pause
exit /b 0

:fail
echo.
echo [build] FAILED (exit %RC%) - check the output above.
echo [build] Tips:
echo         1) install VS 2022 with "Desktop development with C++"
echo         2) close CS2_ESP.exe if it is running (file lock)
echo         3) delete build\ and run again for a clean configure
pause
exit /b 1
