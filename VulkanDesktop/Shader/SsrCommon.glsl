// Shared Hi-Z screen-space reflection utilities (SsrTrace.comp).

#include "AoCommon.glsl"

float Ssr_ViewPosToNdcDepth(vec3 aViewPos, mat4 aProj)
{
    const vec4 clip = aProj * vec4(aViewPos, 1.0);
    return clip.z / clip.w;
}

float Ssr_SampleHiZDepth(sampler2D aHiZ, vec2 aUV, float aMip)
{
    return textureLod(aHiZ, aUV, aMip).r;
}

// Hi-Z ray march in view space. Returns true when the ray passes behind the closest surface.
bool Ssr_TraceHiZ(
    vec3 aRayOriginView,
    vec3 aRayDirView,
    mat4 aProj,
    vec2 aScreenSize,
    sampler2D aHiZ,
    int aMaxSteps,
    float aMaxDistance,
    float aThickness,
    float aHiZMaxMip,
    out vec2 aHitUV)
{
    const int maxSteps = max(aMaxSteps, 1);
    const float stepLen = aMaxDistance / float(maxSteps);
    vec3 pos = aRayOriginView;

    for (int i = 1; i <= maxSteps; ++i) {
        pos += aRayDirView * stepLen;
        const vec2 uv = Ao_ProjectViewPosToUv(pos, aProj);
        if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0) {
            return false;
        }

        const float mip = clamp(float(i) / float(maxSteps) * aHiZMaxMip, 0.0, aHiZMaxMip);
        const float sceneDepth = Ssr_SampleHiZDepth(aHiZ, uv, mip);
        const float rayDepth = Ssr_ViewPosToNdcDepth(pos, aProj);

        if (rayDepth >= sceneDepth && (rayDepth - sceneDepth) <= aThickness) {
            aHitUV = uv;
            return true;
        }
    }
    return false;
}

// Sample previous-frame lit HDR at the hit world position (temporal reprojection).
bool Ssr_SampleLitHdrHistory(
    sampler2D aLitHistory,
    sampler2D aWorldPosTex,
    vec3 aHitWorld,
    mat4 aPrevViewProj,
    float aHistoryValid,
    float aDepthRejectSigma,
    out vec3 aOutRgb,
    out float aOutConfidence)
{
    aOutRgb = vec3(0.0);
    aOutConfidence = 0.0;
    if (aHistoryValid < 0.5) {
        return false;
    }

    const vec4 prevClip = aPrevViewProj * vec4(aHitWorld, 1.0);
    if (abs(prevClip.w) < 1e-5) {
        return false;
    }

    const vec2 histUv = prevClip.xy / prevClip.w * 0.5 + 0.5;
    if (histUv.x < 0.0 || histUv.x > 1.0 || histUv.y < 0.0 || histUv.y > 1.0) {
        return false;
    }

    const vec3 histWorld = texture(aWorldPosTex, histUv).rgb;
    const float depthDiff = length(aHitWorld - histWorld);
    const float depthWeight = exp(-depthDiff / max(aDepthRejectSigma, 1e-4));
    if (depthWeight < 0.05) {
        return false;
    }

    aOutRgb = texture(aLitHistory, histUv).rgb;
    aOutConfidence = depthWeight;
    return true;
}
