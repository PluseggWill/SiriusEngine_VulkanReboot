@echo off
setlocal

cd /d "%~dp0"
call "%~dp0ShaderBuild_Common.bat" init "%~dp0"
set "GLSLC=%SHADER_DIR%..\..\lib\VulkanSDK\1.2.182.0\Bin32\glslc.exe"
set "GEN_DIR=%SHADER_DIR%..\Shader_Generated\"
if not exist "%GEN_DIR%" mkdir "%GEN_DIR%"
set "VS_OUT=%GEN_DIR%TriangleVert.spv"
set "PS_OUT=%GEN_DIR%TrianglePix.spv"

call "%~dp0ShaderBuild_Common.bat" log INFO SHADER_GLSLC "glslc compile started."

if not exist "%GLSLC%" (
    call "%~dp0ShaderBuild_Common.bat" log ERROR SHADER_GLSLC "glslc not found: %GLSLC%"
    exit /b 1
)

"%GLSLC%" "%SHADER_DIR%TriangleVertex.vert" -o "%VS_OUT%"
if errorlevel 1 (
    call "%~dp0ShaderBuild_Common.bat" log ERROR SHADER_GLSLC "Vertex failed: TriangleVertex.vert"
    exit /b 1
)
call "%~dp0ShaderBuild_Common.bat" log INFO SHADER_GLSLC "Vertex OK: Shader_Generated\TriangleVert.spv"

if not exist "%SHADER_DIR%TriangleFrag_Lit.frag" (
    call "%~dp0ShaderBuild_Common.bat" log ERROR SHADER_GLSLC "Missing TriangleFrag_Lit.frag"
    exit /b 1
)

"%GLSLC%" "%SHADER_DIR%TriangleFrag_Lit.frag" -o "%PS_OUT%"
if errorlevel 1 (
    call "%~dp0ShaderBuild_Common.bat" log ERROR SHADER_GLSLC "Fragment failed: TriangleFrag_Lit.frag"
    exit /b 1
)
call "%~dp0ShaderBuild_Common.bat" log INFO SHADER_GLSLC "Fragment OK: Shader_Generated\TrianglePix.spv"

"%GLSLC%" "%SHADER_DIR%TriangleFrag_Lit.frag" -o "%GEN_DIR%TrianglePix_AlphaClip.spv" -DALPHA_CLIP=1
if errorlevel 1 (
    call "%~dp0ShaderBuild_Common.bat" log ERROR SHADER_GLSLC "Fragment failed: TriangleFrag_Lit.frag (ALPHA_CLIP)"
    exit /b 1
)
call "%~dp0ShaderBuild_Common.bat" log INFO SHADER_GLSLC "Fragment OK: Shader_Generated\TrianglePix_AlphaClip.spv"

"%GLSLC%" "%SHADER_DIR%TriangleFrag_Lit_Bindless.frag" -o "%GEN_DIR%TrianglePix_Bindless.spv"
if errorlevel 1 (
    call "%~dp0ShaderBuild_Common.bat" log ERROR SHADER_GLSLC "Fragment failed: TriangleFrag_Lit_Bindless.frag"
    exit /b 1
)
call "%~dp0ShaderBuild_Common.bat" log INFO SHADER_GLSLC "Fragment OK: Shader_Generated\TrianglePix_Bindless.spv"
call "%~dp0ShaderBuild_Common.bat" log INFO SHADER_GLSLC "glslc compile finished."

REM Phase 2a (lit_batch) + 2d (lit_bindless): SPIRV-Reflect + descriptor contracts — stderr/log only; see shader-build.mdc.
call "%~dp0ReflectShaders_Lit.bat"
if errorlevel 1 exit /b 1
call "%~dp0ReflectShaders_LitBindless.bat"
if errorlevel 1 exit /b 1
exit /b 0
