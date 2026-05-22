@echo off
REM Usage: call ShaderBuild_Common.bat init <shaderDir>
REM        call ShaderBuild_Common.bat log <LEVEL> <CATEGORY> <message...>
if /I "%~1"=="init" goto :do_init
if /I "%~1"=="log" goto :do_log
exit /b 1

:do_init
set "SHADER_DIR=%~2"
if not defined SHADER_DIR set "SHADER_DIR=%~dp0"
set "LOG_DIR=%SHADER_DIR%..\..\Logs"
set "LOG_FILE=%LOG_DIR%\shader_compile_log.txt"
if not exist "%LOG_DIR%" mkdir "%LOG_DIR%"
exit /b 0

:do_log
set "LEVEL=%~2"
set "CATEGORY=%~3"
set "MSG=%~4"
if not defined LOG_FILE (
    echo [LOGGER] LOG_FILE not set. Call init first. 1>&2
    exit /b 1
)
for /f "usebackq delims=" %%t in (`powershell -NoProfile -Command "Get-Date -Format 'yyyy-MM-dd HH:mm:ss.fff'"`) do set "TS=%%t"
echo [%TS%] [%LEVEL%] [%CATEGORY%] %MSG% 1>&2
>> "%LOG_FILE%" echo [%TS%] [%LEVEL%] [%CATEGORY%] %MSG%
exit /b 0
