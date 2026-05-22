@echo off
REM Archive Logs\*.txt into Logs\HistoryLogs\<stem>_yyyyMMdd_HHmmss.txt (max 10 per log type).
REM Usage: call RotateEngineLogs.bat [repoRoot]
setlocal EnableDelayedExpansion

set "REPO=%~1"
if not defined REPO set "REPO=%~dp0..\.."
set "LOGS=%REPO%\Logs"
set "HISTORY=%LOGS%\HistoryLogs"
set "MAX_KEEP=10"

if not exist "%HISTORY%" mkdir "%HISTORY%"

set "STAMP="
for /f "delims=" %%a in ('wmic os get localdatetime 2^>nul ^| findstr /r "[0-9]"') do set "STAMP=%%a"
if not defined STAMP (
    set "STAMP=%date:~0,4%%date:~5,2%%date:~8,2%_%time:~0,2%%time:~3,2%%time:~6,2%"
    set "STAMP=!STAMP: =0!"
)
set "STAMP=!STAMP:~0,8!_!STAMP:~8,6!"

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
move /y "%SRC%" "%DEST%" >nul
if errorlevel 1 (
    echo [LOG_ROTATE] Failed to move %NAME% to HistoryLogs 1>&2
    exit /b 1
)
echo [LOG_ROTATE] Archived %NAME% -^> HistoryLogs\!STEM!_!STAMP!.txt 1>&2
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
