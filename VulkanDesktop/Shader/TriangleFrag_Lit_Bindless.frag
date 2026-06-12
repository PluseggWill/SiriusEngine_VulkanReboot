#version 450
#extension GL_EXT_nonuniform_qualifier : enable

#include "PbrDirect.glsl"

layout(set = 0, binding = 1) uniform EnvironmentData {
    vec4 fogColor;
    vec4 fogDistances;  // xyz lighting; w = Gfx_DebugViewMode (see TriangleFrag_Lit.frag)
    vec4 ambientColor;
    vec4 sunlightDirection;
    vec4 sunlightColor;
    vec4 viewWorldPos;
} envData;

// Must match VkDescriptorPolicy::kMaxBindlessTextures (keep in sync with C++ / reflection_lit_bindless.json).
#define VK_MAX_BINDLESS_TEXTURES 64
layout(set = 1, binding = 0) uniform sampler2D u_Textures[VK_MAX_BINDLESS_TEXTURES];

// std430 — must match GpuMaterialTableEntry in Vk_Types.h
struct GpuMaterialEntry {
    uint textureIndex;
    float roughness;
    float metallic;
    float alpha;
    uint alphaMode;
    vec4 baseColorFactor;  // offset 32 (implicit std430 padding after alphaMode)
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

vec4 applyDebugView(vec4 aLitColor, vec3 aWorldNormal)
{
    const uint viewMode = uint(envData.fogDistances.w + 0.5);
    if (viewMode == kDebugViewDepth) {
        return vec4(vec3(gl_FragCoord.z), 1.0);
    }
    if (viewMode == kDebugViewWorldNormal) {
        return vec4(normalize(aWorldNormal) * 0.5 + 0.5, 1.0);
    }
    return aLitColor;
}

void main()
{
    const float textureBlend = clamp(envData.fogDistances.z, 0.0, 1.0);

    const GpuMaterialEntry mat = materials.entries[inMaterialIndex];
    const uint texIndex = mat.textureIndex;
    const float alpha = mat.alpha;

    const vec3 texAlbedo = texture(nonuniformEXT(u_Textures[texIndex]), inTexCoord).rgb;
    const vec3 albedo = mix(inColor, texAlbedo, textureBlend) * mat.baseColorFactor.rgb;

    const vec3 N = normalize(inWorldNormal);
    const vec3 L = normalize(envData.sunlightDirection.xyz);
    const vec3 V = normalize(envData.viewWorldPos.xyz - inWorldPos);
    const float metallic = clamp(mat.metallic, 0.0, 1.0);
    const float roughness = clamp(mat.roughness, 0.0, 1.0);
    const float NdotL = max(dot(N, L), 0.0);

    vec3 color = envData.ambientColor.rgb * albedo;
    if (NdotL > 0.0) {
        color += Pbr_EvalDirect(N, V, L, albedo, metallic, roughness, envData.sunlightColor.rgb) * NdotL;
    }

    outColor = vec4(color, clamp(alpha, 0.0, 1.0));

    if (mat.alphaMode == kAlphaModeMask && outColor.a < 0.5) {  // per-material mask
        discard;
    }

    outColor = applyDebugView(outColor, N);
}
