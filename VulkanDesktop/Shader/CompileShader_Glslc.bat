@echo off
setlocal

cd /d "%~dp0"
call "%~dp0ShaderBuild_Common.bat" init "%~dp0"
set "GLSLC=%SHADER_DIR%..\..\lib\VulkanSDK\1.2.182.0\Bin32\glslc.exe"
set "GEN_DIR=%SHADER_DIR%..\Shader_Generated\"
if not exist "%GEN_DIR%" mkdir "%GEN_DIR%"
set "VS_OUT=%GEN_DIR%TriangleVert.spv"
set "PS_OUT=%GEN_DIR%TrianglePix.spv"

REM POLICY_BINDLESS M7 (#17): PermutationRegistry.json frozen at lit + lit_alpha_clip until hybrid pass 2.
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

REM -I path: no quotes around %SHADER_DIR% (trailing backslash breaks -I"%SHADER_DIR%").
"%GLSLC%" -I%SHADER_DIR% "%SHADER_DIR%TriangleFrag_Lit.frag" -o "%PS_OUT%"
if errorlevel 1 (
    call "%~dp0ShaderBuild_Common.bat" log ERROR SHADER_GLSLC "Fragment failed: TriangleFrag_Lit.frag"
    exit /b 1
)
call "%~dp0ShaderBuild_Common.bat" log INFO SHADER_GLSLC "Fragment OK: Shader_Generated\TrianglePix.spv"

"%GLSLC%" -I%SHADER_DIR% "%SHADER_DIR%TriangleFrag_Lit.frag" -o "%GEN_DIR%TrianglePix_AlphaClip.spv" -DALPHA_CLIP=1
if errorlevel 1 (
    call "%~dp0ShaderBuild_Common.bat" log ERROR SHADER_GLSLC "Fragment failed: TriangleFrag_Lit.frag (ALPHA_CLIP)"
    exit /b 1
)
call "%~dp0ShaderBuild_Common.bat" log INFO SHADER_GLSLC "Fragment OK: Shader_Generated\TrianglePix_AlphaClip.spv"

"%GLSLC%" -I%SHADER_DIR% "%SHADER_DIR%TriangleFrag_Lit_Bindless.frag" -o "%GEN_DIR%TrianglePix_Bindless.spv"
if errorlevel 1 (
    call "%~dp0ShaderBuild_Common.bat" log ERROR SHADER_GLSLC "Fragment failed: TriangleFrag_Lit_Bindless.frag"
    exit /b 1
)
call "%~dp0ShaderBuild_Common.bat" log INFO SHADER_GLSLC "Fragment OK: Shader_Generated\TrianglePix_Bindless.spv"

"%GLSLC%" "%SHADER_DIR%EntityCull.comp" -o "%GEN_DIR%EntityCull.spv"
if errorlevel 1 (
    call "%~dp0ShaderBuild_Common.bat" log ERROR SHADER_GLSLC "Compute failed: EntityCull.comp"
    exit /b 1
)
call "%~dp0ShaderBuild_Common.bat" log INFO SHADER_GLSLC "Compute OK: Shader_Generated\EntityCull.spv"

"%GLSLC%" "%SHADER_DIR%ClusterBuild.comp" -o "%GEN_DIR%ClusterBuild.spv"
if errorlevel 1 (
    call "%~dp0ShaderBuild_Common.bat" log ERROR SHADER_GLSLC "Compute failed: ClusterBuild.comp"
    exit /b 1
)
call "%~dp0ShaderBuild_Common.bat" log INFO SHADER_GLSLC "Compute OK: Shader_Generated\ClusterBuild.spv"

"%GLSLC%" "%SHADER_DIR%GBuffer.vert" -o "%GEN_DIR%GBufferVert.spv"
if errorlevel 1 (
    call "%~dp0ShaderBuild_Common.bat" log ERROR SHADER_GLSLC "Vertex failed: GBuffer.vert"
    exit /b 1
)
call "%~dp0ShaderBuild_Common.bat" log INFO SHADER_GLSLC "Vertex OK: Shader_Generated\GBufferVert.spv"

"%GLSLC%" -I%SHADER_DIR% "%SHADER_DIR%GBuffer.frag" -o "%GEN_DIR%GBufferFrag.spv"
if errorlevel 1 (
    call "%~dp0ShaderBuild_Common.bat" log ERROR SHADER_GLSLC "Fragment failed: GBuffer.frag"
    exit /b 1
)
call "%~dp0ShaderBuild_Common.bat" log INFO SHADER_GLSLC "Fragment OK: Shader_Generated\GBufferFrag.spv"

"%GLSLC%" -I%SHADER_DIR% "%SHADER_DIR%GBufferFrag_Bindless.frag" -o "%GEN_DIR%GBufferFrag_Bindless.spv"
if errorlevel 1 (
    call "%~dp0ShaderBuild_Common.bat" log ERROR SHADER_GLSLC "Fragment failed: GBufferFrag_Bindless.frag"
    exit /b 1
)
call "%~dp0ShaderBuild_Common.bat" log INFO SHADER_GLSLC "Fragment OK: Shader_Generated\GBufferFrag_Bindless.spv"

"%GLSLC%" "%SHADER_DIR%CompositeAlbedo.vert" -o "%GEN_DIR%CompositeAlbedoVert.spv"
if errorlevel 1 (
    call "%~dp0ShaderBuild_Common.bat" log ERROR SHADER_GLSLC "Vertex failed: CompositeAlbedo.vert"
    exit /b 1
)
call "%~dp0ShaderBuild_Common.bat" log INFO SHADER_GLSLC "Vertex OK: Shader_Generated\CompositeAlbedoVert.spv"

"%GLSLC%" "%SHADER_DIR%CompositeAlbedo.frag" -o "%GEN_DIR%CompositeAlbedoFrag.spv"
if errorlevel 1 (
    call "%~dp0ShaderBuild_Common.bat" log ERROR SHADER_GLSLC "Fragment failed: CompositeAlbedo.frag"
    exit /b 1
)
call "%~dp0ShaderBuild_Common.bat" log INFO SHADER_GLSLC "Fragment OK: Shader_Generated\CompositeAlbedoFrag.spv"

"%GLSLC%" "%SHADER_DIR%DeferredLighting.vert" -o "%GEN_DIR%DeferredLightingVert.spv"
if errorlevel 1 (
    call "%~dp0ShaderBuild_Common.bat" log ERROR SHADER_GLSLC "Vertex failed: DeferredLighting.vert"
    exit /b 1
)
call "%~dp0ShaderBuild_Common.bat" log INFO SHADER_GLSLC "Vertex OK: Shader_Generated\DeferredLightingVert.spv"

"%GLSLC%" -I%SHADER_DIR% "%SHADER_DIR%DeferredLighting.frag" -o "%GEN_DIR%DeferredLightingFrag.spv"
if errorlevel 1 (
    call "%~dp0ShaderBuild_Common.bat" log ERROR SHADER_GLSLC "Fragment failed: DeferredLighting.frag"
    exit /b 1
)
call "%~dp0ShaderBuild_Common.bat" log INFO SHADER_GLSLC "Fragment OK: Shader_Generated\DeferredLightingFrag.spv"

"%GLSLC%" -I%SHADER_DIR% "%SHADER_DIR%ShadowMap.vert" -o "%GEN_DIR%ShadowMapVert.spv"
if errorlevel 1 (
    call "%~dp0ShaderBuild_Common.bat" log ERROR SHADER_GLSLC "Vertex failed: ShadowMap.vert"
    exit /b 1
)
call "%~dp0ShaderBuild_Common.bat" log INFO SHADER_GLSLC "Vertex OK: Shader_Generated\ShadowMapVert.spv"

"%GLSLC%" -I%SHADER_DIR% "%SHADER_DIR%ShadowMap.frag" -o "%GEN_DIR%ShadowMapFrag.spv"
if errorlevel 1 (
    call "%~dp0ShaderBuild_Common.bat" log ERROR SHADER_GLSLC "Fragment failed: ShadowMap.frag"
    exit /b 1
)
call "%~dp0ShaderBuild_Common.bat" log INFO SHADER_GLSLC "Fragment OK: Shader_Generated\ShadowMapFrag.spv"

"%GLSLC%" "%SHADER_DIR%DepthPyramid.comp" -o "%GEN_DIR%DepthPyramid.spv"
if errorlevel 1 (
    call "%~dp0ShaderBuild_Common.bat" log ERROR SHADER_GLSLC "Compute failed: DepthPyramid.comp"
    exit /b 1
)
call "%~dp0ShaderBuild_Common.bat" log INFO SHADER_GLSLC "Compute OK: Shader_Generated\DepthPyramid.spv"

"%GLSLC%" -I%SHADER_DIR% "%SHADER_DIR%Ssao.comp" -o "%GEN_DIR%Ssao.spv"
if errorlevel 1 (
    call "%~dp0ShaderBuild_Common.bat" log ERROR SHADER_GLSLC "Compute failed: Ssao.comp"
    exit /b 1
)
call "%~dp0ShaderBuild_Common.bat" log INFO SHADER_GLSLC "Compute OK: Shader_Generated\Ssao.spv"

"%GLSLC%" -I%SHADER_DIR% "%SHADER_DIR%HbaoPlus.comp" -o "%GEN_DIR%HbaoPlus.spv"
if errorlevel 1 (
    call "%~dp0ShaderBuild_Common.bat" log ERROR SHADER_GLSLC "Compute failed: HbaoPlus.comp"
    exit /b 1
)
call "%~dp0ShaderBuild_Common.bat" log INFO SHADER_GLSLC "Compute OK: Shader_Generated\HbaoPlus.spv"

"%GLSLC%" -I%SHADER_DIR% "%SHADER_DIR%Gtao.comp" -o "%GEN_DIR%Gtao.spv"
if errorlevel 1 (
    call "%~dp0ShaderBuild_Common.bat" log ERROR SHADER_GLSLC "Compute failed: Gtao.comp"
    exit /b 1
)
call "%~dp0ShaderBuild_Common.bat" log INFO SHADER_GLSLC "Compute OK: Shader_Generated\Gtao.spv"

"%GLSLC%" -I%SHADER_DIR% "%SHADER_DIR%AoUpsample.comp" -o "%GEN_DIR%AoUpsample.spv"
if errorlevel 1 (
    call "%~dp0ShaderBuild_Common.bat" log ERROR SHADER_GLSLC "Compute failed: AoUpsample.comp"
    exit /b 1
)
call "%~dp0ShaderBuild_Common.bat" log INFO SHADER_GLSLC "Compute OK: Shader_Generated\AoUpsample.spv"

"%GLSLC%" "%SHADER_DIR%SsaoBlur.comp" -o "%GEN_DIR%SsaoBlur.spv"
if errorlevel 1 (
    call "%~dp0ShaderBuild_Common.bat" log ERROR SHADER_GLSLC "Compute failed: SsaoBlur.comp"
    exit /b 1
)
call "%~dp0ShaderBuild_Common.bat" log INFO SHADER_GLSLC "Compute OK: Shader_Generated\SsaoBlur.spv"

"%GLSLC%" "%SHADER_DIR%AoTemporal.comp" -o "%GEN_DIR%AoTemporal.spv"
if errorlevel 1 (
    call "%~dp0ShaderBuild_Common.bat" log ERROR SHADER_GLSLC "Compute failed: AoTemporal.comp"
    exit /b 1
)
call "%~dp0ShaderBuild_Common.bat" log INFO SHADER_GLSLC "Compute OK: Shader_Generated\AoTemporal.spv"

"%GLSLC%" "%SHADER_DIR%DdgiProbeUpdate.comp" -o "%GEN_DIR%DdgiProbeUpdate.spv"
if errorlevel 1 (
    call "%~dp0ShaderBuild_Common.bat" log ERROR SHADER_GLSLC "Compute failed: DdgiProbeUpdate.comp"
    exit /b 1
)
call "%~dp0ShaderBuild_Common.bat" log INFO SHADER_GLSLC "Compute OK: Shader_Generated\DdgiProbeUpdate.spv"

"%GLSLC%" -I%SHADER_DIR% "%SHADER_DIR%ShadowAoPack.comp" -o "%GEN_DIR%ShadowAoPack.spv"
if errorlevel 1 (
    call "%~dp0ShaderBuild_Common.bat" log ERROR SHADER_GLSLC "Compute failed: ShadowAoPack.comp"
    exit /b 1
)
call "%~dp0ShaderBuild_Common.bat" log INFO SHADER_GLSLC "Compute OK: Shader_Generated\ShadowAoPack.spv"

"%GLSLC%" "%SHADER_DIR%ShadowAoBlur.comp" -o "%GEN_DIR%ShadowAoBlur.spv"
if errorlevel 1 (
    call "%~dp0ShaderBuild_Common.bat" log ERROR SHADER_GLSLC "Compute failed: ShadowAoBlur.comp"
    exit /b 1
)
call "%~dp0ShaderBuild_Common.bat" log INFO SHADER_GLSLC "Compute OK: Shader_Generated\ShadowAoBlur.spv"

"%GLSLC%" "%SHADER_DIR%Tonemap.vert" -o "%GEN_DIR%TonemapVert.spv"
if errorlevel 1 (
    call "%~dp0ShaderBuild_Common.bat" log ERROR SHADER_GLSLC "Vertex failed: Tonemap.vert"
    exit /b 1
)
call "%~dp0ShaderBuild_Common.bat" log INFO SHADER_GLSLC "Vertex OK: Shader_Generated\TonemapVert.spv"

"%GLSLC%" "%SHADER_DIR%Tonemap.frag" -o "%GEN_DIR%TonemapFrag.spv"
if errorlevel 1 (
    call "%~dp0ShaderBuild_Common.bat" log ERROR SHADER_GLSLC "Fragment failed: Tonemap.frag"
    exit /b 1
)
call "%~dp0ShaderBuild_Common.bat" log INFO SHADER_GLSLC "Fragment OK: Shader_Generated\TonemapFrag.spv"

"%GLSLC%" "%SHADER_DIR%BloomThreshold.comp" -o "%GEN_DIR%BloomThreshold.spv"
if errorlevel 1 (
    call "%~dp0ShaderBuild_Common.bat" log ERROR SHADER_GLSLC "Compute failed: BloomThreshold.comp"
    exit /b 1
)
call "%~dp0ShaderBuild_Common.bat" log INFO SHADER_GLSLC "Compute OK: Shader_Generated\BloomThreshold.spv"

"%GLSLC%" "%SHADER_DIR%BloomBlur.comp" -o "%GEN_DIR%BloomBlur.spv"
if errorlevel 1 (
    call "%~dp0ShaderBuild_Common.bat" log ERROR SHADER_GLSLC "Compute failed: BloomBlur.comp"
    exit /b 1
)
call "%~dp0ShaderBuild_Common.bat" log INFO SHADER_GLSLC "Compute OK: Shader_Generated\BloomBlur.spv"

call "%~dp0ShaderBuild_Common.bat" log INFO SHADER_GLSLC "glslc compile finished."

REM Phase 2a (lit_batch) + 2d (lit_bindless): SPIRV-Reflect + descriptor contracts — stderr/log only; see shader-build.mdc.
call "%~dp0ReflectShaders_Lit.bat"
if errorlevel 1 exit /b 1
call "%~dp0ReflectShaders_LitBindless.bat"
if errorlevel 1 exit /b 1
exit /b 0
