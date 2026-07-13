// PbrIbl.glsl — split-sum IBL. Included by lit/deferred; no #version.
// Requires PbrDirect.glsl before this file (PBR_DIELECTRIC_F0, PBR_MIN_ROUGHNESS, Pbr_FresnelSchlick).

const uint PBR_IBL_ENABLED_FLAG = 1u;

vec2 Pbr_BrdfLut(float NdotV, float roughness, sampler2D brdfLut)
{
    const vec2 uv = vec2(clamp(NdotV, 0.0, 1.0), clamp(roughness, 0.0, 1.0));
    return texture(brdfLut, uv).rg;
}

vec3 Pbr_SampleIrradiance(samplerCube irradianceMap, vec3 N)
{
    // Offline bake stores cosine-weighted average radiance (E/π). Lambert diffuse uses albedo * (E/π).
    return texture(irradianceMap, normalize(N)).rgb;
}

vec3 Pbr_SamplePrefilter(samplerCube prefilterMap, vec3 R, float roughness, float maxMipLevel)
{
    const float lod = clamp(roughness, 0.0, 1.0) * max(maxMipLevel, 0.0);
    return textureLod(prefilterMap, normalize(R), lod).rgb;
}

// Frostbite Listing 26 / Filament SpecularAO_Lagarde — roughness-aware specular occlusion from SSAO visibility.
float Pbr_SpecularOcclusion(float aNdotV, float aVisibility, float aRoughness)
{
    const float clampedRoughness = clamp(aRoughness, 0.0, 1.0);
    const float visibility = clamp(aVisibility, 0.0, 1.0);
    const float ndotv = clamp(aNdotV, 0.0, 1.0);
    return clamp(pow(ndotv + visibility, exp2(-16.0 * clampedRoughness - 1.0)) - 1.0 + visibility, 0.0, 1.0);
}

// Jimenez 2016 / Filament SpecularAO_Cones — bent-normal cone vs reflection vector.
float Pbr_SpecularOcclusionCones(vec3 aBentNormal, vec3 aReflection, float aVisibility, float aRoughness)
{
    const vec3 bentN = normalize(aBentNormal);
    const vec3 R = normalize(aReflection);
    const float visibility = clamp(aVisibility, 0.0, 1.0);
    const float cosB = clamp(dot(bentN, R), 0.0, 1.0);
    const float cosAlpha = clamp(1.0 - aRoughness * aRoughness, 0.0, 1.0);
    return visibility * clamp((cosAlpha + cosB - cosAlpha * cosB) / max(cosB, 1e-4), 0.0, 1.0);
}

// Parallax-corrected box probe (Frostbite §4.9.5) — uses distant prefilter as probe content.
vec3 Pbr_SampleParallaxBoxProbe(samplerCube aProbeMap, vec3 aWorldPos, vec3 aReflection, vec3 aBoxMin, vec3 aBoxMax, float aRoughness, float aMaxMipLevel)
{
    const vec3 nrdir = normalize(aReflection);
    const vec3 rbmax = (aBoxMax - aWorldPos) / nrdir;
    const vec3 rbmin = (aBoxMin - aWorldPos) / nrdir;
    const vec3 rbminmax = mix(rbmin, rbmax, step(vec3(0.0), nrdir));
    const float correction = min(rbminmax.x, min(rbminmax.y, rbminmax.z));
    const vec3 pos = aWorldPos + nrdir * max(correction, 0.0);
    const vec3 boxCenter = 0.5 * (aBoxMax + aBoxMin);
    const vec3 localPos = pos - boxCenter;
    const vec3 extents = max(0.5 * (aBoxMax - aBoxMin), vec3(1e-4));
    const vec3 dir = localPos / extents;
    return Pbr_SamplePrefilter(aProbeMap, dir, aRoughness, aMaxMipLevel);
}

// Octahedral encode/decode for bent-normal export (GTAO → specular cones).
vec2 Pbr_EncodeOctahedral(vec3 aN)
{
    const vec3 n = normalize(aN);
    vec2 p = n.xy * (1.0 / (abs(n.x) + abs(n.y) + abs(n.z)));
    if (n.z < 0.0) {
        p = (1.0 - abs(p.yx)) * sign(p);
    }
    return p * 0.5 + 0.5;
}

vec3 Pbr_DecodeOctahedral(vec2 aEnc)
{
    vec2 f = aEnc * 2.0 - 1.0;
    vec3 n = vec3(f.x, f.y, 1.0 - abs(f.x) - abs(f.y));
    if (n.z < 0.0) {
        n.xy = (1.0 - abs(n.yx)) * sign(n.xy);
    }
    return normalize(n);
}

vec3 Pbr_SampleSky(samplerCube skyMap, vec3 worldPos, vec3 cameraPos)
{
    const vec3 dir = normalize(worldPos - cameraPos);
    return textureLod(skyMap, dir, 0.0).rgb;
}
