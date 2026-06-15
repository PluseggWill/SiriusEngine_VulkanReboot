#version 450
#extension GL_EXT_nonuniform_qualifier : enable

#include "PbrDirect.glsl"
#include "PbrIbl.glsl"
#include "LightingBindings.glsl"

layout(set = 0, binding = 1) uniform EnvironmentData {
    vec4 fogColor;
    vec4 fogDistances;  // z = texture blend; w = Gfx_DebugViewMode
    vec4 ambientColor;
    vec4 sunlightDirection;
    vec4 sunlightColor;
    vec4 viewWorldPos;
} envData;

#define VK_MAX_BINDLESS_TEXTURES 64
layout(set = 1, binding = 0) uniform sampler2D u_Textures[VK_MAX_BINDLESS_TEXTURES];

struct GpuMaterialEntry {
    uint textureIndex;
    float roughness;
    float metallic;
    float alpha;
    uint alphaMode;
    vec4 baseColorFactor;
};

layout(set = 1, binding = 1) readonly buffer MaterialTable {
    GpuMaterialEntry entries[];
} materials;

layout(location = 0) in vec3 inColor;
layout(location = 1) in vec2 inTexCoord;
layout(location = 2) in vec3 inWorldNormal;
layout(location = 3) in vec3 inWorldPos;
layout(location = 4) flat in uint inMaterialIndex;

layout(location = 0) out vec4 outColor;

const uint kAlphaModeMask = 1u;
const uint kDebugViewDepth = 1u;
const uint kDebugViewWorldNormal = 2u;
const uint kDebugViewShadowMap = 3u;

vec4 applyDebugView(vec4 aLitColor, vec3 aWorldNormal, vec3 aWorldPos)
{
    const uint viewMode = uint(envData.fogDistances.w + 0.5);
    if (viewMode == kDebugViewDepth) {
        return vec4(vec3(gl_FragCoord.z), 1.0);
    }
    if (viewMode == kDebugViewWorldNormal) {
        return vec4(normalize(aWorldNormal) * 0.5 + 0.5, 1.0);
    }
    if (viewMode == kDebugViewShadowMap) {
        const float visibility = Pbr_ShadowVisibility(shadowMap, lightingGlobals.lightViewProj, aWorldPos, lightingGlobals.shadowParams);
        return vec4(vec3(visibility), 1.0);
    }
    return aLitColor;
}

void main()
{
    const float textureBlend = clamp(envData.fogDistances.z, 0.0, 1.0);
    const GpuMaterialEntry mat = materials.entries[inMaterialIndex];

    const vec3 texAlbedo = texture(nonuniformEXT(u_Textures[mat.textureIndex]), inTexCoord).rgb;
    const vec3 albedo = mix(inColor, texAlbedo, textureBlend) * mat.baseColorFactor.rgb;

    const vec3 N = normalize(inWorldNormal);
    const vec3 V = normalize(envData.viewWorldPos.xyz - inWorldPos);
    const vec2 mr = Pbr_ClampMetallicRoughness(mat.metallic, mat.roughness);

    vec3 color = Pbr_EvalSceneAmbient(N, V, albedo, mr.x, mr.y, envData.ambientColor.rgb, inWorldPos);

    const vec3 sunRadiance = Pbr_EvalSceneSunRadiance(inWorldPos, envData.sunlightColor.rgb);
    color += Pbr_EvalDirect(N, V, normalize(envData.sunlightDirection.xyz), albedo, mr.x, mr.y, sunRadiance);

    outColor = vec4(color, clamp(mat.alpha, 0.0, 1.0));

    if (mat.alphaMode == kAlphaModeMask && outColor.a < 0.5) {
        discard;
    }

    outColor = applyDebugView(outColor, N, inWorldPos);
}
