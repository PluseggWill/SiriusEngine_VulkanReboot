#pragma once

#include <cstdint>

#include "Gfx_AoMethod.h"

// CPU-side screen-space AO tuning (HybridDeferred; ImGui via Util_LightingPanel).
// Algorithm selection: Gfx_AoMethod — swap compute shader in Vk_AoPass without touching deferred.
struct Gfx_AoSettings {
    Gfx_AoMethod myMethod                = Gfx_AoMethod::HbaoPlus;
    bool         myEnabled               = true;
    float        myRadius                = 0.1f;    // view/world sample radius (method-specific scale in shader)
    float        myBias                  = 0.02f;   // horizon / depth compare bias
    float        myIntensity             = 0.5f;    // composite strength (deferred)
    float        myPower                 = 1.5f;    // contrast curve (applied once in deferred)
    uint32_t     myHiZDebugMip           = 0u;      // Util_RenderDebugPanel Hi-Z slice
    uint32_t     myHbaoDirections        = 4u;      // HBAO+ horizon directions (1–8)
    uint32_t     myHbaoSteps             = 4u;      // HBAO+ steps per direction (1–8)
    uint32_t     myGtaoSlices            = 8u;      // GTAO slice count (1–16)
    uint32_t     myGtaoStepsPerSlice     = 4u;      // GTAO steps per slice (1–16)
    float        myGtaoThickness         = 0.5f;    // reject occluders beyond this view-space height
    float        myGtaoFalloff           = 2.0f;    // distance falloff power (screen march)
    float        myUpsampleDepthSigma    = 0.025f;  // half-res → full-res bilateral threshold (NDC depth)
    bool         myContactSoftEnabled    = true;    // joint screen-space AO + shadow blur (ShadowAoSoft pass)
    float        myContactSoftBlurRadius = 2.0f;    // blur tap stride (1–4 px)
    float        myContactSoftDepthSigma = 0.025f;  // contact blur bilateral threshold (NDC)
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
