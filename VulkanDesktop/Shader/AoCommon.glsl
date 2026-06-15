// Shared screen-space AO utilities (included by Ssao.comp / HbaoPlus.comp / AoUpsample.comp).

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

// GTAO slice arc integration (Jimenez 2016 eq. 12; h1/h2 = horizon angles, nAngle = normal vs slice plane).
float Ao_GtaoIntegrateArc(float h1, float h2, float nAngle)
{
    const float cosN = cos(nAngle);
    const float sinN = sin(nAngle);
    return 0.25 * (
        -cos(2.0 * h1 - 2.0 * nAngle) - cos(2.0 * h2 - 2.0 * nAngle)
        + 2.0 * cosN * (h1 * sinN + h2 * sinN)
        - 2.0 * cosN * (cos(h1) * sinN + cos(h2) * sinN));
}

// Screen-space march falloff (0 at radius, 1 at contact).
float Ao_GtaoDistanceFalloff(float aDist, float aRadius, float aPower)
{
    return 1.0 - clamp(pow(aDist / max(aRadius, 1e-4), aPower), 0.0, 1.0);
}
