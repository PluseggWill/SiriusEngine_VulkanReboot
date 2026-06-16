#pragma once

#include "../RenderContract/GpuAoSettings.h"

using Gfx_AoSettings = GpuAoSettings;

inline uint32_t Gfx_ComputeHiZMipLevelCount( uint32_t aWidth, uint32_t aHeight, uint32_t aMaxMips = 8u ) {
    return Gpu_ComputeHiZMipLevelCount( aWidth, aHeight, aMaxMips );
}
