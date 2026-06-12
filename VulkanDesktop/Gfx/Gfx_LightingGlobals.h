#pragma once

#include <glm/glm.hpp>

// std140 UBO — matches LightingGlobals in lit/deferred shaders (Set 0 binding eVk_LightingGlobalsBinding).
struct GpuLightingGlobals {
    alignas( 16 ) glm::mat4 myLightViewProj{};
    alignas( 16 ) glm::vec4 myShadowParams{ 0.0025f, 0.001f, 1.0f, 1.0f / 2048.0f };  // bias, normalOffset, enabled, texelSize
    alignas( 16 ) glm::vec4 myIblParams{ 1.0f, 1.0f, 0.0f, 0.0f };                     // intensity, enabled, pad, pad
};

static_assert( sizeof( GpuLightingGlobals ) == 96, "GpuLightingGlobals must be std140-compatible (96 bytes)" );

struct Gfx_LightingSettings {
    bool  myShadowsEnabled = true;
    bool  myIblEnabled     = true;
    float myIblIntensity   = 1.0f;
};

inline GpuLightingGlobals Gfx_BuildLightingGlobals( const Gfx_LightingSettings& aSettings, const glm::mat4& aLightViewProj, float aShadowMapSize ) {
    GpuLightingGlobals globals{};
    globals.myLightViewProj  = aLightViewProj;
    globals.myShadowParams.x = 0.0025f;
    globals.myShadowParams.y = 0.001f;
    globals.myShadowParams.z = aSettings.myShadowsEnabled ? 1.0f : 0.0f;
    globals.myShadowParams.w = 1.0f / aShadowMapSize;
    globals.myIblParams.x    = aSettings.myIblIntensity;
    globals.myIblParams.y    = aSettings.myIblEnabled ? 1.0f : 0.0f;
    return globals;
}
