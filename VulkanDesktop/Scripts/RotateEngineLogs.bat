@echo off
REM Archive Logs\*.txt into Logs\HistoryLogs\<stem>_yyyyMMdd_HHmmss.txt (max 10 per log type).
REM Usage: call RotateEngineLogs.bat [repoRoot]
setlocal EnableDelayedExpansion

set "REPO=%~1"
if not defined REPO set "REPO=%~dp0..\.."
REM Normalize trailing backslash (optional).
if "%REPO:~-1%"=="\" set "REPO=%REPO:~0,-1%"

set "LOGS=%REPO%\Logs"
set "HISTORY=%LOGS%\HistoryLogs"
set "MAX_KEEP=10"

if not exist "%LOGS%" mkdir "%LOGS%" 2>nul
if not exist "%HISTORY%" mkdir "%HISTORY%" 2>nul

set "STAMP="
REM wmic removed on recent Windows; PowerShell timestamp is locale-safe and path-safe.
for /f "delims=" %%I in ('powershell -NoProfile -Command "(Get-Date).ToString('yyyyMMdd_HHmmss')" 2^>nul') do set "STAMP=%%I"
if not defined STAMP (
    REM Last-resort: digits only from date/time (no colons or slashes).
    for /f "tokens=1-3 delims=/ " %%a in ("%date%") do (
        set "STAMP=%%c%%a%%b"
    )
    set "STAMP=!STAMP: =0!"
    for /f "tokens=1-3 delims=:." %%h in ("%time%") do (
        set "STAMP=!STAMP!_%%h%%i%%j"
    )
)

call :ArchiveOne engine_runtime_log.txt
call :ArchiveOne shader_compile_log.txt
exit /b 0

:ArchiveOne
set "NAME=%~1"
set "SRC=%LOGS%\%NAME%"
if not exist "%SRC%" exit /b 0
for %%A in ("%SRC%") do if %%~zA==0 (
    del /f /q "%SRC%" >nul 2>&1
    exit /b 0
)
for %%F in ("%NAME%") do set "STEM=%%~nF"
set "DEST=%HISTORY%\!STEM!_!STAMP!.txt"

move /y "%SRC%" "%DEST%" >nul 2>&1
if not errorlevel 1 (
    echo [LOG_ROTATE] Archived %NAME% -^> HistoryLogs\!STEM!_!STAMP!.txt 1>&2
    call :PrunePrefix "!STEM!_"
    exit /b 0
)

REM Source may be locked (app still running); copy to history and leave current log in place.
copy /y "%SRC%" "%DEST%" >nul 2>&1
if errorlevel 1 (
    echo [LOG_ROTATE] Failed to archive %NAME% ^(move and copy failed; is the file open?^) 1>&2
    exit /b 1
)
echo [LOG_ROTATE] Copied %NAME% -^> HistoryLogs\!STEM!_!STAMP!.txt ^(source kept; file in use^) 1>&2
call :PrunePrefix "!STEM!_"
exit /b 0

:PrunePrefix
set "PREFIX=%~1"
set /a COUNT=0
for /f "delims=" %%F in ('dir /b /o-d "%HISTORY%\%PREFIX%*.txt" 2^>nul') do (
    set /a COUNT+=1
    if !COUNT! gtr %MAX_KEEP% del /f /q "%HISTORY%\%%F" >nul 2>&1
)
exit /b 0
