// PbrShadow.glsl — directional shadow compare (direct sun only). No #version.
// CONTRACT: shadowParams.x = SSR enabled; shadowParams.y = specular occlusion enabled;
// shadowParams.z = compare active (0/1); shadowParams.w = 1/shadowMapSize for PCF texel stride.
// IBL diffuse is not modulated by sun shadow; specular IBL uses mix(kMin, 1.0, sunShadow) in deferred/forward.

void Pbr_ShadowProject(mat4 lightViewProj, vec3 worldPos, out vec2 shadowUv, out float compareDepth)
{
    vec4 projectedCoord = lightViewProj * vec4(worldPos, 1.0);
    projectedCoord /= projectedCoord.w;
    shadowUv = projectedCoord.xy * 0.5 + 0.5;
    // GLM clip Z is [-1, 1]; hardware depth compare uses the same viewport mapping as the shadow pass write.
    compareDepth = projectedCoord.z * 0.5 + 0.5;
}

float Pbr_ShadowCompareTap(sampler2DShadow shadowMap, vec2 shadowUv, float compareDepth)
{
    // GREATER_OR_EQUAL + border white: out-of-frustum samples resolve to lit (Khronos contract).
    return texture(shadowMap, vec3(shadowUv, compareDepth));
}

float Pbr_ShadowVisibility(sampler2DShadow shadowMap, mat4 lightViewProj, vec3 worldPos, vec4 shadowParams)
{
    if (shadowParams.z < 0.5) {
        return 1.0;
    }

    vec2 shadowUv;
    float compareDepth;
    Pbr_ShadowProject(lightViewProj, worldPos, shadowUv, compareDepth);

    const float texel = shadowParams.w;
    if (texel <= 0.0) {
        return Pbr_ShadowCompareTap(shadowMap, shadowUv, compareDepth);
    }

    // 3x3 PCF — stable contact shadows without single-tap noise.
    float visibility = 0.0;
    for (int y = -1; y <= 1; ++y) {
        for (int x = -1; x <= 1; ++x) {
            const vec2 offset = vec2(float(x), float(y)) * texel;
            visibility += Pbr_ShadowCompareTap(shadowMap, shadowUv + offset, compareDepth);
        }
    }
    return visibility / 9.0;
}
