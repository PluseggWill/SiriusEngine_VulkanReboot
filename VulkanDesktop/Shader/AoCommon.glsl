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

// Weight for samples beyond radius (1 at contact, 0 at radius). Used by GTAO and HBAO world-range tests.
float Ao_GtaoDistanceFalloff(float aDist, float aRadius, float aPower)
{
    return 1.0 - clamp(pow(aDist / max(aRadius, 1e-4), aPower), 0.0, 1.0);
}

// Octahedral normal encode for bent-normal export (RG8).
vec2 Ao_EncodeOctahedral(vec3 aN)
{
    const vec3 n = normalize(aN);
    vec2 p = n.xy * (1.0 / (abs(n.x) + abs(n.y) + abs(n.z)));
    if (n.z < 0.0) {
        p = (1.0 - abs(p.yx)) * sign(p);
    }
    return p * 0.5 + 0.5;
}

// Occluder height above the surface tangent plane (view space). ~0 on coplanar ground.
float Ao_SurfaceElevation(vec3 aDeltaView, vec3 aNormalView)
{
    return dot(aDeltaView, aNormalView);
}

// Project view-space position to NDC UV (xy) + depth (z).
vec2 Ao_ProjectViewPosToUv(vec3 aViewPos, mat4 aProj)
{
    const vec4 clip = aProj * vec4(aViewPos, 1.0);
    const vec3 ndc = clip.xyz / clip.w;
    return ndc.xy * 0.5 + 0.5;
}

// Fetch world + view position from G-buffer; false if UV is off-screen.
bool Ao_TryFetchGBufferPos(sampler2D aWorldPosTex, mat4 aView, vec2 aUV, out vec3 aOutWorldPos, out vec3 aOutViewPos)
{
    if (aUV.x < 0.0 || aUV.x > 1.0 || aUV.y < 0.0 || aUV.y > 1.0) {
        return false;
    }
    aOutWorldPos = texture(aWorldPosTex, aUV).rgb;
    aOutViewPos = (aView * vec4(aOutWorldPos, 1.0)).xyz;
    return true;
}
