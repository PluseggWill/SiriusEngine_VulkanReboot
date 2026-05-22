@echo off
setlocal

cd /d "%~dp0"
call "%~dp0ShaderBuild_Common.bat" init "%~dp0"
set "GLSLC=%SHADER_DIR%..\..\lib\VulkanSDK\1.2.182.0\Bin32\glslc.exe"
set "VS_OUT=%SHADER_DIR%TriangleVert.spv"
set "PS_OUT=%SHADER_DIR%TrianglePix.spv"

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
call "%~dp0ShaderBuild_Common.bat" log INFO SHADER_GLSLC "Vertex OK: TriangleVert.spv"

if not exist "%SHADER_DIR%TriangleFrag_Lit.frag" (
    call "%~dp0ShaderBuild_Common.bat" log ERROR SHADER_GLSLC "Missing TriangleFrag_Lit.frag"
    exit /b 1
)

"%GLSLC%" "%SHADER_DIR%TriangleFrag_Lit.frag" -o "%PS_OUT%"
if errorlevel 1 (
    call "%~dp0ShaderBuild_Common.bat" log ERROR SHADER_GLSLC "Fragment failed: TriangleFrag_Lit.frag"
    exit /b 1
)
call "%~dp0ShaderBuild_Common.bat" log INFO SHADER_GLSLC "Fragment OK: TrianglePix.spv"
call "%~dp0ShaderBuild_Common.bat" log INFO SHADER_GLSLC "glslc compile finished."
exit /b 0
