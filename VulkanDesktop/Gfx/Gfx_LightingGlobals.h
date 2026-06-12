#pragma once

#include <glm/glm.hpp>

// std140 UBO — matches LightingGlobals in lit/deferred shaders (Set 0 binding eVk_LightingGlobalsBinding).
struct GpuLightingGlobals {
    alignas( 16 ) glm::mat4 myLightViewProj{};
    alignas( 16 ) glm::vec4 myShadowParams{ 0.0f, 0.0f, 1.0f, 0.0f };  // z = enabled (0/1)
    alignas( 16 ) glm::vec4 myIblParams{ 1.0f, 1.0f, 4.0f, 0.0f };     // x = intensity, y = enabled, z = prefilter max mip
};

static_assert( sizeof( GpuLightingGlobals ) == 96, "GpuLightingGlobals must be std140-compatible (96 bytes)" );

struct Gfx_LightingSettings {
    bool  myShadowsEnabled = true;
    bool  myIblEnabled     = true;
    float myIblIntensity   = 1.0f;
};

inline GpuLightingGlobals Gfx_BuildLightingGlobals( const Gfx_LightingSettings& aSettings, const glm::mat4& aLightViewProj, float aPrefilterMaxMipLevel ) {
    GpuLightingGlobals globals{};
    globals.myLightViewProj  = aLightViewProj;
    globals.myShadowParams.z = aSettings.myShadowsEnabled ? 1.0f : 0.0f;
    globals.myIblParams.x    = aSettings.myIblIntensity;
    globals.myIblParams.y    = aSettings.myIblEnabled ? 1.0f : 0.0f;
    globals.myIblParams.z    = aPrefilterMaxMipLevel;
    return globals;
}
