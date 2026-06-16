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
const uint kDebugViewAo = 4u;
const uint kDebugViewHiZ = 5u;
const uint kDebugViewDdgi = 6u;

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
    vec4 legacyAndDebug;     // x = AO enabled, y = AO intensity, z = Gfx_DebugViewMode, w = AO power
    vec4 contactSoftParams;  // x = contact soft enabled, y = ddgi enabled, z = ddgi intensity, w = ddgi debug overlay
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
    vec4 shadowParams;  // z = compare active (0/1), w = 1/shadowMapSize
    vec4 iblParams;     // x = intensity, y = enabled, z = prefilter max mip, w = specular shadow min
} lightingGlobals;
layout(set = 0, binding = 6) uniform sampler2DShadow shadowMap;
layout(set = 0, binding = 7) uniform samplerCube irradianceMap;
layout(set = 0, binding = 8) uniform samplerCube prefilterMap;
layout(set = 0, binding = 9) uniform sampler2D brdfLut;
layout(set = 0, binding = 10) uniform samplerCube skyMap;
// binding 11: non-compare depth read (descriptor parity; optional tooling).
layout(set = 0, binding = 11) uniform sampler2D shadowMapDepth;
layout(set = 0, binding = 12) uniform sampler2D gbufferWorldPosition;
layout(set = 0, binding = 13) uniform sampler2D aoMap;
layout(set = 0, binding = 14) uniform sampler2D hiZPyramid;
layout(set = 0, binding = 15) uniform sampler2D ddgiProbeAtlas;

layout(location = 0) in vec2 vUV;
layout(location = 0) out vec4 outColor;

vec3 reconstructWorldPos(vec2 aUV, float aDepth)
{
    const vec4 ndc = vec4(aUV * 2.0 - 1.0, aDepth, 1.0);
    const vec4 worldH = pc.invViewProj * ndc;
    return worldH.xyz / worldH.w;
}

vec4 applyDebugView(vec4 aLitColor, vec3 aWorldNormal, float aDepth, vec3 aWorldPos, vec2 aUV, float aAo)
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
        const float visibility = Pbr_ShadowVisibility(shadowMap, lightingGlobals.lightViewProj, aWorldPos, lightingGlobals.shadowParams);
        return vec4(vec3(visibility), 1.0);
    }
    if (viewMode == kDebugViewAo) {
        return vec4(vec3(aAo), 1.0);
    }
    if (viewMode == kDebugViewHiZ) {
        const uint mip = min(pc.grid.w, 7u);
        const float hiZ = textureLod(hiZPyramid, aUV, float(mip)).r;
        return vec4(vec3(hiZ), 1.0);
    }
    if (viewMode == kDebugViewDdgi) {
        return aLitColor;
    }
    return aLitColor;
}

vec3 sampleDdgiIrradiance(vec3 aWorldPos) {
    const float ddgiEnabled = pc.contactSoftParams.y;
    if (ddgiEnabled < 0.5) {
        return vec3(0.0);
    }

    // S8 v0: map world XY to probe atlas UV; Y contributes as vertical term.
    const vec2 uv = fract(aWorldPos.xy * 0.05 + vec2(0.5));
    const vec3 probe = texture(ddgiProbeAtlas, uv).rgb;
    return probe * pc.contactSoftParams.z;
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

    float sunShadow = Pbr_ShadowVisibility(shadowMap, lightingGlobals.lightViewProj, worldPos, lightingGlobals.shadowParams);
    float ao = texture(aoMap, vUV).r;
    if (pc.contactSoftParams.x > 0.5) {
        const vec2 contact = texture(aoMap, vUV).rg;
        ao = contact.r;
        sunShadow = contact.g;
    }

    const uint iblEnabled = uint(lightingGlobals.iblParams.y + 0.5);
    // sunShadow may be contact-soft blurred; specular IBL only (diffuse stays full).
    const float specularShadowScale = Pbr_IblSpecularShadowScale(sunShadow, lightingGlobals.iblParams.w);
    vec3 ambient = Pbr_EvalIbl(N, V, albedo, metallic, roughness, irradianceMap, prefilterMap, brdfLut, lightingGlobals.iblParams.x, iblEnabled,
                               lightingGlobals.iblParams.z, specularShadowScale);
    if (iblEnabled == 0u) {
        ambient = pc.ambientColor.rgb * albedo;
    }

    const float aoEnabled = pc.legacyAndDebug.x;
    const float aoIntensity = pc.legacyAndDebug.y;
    const float aoPower = pc.legacyAndDebug.w;
    ao = mix(1.0, pow(clamp(ao, 0.0, 1.0), aoPower), aoIntensity);
    if (aoEnabled < 0.5) {
        ao = 1.0;
    }

    vec3 ddgi = sampleDdgiIrradiance(worldPos);
    vec3 lit = (ambient + ddgi) * ao;

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

    if (uint(pc.legacyAndDebug.z + 0.5) == kDebugViewDdgi) {
        outColor = vec4(ddgi, 1.0);
        return;
    }
    outColor = applyDebugView(vec4(lit, 1.0), N, depth, worldPos, vUV, ao);
}
