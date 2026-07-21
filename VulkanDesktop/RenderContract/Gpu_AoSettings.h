#pragma once

#include "Gpu_AoMethod.h"

#include <cstdint>

// Runtime AO tuning (ImGui → Renderer session). Not scene SoA data.
struct Gpu_AoSettings {
    Gpu_AoMethod myMethod                = Gpu_AoMethod::Gtao;
    bool         myEnabled               = true;
    float        myRadius                = 0.26f;
    float        myBias                  = 0.024f;
    float        myIntensity             = 0.55f;
    float        myPower                 = 1.5f;
    uint32_t     myHiZDebugMip           = 0u;
    uint32_t     myHbaoDirections        = 4u;
    uint32_t     myHbaoSteps             = 4u;
    uint32_t     myGtaoSlices            = 4u;
    uint32_t     myGtaoStepsPerSlice     = 3u;
    float        myGtaoFalloff           = 2.0f;
    float        myUpsampleDepthSigma    = 0.025f;
    float        myNormalAwareRadius     = 0.35f;
    bool         myTemporalEnabled       = false;
    float        myTemporalBlend         = 0.9f;
    bool         myContactSoftEnabled    = true;
    float        myContactSoftBlurRadius = 1.0f;   // ±2 texels with 5-tap kernel (was 2.0 = ±4, dissolved hard contact shadows)
    float        myContactSoftDepthSigma = 0.04f;  // tighter depth-edge rejection (was 0.025, leaked across depth discontinuities)
};

inline uint32_t Gpu_ComputeHiZMipLevelCount( uint32_t aWidth, uint32_t aHeight, uint32_t aMaxMips = 8u ) {
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
