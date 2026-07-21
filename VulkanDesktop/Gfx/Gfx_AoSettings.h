#pragma once

#include "../RenderContract/Gpu_AoSettings.h"

using Gfx_AoSettings = Gpu_AoSettings;

inline uint32_t Gfx_ComputeHiZMipLevelCount( uint32_t aWidth, uint32_t aHeight, uint32_t aMaxMips = 8u ) {
    return Gpu_ComputeHiZMipLevelCount( aWidth, aHeight, aMaxMips );
}
