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
    return texture(irradianceMap, normalize(N)).rgb;
}

vec3 Pbr_SamplePrefilter(samplerCube prefilterMap, vec3 R, float roughness, float maxMipLevel)
{
    const float lod = clamp(roughness, 0.0, 1.0) * max(maxMipLevel, 0.0);
    return textureLod(prefilterMap, normalize(R), lod).rgb;
}

vec3 Pbr_EvalIbl(vec3 N, vec3 V, vec3 albedo, float metallic, float roughness,
                 samplerCube irradianceMap, samplerCube prefilterMap, sampler2D brdfLut,
                 float iblIntensity, uint iblEnabled, float prefilterMaxMip)
{
    if ((iblEnabled & PBR_IBL_ENABLED_FLAG) == 0u) {
        return vec3(0.0);
    }

    const float clampedRoughness = clamp(max(roughness, PBR_MIN_ROUGHNESS), 0.0, 1.0);
    const vec3 F0 = mix(vec3(PBR_DIELECTRIC_F0), albedo, metallic);
    const vec3 R = reflect(-V, N);
    const float NdotV = max(dot(N, V), 0.0);

    const vec3 irradiance = Pbr_SampleIrradiance(irradianceMap, N);
    const vec3 prefilteredColor = Pbr_SamplePrefilter(prefilterMap, R, clampedRoughness, prefilterMaxMip);
    const vec2 brdf = Pbr_BrdfLut(NdotV, clampedRoughness, brdfLut);
    const vec3 F = Pbr_FresnelSchlick(NdotV, F0);

    const vec3 kD = (vec3(1.0) - F) * (1.0 - metallic);
    const vec3 diffuse = irradiance * kD * albedo;
    const vec3 specular = prefilteredColor * (F * brdf.x + brdf.y);

    return (diffuse + specular) * iblIntensity;
}

vec3 Pbr_SampleSky(samplerCube skyMap, vec3 worldPos, vec3 cameraPos)
{
    const vec3 dir = normalize(worldPos - cameraPos);
    return textureLod(skyMap, dir, 0.0).rgb;
}
