// Shared screen-space AO utilities (Ssao / HbaoPlus / Gtao / AoUpsample).

// Reconstruct view-space position from NDC depth and inverse projection.
vec3 Ao_ReconstructViewPos(vec2 aUV, float aDepth, mat4 aInvProj)
{
    const vec4 clip = vec4(aUV * 2.0 - 1.0, aDepth, 1.0);
    const vec4 viewH = aInvProj * clip;
    return viewH.xyz / viewH.w;
}

// World-space G-buffer normal → view-space (unit).
vec3 Ao_WorldNormalToView(vec3 aWorldNormal, mat4 aView)
{
    return normalize(mat3(aView) * aWorldNormal);
}

// Surface-stable tangent frame in world space (Z-up reference; avoids camera-locked slice rotation).
void Ao_BuildWorldTangentBasis(vec3 aNormalWorld, out vec3 aTangentW, out vec3 aBitangentW)
{
    const vec3 n = normalize(aNormalWorld);
    const vec3 upRef = (abs(n.z) < 0.999) ? vec3(0.0, 0.0, 1.0) : vec3(0.0, 1.0, 0.0);
    aTangentW = normalize(cross(upRef, n));
    aBitangentW = cross(n, aTangentW);
}

// Bilateral depth weight for upsample / blur (NDC depth in [0,1]).
float Ao_DepthEdgeWeight(float aCenterDepth, float aSampleDepth, float aSigma)
{
    return exp(-abs(aSampleDepth - aCenterDepth) / max(aSigma, 1e-4));
}

// GTAO per-side arc integral (Jimenez / XeGTAO). Returns visibility contribution for one horizon.
float Ao_GtaoIntegrateSide(float h, float nAngle, float cosNorm)
{
    const float sinN = sin(nAngle);
    return (cosNorm + 2.0 * h * sinN - cos(2.0 * h - nAngle)) * 0.25;
}

// Weight for screen-space march samples (1 at contact, 0 at sample radius).
float Ao_GtaoDistanceFalloff(float aDist, float aRadius, float aPower)
{
    return 1.0 - clamp(pow(aDist / max(aRadius, 1e-4), aPower), 0.0, 1.0);
}

// Project view-space position to NDC UV (xy) + depth (z).
vec2 Ao_ProjectViewPosToUv(vec3 aViewPos, mat4 aProj)
{
    const vec4 clip = aProj * vec4(aViewPos, 1.0);
    const vec3 ndc = clip.xyz / clip.w;
    return ndc.xy * 0.5 + 0.5;
}

// Fetch view-space position from world-position G-buffer; false if UV is off-screen.
bool Ao_TryFetchViewPosFromGBuffer(sampler2D aWorldPosTex, mat4 aView, vec2 aUV, out vec3 aOutViewPos)
{
    if (aUV.x < 0.0 || aUV.x > 1.0 || aUV.y < 0.0 || aUV.y > 1.0) {
        return false;
    }
    const vec3 worldPos = texture(aWorldPosTex, aUV).rgb;
    aOutViewPos = (aView * vec4(worldPos, 1.0)).xyz;
    return true;
}
