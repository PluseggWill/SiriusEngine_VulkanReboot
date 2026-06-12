#version 450

// FG v0: clustered deferred resolve — Cook-Torrance direct PBR + IBL + shadow (S5).
#include "PbrDirect.glsl"
#include "PbrIbl.glsl"
#include "PbrShadow.glsl"

const uint kMaxLightsPerCluster = 8u;
const uint kSunLightIndex = 0u;
const uint kDebugViewDepth = 1u;
const uint kDebugViewWorldNormal = 2u;
const uint kDebugViewShadowMap = 3u;

struct ClusterLight {
    vec4 direction;
    vec4 color;
};

struct ClusterLightList {
    uint lightCount;
    uint lightIndices[kMaxLightsPerCluster];
};

layout(push_constant) uniform PushConstants {
    uvec4 grid;              // tilesX, tilesY, tileSize, depthSlice
    vec4 ambientColor;
    vec4 viewWorldPos;
    vec4 legacyAndDebug;     // x/y unused (layout pad); z = Gfx_DebugViewMode
    mat4 invViewProj;
} pc;

layout(set = 0, binding = 0) uniform sampler2D gbufferAlbedo;
layout(set = 0, binding = 1) uniform sampler2D gbufferNormalRoughness;
layout(set = 0, binding = 2) readonly buffer Lights {
    ClusterLight lights[];
};
layout(set = 0, binding = 3) readonly buffer ClusterLists {
    ClusterLightList lists[];
};
layout(set = 0, binding = 4) uniform sampler2D gbufferDepth;
layout(set = 0, binding = 5) uniform LightingGlobals {
    mat4 lightViewProj;
    vec4 shadowParams;  // z = enabled
    vec4 iblParams;     // x = intensity, y = enabled, z = prefilter max mip
} lightingGlobals;
layout(set = 0, binding = 6) uniform sampler2DShadow shadowMap;
layout(set = 0, binding = 7) uniform samplerCube irradianceMap;
layout(set = 0, binding = 8) uniform samplerCube prefilterMap;
layout(set = 0, binding = 9) uniform sampler2D brdfLut;
layout(set = 0, binding = 10) uniform samplerCube skyMap;
// binding 11: non-compare depth read (descriptor parity; optional tooling).
layout(set = 0, binding = 11) uniform sampler2D shadowMapDepth;
layout(set = 0, binding = 12) uniform sampler2D gbufferWorldPosition;

layout(location = 0) in vec2 vUV;
layout(location = 0) out vec4 outColor;

vec3 reconstructWorldPos(vec2 aUV, float aDepth)
{
    const vec4 ndc = vec4(aUV * 2.0 - 1.0, aDepth, 1.0);
    const vec4 worldH = pc.invViewProj * ndc;
    return worldH.xyz / worldH.w;
}

vec4 applyDebugView(vec4 aLitColor, vec3 aWorldNormal, float aDepth, vec3 aWorldPos)
{
    const uint viewMode = uint(pc.legacyAndDebug.z + 0.5);
    if (viewMode == kDebugViewDepth) {
        return vec4(vec3(aDepth), 1.0);
    }
    if (viewMode == kDebugViewWorldNormal) {
        return vec4(normalize(aWorldNormal) * 0.5 + 0.5, 1.0);
    }
    if (viewMode == kDebugViewShadowMap) {
        if (aDepth >= 0.999) {
            return vec4(0.0, 0.0, 0.0, 1.0);
        }
        // Same compare as lit path (forward deferred parity); not raw depth map.
        const float visibility = Pbr_ShadowVisibility(shadowMap, lightingGlobals.lightViewProj, aWorldPos, lightingGlobals.shadowParams);
        return vec4(vec3(visibility), 1.0);
    }
    return aLitColor;
}

void main()
{
    const vec4 albedoMetallic = texture(gbufferAlbedo, vUV);
    const vec3 albedo = albedoMetallic.rgb;
    const float metallic = albedoMetallic.a;
    const vec4 normalRoughness = texture(gbufferNormalRoughness, vUV);
    const vec3 N = normalize(normalRoughness.xyz);
    const float roughness = normalRoughness.w;
    const float depth = texture(gbufferDepth, vUV).r;

    vec3 worldPos;
    if (depth >= 0.999) {
        worldPos = reconstructWorldPos(vUV, depth);
    } else {
        worldPos = texture(gbufferWorldPosition, vUV).rgb;
    }

    const vec3 V = normalize(pc.viewWorldPos.xyz - worldPos);

    if (depth >= 0.999) {
        outColor = vec4(Pbr_SampleSky(skyMap, worldPos, pc.viewWorldPos.xyz), 1.0);
        return;
    }

    const uint tileX = uint(gl_FragCoord.x) / pc.grid.z;
    const uint tileY = uint(gl_FragCoord.y) / pc.grid.z;
    const uint clusterId = tileX + tileY * pc.grid.x + pc.grid.w * pc.grid.x * pc.grid.y;

    const float sunShadow = Pbr_ShadowVisibility(shadowMap, lightingGlobals.lightViewProj, worldPos, lightingGlobals.shadowParams);

    const uint iblEnabled = uint(lightingGlobals.iblParams.y + 0.5);
    vec3 lit = Pbr_EvalIbl(N, V, albedo, metallic, roughness, irradianceMap, prefilterMap, brdfLut, lightingGlobals.iblParams.x, iblEnabled,
                           lightingGlobals.iblParams.z);
    if (iblEnabled == 0u) {
        lit = pc.ambientColor.rgb * albedo;
    }
    lit = Pbr_ModulateAmbientForSunShadow(lit, sunShadow, lightingGlobals.shadowParams);

    const ClusterLightList cluster = lists[clusterId];
    const uint lightCount = min(cluster.lightCount, kMaxLightsPerCluster);
    for (uint i = 0u; i < lightCount; ++i) {
        const ClusterLight light = lights[cluster.lightIndices[i]];
        const vec3 L = normalize(light.direction.xyz);
        vec3 radiance = light.color.rgb;
        if (cluster.lightIndices[i] == kSunLightIndex) {
            radiance *= sunShadow;
        }
        lit += Pbr_EvalDirect(N, V, L, albedo, metallic, roughness, radiance);
    }

    outColor = applyDebugView(vec4(lit, 1.0), N, depth, worldPos);
}
