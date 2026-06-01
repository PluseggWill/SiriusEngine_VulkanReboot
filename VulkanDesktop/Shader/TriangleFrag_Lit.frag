#version 450

// Set 0 — frame (matches TriangleVertex.vert set 0 for camera; env here for fragment).
layout(set = 0, binding = 1) uniform EnvironmentData {
    vec4 fogColor;
    vec4 fogDistances;
    vec4 ambientColor;
    vec4 sunlightDirection;
    vec4 sunlightColor;
    vec4 viewWorldPos;
} envData;

// Set 1 — material batch (bound once per batch in RecordScenePass).
layout(set = 1, binding = 0) uniform sampler2D texSampler;
// Stage 1 forward contract (std140). PBR fields uploaded; shading still Blinn-Phong until PBR perm lands.
layout(set = 1, binding = 1) uniform MaterialData {
    vec4 baseColorFactor;
    float roughness;
    float metallic;
    float alpha;
    uint alphaMode;  // 0=opaque, 1=mask, 2=blend
} material;

layout(location = 0) in vec3 inColor;
layout(location = 1) in vec2 inTexCoord;
layout(location = 2) in vec3 inWorldNormal;
layout(location = 3) in vec3 inWorldPos;
layout(location = 4) flat in uint inMaterialIndex;  // from Set 2; unused on batch material path

layout(location = 0) out vec4 outColor;

void main()
{
    const float specularStrength = envData.fogDistances.x;
    const float shininess = max(envData.fogDistances.y, 1.0);
    const float textureBlend = clamp(envData.fogDistances.z, 0.0, 1.0);

    const vec3 texAlbedo = texture(texSampler, inTexCoord).rgb;
    const vec3 albedo = mix(inColor, texAlbedo, textureBlend);

    const vec3 N = normalize(inWorldNormal);
    const vec3 L = normalize(envData.sunlightDirection.xyz);
    const vec3 V = normalize(envData.viewWorldPos.xyz - inWorldPos);
    const vec3 H = normalize(L + V);

    const vec3 ambient = envData.ambientColor.rgb * albedo;
    const float ndotl = max(dot(N, L), 0.0);
    const vec3 diffuse = envData.sunlightColor.rgb * albedo * ndotl;

    const float spec = specularStrength * pow(max(dot(N, H), 0.0), shininess);
    const vec3 specular = envData.sunlightColor.rgb * spec;

    outColor = vec4(ambient + diffuse + specular, clamp(material.alpha, 0.0, 1.0));
#ifdef ALPHA_CLIP
    if (outColor.a < 0.5) {
        discard;
    }
#endif
}
