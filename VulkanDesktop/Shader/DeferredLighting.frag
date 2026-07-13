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
    uvec4 ddgiProbeCounts;   // xyz = probe counts; w = feature flags (bit0 cones, bit2 local probe)
    vec4 ddgiVolumeMin;
    vec4 ddgiVolumeMax;
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
    vec4 shadowParams;  // x = SSR enabled, y = specular occlusion enabled, z = compare active (0/1), w = 1/shadowMapSize
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
layout(set = 0, binding = 16) uniform sampler2D ddgiVisibilityAtlas;
layout(set = 0, binding = 17) uniform sampler2D ssrMap;
layout(set = 0, binding = 18) uniform sampler2D bentNormalMap;

layout(location = 0) in vec2 vUV;
layout(location = 0) out vec4 outColor;

vec3 reconstructWorldPos(vec2 aUV, float aDepth)
{
    // aDepth = G-buffer clip Z (camera convention). Shadow uses ZO lightViewProj + ClipDepth.glsl.
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

uint ddgiProbeIndex(uvec3 aCoord, uvec3 aDims) {
    return aCoord.x + aCoord.y * aDims.x + aCoord.z * aDims.x * aDims.y;
}

vec2 ddgiProbeUv(uint aProbeIndex, uvec2 aAtlasSize) {
    const uint px = aProbeIndex % aAtlasSize.x;
    const uint py = aProbeIndex / aAtlasSize.x;
    return (vec2(px, py) + 0.5) / vec2(aAtlasSize);
}

vec3 sampleDdgiIrradiance(vec3 aWorldPos, vec3 aNormal) {
    const float ddgiEnabled = pc.contactSoftParams.y;
    if (ddgiEnabled < 0.5) {
        return vec3(0.0);
    }

    const vec3 minP = pc.ddgiVolumeMin.xyz;
    const vec3 maxP = pc.ddgiVolumeMax.xyz;
    const vec3 extents = maxP - minP;
    if (extents.x <= 1e-4 || extents.y <= 1e-4 || extents.z <= 1e-4) {
        return vec3(0.0);
    }

    const uvec3 dims = max(pc.ddgiProbeCounts.xyz, uvec3(1u));
    const vec3 local = clamp((aWorldPos - minP) / extents, 0.0, 1.0);
    const vec3 gridPos = local * vec3(max(dims - uvec3(1u), uvec3(1u)));
    const ivec3 base = ivec3(floor(gridPos));
    const vec3 fracW = fract(gridPos);
    const uvec2 atlasSize = uvec2(textureSize(ddgiProbeAtlas, 0));

    vec3 accum = vec3(0.0);
    float accumW = 0.0;
    for (uint dz = 0u; dz < 2u; ++dz) {
        for (uint dy = 0u; dy < 2u; ++dy) {
            for (uint dx = 0u; dx < 2u; ++dx) {
                const uvec3 c = uvec3(clamp(base + ivec3(dx, dy, dz), ivec3(0), ivec3(dims) - ivec3(1)));
                const uint idx = ddgiProbeIndex(c, dims);
                const vec2 uv = ddgiProbeUv(idx, atlasSize);
                const vec3 probeIrr = texture(ddgiProbeAtlas, uv).rgb;
                const float probeVis = texture(ddgiVisibilityAtlas, uv).r;

                const vec3 probePos = mix(minP, maxP, vec3(c) / vec3(max(dims - uvec3(1u), uvec3(1u))));
                const vec3 toProbe = normalize(probePos - aWorldPos);
                const float facing = max(0.05, dot(aNormal, toProbe) * 0.5 + 0.5);
                const vec3 w3 = mix(vec3(1.0) - fracW, fracW, vec3(dx, dy, dz));
                const float triW = w3.x * w3.y * w3.z;
                const float w = triW * facing * clamp(probeVis, 0.02, 1.0);
                accum += probeIrr * w;
                accumW += w;
            }
        }
    }
    if (accumW <= 1e-6) {
        return vec3(0.0);
    }
    return (accum / accumW);
}

vec3 sampleDdgiDebugView(vec3 aWorldPos, vec3 aNormal) {
    const vec3 minP = pc.ddgiVolumeMin.xyz;
    const vec3 maxP = pc.ddgiVolumeMax.xyz;
    const vec3 extents = maxP - minP;
    if (extents.x <= 1e-4 || extents.y <= 1e-4 || extents.z <= 1e-4) {
        return vec3(0.0);
    }

    const vec3 local = clamp((aWorldPos - minP) / extents, 0.0, 1.0);
    const uvec3 dims = max(pc.ddgiProbeCounts.xyz, uvec3(1u));
    const vec3 gridPos = local * vec3(max(dims - uvec3(1u), uvec3(1u)));
    const ivec3 nearest = ivec3(round(gridPos));
    const uvec3 c = uvec3(clamp(nearest, ivec3(0), ivec3(dims) - ivec3(1)));
    const uvec2 atlasSize = uvec2(textureSize(ddgiProbeAtlas, 0));
    const uint idx = ddgiProbeIndex(c, dims);
    const vec2 uv = ddgiProbeUv(idx, atlasSize);
    const vec3 probeIrr = texture(ddgiProbeAtlas, uv).rgb;
    const float probeVis = texture(ddgiVisibilityAtlas, uv).r;

    // Grid line emphasis to show probe-cell structure in scene.
    const vec3 gridFrac = abs(fract(gridPos) - 0.5);
    const float line = 1.0 - smoothstep(0.42, 0.50, min(gridFrac.x, min(gridFrac.y, gridFrac.z)));
    const float facing = max(0.0, dot(aNormal, normalize(mix(minP, maxP, vec3(c) / vec3(max(dims - uvec3(1u), uvec3(1u)))) - aWorldPos)));

    vec3 debugColor = probeIrr * (0.4 + 0.6 * clamp(probeVis, 0.0, 1.0));
    debugColor += vec3(0.15, 0.7, 1.0) * line * 0.35;
    debugColor *= (0.25 + 0.75 * facing);
    return debugColor;
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
    const vec3 R = reflect(-V, N);
    const float NdotV = max(dot(N, V), 0.0);
    vec3 diffuseIbl  = vec3(0.0);
    vec3 specularIbl = vec3(0.0);
    if (iblEnabled != 0u) {
        const float clampedRoughness = clamp(max(roughness, PBR_MIN_ROUGHNESS), 0.0, 1.0);
        const vec3  F0               = mix(vec3(PBR_DIELECTRIC_F0), albedo, metallic);
        const vec3  F                = Pbr_FresnelSchlick(NdotV, F0);
        // Diffuse IBL — not attenuated by sun shadow; full hemispherical irradiance.
        const vec3 kD = (vec3(1.0) - F) * (1.0 - metallic);
        diffuseIbl = Pbr_SampleIrradiance(irradianceMap, N) * kD * albedo * lightingGlobals.iblParams.x;
        // Specular IBL — attenuated in sun-shadow zones (removes dominant sun-specular baked into prefilter).
        const float specularShadowScale = Pbr_IblSpecularShadowScale(sunShadow, lightingGlobals.iblParams.w);
        const vec3  prefilteredColor    = Pbr_SamplePrefilter(prefilterMap, R, clampedRoughness, lightingGlobals.iblParams.z);
        const vec2  brdf                = Pbr_BrdfLut(NdotV, clampedRoughness, brdfLut);
        vec3 specularEnv = prefilteredColor * (F * brdf.x + brdf.y) * specularShadowScale * lightingGlobals.iblParams.x;
        // Reflection stack (Frostbite §4.9): distant prefilter → local box probe → SSR (before specular occlusion).
        const uint featureFlags = pc.ddgiProbeCounts.w;
        const bool localProbe = (featureFlags & 2u) != 0u;
        if (localProbe) {
            const vec3 probeMin = pc.ddgiVolumeMin.xyz;
            const vec3 probeMax = pc.ddgiVolumeMax.xyz;
            const vec3 boxCenter = 0.5 * (probeMin + probeMax);
            const vec3 boxExtents = max(0.5 * (probeMax - probeMin), vec3(1e-4));
            const vec3 rel = abs(worldPos - boxCenter) / boxExtents;
            const float probeWeight = clamp(1.0 - max(rel.x, max(rel.y, rel.z)), 0.0, 1.0) * pc.contactSoftParams.z;
            const vec3 probeSpec = Pbr_SampleParallaxBoxProbe(prefilterMap, worldPos, R, probeMin, probeMax, clampedRoughness, lightingGlobals.iblParams.z);
            specularEnv = mix(specularEnv, probeSpec * lightingGlobals.iblParams.x, probeWeight);
        }
        specularIbl = specularEnv;
        const float ssrEnabled = lightingGlobals.shadowParams.x;
        if (ssrEnabled > 0.5) {
            const vec4 ssr = texture(ssrMap, vUV);
            specularIbl = mix(specularIbl, ssr.rgb, clamp(ssr.a, 0.0, 1.0));
        }
    }
    vec3 ambient = diffuseIbl + specularIbl;
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

    vec3 ddgi = sampleDdgiIrradiance(worldPos, N);
    // AO attenuates diffuse IBL + DDGI; specular IBL uses roughness-aware specular occlusion (Frostbite §4.10.2).
    const float specularOccEnabled = lightingGlobals.shadowParams.y;
    float specularOcc = 1.0;
    if (specularOccEnabled > 0.5) {
        const uint featureFlags = pc.ddgiProbeCounts.w;
        const bool useCones = (featureFlags & 1u) != 0u;
        if (useCones) {
            vec3 bentN = Pbr_DecodeOctahedral(texture(bentNormalMap, vUV).rg);
            if (length(bentN) < 0.1) {
                bentN = N;
            }
            specularOcc = Pbr_SpecularOcclusionCones(bentN, R, ao, roughness);
        } else {
            specularOcc = Pbr_SpecularOcclusion(NdotV, ao, roughness);
        }
    }
    vec3 lit = (diffuseIbl + ddgi) * ao + specularIbl * specularOcc;
    ambient = diffuseIbl + specularIbl;  // keep ambient for ddgi-debug overlay blend below
    lit = mix(lit, ddgi, clamp(pc.contactSoftParams.w, 0.0, 1.0));

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
        outColor = vec4(sampleDdgiDebugView(worldPos, N), 1.0);
        return;
    }
    outColor = applyDebugView(vec4(lit, 1.0), N, depth, worldPos, vUV, ao);
}
