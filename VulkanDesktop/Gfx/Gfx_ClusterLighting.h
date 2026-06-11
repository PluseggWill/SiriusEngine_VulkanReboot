#pragma once

#include <cstdint>

// FG v0 slice 2: clustered deferred grid constants (CPU + GLSL must stay in sync).
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

static_assert( sizeof( GpuClusterLight ) == 32, "GpuClusterLight must match ClusterBuild.comp std430 layout" );
static_assert( sizeof( GpuClusterLightList ) == sizeof( uint32_t ) * ( 1 + kMaxLightsPerCluster ), "GpuClusterLightList must match ClusterBuild.comp" );
static_assert( sizeof( Gfx_ClusterBuildPushConstants ) == 16, "push constants must match ClusterBuild.comp" );

}  // namespace Gfx_ClusterLighting
