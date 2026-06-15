#pragma once

#include <cstdint>

// CPU-side SSAO + Hi-Z tuning (HybridDeferred; ImGui via Util_LightingPanel).
struct Gfx_AoSettings {
    bool     myEnabled     = true;
    float    myRadius      = 0.45f;   // world-space sample radius
    float    myBias        = 0.025f;  // depth compare bias
    float    myIntensity   = 1.0f;    // composite strength
    float    myPower       = 2.0f;    // contrast curve
    uint32_t myHiZDebugMip = 0u;      // Util_RenderDebugPanel Hi-Z slice
};

inline uint32_t Gfx_ComputeHiZMipLevelCount( uint32_t aWidth, uint32_t aHeight, uint32_t aMaxMips = 8u ) {
    const uint32_t maxDim = aWidth > aHeight ? aWidth : aHeight;
    if ( maxDim <= 1u ) {
        return 1u;
    }
    uint32_t levels = 1u;
    uint32_t dim    = maxDim;
    while ( dim > 1u && levels < aMaxMips ) {
        dim >>= 1u;
        ++levels;
    }
    return levels;
}
