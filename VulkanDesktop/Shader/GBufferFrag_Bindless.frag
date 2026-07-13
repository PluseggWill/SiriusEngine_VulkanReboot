#version 450
#extension GL_EXT_nonuniform_qualifier : enable

#include "PbrDirect.glsl"

layout(set = 0, binding = 1) uniform EnvironmentData {
    vec4 fogColor;
    vec4 fogDistances;
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
layout(location = 5) in vec4 inCurrClip;
layout(location = 6) in vec4 inPrevClip;

layout(location = 0) out vec4 outAlbedo;
layout(location = 1) out vec4 outNormalRoughness;
layout(location = 2) out vec4 outWorldPosition;
layout(location = 3) out vec2 outMotionVector;

void main()
{
    const float textureBlend = clamp(envData.fogDistances.z, 0.0, 1.0);
    const GpuMaterialEntry mat = materials.entries[inMaterialIndex];
    const vec3 texAlbedo = texture(nonuniformEXT(u_Textures[mat.textureIndex]), inTexCoord).rgb;
    const vec3 albedo = mix(inColor, texAlbedo, textureBlend) * mat.baseColorFactor.rgb;

    const vec3 N = normalize(inWorldNormal);
    const vec2 mr = Pbr_ClampMetallicRoughness(mat.metallic, mat.roughness);
    outAlbedo = vec4(albedo, mr.x);
    outNormalRoughness = vec4(N, mr.y);
    outWorldPosition = vec4(inWorldPos, 1.0);

    vec2 currUv = inCurrClip.xy / max(inCurrClip.w, 1e-6) * 0.5 + 0.5;
    vec2 prevUv = inPrevClip.xy / max(inPrevClip.w, 1e-6) * 0.5 + 0.5;
    outMotionVector = currUv - prevUv;
}
