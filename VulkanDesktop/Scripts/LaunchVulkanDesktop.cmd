@echo off
REM Manual launcher: rotate logs, then start the built executable (VS F5 debugs the .exe directly).
setlocal
set "TARGET=%~1"
if not defined TARGET (
    echo [LAUNCH] Missing target exe path. 1>&2
    exit /b 1
)
if not exist "%TARGET%" (
    echo [LAUNCH] Executable not found: %TARGET% 1>&2
    exit /b 1
)

call "%~dp0RotateEngineLogs.bat" "%~dp0..\.."
if errorlevel 1 exit /b 1

cd /d "%~dp1"
"%TARGET%"
exit /b %ERRORLEVEL%
