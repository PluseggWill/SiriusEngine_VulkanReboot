@echo off
REM MSBuild hook (after bindless frag glslc): SPIRV-Reflect lit_bindless (vert + TrianglePix_Bindless.spv) vs DescriptorContract_LitBindless.json.
setlocal
cd /d "%~dp0"
call "%~dp0ShaderBuild_Common.bat" init "%~dp0"

set "GEN_DIR=%~dp0..\Shader_Generated\"
set "CONTRACT=%~dp0DescriptorContract_LitBindless.json"
set "OUT_JSON=%GEN_DIR%reflection_lit_bindless.json"
set "REFLECT_BAT=%~dp0..\Tools\ShaderReflect\BuildShaderReflect.bat"
set "REFLECT_EXE=%~dp0..\Tools\ShaderReflect\bin\ShaderReflect.exe"
set "VS_SPV=%GEN_DIR%TriangleVert.spv"
set "PS_SPV=%GEN_DIR%TrianglePix_Bindless.spv"

if not exist "%REFLECT_EXE%" (
    call "%REFLECT_BAT%"
    if errorlevel 1 (
        call "%~dp0ShaderBuild_Common.bat" log ERROR SHADER-REFLECT "ShaderReflect build failed"
        exit /b 1
    )
)

if not exist "%VS_SPV%" (
    call "%~dp0ShaderBuild_Common.bat" log ERROR SHADER-REFLECT "Missing TriangleVert.spv (run glslc first)"
    exit /b 1
)
if not exist "%PS_SPV%" (
    call "%~dp0ShaderBuild_Common.bat" log ERROR SHADER-REFLECT "Missing TrianglePix_Bindless.spv (run glslc first)"
    exit /b 1
)

"%REFLECT_EXE%" --group lit_bindless ^
    --spv "%VS_SPV%@vertex" ^
    --spv "%PS_SPV%@fragment" ^
    --contract "%CONTRACT%" ^
    --out "%OUT_JSON%"
set "RC=%ERRORLEVEL%"
if not "%RC%"=="0" (
    call "%~dp0ShaderBuild_Common.bat" log ERROR SHADER-REFLECT "lit_bindless validation failed (exit %RC%)"
    exit /b %RC%
)

call "%~dp0ShaderBuild_Common.bat" log INFO SHADER-REFLECT "lit_bindless OK (Shader_Generated\reflection_lit_bindless.json)"
exit /b 0
