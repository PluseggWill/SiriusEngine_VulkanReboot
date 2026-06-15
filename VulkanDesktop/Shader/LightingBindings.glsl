// LightingBindings.glsl — Set 0 lighting resources (S5). Include after EnvironmentData block.
#include "PbrShadow.glsl"

layout(set = 0, binding = 2) uniform LightingGlobals {
    mat4 lightViewProj;
    vec4 shadowParams;  // z = enabled (0/1)
    vec4 iblParams;     // x = intensity, y = enabled, z = prefilter max mip level
} lightingGlobals;

layout(set = 0, binding = 3) uniform sampler2DShadow shadowMap;
layout(set = 0, binding = 4) uniform samplerCube irradianceMap;
layout(set = 0, binding = 5) uniform samplerCube prefilterMap;
layout(set = 0, binding = 6) uniform sampler2D brdfLut;
layout(set = 0, binding = 7) uniform samplerCube skyMap;

float Pbr_SceneSunShadow(vec3 worldPos)
{
    return Pbr_ShadowVisibility(shadowMap, lightingGlobals.lightViewProj, worldPos, lightingGlobals.shadowParams);
}

vec3 Pbr_EvalSceneSunRadiance(vec3 worldPos, vec3 sunColor)
{
    return sunColor * Pbr_SceneSunShadow(worldPos);
}

vec3 Pbr_EvalSceneAmbient(vec3 N, vec3 V, vec3 albedo, float metallic, float roughness, vec3 fallbackAmbient, vec3 worldPos)
{
    const uint iblEnabled = uint(lightingGlobals.iblParams.y + 0.5);
    const float sunShadow = Pbr_SceneSunShadow(worldPos);

    vec3 ambient;
    if (iblEnabled != 0u) {
        ambient = Pbr_EvalIbl(N, V, albedo, metallic, roughness, irradianceMap, prefilterMap, brdfLut, lightingGlobals.iblParams.x, iblEnabled,
                              lightingGlobals.iblParams.z);
    } else {
        ambient = fallbackAmbient * albedo;
    }
    return Pbr_ModulateAmbientForSunShadow(ambient, sunShadow, lightingGlobals.shadowParams);
}
