// LightingBindings.glsl — Set 0 lighting resources (S5). Include after EnvironmentData block.
#include "PbrShadow.glsl"

layout(set = 0, binding = 2) uniform LightingGlobals {
    mat4 lightViewProj;
    vec4 shadowParams;  // x = SSR enabled, y = specular occlusion enabled, z = compare active (0/1), w = 1/shadowMapSize
    vec4 iblParams;     // x = intensity, y = enabled, z = prefilter max mip, w = unused
} lightingGlobals;

layout(set = 0, binding = 3) uniform sampler2DShadow shadowMap;
layout(set = 0, binding = 4) uniform samplerCube irradianceMap;
layout(set = 0, binding = 5) uniform samplerCube prefilterMap;
layout(set = 0, binding = 6) uniform sampler2D brdfLut;
layout(set = 0, binding = 7) uniform samplerCube skyMap;
layout(set = 0, binding = 8) uniform sampler2D aoMap;  // reserved; forward path does not sample yet (1x1 white fallback)

float Pbr_SceneSunShadow(vec3 worldPos)
{
    return Pbr_ShadowVisibility(shadowMap, lightingGlobals.lightViewProj, worldPos, lightingGlobals.shadowParams);
}

vec3 Pbr_EvalSceneSunRadiance(vec3 worldPos, vec3 sunColor)
{
    return sunColor * Pbr_SceneSunShadow(worldPos);
}

// Returns (diffuseIbl, specularIbl) — AO applies to diffuse only (mirrors DeferredLighting split).
vec3 Pbr_EvalSceneAmbient(vec3 N, vec3 V, vec3 albedo, float metallic, float roughness, vec3 fallbackAmbient, vec3 worldPos, out vec3 outSpecularIbl)
{
    outSpecularIbl = vec3(0.0);
    const uint iblEnabled = uint(lightingGlobals.iblParams.y + 0.5);
    if (iblEnabled != 0u) {
        const float clampedRoughness = clamp(max(roughness, PBR_MIN_ROUGHNESS), 0.0, 1.0);
        const vec3  F0               = mix(vec3(PBR_DIELECTRIC_F0), albedo, metallic);
        const vec3  R                = reflect(-V, N);
        const float NdotV            = max(dot(N, V), 0.0);
        const vec3  F                = Pbr_FresnelSchlick(NdotV, F0);
        // Diffuse IBL
        const vec3 kD = (vec3(1.0) - F) * (1.0 - metallic);
        vec3 diffuseIbl = Pbr_SampleIrradiance(irradianceMap, N) * kD * albedo * lightingGlobals.iblParams.x;
        const float sunShadow = Pbr_SceneSunShadow(worldPos);
        const vec3  prefilteredColor = Pbr_SamplePrefilter(prefilterMap, R, clampedRoughness, lightingGlobals.iblParams.z);
        const vec2  brdf             = Pbr_BrdfLut(NdotV, clampedRoughness, brdfLut);
        outSpecularIbl = prefilteredColor * (F * brdf.x + brdf.y) * sunShadow * lightingGlobals.iblParams.x;
        // Forward path does not have a stable screen-space AO UV mapping yet.
        return diffuseIbl;
    }
    return fallbackAmbient * albedo;
}
