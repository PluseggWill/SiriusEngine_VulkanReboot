// PbrIbl.glsl — split-sum IBL + directional shadow PCF. Included by lit/deferred; no #version.
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

vec3 Pbr_SamplePrefilter(samplerCube prefilterMap, vec3 R, float roughness)
{
    const float lod = roughness * 4.0;
    return textureLod(prefilterMap, normalize(R), lod).rgb;
}

float Pbr_ShadowVisibility(sampler2DShadow shadowMap, mat4 lightViewProj, vec3 worldPos, vec3 N, vec4 shadowParams)
{
    if (shadowParams.z < 0.5) {
        return 1.0;
    }

    const vec4 lightClip = lightViewProj * vec4(worldPos, 1.0);
    const vec3 projCoords = lightClip.xyz / lightClip.w;
    const vec2 shadowUv = projCoords.xy * 0.5 + 0.5;
    if (projCoords.z > 1.0
        || shadowUv.x < 0.0 || shadowUv.x > 1.0
        || shadowUv.y < 0.0 || shadowUv.y > 1.0) {
        return 1.0;
    }

    const float bias = shadowParams.x + shadowParams.y * (1.0 - max(dot(N, normalize(-V)), 0.0));
    const float compareDepth = projCoords.z - bias;

    float visibility = 0.0;
    const vec2 texelSize = vec2(shadowParams.w);
    for (int x = -1; x <= 1; ++x) {
        for (int y = -1; y <= 1; ++y) {
            const vec2 offset = vec2(float(x), float(y)) * texelSize;
            visibility += texture(shadowMap, vec3(shadowUv + offset, compareDepth));
        }
    }
    return visibility / 9.0;
}

vec3 Pbr_EvalIbl(vec3 N, vec3 V, vec3 albedo, float metallic, float roughness,
                 samplerCube irradianceMap, samplerCube prefilterMap, sampler2D brdfLut,
                 float iblIntensity, uint iblEnabled)
{
    if ((iblEnabled & PBR_IBL_ENABLED_FLAG) == 0u) {
        return vec3(0.0);
    }

    const float clampedRoughness = clamp(max(roughness, PBR_MIN_ROUGHNESS), 0.0, 1.0);
    const vec3 F0 = mix(vec3(PBR_DIELECTRIC_F0), albedo, metallic);
    const vec3 R = reflect(-V, N);
    const float NdotV = max(dot(N, V), 0.0);

    const vec3 irradiance = Pbr_SampleIrradiance(irradianceMap, N);
    const vec3 prefilteredColor = Pbr_SamplePrefilter(prefilterMap, R, clampedRoughness);
    const vec2 brdf = Pbr_BrdfLut(NdotV, clampedRoughness, brdfLut);
    const vec3 F = Pbr_FresnelSchlick(NdotV, F0);

    const vec3 diffuse = irradiance * albedo * (1.0 - metallic);
    const vec3 specular = prefilteredColor * (F * brdf.x + brdf.y);

    return (diffuse + specular) * iblIntensity;
}

vec3 Pbr_SampleSky(samplerCube skyMap, vec3 worldPos, vec3 cameraPos)
{
    const vec3 dir = normalize(worldPos - cameraPos);
    return textureLod(skyMap, dir, 0.0).rgb;
}
