@echo off
REM Builds Tools/ShaderReflect/bin/ShaderReflect.exe (SPIRV-Reflect + nlohmann). Called from ReflectShaders_Lit.bat when missing.
setlocal
cd /d "%~dp0"

set "OUT_DIR=%~dp0bin"
set "EXE=%OUT_DIR%\ShaderReflect.exe"
set "SDK=%~dp0..\..\..\lib\VulkanSDK\1.2.182.0\Source\SPIRV-Reflect"
set "JSON_INC=%~dp0..\..\..\lib"
set "SRC_CPP=%~dp0main.cpp"
set "SRC_C=%SDK%\spirv_reflect.c"

if not exist "%OUT_DIR%" mkdir "%OUT_DIR%"

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
