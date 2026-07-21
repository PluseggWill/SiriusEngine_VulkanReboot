#pragma once

#include <cstdint>

// Push constants for ClusterBuild.comp / DeferredLighting.frag — must match GLSL layouts.
struct Gpu_ClusterBuildPushConstants {
    uint32_t clusterCount = 0;
    uint32_t lightCount   = 0;
    uint32_t pad0         = 0;
    uint32_t pad1         = 0;
};

struct Gpu_DeferredLightingPushConstants {
    uint32_t tilesX     = 0;
    uint32_t tilesY     = 0;
    uint32_t tileSize   = 16u;
    uint32_t depthSlice = 0;  // stub: always 0
    float    ambientColor[ 4 ];
    float    viewWorldPos[ 4 ];
    float    legacySpecularStrength = 0.0f;  // unused under PBR; keeps vec4 layout for legacyAndDebug.xy
    float    legacyShininess        = 1.0f;
    float    debugView              = 0.0f;  // Gfx_DebugViewMode packed (match forward fogDistances.w)
    float    legacyPad              = 0.0f;
    float    contactSoftEnabled     = 1.0f;  // contactSoftParams.x
    float    ddgiEnabled            = 0.0f;  // contactSoftParams.y
    float    ddgiIntensity          = 0.0f;  // contactSoftParams.z
    float    ddgiDebugOverlay       = 0.0f;  // contactSoftParams.w
    uint32_t ddgiProbeCountX        = 1;
    uint32_t ddgiProbeCountY        = 1;
    uint32_t ddgiProbeCountZ        = 1;
    uint32_t ddgiProbeCountPad      = 0u;  // bit0=bent-normal cones, bit2=local reflection probe
    float    ddgiVolumeMin[ 4 ]     = {};
    float    ddgiVolumeMax[ 4 ]     = {};
    float    invViewProj[ 16 ];  // column-major glm; inverse(proj * view)
};

static_assert( sizeof( Gpu_ClusterBuildPushConstants ) == 16, "push constants must match ClusterBuild.comp" );
static_assert( sizeof( Gpu_DeferredLightingPushConstants ) == 192, "push constants must match DeferredLighting.frag" );
