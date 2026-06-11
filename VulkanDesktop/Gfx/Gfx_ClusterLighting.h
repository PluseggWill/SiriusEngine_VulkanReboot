#pragma once

#include <cstdint>

// FG v0 hybrid deferred: cluster grid + SSBO layouts (CPU / GLSL must stay in sync).
namespace Gfx_ClusterLighting {

constexpr uint32_t kTileSize                  = 16u;
constexpr uint32_t kDepthSlices               = 24u;
constexpr uint32_t kMaxLights                 = 1u;  // slice 2 stub: directional sun only
constexpr uint32_t kMaxLightsPerCluster       = 8u;
constexpr uint32_t kClusterBuildWorkgroupSize = 64u;

inline uint32_t TilesForExtent( uint32_t aPixels, uint32_t aTileSize = kTileSize ) {
    return ( aPixels + aTileSize - 1u ) / aTileSize;
}

inline uint32_t ClusterCount( uint32_t aWidth, uint32_t aHeight ) {
    return TilesForExtent( aWidth ) * TilesForExtent( aHeight ) * kDepthSlices;
}

// Screen-space cluster index (slice 3 stub: fixed depth slice 0).
inline uint32_t ClusterIndexFromTile( uint32_t aTileX, uint32_t aTileY, uint32_t aTilesX, uint32_t aTilesY, uint32_t aDepthSlice = 0u ) {
    return aTileX + aTileY * aTilesX + aDepthSlice * aTilesX * aTilesY;
}

// std430 — matches ClusterBuild.comp
struct GpuClusterLight {
    float direction[ 4 ];  // xyz toward light (normalized on CPU)
    float color[ 4 ];
};

struct GpuClusterLightList {
    uint32_t lightCount;
    uint32_t lightIndices[ kMaxLightsPerCluster ];
};

struct Gfx_ClusterBuildPushConstants {
    uint32_t clusterCount = 0;
    uint32_t lightCount   = 0;
    uint32_t pad0         = 0;
    uint32_t pad1         = 0;
};

// Push constants for DeferredLighting.frag (grid + ambient + specular v0 via depth reconstruct).
struct Gfx_DeferredLightingPushConstants {
    uint32_t tilesX     = 0;
    uint32_t tilesY     = 0;
    uint32_t tileSize   = kTileSize;
    uint32_t depthSlice = 0;  // slice 3 stub: always 0
    float    ambientColor[ 4 ];
    float    viewWorldPos[ 4 ];
    float    specularStrength = 0.0f;
    float    shininess        = 1.0f;
    float    debugView        = 0.0f;  // Gfx_DebugViewMode packed (match forward fogDistances.w)
    float    pad1             = 0.0f;
    float    invViewProj[ 16 ];  // column-major glm; inverse(proj * view) for depth reconstruct
};

static_assert( sizeof( GpuClusterLight ) == 32, "GpuClusterLight must match ClusterBuild.comp std430 layout" );
static_assert( sizeof( GpuClusterLightList ) == sizeof( uint32_t ) * ( 1 + kMaxLightsPerCluster ), "GpuClusterLightList must match ClusterBuild.comp" );
static_assert( sizeof( Gfx_ClusterBuildPushConstants ) == 16, "push constants must match ClusterBuild.comp" );
static_assert( sizeof( Gfx_DeferredLightingPushConstants ) == 128, "push constants must match DeferredLighting.frag" );

}  // namespace Gfx_ClusterLighting
