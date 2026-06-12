// LightingBindings.glsl — Set 0 lighting resources (S5). Include after EnvironmentData block.
layout(set = 0, binding = 2) uniform LightingGlobals {
    mat4 lightViewProj;
    vec4 shadowParams;  // x=bias, y=normalOffset, z=enabled, w=texelSize
    vec4 iblParams;     // x=intensity, y=enabled, zw pad
} lightingGlobals;

layout(set = 0, binding = 3) uniform sampler2DShadow shadowMap;
layout(set = 0, binding = 4) uniform samplerCube irradianceMap;
layout(set = 0, binding = 5) uniform samplerCube prefilterMap;
layout(set = 0, binding = 6) uniform sampler2D brdfLut;
layout(set = 0, binding = 7) uniform samplerCube skyMap;

vec3 Pbr_EvalSceneSunRadiance(vec3 N, vec3 V, vec3 worldPos, vec3 sunDirection, vec3 sunColor)
{
    vec3 radiance = sunColor;
    radiance *= Pbr_ShadowVisibility(shadowMap, lightingGlobals.lightViewProj, worldPos, N, sunDirection, lightingGlobals.shadowParams);
    return radiance;
}

vec3 Pbr_EvalSceneAmbient(vec3 N, vec3 V, vec3 albedo, float metallic, float roughness, vec3 fallbackAmbient, vec3 albedoScale)
{
    const uint iblEnabled = uint(lightingGlobals.iblParams.y + 0.5);
    if (iblEnabled != 0u) {
        return Pbr_EvalIbl(N, V, albedo, metallic, roughness, irradianceMap, prefilterMap, brdfLut, lightingGlobals.iblParams.x, iblEnabled);
    }
    return fallbackAmbient * albedoScale;
}
