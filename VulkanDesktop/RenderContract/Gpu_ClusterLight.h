#pragma once

#include <cstdint>

// std430 cluster light SSBO layouts — must match ClusterBuild.comp / DeferredLighting.frag consumers.
inline constexpr uint32_t Gpu_ClusterMaxLightsPerCluster = 8u;

struct Gpu_ClusterLight {
    float direction[ 4 ];  // xyz toward light (normalized on CPU)
    float color[ 4 ];
};

struct Gpu_ClusterLightList {
    uint32_t lightCount = 0;
    uint32_t lightIndices[ Gpu_ClusterMaxLightsPerCluster ]{};
};

static_assert( sizeof( Gpu_ClusterLight ) == 32, "Gpu_ClusterLight must match ClusterBuild.comp std430 layout" );
static_assert( sizeof( Gpu_ClusterLightList ) == sizeof( uint32_t ) * ( 1 + Gpu_ClusterMaxLightsPerCluster ), "Gpu_ClusterLightList must match ClusterBuild.comp" );
