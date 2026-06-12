// PbrShadow.glsl — directional shadow compare + sun-shadow lighting helpers. No #version.

// Scale IBL in sun shadow (skylight bounce stays partially lit for readable contact shadows).
const float PBR_SHADOWED_IBL_SCALE = 0.25;

void Pbr_ShadowProject(mat4 lightViewProj, vec3 worldPos, out vec2 shadowUv, out float compareDepth)
{
    vec4 projectedCoord = lightViewProj * vec4(worldPos, 1.0);
    projectedCoord /= projectedCoord.w;
    shadowUv = projectedCoord.xy * 0.5 + 0.5;
    compareDepth = projectedCoord.z;
}

float Pbr_ShadowVisibility(sampler2DShadow shadowMap, mat4 lightViewProj, vec3 worldPos, vec4 shadowParams)
{
    if (shadowParams.z < 0.5) {
        return 1.0;
    }

    vec2 shadowUv;
    float compareDepth;
    Pbr_ShadowProject(lightViewProj, worldPos, shadowUv, compareDepth);

    // Border-white compare sampler handles out-of-frustum (CLAMP_TO_BORDER + white).
    return texture(shadowMap, vec3(shadowUv, compareDepth));
}

vec3 Pbr_ModulateAmbientForSunShadow(vec3 ambient, float sunShadow, vec4 shadowParams)
{
    if (shadowParams.z < 0.5) {
        return ambient;
    }
    return ambient * mix(PBR_SHADOWED_IBL_SCALE, 1.0, sunShadow);
}
