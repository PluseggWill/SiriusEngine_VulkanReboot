#version 450

// FG v0 slice 3: clustered deferred resolve (diffuse + ambient; sun via cluster SSBO).
// Keep in sync with Gfx_ClusterLighting.h.

const uint kMaxLightsPerCluster = 8u;

struct ClusterLight {
    vec4 direction;
    vec4 color;
};

struct ClusterLightList {
    uint lightCount;
    uint lightIndices[kMaxLightsPerCluster];
};

layout(push_constant) uniform PushConstants {
    uvec4 grid;           // tilesX, tilesY, tileSize, depthSlice
    vec4 ambientColor;
    vec4 viewWorldPos;    // reserved (specular / PBR later)
} pc;

layout(set = 0, binding = 0) uniform sampler2D gbufferAlbedo;
layout(set = 0, binding = 1) uniform sampler2D gbufferNormalRoughness;

layout(set = 0, binding = 2) readonly buffer Lights {
    ClusterLight lights[];
};

layout(set = 0, binding = 3) readonly buffer ClusterLists {
    ClusterLightList lists[];
};

layout(location = 0) in vec2 vUV;
layout(location = 0) out vec4 outColor;

void main()
{
    const vec3 albedo = texture(gbufferAlbedo, vUV).rgb;
    const vec4 normalRoughness = texture(gbufferNormalRoughness, vUV);
    const vec3 N = normalize(normalRoughness.xyz);

    const uint tileX = uint(gl_FragCoord.x) / pc.grid.z;
    const uint tileY = uint(gl_FragCoord.y) / pc.grid.z;
    const uint clusterId = tileX + tileY * pc.grid.x + pc.grid.w * pc.grid.x * pc.grid.y;

    vec3 lit = pc.ambientColor.rgb * albedo;

    const ClusterLightList cluster = lists[clusterId];
    const uint lightCount = min(cluster.lightCount, kMaxLightsPerCluster);
    for (uint i = 0u; i < lightCount; ++i) {
        const ClusterLight light = lights[cluster.lightIndices[i]];
        const vec3 L = normalize(light.direction.xyz);
        const float ndotl = max(dot(N, L), 0.0);
        lit += light.color.rgb * albedo * ndotl;
    }

    outColor = vec4(lit, 1.0);
}
