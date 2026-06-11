#version 450

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

layout(location = 0) out vec4 outAlbedo;
layout(location = 1) out vec4 outNormalRoughness;

void main()
{
    const float textureBlend = clamp(envData.fogDistances.z, 0.0, 1.0);
    const vec3 texAlbedo = texture(texSampler, inTexCoord).rgb;
    const vec3 albedo = mix(inColor, texAlbedo, textureBlend) * material.baseColorFactor.rgb;

    const vec3 N = normalize(inWorldNormal);
    outAlbedo = vec4(albedo, 1.0);
    outNormalRoughness = vec4(N, material.roughness);
}
