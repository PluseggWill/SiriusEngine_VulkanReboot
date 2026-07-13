#version 450

#include "PbrDirect.glsl"

layout(set = 0, binding = 1) uniform EnvironmentData {
    vec4 fogColor;
    vec4 fogDistances;
    vec4 ambientColor;
    vec4 sunlightDirection;
    vec4 sunlightColor;
    vec4 viewWorldPos;
} envData;

layout(set = 1, binding = 0) uniform sampler2D texSampler;
layout(set = 1, binding = 1) uniform MaterialData {
    vec4 baseColorFactor;
    float roughness;
    float metallic;
    float alpha;
    uint alphaMode;
} material;

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
    const vec3 texAlbedo = texture(texSampler, inTexCoord).rgb;
    const vec3 albedo = mix(inColor, texAlbedo, textureBlend) * material.baseColorFactor.rgb;

    const vec3 N = normalize(inWorldNormal);
    const vec2 mr = Pbr_ClampMetallicRoughness(material.metallic, material.roughness);
    outAlbedo = vec4(albedo, mr.x);
    outNormalRoughness = vec4(N, mr.y);
    outWorldPosition = vec4(inWorldPos, 1.0);

    vec2 currUv = inCurrClip.xy / max(inCurrClip.w, 1e-6) * 0.5 + 0.5;
    vec2 prevUv = inPrevClip.xy / max(inPrevClip.w, 1e-6) * 0.5 + 0.5;
    outMotionVector = currUv - prevUv;
}
