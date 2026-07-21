#pragma once

#include "../RenderContract/Gpu_ClusterLight.h"

#include "../RenderContract/Gpu_ClusterPush.h"

#include <cstdint>

// FG v0 hybrid deferred: cluster grid helpers (GPU light + push layouts live in RenderContract).

namespace Gfx_ClusterLighting {

constexpr uint32_t kTileSize = 16u;

constexpr uint32_t kDepthSlices = 24u;

constexpr uint32_t kMaxLights = 1u;  // slice 2 stub: directional sun only

constexpr uint32_t kMaxLightsPerCluster = Gpu_ClusterMaxLightsPerCluster;

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

}  // namespace Gfx_ClusterLighting
