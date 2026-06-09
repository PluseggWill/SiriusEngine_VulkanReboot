@echo off
REM Builds Tools/ShaderReflect/bin/ShaderReflect.exe (SPIRV-Reflect + nlohmann).
REM Rebuilds when main.cpp or spirv_reflect.c is newer than the exe (avoids stale absolute-path output).
setlocal EnableDelayedExpansion
cd /d "%~dp0"

set "OUT_DIR=%~dp0bin"
set "EXE=%OUT_DIR%\ShaderReflect.exe"
set "SDK=%~dp0..\..\..\lib\VulkanSDK\1.2.182.0\Source\SPIRV-Reflect"
set "JSON_INC=%~dp0..\..\..\lib"
set "SRC_CPP=%~dp0main.cpp"
set "SRC_C=%SDK%\spirv_reflect.c"

if not exist "%OUT_DIR%" mkdir "%OUT_DIR%"

set "NEED_BUILD=0"
if not exist "%EXE%" (
    set "NEED_BUILD=1"
) else (
    for %%F in ("%EXE%") do set "EXE_TIME=%%~tF"
    for %%S in ("%SRC_CPP%" "%SRC_C%") do (
        if "%%~tS" gtr "!EXE_TIME!" set "NEED_BUILD=1"
    )
)
if "!NEED_BUILD!"=="0" exit /b 0

for /f "usebackq delims=" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -property installationPath`) do set "VSWDIR=%%i"
if not defined VSWDIR (
    echo [SHADER-REFLECT] vswhere: Visual Studio not found 1>&2
    exit /b 1
)
call "%VSWDIR%\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
if errorlevel 1 (
    echo [SHADER-REFLECT] vcvars64 failed 1>&2
    exit /b 1
)

cl /nologo /EHsc /std:c++17 /W4 /O2 ^
    /I"%SDK%" /I"%JSON_INC%" ^
    "%SRC_CPP%" "%SRC_C%" ^
    /Fe:"%EXE%" /Fo:"%OUT_DIR%\\" ^
    1>&2
if errorlevel 1 exit /b 1
exit /b 0
