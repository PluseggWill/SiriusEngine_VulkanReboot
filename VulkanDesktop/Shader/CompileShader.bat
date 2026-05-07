@echo off
setlocal

cd /d "%~dp0"

set "LOG_DIR=%~dp0..\..\Logs"
set "LOG_FILE=%LOG_DIR%\shader_compile_log.txt"
if not exist "%LOG_DIR%" mkdir "%LOG_DIR%"
set "GLSLC=%~dp0..\..\lib\VulkanSDK\1.2.182.0\Bin32\glslc.exe"

call :log INFO SHADER_BUILD "Compile script started."

if /I "%USE_DXC%"=="1" (
    call :log INFO SHADER_BUILD "USE_DXC=1, trying dxc path."
    where dxc >nul 2>&1
    if errorlevel 1 (
        call :log WARN SHADER_BUILD "dxc not found in PATH, fallback to glslc."
        goto :compile_with_glslc
    )

    call :log INFO SHADER_BUILD "dxc found in PATH."
    dxc -T vs_6_0 -spirv -E VSMain -Fo TriangleVert.spv TriangleShader.hlsl
    if errorlevel 1 (
        call :log WARN SHADER_BUILD "dxc vertex compile failed, fallback to glslc."
        goto :compile_with_glslc
    )

    dxc -T ps_6_0 -spirv -E PSMain -Fo TrianglePix.spv TriangleShader.hlsl
    if errorlevel 1 (
        call :log WARN SHADER_BUILD "dxc pixel compile failed, fallback to glslc."
        goto :compile_with_glslc
    )

    call :log INFO SHADER_BUILD "dxc shader compilation succeeded."
    call :log INFO SHADER_BUILD "Compile script finished."
    exit /b 0
)

call :log INFO SHADER_BUILD "USE_DXC not enabled, using glslc by default."
goto :compile_with_glslc

:compile_with_glslc
if not exist "%GLSLC%" (
    call :log ERROR SHADER_BUILD "glslc not found at expected path: %GLSLC%"
    exit /b 1
)

"%GLSLC%" TriangleVertex.vert -o TriangleVert.spv
if errorlevel 1 (
    call :log ERROR SHADER_BUILD "glslc vertex compile failed: TriangleVertex.vert -> TriangleVert.spv"
    exit /b 1
)
call :log INFO SHADER_BUILD "glslc vertex compilation succeeded."

if not exist "TriangleFrag_Lit.frag" (
    call :log ERROR SHADER_BUILD "TriangleFrag_Lit.frag not found for Vulkan descriptor layout."
    exit /b 1
)

"%GLSLC%" TriangleFrag_Lit.frag -o TrianglePix.spv
if errorlevel 1 (
    call :log ERROR SHADER_BUILD "glslc fragment compile failed: TriangleFrag_Lit.frag -> TrianglePix.spv"
    exit /b 1
)
call :log INFO SHADER_BUILD "glslc fragment compilation succeeded using TriangleFrag_Lit.frag."

call :log INFO SHADER_BUILD "Compile script finished."
exit /b 0

:log
set "LEVEL=%~1"
set "CATEGORY=%~2"
set "MESSAGE=%~3"
for /f "usebackq delims=" %%t in (`powershell -NoProfile -Command "Get-Date -Format 'yyyy-MM-dd HH:mm:ss.fff'"`) do set "TS=%%t"
echo [%TS%] [%LEVEL%] [%CATEGORY%] %MESSAGE%
>> "%LOG_FILE%" echo [%TS%] [%LEVEL%] [%CATEGORY%] %MESSAGE%
exit /b 0
