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
